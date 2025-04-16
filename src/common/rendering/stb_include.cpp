// stb_include.h - v0.02gz - parse and process #include directives - public domain
//
// To build this, in one source file that includes this file do
//      #define STB_INCLUDE_IMPLEMENTATION
//
// This program parses a string and replaces lines of the form
//         #include "foo"
// with the contents of a file named "foo". It also embeds the
// appropriate #line directives. Note that all include files must
// reside in the location specified in the gzdoom filesystem.
//
// Credits:
//
// Written by Sean Barrett.
//
// Fixes:
//  Michal Klos
//
// GZDoom Conversion:
//  Jay

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

struct include_info
{
    int64_t offset;
    int64_t end;
    FString filename;
    int64_t next_line_after;
};

static bool stb_include_isspace(int ch)
{
    return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
}

static void skip_whitespace(const char * &s)
{
    while (*s == ' ' || *s == '\t')
    {
        ++s;
    }
}

static void skip_newline(const char * &s)
{

    while (*s != '\r' && *s != '\n' && *s != 0)
    {
        ++s;
    }

    if(s[0] == '\r' && s[1] == '\n')
    {
        s += 2;
    }
    else if (*s == '\r' || *s == '\n')
    {
        s++;
    }
}

static void find_end_quote(const char * &t)
{
    while (*t != '"' && *t != '\n' && *t != '\r' && *t != 0)
    {
        ++t;
    }
}

static void find_newline(const char * &s)
{
    while (*s != '\r' && *s != '\n' && *s != 0)
    {
        ++s;
    }
}

// find location of all #include and #inject
static int64_t stb_include_find_includes(const char *text, TArray<include_info> &plist)
{
    int64_t line_count = 1;
    int64_t inc_count = 0;
    const char *s = text;
    const char *start;

    TArray<include_info> list;
    while (*s)
    {
        // parse is always at start of line when we reach here
        start = s;
        skip_whitespace(s);

        if (*s == '#')
        {
            ++s;
            skip_whitespace(s);

            if (0==strncmp(s, "include", 7) && stb_include_isspace(s[7]))
            {
                s += 7;
                skip_whitespace(s);

                if (*s == '"')
                {
                    const char * include_filename = ++s;

                    find_end_quote(include_filename);

                    if (*include_filename == '"')
                    { // ignore unterminated quotes
                        FString filename;
                        filename.AppendCStrPart(s, include_filename - s);
                        s = include_filename;

                        find_newline(s);

                        // s points to the newline, so s-start is everything except the newline
                        list.Push({start - text, s - text, filename, line_count + 1});
                        inc_count++;
                    }
                }
            }
        }

        skip_newline(s);

        ++line_count;
    }
   
    plist = std::move(list);
   
    return inc_count;
}

FString stb_include_string(FString str, FString filename, TArray<FString> &filenames, FString &error)
{
    error = "";
    TArray<include_info> inc_list;
    int64_t num = stb_include_find_includes(str.GetChars(), inc_list);
    FString text = "";
    size_t last = 0;

    filenames.Push(filename);
    size_t curIndex = filenames.Size();

    text.AppendFormat("\n#line 1 %d // %s\n", curIndex, filename.GetChars());

    for (int64_t i = 0; i < num; ++i)
    {
        text.AppendCStrPart(str.GetChars() + last, inc_list[i].offset - last);

        FString inc = stb_include_file(inc_list[i].filename.GetChars(), filenames, error);
        if (!error.IsEmpty())
        {
            return "";
        }
        text += inc;

        text.AppendFormat("\n#line %d %d // %s\n", inc_list[i].next_line_after, curIndex, filename.GetChars());
        // no newlines, because we kept the #include newlines, which will get appended next
        last = inc_list[i].end;
    }
    text.AppendCStrPart(str.GetChars() + last, str.Len() - last);
    return text;
}

FString stb_include_file(FString filename, TArray<FString> &filenames, FString &error)
{
   FString text;
   if (!stb_include_load_file(filename, text))
   {
        error += "Error: couldn't load '";
        error += filename;
        error += "'";
        return "";
   }
   return stb_include_string(text, filename, filenames, error);
}
