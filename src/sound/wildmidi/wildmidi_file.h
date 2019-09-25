/*
 wildmidi_file.h
 
 Midi Wavetable Processing library
 
 Copyright (C) 2019 Christoph Oelckers
 
 WildMIDI is free software: you can redistribute and/or modify the player
 under the terms of the GNU General Public License and you can redistribute
 and/or modify the library under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation, either version 3 of
 the licenses, or(at your option) any later version.
 
 WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 the GNU Lesser General Public License for more details.
 
 You should have received a copy of the GNU General Public License and the
 GNU Lesser General Public License along with WildMIDI.  If not,  see
 <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <stdio.h>
#include <vector>
#include <string>

namespace WildMidi
{
	
	struct wildmidi_file
	{
		std::string filename;
		
		virtual ~wildmidi_file() {}
		virtual char* gets(char* buff, int n) = 0;
		virtual long read(void* buff, int32_t size, int32_t nitems) = 0;
		long read(void* buff, int32_t size) { return read(buff, 1, size); }
		virtual long seek(long offset, int whence) = 0;
		virtual long tell() = 0;
		virtual void close() = 0;
	};
	
	class SoundFontReaderInterface
	{
	public:
		virtual ~SoundFontReaderInterface() {}
		virtual struct wildmidi_file* open_wildmidi_file(const char* fn) = 0;
		virtual void wildmidi_add_path(const char* path) = 0;
	};
	
	
	// A minimalistic sound font reader interface. Normally this should be replaced with something better tied into the host application.
#ifdef USE_BASE_INTERFACE
	
	// Base version of timidity_file using stdio's FILE.
	struct wildmidi_file_FILE : public wildmidi_file
	{
		FILE* f = nullptr;
		
		~wildmidi_file_FILE()
		{
			if (f) fclose(f);
		}
		char* gets(char* buff, int n) override
		{
			if (!f) return nullptr;
			return fgets(buff, n, f);
		}
		long read(void* buff, int32_t size, int32_t nitems) override
		{
			if (!f) return 0;
			return (long)fread(buff, size, nitems, f);
		}
		long seek(long offset, int whence) override
		{
			if (!f) return 0;
			return fseek(f, offset, whence);
		}
		long tell() override
		{
			if (!f) return 0;
			return ftell(f);
		}
		void close()
		{
			if (f) fclose(f);
			delete this;
		}
	};
	
	class BaseSoundFontReader : public SoundFontReaderInterface
	{
		std::vector<std::string> paths;
		
		bool IsAbsPath(const char *name)
		{
			if (name[0] == '/' || name[0] == '\\') return true;
#ifdef _WIN32
			/* [A-Za-z]: (for Windows) */
			if (isalpha(name[0]) && name[1] == ':') return true;
#endif /* _WIN32 */
			return 0;
		}
		
		struct wildmidi_file* open_wildmidi_file(const char* fn)
		{
			FILE *f = nullptr;
			std::string fullname;
			if (!fn)
			{
				f = fopen("timidity.cfg", "rt");
				fullname = "timidity.cfg";
			}
			else
			{
				if (!IsAbsPath(fn))
				{
					for(int i = (int)paths.size()-1; i>=0; i--)
					{
						fullname = paths[i] + fn;
						f = fopen(fullname.c_str(), "rt");
						break;
					}
				}
				if (!f) f = fopen(fn, "rt");
			}
			if (!f) return nullptr;
			auto tf = new wildmidi_file_FILE;
			tf->f = f;
			tf->filename = fullname;
			return tf;
		}
		
		void wildmidi_add_path(const char* path)
		{
			std::string p  = path;
			if (p.back() != '/' && p.back() != '\\') p += '/';	// always let it end with a slash.
			paths.push_back(p);
		}
	};
#endif
	
}

