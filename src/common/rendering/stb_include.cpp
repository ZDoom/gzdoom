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

static bool stb_include_load_file(FString filename, FString &out)
{
    int f = fileSystem.FindFile(filename.GetChars());
    if(f < 0) return false;
    out = GetStringFromLump(f, false);
    return true;
}

typedef struct
{
   int offset;
   int end;
   FString filename;
   int next_line_after;
} include_info;

static void stb_include_append_include(TArray<include_info> &array, int offset, int end, FString filename, int next_line)
{
    array.Push({offset, end, filename, next_line});
}

static int stb_include_isspace(int ch)
{
   return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
}

// find location of all #include and #inject
static int stb_include_find_includes(const char *text, TArray<include_info> &plist)
{
   int line_count = 1;
   int inc_count = 0;
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
                  stb_include_append_include(list, start-text, s-text, filename, line_count+1);
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

// avoid dependency on sprintf()
static void stb_include_itoa(char str[9], int n)
{
   int i;
   for (i=0; i < 8; ++i)
      str[i] = ' ';
   str[i] = 0;

   for (i=1; i < 8; ++i) {
      str[7-i] = '0' + (n % 10);
      n /= 10;
      if (n == 0)
         break;
   }
}

static void stb_include_append(FString &str, FString &addstr)
{
    str += addstr;
}

static void stb_include_append(FString &str, const char * addstr, size_t addlen)
{
    str.AppendCStrPart(addstr, addlen);
}

FString stb_include_string(FString str, FString &error)
{
    error = "";
    char temp[4096];
    TArray<include_info> inc_list;
    int i, num = stb_include_find_includes(str.GetChars(), inc_list);
    size_t source_len = str.Len();
    FString text;
    size_t last = 0;
    for (i=0; i < num; ++i)
    {
        stb_include_append(text, str.GetChars() + last, inc_list[i].offset - last);
        // write out line directive for the include
        strcpy(temp, "#line ");
        stb_include_itoa(temp+6, 1);
        strcat(temp, " ");
        stb_include_itoa(temp+15, i+1);
        strcat(temp, "\n");
        stb_include_append(text, temp, strlen(temp));

        FString inc = stb_include_file(inc_list[i].filename.GetChars(), error);
        if (!error.IsEmpty())
        {
            return "";
        }
        stb_include_append(text, inc);

        // write out line directive
        strcpy(temp, "\n#line ");
        stb_include_itoa(temp+6, inc_list[i].next_line_after);
        strcat(temp, " ");
        stb_include_itoa(temp+15, 0);
        stb_include_append(text, temp, strlen(temp));
        // no newlines, because we kept the #include newlines, which will get appended next
        last = inc_list[i].end;
    }
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
