/* $Id: substr.h,v 1.3 2004/05/13 02:58:18 nuffer Exp $ */
#ifndef _substr_h
#define _substr_h

#include <iostream>
#include "basics.h"

class SubStr {
public:
    char		*str;
    uint		len;
public:
    friend bool operator==(const SubStr &, const SubStr &);
    SubStr(uchar*, uint);
    SubStr(char*, uint);
    SubStr(const SubStr&);
    void out(std::ostream&) const;
};

class Str: public SubStr {
public:
    Str(const SubStr&);
    Str(Str&);
    Str();
    ~Str();
};

inline std::ostream& operator<<(std::ostream& o, const SubStr &s){
    s.out(o);
    return o;
}

inline std::ostream& operator<<(std::ostream& o, const SubStr* s){
    return o << *s;
}

inline SubStr::SubStr(uchar *s, uint l)
    : str((char*) s), len(l) { }

inline SubStr::SubStr(char *s, uint l)
    : str(s), len(l) { }

inline SubStr::SubStr(const SubStr &s)
    : str(s.str), len(s.len) { }

#endif
