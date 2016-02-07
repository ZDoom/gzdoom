bool lex (const char * cursor, const char * const limit)
{
    const char * marker;
    const char * ctxmarker;
#   define YYCTYPE        char
#   define YYPEEK()       *cursor
#   define YYSKIP()       ++cursor
#   define YYBACKUP()     marker = cursor
#   define YYBACKUPCTX()  ctxmarker = cursor
#   define YYRESTORE()    cursor = marker
#   define YYRESTORECTX() cursor = ctxmarker
#   define YYLESSTHAN(n)  limit - cursor < n
#   define YYFILL(n)      {}
    /*!re2c
        "int buffer " / "[" [0-9]+ "]" { return true; }
        *                              { return false; }
    */
}

int main ()
{
    char buffer [] = "int buffer [1024]";
    return !lex (buffer, buffer + sizeof (buffer));
}
