#include <algorithm>
#include <string>

#include "GameInterfaces.h"
#include "PexCache.h"
#include "Utilities.h"

#include "filesystem.h"
#include "resourcefile.h"

namespace DebugServer
{

static void NormalizeArchivePath(std::string &path)
{
	if (path.find(":") != std::string::npos)
	{
		path.erase(std::remove(path.begin(), path.end(), ':'), path.end());
	}
}

bool PexCache::HasScript(const int scriptReference)
{
	scripts_lock scriptLock(m_scriptsMutex);
	return m_scripts.find(scriptReference) != m_scripts.end();
}
bool PexCache::HasScript(const std::string &scriptName) { return HasScript(GetScriptReference(scriptName)); }

PexCache::BinaryPtr PexCache::GetCachedScript(const int ref)
{
	scripts_lock scriptLock(m_scriptsMutex);
	const auto entry = m_scripts.find(ref);
	return entry != m_scripts.end() ? entry->second : nullptr;
}

void PexCache::PrintOutAllLoadedScripts()
{
	scripts_lock scriptLock(m_scriptsMutex);
	for (auto &script : m_scripts)
	{
		Printf("Loaded %zu functions from script: %s", script.second->GetFunctionCount(), script.second->GetQualifiedPath().c_str());
	}
}

PexCache::BinaryPtr PexCache::GetScript(const dap::Source &source)
{
	auto binary = GetCachedScript(GetSourceReference(source));
	if (binary)
	{
		return binary;
	}
	return GetScript(GetScriptWithQual(source.path.value(""), source.origin.value("")));
}


PexCache::BinaryPtr PexCache::makeEmptyBinary(const std::string &scriptPath, int lump)
{
	auto binary = std::make_shared<Binary>();
	auto truncScriptPath = GetScriptPathNoQual(scriptPath);
	binary->lump = lump;
	int wadnum = fileSystem.GetFileContainer(binary->lump);
	binary->scriptName = FileSys::ExtractBaseName(truncScriptPath.c_str(), true);
	binary->unqualifiedScriptPath = truncScriptPath;
	// check for the archive name in the script path
	binary->archivePath = wadnum >= 0 ? fileSystem.GetResourceFileFullName(wadnum) : GetArchiveNameFromPath(scriptPath);
	binary->archiveName = wadnum >= 0 ? fileSystem.GetResourceFileName(wadnum) : binary->archivePath;
	NormalizeArchivePath(binary->archivePath);
	binary->scriptReference = GetScriptReference(binary->GetQualifiedPath());
	return binary;
}

void PexCache::PopulateCodeMap(PexCache::BinaryPtr binary, Binary::FunctionCodeMap &functionCodeMap)
{
	if (!binary)
	{
		return;
	}
	auto qualPath = binary->GetQualifiedPath();
	int i = 0;
	for (auto &func : binary->functions)
	{
		i++;
		for (auto &variant : func.second->Variants)
		{
			auto scriptFunc = GetVMScriptFunction(variant.Implementation);
			if (!scriptFunc || IsFunctionAbstract(scriptFunc)) continue;
			Binary::FunctionCodeMap::range_type codeRange(scriptFunc->Code, scriptFunc->Code + scriptFunc->CodeSize, scriptFunc);
			functionCodeMap.insert(true, codeRange);
		}
	}
	for (auto &pair : binary->stateFunctions)
	{
		auto scriptFunc = GetVMScriptFunction(pair.second);
		if (!scriptFunc || IsFunctionAbstract(scriptFunc)) continue;
		Binary::FunctionCodeMap::range_type codeRange(scriptFunc->Code, scriptFunc->Code + scriptFunc->CodeSize, scriptFunc);
		functionCodeMap.insert(true, codeRange);
	}
}

void PexCache::ScanAllScripts()
{
	scripts_lock scriptLock(m_scriptsMutex);
	m_scripts.clear();
	m_globalCodeMap.clear();

	ScanScriptsInContainer(-1, m_scripts);
	for (auto &bin : m_scripts)
	{
		PopulateCodeMap(bin.second, m_globalCodeMap);
	}
#ifndef NDEBUG
	PrintOutAllLoadedScripts();
#endif
}

void PexCache::PopulateFromPaths(const std::map<std::string, int> &scripts, BinaryMap &p_scripts, bool clobber)
{
	for (auto &script : scripts)
	{
		auto ref = GetScriptReference(script.first);
		if (clobber || p_scripts.find(ref) == p_scripts.end())
		{
			p_scripts[ref] = makeEmptyBinary(script.first, script.second);
		}
	}
}


void PexCache::ScanScriptsInContainer(int baselump, BinaryMap &p_scripts, const std::string &filter)
{
	// TODO: Get md5 hash of script
	TArray<PNamespace *> namespaces;
	std::string filterPath = filter;
	std::vector<int> filterRefs;
	if (!filter.empty())
	{
		// get the archive name
		std::string namespaceName = GetArchiveNameFromPath(filter);
		if (!namespaceName.empty())
		{
			int containerLump = fileSystem.CheckIfResourceFileLoaded(namespaceName.c_str());
			if (containerLump == -1)
			{
				return;
			}
		}
		auto found = FindScripts(filter, baselump);
		if (found.empty())
		{
			return;
		}
		for (auto &script : found)
		{
			filterRefs.push_back(GetScriptReference(script.first));
		}
		PopulateFromPaths(found, p_scripts, true);
	}
	else
	{
		auto found = FindAllScripts(baselump);
		if (found.empty())
		{
			return;
		}
		PopulateFromPaths(found, p_scripts, true);
	}
	auto addEmptyBinIfNotExists = [&](int ref, const char *scriptPath)
	{
		if (p_scripts.find(ref) == p_scripts.end())
		{
			p_scripts[ref] = makeEmptyBinary(scriptPath, GetScriptFileID(scriptPath));
		}
	};
	for (auto &func : VMFunction::AllFunctions){
		auto vmscriptfunc = GetVMScriptFunction(func);
		if (!vmscriptfunc){
			continue;
		}
		std::string source_name = vmscriptfunc->SourceFileName.GetChars();
		if (source_name.empty() && !IsFunctionAbstract(vmscriptfunc)){
			auto class_name = GetFunctionClassName(func);
			if (class_name.empty()){
				continue;
			}
			auto *cls = PClass::FindClass(class_name.c_str());
			if (!cls){
				continue;
			}
			source_name = cls->SourceLumpName.GetChars();
		}
		if (source_name.empty()){
			continue;
		}
		auto ref = GetScriptReference(source_name);
		if (!filterRefs.empty() && std::find(filterRefs.begin(), filterRefs.end(), ref) == filterRefs.end())
		{
			continue;
		}
		PFunction * pfunc = GetFunctionSymbol(func);
		addEmptyBinIfNotExists(ref, vmscriptfunc->SourceFileName.GetChars());
		if (!pfunc && IsNonAbstractScriptFunction(func)){
			p_scripts[ref]->stateFunctions[vmscriptfunc->QualifiedName] = vmscriptfunc;
		} else if (pfunc) {
			std::string symbolName = pfunc->SymbolName.GetChars();
			for (auto &variant : pfunc->Variants)
			{
				if (variant.Implementation)
				{
					auto vmfunc = variant.Implementation;
					if (!IsNonAbstractScriptFunction(vmfunc))
					{
						continue;
					}
					auto scriptFunc = static_cast<VMScriptFunction *>(vmfunc);
					// add to the function map
					p_scripts[ref]->functions[vmscriptfunc->QualifiedName] = pfunc;
				}
			}
		}
	}

	for (auto &bin : p_scripts)
	{
		bin.second->PopulateFunctionMaps();
	}
}


std::shared_ptr<Binary> PexCache::AddScript(const std::string &scriptPath)
{
	scripts_lock scriptLock(m_scriptsMutex);
	std::shared_ptr<Binary> bin;

	ScanScriptsInContainer(-1, m_scripts, scriptPath);
	bin = GetCachedScript(GetScriptReference(scriptPath));
	PopulateCodeMap(bin, m_globalCodeMap);

	return bin;
}

std::vector<VMFunction *> PexCache::GetFunctionsAtAddress(void *address)
{
	std::lock_guard<std::recursive_mutex> lock(m_scriptsMutex);
	if (!address)
	{
		return {};
	}
	std::stack<Binary::FunctionCodeMap::iterator> found;
	found = m_globalCodeMap.find_ranges(address);
	std::vector<VMFunction *> funcs;
	while (!found.empty())
	{
		auto func = found.top()->mapped();
		if (func)
		{
			funcs.push_back(func);
		}
		found.pop();
	}

	return funcs;
}
std::shared_ptr<Binary> PexCache::GetScript(std::string fqsn)
{
	uint32_t reference = GetScriptReference(fqsn);
	auto binary = GetCachedScript(reference);
	if (binary)
	{
		return binary;
	}
	return AddScript(fqsn);
}

bool PexCache::GetSourceContent(const std::string &scriptPath, std::string &decompiledSource)
{
	auto lump = GetScriptFileID(scriptPath);
	if (lump == -1)
	{
		return false;
	}
	auto size = fileSystem.FileLength(lump);
	if (size == 0)
	{
		return false;
	}

	std::vector<uint8_t> buffer(size);
	// Godspeed, you magnificent bastard
	fileSystem.ReadFile(lump, buffer.data());
	// Check if the file is binary; just check the first 8000 bytes for 0s
	for (size_t i = 0; i < std::min((size_t)8000, (size_t)size); i++)
	{
		if (buffer[i] == 0)
		{
			return false;
		}
	}
	decompiledSource = std::string(buffer.begin(), buffer.end());
	return true;
}

bool PexCache::GetOrCacheSource(BinaryPtr binary, std::string &decompiledSource)
{
	if (!binary)
	{
		return false;
	}
	if (!binary->cachedSourceCode.empty())
	{
		decompiledSource = binary->cachedSourceCode;
		return true;
	}
	{
		scripts_lock scriptLock(m_scriptsMutex);

		if (binary->cachedSourceCode.empty() && !GetSourceContent(binary->GetQualifiedPath(), binary->cachedSourceCode))
		{
			return false;
		}
	}
	decompiledSource = binary->cachedSourceCode;
	return true;

}

bool PexCache::GetDecompiledSource(const dap::Source &source, std::string &decompiledSource)
{
	return GetOrCacheSource(this->GetScript(source), decompiledSource);
}

bool PexCache::GetDecompiledSource(const std::string &fqpn, std::string &decompiledSource)
{
	return GetOrCacheSource(this->GetScript(fqpn), decompiledSource);
}

bool PexCache::GetSourceData(const std::string &scriptName, dap::Source &data)
{
	auto binary = GetScript(scriptName);
	if (!binary)
	{
		return false;
	}
	data = binary->GetDapSource();
	return true;
}

void PexCache::Clear()
{
	scripts_lock scriptLock(m_scriptsMutex);
	m_scripts.clear();
	m_globalCodeMap.clear();
	m_disassemblyMap.clear();
}

dap::ResponseOrError<dap::LoadedSourcesResponse> PexCache::GetLoadedSources(const dap::LoadedSourcesRequest &request)
{
	dap::LoadedSourcesResponse response;
	ScanAllScripts();
	scripts_lock scriptLock(m_scriptsMutex);
	for (auto &bin : m_scripts)
	{
		response.sources.push_back(bin.second->GetDapSource());
	}
	return response;
}

inline bool LineIsFunctionDeclaration(const std::string &line, const std::string &function_name)
{
	std::string new_line = line;
	new_line.erase(0, new_line.find_first_not_of(" \t\n"));
	new_line.erase(new_line.find_last_not_of(" \t\n") + 1);

	auto func_name_pos = new_line.find(function_name);
	if (func_name_pos == std::string::npos)
	{
		return false;
	}

	// hack for deprecated annotated functions
	if (new_line.find("deprecated(") != std::string::npos)
	{
		return true;
	}
	auto func_name_end = func_name_pos + function_name.size();
	if (func_name_end >= line.size())
	{
		return false;
	}
	// trim the whitespace of the line
	// if there's no whitespace in the line now, it's not a function declaration
	if (new_line.find_first_of(" \t\n") == std::string::npos)
	{
		return false;
	}
	auto open_paren_pos = new_line.find_first_not_of(" \t\n", func_name_end);
	if (open_paren_pos == std::string::npos || new_line[open_paren_pos] != '(')
	{
		return false;
	}
	if (new_line.find("super.") != std::string::npos)
	{
		return false;
	}
	return true;
}

// TODO: rely on compiler information somehow instead of this
// find the LINE that the function declaration starts on, lines starting at 1
int PexCache::FindFunctionDeclaration(const std::shared_ptr<Binary> &source, const VMScriptFunction *func, int start_line_from_1)
{
	
	std::string source_code;
	if (!GetOrCacheSource(source, source_code))
	{
		return 0;
	}
	// convert source_code to lowercase
	std::transform(source_code.begin(), source_code.end(), source_code.begin(), ::tolower);
	std::string function_name = func->Name.GetChars();
	std::transform(function_name.begin(), function_name.end(), function_name.begin(), ::tolower);

	auto lines = Split(source_code, "\n");
	auto func_decl_line = source->GetFunctionLineRange(func).first;
	// void funcs have a minimum line that is equal to the func decl line because the hidden return is mapped to it
	if (func_decl_line - 1 > 0 && lines[func_decl_line - 1].find(function_name) != std::string::npos && lines[func_decl_line - 1].find("void") != std::string::npos)
	{
		return func_decl_line;
	}

	if (start_line_from_1 == 0)
	{
		start_line_from_1 = lines.size() - 1;
	}
	else
	{
		start_line_from_1 = start_line_from_1 - 1; //std::min(start_line_from_1, std::min(line_range_from_1.second, lines.size())) - 1;
	}
	for (int i = start_line_from_1; i >= 0; i--)
	{
		auto &line = lines[i];
		// find the line that contains the function name
		auto func_name_pos = line.find(function_name);
		if (func_name_pos != std::string::npos && LineIsFunctionDeclaration(lines[i], function_name))
		{

			return i + 1;
		}
	}
	return 0;
}

std::shared_ptr<DisassemblyLine>
PexCache::MakeInstruction(VMScriptFunction *func, int ref, const std::string &instruction_text, const std::string &opcode, const std::string &comment, unsigned long long ipnum, const std::string &pointed_symbol)
{
	std::shared_ptr<DisassemblyLine> instruction = std::make_shared<DisassemblyLine>();
	instruction->funcPtr = func;
	instruction->function = func->QualifiedName;
	instruction->instruction = instruction_text;
	instruction->address = (void *)ipnum;
	instruction->bytes = opcode;
	instruction->comment = comment;
	instruction->ref = ref;
	instruction->line = func->PCToLine((const VMOP *)instruction->address);
	instruction->is_valid_bp = true;
	if (instruction->line < 0)
	{
		// find the max line number
		int max_line = 0;
		for (size_t li = 0; li < func->LineInfoCount; ++li)
		{
			if (func->LineInfo[li].LineNumber > max_line)
			{
				max_line = func->LineInfo[li].LineNumber;
			}
		}
		instruction->line = max_line + 1;
	}
	instruction->endLine = instruction->line;
	instruction->pointed_symbol = pointed_symbol;
	return instruction;
}

std::vector<dap::Module> PexCache::GetModules()
{
	std::vector<dap::Module> modules;
	int count = fileSystem.GetNumWads();
	for (int i = 0; i < count; i++)
	{
		dap::Module module;
		module.id = dap::integer(i);
		std::string name = fileSystem.GetResourceFileName(i);
		std::string path = fileSystem.GetResourceFileFullName(i);
		NormalizeArchivePath(name);
		module.name = name;
		module.path = path;
		modules.push_back(module);
	}		
	return modules;
}

uint64_t PexCache::AddDisassemblyLines(VMScriptFunction *func, DisassemblyMap &instructions)
{
#if defined(_WIN32) || defined(_WIN64)
	// TODO: add a windows-compatible fmemopen
	return 0;
#else
	if (!func || IsFunctionAbstract(func) || IsFunctionNative(func))
	{
		return 0;
	}
	// we need to create a temporary FILE* to pass to Disassemble
	// we can't use a string because Disassemble expects a FILE*
	// assume 256 bytes per instruction
	size_t buf_size = func->CodeSize * 256;
	std::vector<uint8_t> buffer(buf_size);
	FILE *f = fmemopen(buffer.data(), buf_size, "w");
	if (!f)
	{
		LogError("Failed to create a temporary file for disassembly");
		return 0;
	}
	auto ref = GetScriptReference(func->SourceFileName.GetChars());
	auto startPointer = func->Code;
	auto endPointer = func->Code + func->CodeSize;
	auto currCodePointer = func->Code;

	VMDisasm(f, func->Code, func->CodeSize, func, (uint64_t)func->Code);
	// close the file
	fclose(f);

	// now we can read the disassembled code from CodeBytes
	std::string disassembly = std::string(buffer.begin(), std::find(buffer.begin(), buffer.end(), 0));
	// split it into lines
	auto lines = Split(disassembly, "\n");


	auto ret = m_disassemblyMap.insert(true, {startPointer, endPointer, {}});
	if (!ret.second)
	{
		if (!(ret.first->start_pt() == startPointer && ret.first->end_pt() == endPointer))
		{
			LogError("Failed to insert the disassembly lines into the map");
			return 0;
		}
		// else, just use the already existing one but clear it
		ret.first->mapped().clear();
	}
	auto &lines_map = ret.first->mapped();
	// check if the last line in lines is empty; if so, remove it
	std::vector<size_t> lines_to_remove;
	auto script_name = func->SourceFileName.GetChars();
	auto source = GetScript(script_name);
	int min_line = INT_MAX;

	for (size_t i = 0; i < lines.size(); i++)
	{
		auto &line = lines[i];
		if (line.empty())
		{
			continue;
		}
		auto comment_pos = line.find(';');
		if (comment_pos == std::string::npos)
		{
			// there was a string literal with a newline in it, so we need to check the next line(s) for a comment
			for (size_t j = i + 1; j < lines.size(); j++)
			{
				auto &next_line = lines[j];
				line += "\n" + next_line;
				auto next_comment_pos = next_line.find(';');
				next_line = "";
				if (next_comment_pos != std::string::npos)
				{
					comment_pos = line.find(';');
					break;
				}
			}
			if (comment_pos == std::string::npos)
			{
				LogError("!!!!!!Disassembly line %zu has no comment!!!!!", i);
				continue;
			}
		}
		if (line.size() < 19)
		{
			LogError("!!!!!!Disassembly line %zu too short!!!!!", i);
			continue;
		}
		// lines go like this:
		// ip        opcode   op      arg1, arg2, arg3             ;arg1,arg2,arg3 {[resolved symbol]}(optional)
		// 00000464: 611e0201 call    [0x1319fc620],2,1            ;30,2,1  [ZTBotController.PickTeam]
		//
		// we want to extract the ip, the opcode, and the op
		// we also want to remove the ip and the opcode from the line
		auto col_pos = line.find(':');
		auto ipStr = line.substr(0, col_pos);
		auto opcode = line.substr(col_pos + 2, 8);
		auto op = line.substr(col_pos + 11, 8);
		op.erase(std::remove(op.begin(), op.end(), ' '), op.end());

		auto inst_str = line.substr(col_pos + 11, comment_pos - col_pos - 11);
		// trim the whitespace from inst_str
		inst_str.erase(std::remove(inst_str.begin(), inst_str.end(), ' '), inst_str.end());
		std::string comment;
		comment = line.substr(comment_pos + 1);
		// get the resolved_symbol if it exists
		std::string resolved_symbol;
		if (!comment.empty())
		{
			// find the first open bracket in the comment
			auto open_bracket = comment.find('[');
			if (open_bracket != std::string::npos)
			{
				// find the last close bracket
				auto close_bracket = comment.find_last_of(']');
				if (close_bracket != std::string::npos)
				{
					resolved_symbol = comment.substr(open_bracket + 1, close_bracket - open_bracket - 1);
				}
			}
		}
		auto ipnum = std::stoull(ipStr, nullptr, 16);

		auto instruction = std::make_shared<DisassemblyLine>();
		instruction = MakeInstruction(func, ref, line.substr(col_pos + 11), opcode, comment, ipnum, resolved_symbol);
		if (instruction->line > -1)
		{
			min_line = std::min(min_line, instruction->line);
		}
		else
		{
			int j = 0;
		}
		// The reason for this is that the disassembler decodes a cmp and then {jne,je,etc} as a single {bne,be,etc} instruction
		// We want the instructions to always be 4 bytes long, so we add a dummy instruction if the instruction starts with "b"
		if (instruction->instruction.front() == 'b')
		{
			if (instruction->instruction[1] == 'n' || instruction->instruction[1] == 'e' || instruction->instruction[1] == 'l' || instruction->instruction[1] == 'g')
			{
				// instruction->bytesize = 8;
				// currCodePointer++;
				// instruction->bytes += StringFormat("%02X%02X%02X%02X", currCodePointer->op, currCodePointer->a, currCodePointer->b, currCodePointer->c);

				lines_map.insert({(void *)currCodePointer, instruction});
				currCodePointer++;

				instruction = MakeInstruction(
					func, ref, "--", StringFormat("%02hX%02hX%02hX%02hX", currCodePointer->op, currCodePointer->a, currCodePointer->b, currCodePointer->c), StringFormat("; jmp %08X", currCodePointer->i24), ipnum + 4, resolved_symbol);
				min_line = std::min(min_line, instruction->line);
				if (instruction->line > -1)
				{
					min_line = std::min(min_line, instruction->line);
				}
				else
				{
					int j = 0;
				}
			}
		}
		lines_map.insert({(void *)currCodePointer, instruction});
		currCodePointer++;
	}
	bool hit_min_line = false;
	bool after_min_line = false;
	auto func_decl_line = FindFunctionDeclaration(source, func, min_line);
	if (func_decl_line > 0)
	{
		for (auto &pr : lines_map)
		{
			auto &instruction = pr.second;
			// the objective is to show the function declaration line in the assembly; but we only want to show it for the first set of instructions that map to the min line
			if (!after_min_line && instruction->line == min_line)
			{
				hit_min_line = true;
				instruction->line = func_decl_line;
			}
			else if (hit_min_line == true)
			{
				after_min_line = true;
			}
		}
	}


	return instructions.size();
#endif
}


bool PexCache::GetDisassemblyLines(const VMOP *address, int64_t p_instructionOffset, int64_t p_count, std::vector<std::shared_ptr<DisassemblyLine>> &lines_vec)
{
	scripts_lock scriptLock(m_scriptsMutex);
	if (!address)
	{
		return false;
	}
	auto ret = m_globalCodeMap.find_ranges((void *)address);
	int64_t extra = 0;
	if (ret.empty())
	{
		// back up until we find a valid address
		if (p_instructionOffset < 0)
		{
			int64_t i = 1;
			ret = m_globalCodeMap.find_ranges((void *)(address - i));
			while (ret.empty())
			{
				i++;
				ret = m_globalCodeMap.find_ranges((void *)(address - i));
				if (i > std::abs(p_instructionOffset) + p_count)
				{
					return false;
				}
			}
			extra = i;
		}
		else
		{
			auto it = m_globalCodeMap.lower_bound((void *)address);
			if (it == m_globalCodeMap.end())
			{
				return false;
			}
			ret.push(it);
			// get the difference between the start point and the address
			extra = address - (VMOP *)it->start_pt();
		}
	}
	auto &it = ret.top();
	Binary::FunctionCodeMap::iterator reverse_it = ret.top();
	std::map<void *, std::shared_ptr<DisassemblyLine>> instruction_map;
	auto firstFunc = it->mapped();
	auto firstFuncInstcount = firstFunc->CodeSize;

	auto addToInstMap = [&](const std::map<void *, std::shared_ptr<DisassemblyLine>> &lines)
	{
		size_t added = 0;
		for (auto &pr : lines)
		{
			auto &line = pr.second;
			auto result = instruction_map.insert({line->address, line});
			if (result.second)
			{
				added++;
			}
		}
		return added;
	};
	{
		int64_t instructionOffset = p_instructionOffset;
		int64_t count = p_count + std::abs(p_instructionOffset) + firstFuncInstcount + extra;
		// if the offset is negative, we get the previous instructions
		if (instructionOffset < 0)
		{
			// get the difference between instructionOffset and 0 (+ the first func count to the requested address)
			int64_t instructionsToGet = count;
			// keep going backwards until we find enough instructions to fill the request
			auto &prev_it = reverse_it;
			while (reverse_it != m_globalCodeMap.end())
			{
				auto found = m_disassemblyMap.find_ranges(reverse_it->start_pt());
				if (found.empty())
				{
					AddDisassemblyLines(reverse_it->mapped(), m_disassemblyMap);
					found = m_disassemblyMap.find_ranges(reverse_it->start_pt());
				}
				auto &found_lines = found.top()->mapped();
				instructionsToGet -= addToInstMap(found_lines);

				if (instructionsToGet <= 0)
				{
					break;
				}
				--reverse_it;
			}
			if (std::abs(std::abs(instructionOffset) - count) > 0)
			{
				instructionOffset = 0;
			}
		}
		// forward...
		if (instructionOffset >= 0)
		{
			int64_t instructionsToGet = count;

			while (it != m_globalCodeMap.end())
			{
				auto found = m_disassemblyMap.find_ranges(it->start_pt());
				if (found.empty())
				{
					AddDisassemblyLines(it->mapped(), m_disassemblyMap);
					found = m_disassemblyMap.find_ranges(it->start_pt());
				}
				auto &found_lines = found.top()->mapped();
				auto mapped = it->mapped();
				instructionsToGet -= addToInstMap(found_lines);

				if (instructionsToGet <= 0)
				{
					break;
				}
				++it;
			}
		}
	}
	auto actualaddress = (void *)((VMOP *)address + p_instructionOffset);
	for (int64_t i = 0; i < p_count; i++)
	{
		auto curaddr = (VMOP *)actualaddress + i;
		auto found = instruction_map.find(curaddr);
		if (found != instruction_map.end())
		{
			lines_vec.push_back(found->second);
		}
		else
		{
			auto invalid_inst = lines_vec.emplace_back(std::make_shared<DisassemblyLine>());
			invalid_inst->is_valid_bp = false;
			invalid_inst->address = curaddr;
			invalid_inst->instruction = "<INVALID>";
			invalid_inst->bytes = "<INVALID>";
		}
	}

	return true;
}
}

std::string DebugServer::Binary::GetQualifiedPath() const { return archiveName + ":" + unqualifiedScriptPath; }
std::string DebugServer::Binary::GetArchiveName() const { return archiveName; }
std::string DebugServer::Binary::GetArchivePath() const
{
	return archivePath;
}

size_t DebugServer::Binary::GetFunctionCount() const
{
	return functions.size() + stateFunctions.size();
}

std::stack<DebugServer::Binary::FunctionLineMap::const_iterator> DebugServer::Binary::FindFunctionRangesByLine(int line) const { return functionLineMap.find_ranges(line); }

std::stack<beneficii::range_map<void *, VMScriptFunction *>::const_iterator> DebugServer::Binary::FindFunctionRangesByCode(void *address) const
{
	return functionCodeMap.find_ranges(address);
}

bool DebugServer::Binary::HasFunctions() const
{
	return !functions.empty() || !functionCodeMap.empty();
}

bool DebugServer::Binary::HasFunctionLines() const
{
	return !functionLineMap.empty();
}

void DebugServer::Binary::ProcessScriptFunction(const std::string &qualPath, VMFunction *vmfunc)
{
	if (IsFunctionNative(vmfunc) || IsFunctionAbstract(vmfunc))
	{
		return;
	}
	auto scriptFunc = static_cast<VMScriptFunction *>(vmfunc);
	if (!CaseInsensitiveEquals(scriptFunc->SourceFileName.GetChars(), qualPath))
	{
		return;
	}

	uint32_t firstLine = INT_MAX;
	uint32_t lastLine = 0;

	for (uint32_t i = 0; i < scriptFunc->LineInfoCount; ++i)
	{
		if (scriptFunc->LineInfo[i].LineNumber < firstLine)
		{
			firstLine = scriptFunc->LineInfo[i].LineNumber;
		}
		if (scriptFunc->LineInfo[i].LineNumber > lastLine)
		{
			lastLine = scriptFunc->LineInfo[i].LineNumber;
		}
	}

	FunctionLineMap::range_type range(firstLine, lastLine + 1, scriptFunc);
	auto ret = functionLineMap.insert(true, range);
	if (ret.second == false)
	{
		// Probably a mixin, just continue
		return;
	}
	void *code = scriptFunc->Code;
	void *end = scriptFunc->Code + scriptFunc->CodeSize;
	FunctionCodeMap::range_type codeRange(code, end, scriptFunc);
	functionCodeMap.insert(true, codeRange);
	return;
}

void DebugServer::Binary::PopulateFunctionMaps()
{
	functionLineMap.clear();
	functionCodeMap.clear();
	auto qualPath = GetQualifiedPath();
	int i = 0;
	for (auto &func : functions)
	{
		i++;
		for (auto &variant : func.second->Variants)
		{

			auto vmfunc = variant.Implementation;
			ProcessScriptFunction(qualPath, vmfunc);
		}
	}
	for (auto &pair : stateFunctions)
	{
		auto vmfunc = pair.second;
		ProcessScriptFunction(qualPath, vmfunc);
	}
}

dap::Source DebugServer::Binary::GetDapSource() const
{
	dap::Source source;
	source.name = scriptName;
	source.origin = archiveName;
	source.path = unqualifiedScriptPath;
	source.sourceReference = scriptReference;
	source.adapterData = archivePath;
	return source;
}

std::pair<int, int> DebugServer::Binary::GetFunctionLineRange(const VMScriptFunction *func) const
{
	if (!func)
	{
		return {0, 0};
	}
	for (auto &pair : functionLineMap)
	{
		if (pair.mapped() == func)
		{
			return {pair.range().get_left(), pair.range().get_right()};
		}
	}
	return {0, 0};
}
