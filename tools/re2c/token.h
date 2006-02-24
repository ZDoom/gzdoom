/* $Id: token.h,v 1.2 2004/01/31 15:44:39 nuffer Exp $ */
#ifndef _token_h
#define	_token_h

#include "substr.h"

class Token {
  public:
    Str			text;
    uint		line;
  public:
    Token(SubStr, uint);
};

inline Token::Token(SubStr t, uint l) : text(t), line(l) {
    ;
}

#endif
