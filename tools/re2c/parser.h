/* $Id: parser.h,v 1.4 2004/05/13 02:58:17 nuffer Exp $ */
#ifndef _parser_h
#define _parser_h

#include "scanner.h"
#include "re.h"
#include <iosfwd>

class Symbol {
public:
    static Symbol	*first;
    Symbol		*next;
    Str			name;
    RegExp		*re;
public:
    Symbol(const SubStr&);
    static Symbol *find(const SubStr&);
};

void line_source(unsigned int, std::ostream&);
void parse(std::istream&, std::ostream&);

#endif
