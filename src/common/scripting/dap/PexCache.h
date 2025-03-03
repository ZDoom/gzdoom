#pragma once

#include <map>

#include <dap/protocol.h>
#include <dap/session.h>
#include <mutex>
#include <string>
#include "Utilities.h"
#include <range_map/range_map.h>
#include <name.h>
#include <shared_mutex>
#include <vmintern.h>

class PFunction;
class PClassType;
class PStruct;
class VMFunction;
class VMScriptFunction;

// TODO: we may not want to do it like this, but this is the easiest way to get the opinfo.
extern const VMOpInfo OpInfo[NUM_OPS];

namespace DebugServer
{
struct Binary
{
	using NameFunctionMap = std::map<FName, PFunction *>;
	using NameClassMap = std::map<FName, PClassType *>;
	using NameStructMap = std::map<FName, PStruct *>;
	using FunctionLineMap = beneficii::range_map<uint32_t, VMScriptFunction *, std::less<uint32_t>, std::allocator<beneficii::range_map_item<uint32_t, VMScriptFunction *>>, true>;
	using FunctionCodeMap = beneficii::range_map<void *, VMScriptFunction *>;
	std::string archiveName;
	std::string archivePath;
	std::string scriptName;
	std::string scriptPath;
	std::string compiledPath;
	std::string sourceCode;
	int lump;
	int scriptReference;
	NameFunctionMap functions;
	NameClassMap classes;
	NameStructMap structs;
	FunctionLineMap functionLineMap;
	FunctionCodeMap functionCodeMap;
	void populateFunctionMaps();
	std::pair<int, int> GetFunctionLineRange(const VMScriptFunction *functionName) const;
	std::string GetQualifiedPath() const;
	dap::Source GetDapSource() const;
};
struct DisassemblyLine
{
	void *address;
	int line = -1;
	int endLine = -1;
	int ref = -1;
	uint8_t bytesize = 4;
	bool is_valid_bp = false;
	std::string bytes;
	std::string instruction;
	std::string comment;
	std::string pointed_symbol;
	std::string function;
};

class PexCache
{
	public:
	using BinaryPtr = std::shared_ptr<Binary>;
	using BinaryMap = std::map<int, BinaryPtr>;
	using DisassemblyLinePtr = std::shared_ptr<DisassemblyLine>;
	using DisassemblyMap = beneficii::range_map<void *, std::vector<DisassemblyLinePtr>>;
	PexCache() = default;
	bool HasScript(int scriptReference);
	bool HasScript(const std::string &scriptName);

	std::shared_ptr<Binary> GetCachedScript(const int ref);
	std::shared_ptr<Binary> GetScript(const dap::Source &source);

	std::shared_ptr<Binary> GetScript(std::string fqsn);
	bool GetDecompiledSource(const dap::Source &source, std::string &decompiledSource);

	bool GetDecompiledSource(const std::string &fqpn, std::string &decompiledSource);
	bool GetSourceData(const std::string &scriptName, dap::Source &data);
	void Clear();
	void ScanAllScripts();
	dap::ResponseOrError<dap::LoadedSourcesResponse> GetLoadedSources(const dap::LoadedSourcesRequest &request);

	static std::shared_ptr<DisassemblyLine>
	MakeInstruction(VMScriptFunction *func, int ref, const std::string &instruction_text, const std::string &opcode, const std::string &comment, unsigned long long ipnum, const std::string &pointed_symbol);

	uint64_t AddDisassemblyLines(VMScriptFunction *func, DisassemblyMap &instructions);
	bool GetDisassemblyLines(const VMOP *address, int64_t instructionOffset, uint64_t count, std::vector<std::shared_ptr<DisassemblyLine>> &lines);
	std::shared_ptr<Binary> AddScript(const std::string &scriptPath);
	private:
	using scripts_lock = std::scoped_lock<std::recursive_mutex>;
	BinaryPtr _AddScript(const std::string &scriptPath);

	static void PopulateFromPaths(const std::vector<std::string> &scripts, BinaryMap &p_scripts, bool clobber = false);

	static void ScanScriptsInContainer(int baselump, BinaryMap &m_scripts, const std::string &filter = "");
	static BinaryPtr makeEmptyBinary(const std::string &scriptPath);
	DisassemblyMap m_disassemblyMap;
	Binary::FunctionCodeMap m_globalCodeMap;
	std::recursive_mutex m_scriptsMutex;
	BinaryMap m_scripts;

	uint64_t AddDisassemblyLines(VMScriptFunction *func, std::vector<std::shared_ptr<DisassemblyLine>> &lines);
};
}
