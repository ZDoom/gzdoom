// Build with "--input custom" re2c switch.
//
// This is an example of handling fixed-length buffer with "--input custom":
// on each YYPEEK we check for the end of input, thus YYFILL generation
// can be safely suppressed.
//
// Note that YYLIMIT points not to terminating NULL, but to the previous
// character: we emulate the case when input has no terminating NULL.
//
// For a real-life example see https://github.com/sopyer/mjson
// or mjson.re from re2c test collection.

bool lex (const char * cursor, const char * const limit)
{
    const char * marker;
    const char * ctxmarker;
#   define YYCTYPE        char
#   define YYPEEK()       (cursor >= limit ? 0 : *cursor)
#   define YYSKIP()       ++cursor
#   define YYBACKUP()     marker = cursor
#   define YYBACKUPCTX()  ctxmarker = cursor
#   define YYRESTORE()    cursor = marker
#   define YYRESTORECTX() cursor = ctxmarker
    /*!re2c
        re2c:yyfill:enable = 0;
        "int buffer " / "[" [0-9]+ "]" { return true; }
        *                              { return false; }
    */
}

int main ()
{
    char buffer [] = "int buffer [1024]";
    return !lex (buffer, buffer + sizeof (buffer) - 1);
}
