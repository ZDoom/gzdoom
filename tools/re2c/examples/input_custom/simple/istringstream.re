#include <sstream>

bool lex (std::istringstream & is, const std::streampos limit)
{
    std::streampos marker;
    std::streampos ctxmarker;
#   define YYCTYPE        char
#   define YYPEEK()       is.peek ()
#   define YYSKIP()       is.ignore ()
#   define YYBACKUP()     marker = is.tellg ()
#   define YYBACKUPCTX()  ctxmarker = is.tellg ()
#   define YYRESTORE()    is.seekg (marker)
#   define YYRESTORECTX() is.seekg (ctxmarker)
#   define YYLESSTHAN(n)  limit - is.tellg () < n
#   define YYFILL(n)      {}
    /*!re2c
        "int buffer " / "[" [0-9]+ "]" { return true; }
        *                              { return false; }
    */
}

int main ()
{
    const char buffer [] = "int buffer [1024]";
    std::istringstream is (buffer);
    return !lex (is, sizeof (buffer));
}
