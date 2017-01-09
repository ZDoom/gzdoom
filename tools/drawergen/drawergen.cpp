/*
**  LLVM code generated drawers
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "precomp.h"
#include "timestamp.h"
#include "exception.h"
#include "llvmdrawers.h"

std::string &AllTimestamps()
{
	static std::string timestamps;
	return timestamps;
}

void AddSourceFileTimestamp(const char *timestamp)
{
	if (!AllTimestamps().empty()) AllTimestamps().push_back(' ');
	AllTimestamps() += timestamp;
}

int main(int argc, char **argv)
{
	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: " << argv[0] << "<output filename>" << std::endl;
			return 1;
		}

		std::string timestamp_filename = argv[1] + std::string(".timestamp");

		FILE *file = fopen(timestamp_filename.c_str(), "rb");
		if (file != nullptr)
		{
			char buffer[4096];
			int bytes_read = fread(buffer, 1, 4096, file);
			fclose(file);
			std::string last_timestamp;
			if (bytes_read > 0)
				last_timestamp = std::string(buffer, bytes_read);

			if (AllTimestamps() == last_timestamp)
			{
				std::cout << "Not recompiling drawers because the object file is already up to date." << std::endl;
				exit(0);
			}
		}

		llvm::install_fatal_error_handler([](void *user_data, const std::string& reason, bool gen_crash_diag)
		{
			std::cerr << "LLVM fatal error: " << reason;
			exit(1);
		});

		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmPrinter();

		std::string triple = llvm::sys::getDefaultTargetTriple();
		
#ifdef __APPLE__
		// Target triple is x86_64-apple-darwin15.6.0
		auto pos = triple.find("-apple-darwin");
		if (pos != std::string::npos)
		{
			triple = triple.substr(0, pos) + "-apple-darwin10.11.0";
		}
#endif
		
		std::cout << "Target triple is " << triple << std::endl;

#ifdef __arm__
		std::string cpuName = "armv8";
#else
		std::string cpuName = "pentium4";
#endif
		std::string features;
		std::cout << "Compiling drawer code for " << cpuName << ".." << std::endl;

		LLVMDrawers drawersSSE2(triple, cpuName, features, "_SSE2");

		file = fopen(argv[1], "wb");
		if (file == nullptr)
		{
			std::cerr << "Unable to open " << argv[1] << " for writing." << std::endl;
			return 1;
		}

		int result = fwrite(drawersSSE2.ObjectFile.data(), drawersSSE2.ObjectFile.size(), 1, file);
		fclose(file);

		if (result != 1)
		{
			std::cerr << "Could not write data to " << argv[1] << std::endl;
			return 1;
		}

		file = fopen(timestamp_filename.c_str(), "wb");
		if (file == nullptr)
		{
			std::cerr << "Could not create timestamp file" << std::endl;
			return 1;
		}
		result = fwrite(AllTimestamps().data(), AllTimestamps().length(), 1, file);
		fclose(file);
		if (result != 1)
		{
			std::cerr << "Could not write timestamp file" << std::endl;
			return 1;
		}

		//LLVMDrawers drawersSSE4("core2");
		//LLVMDrawers drawersAVX("sandybridge");
		//LLVMDrawers drawersAVX2("haswell");

		return 0;
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
}
