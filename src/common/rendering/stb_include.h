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

#ifndef STB_INCLUDE_STB_INCLUDE_H
#define STB_INCLUDE_STB_INCLUDE_H

#include "zstring.h"

// Do include-processing on the string 'str'.
FString stb_include_string(FString str, FString filename, TArray<FString> &filenames, FString &error);

// Load the file 'filename' and do include-processing on the string therein.
FString stb_include_file(FString filename, TArray<FString> &filenames, FString &error);

#endif
