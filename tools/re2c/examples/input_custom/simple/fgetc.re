#include <stdio.h>

char peek (FILE * f)
{
    char c = fgetc (f);
    ungetc (c, f);
    return c;
}

bool lex (FILE * f, const long limit)
{
    long marker;
    long ctxmarker;
#   define YYCTYPE        char
#   define YYPEEK()       peek (f)
#   define YYSKIP()       fgetc (f)
#   define YYBACKUP()     marker = ftell (f)
#   define YYBACKUPCTX()  ctxmarker = ftell (f)
#   define YYRESTORE()    fseek (f, marker, SEEK_SET)
#   define YYRESTORECTX() fseek (f, ctxmarker, SEEK_SET)
#   define YYLESSTHAN(n)  limit - ftell (f) < n
#   define YYFILL(n)      {}
    /*!re2c
        "int buffer " / "[" [0-9]+ "]" { return true; }
        *                              { return false; }
    */
}

int main ()
{
    const char buffer [] = "int buffer [1024]";
    const char fn [] = "input.txt";

    FILE * f = fopen (fn, "w");
    fwrite (buffer, 1, sizeof (buffer), f);
    fclose (f);

    f = fopen (fn, "rb");
    int result = !lex (f, sizeof (buffer));
    fclose (f);

    return result;
}
