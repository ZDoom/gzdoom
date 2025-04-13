// stb_include.h - v0.02 - parse and process #include directives - public domain
//
// To build this, in one source file that includes this file do
//      #define STB_INCLUDE_IMPLEMENTATION
//
// This program parses a string and replaces lines of the form
//         #include "foo"
// with the contents of a file named "foo". It also embeds the
// appropriate #line directives. Note that all include files must
// reside in the location specified in the path passed to the API;
// it does not check multiple directories.
//
// If the string contains a line of the form
//         #inject
// then it will be replaced with the contents of the string 'inject' passed to the API.
//
// Options:
//
//      Define STB_INCLUDE_LINE_GLSL to get GLSL-style #line directives
//      which use numbers instead of filenames.
//
//      Define STB_INCLUDE_LINE_NONE to disable output of #line directives.
//
// Standard libraries:
//
//      stdio.h     FILE, fopen, fclose, fseek, ftell
//      stdlib.h    malloc, realloc, free
//      string.h    strcpy, strncmp, memcpy
//
// Credits:
//
// Written by Sean Barrett.
//
// Fixes:
//  Michal Klos

#include "stb_include.h"
#include "filesystem.h"
#include "cmdlib.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

static bool stb_include_load_file(FString filename, FString &out)
{
    int f = fileSystem.FindFile(filename.GetChars());
    if(f < 0) return false;
    out = GetStringFromLump(f, false);
    return true;
}

typedef struct
{
    int64_t offset;
    int64_t end;
    FString filename;
    int64_t next_line_after;
} include_info;

static bool stb_include_isspace(int ch)
{
   return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
}

// find location of all #include and #inject
static int64_t stb_include_find_includes(const char *text, TArray<include_info> &plist)
{
   int64_t line_count = 1;
   int64_t inc_count = 0;
   const char *s = text, *start;

   TArray<include_info> list;
   while (*s) {
      // parse is always at start of line when we reach here
      start = s;
      while (*s == ' ' || *s == '\t')
         ++s;
      if (*s == '#') {
         ++s;
         while (*s == ' ' || *s == '\t')
            ++s;
         if (0==strncmp(s, "include", 7) && stb_include_isspace(s[7])) {
            s += 7;
            while (*s == ' ' || *s == '\t')
               ++s;
            if (*s == '"') {
               const char *t = ++s;
               while (*t != '"' && *t != '\n' && *t != '\r' && *t != 0)
                  ++t;
               if (*t == '"') {
                  FString filename;
                  filename.AppendCStrPart(s, t-s);
                  s=t;
                  while (*s != '\r' && *s != '\n' && *s != 0)
                     ++s;
                  // s points to the newline, so s-start is everything except the newline
                  list.Push({start-text, s-text, filename, line_count+1});
                  inc_count++;
               }
            }
         }
      }
      while (*s != '\r' && *s != '\n' && *s != 0)
         ++s;
      if (*s == '\r' || *s == '\n') {
         s = s + (s[0] + s[1] == '\r' + '\n' ? 2 : 1);
      }
      ++line_count;
   }
   
   plist = std::move(list);
   
   return inc_count;
}

FString stb_include_string(FString str, FString &error)
{
    error = "";
    TArray<include_info> inc_list;
    int64_t num = stb_include_find_includes(str.GetChars(), inc_list);
    FString text = "";
    size_t last = 0;
    for (int64_t i = 0; i < num; ++i)
    {
        text.AppendCStrPart(str.GetChars() + last, inc_list[i].offset - last);

        text.AppendFormat("#line 1 %d\n", i + 1);

        FString inc = stb_include_file(inc_list[i].filename.GetChars(), error);
        if (!error.IsEmpty())
        {
            return "";
        }
        text += inc;

        text.AppendFormat("#line %d 0\n", inc_list[i].next_line_after);
        // no newlines, because we kept the #include newlines, which will get appended next
        last = inc_list[i].end;
    }
    text.AppendCStrPart(str.GetChars() + last, str.Len() - last);
    return text;
}

FString stb_include_file(FString filename, FString &error)
{
   FString text;
   if (!stb_include_load_file(filename, text))
   {
        error += "Error: couldn't load '";
        error += filename;
        error += "'";
        return "";
   }
   return stb_include_string(text, error);
}
