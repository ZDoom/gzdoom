/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     CLOSESIZE = 258,
     CLOSE = 259,
     ID = 260,
     CODE = 261,
     RANGE = 262,
     STRING = 263,
     CONFIG = 264,
     VALUE = 265,
     NUMBER = 266
   };
#endif
/* Tokens.  */
#define CLOSESIZE 258
#define CLOSE 259
#define ID 260
#define CODE 261
#define RANGE 262
#define STRING 263
#define CONFIG 264
#define VALUE 265
#define NUMBER 266




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 58 "parser.y"
typedef union YYSTYPE {
    re2c::Symbol	*symbol;
    re2c::RegExp	*regexp;
    re2c::Token 	*token;
    char        	op;
    int         	number;
    re2c::ExtOp 	extop;
    re2c::Str   	*str;
} YYSTYPE;
/* Line 1447 of yacc.c.  */
#line 70 "y.tab.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



