#include "src/util/c99_stdint.h"
#include <stddef.h>
#include <algorithm>
#include <set>
#include <string>

#include "src/codegen/bitmap.h"
#include "src/codegen/output.h"
#include "src/conf/opt.h"
#include "src/globals.h"
#include "src/ir/regexp/encoding/enc.h"
#include "src/ir/rule_rank.h"
#include "src/ir/skeleton/skeleton.h"

namespace re2c
{

static void exact_uint (OutputFile & o, size_t width)
{
	if (width == sizeof (char))
	{
		o.ws("unsigned char");
	}
	else if (width == sizeof (short))
	{
		o.ws("unsigned short");
	}
	else if (width == sizeof (int))
	{
		o.ws("unsigned int");
	}
	else if (width == sizeof (long))
	{
		o.ws("unsigned long");
	}
	else
	{
		o.ws("uint").wu64 (width * 8).ws("_t");
	}
}

static void from_le(OutputFile &o, uint32_t ind, size_t size, const char *expr)
{
	o.ws("\n").wind(ind).ws("/* from little-endian to host-endian */");
	o.ws("\n").wind(ind).ws("unsigned char *p = (unsigned char*)&").ws(expr).ws(";");
	o.ws("\n").wind(ind).ws(expr).ws(" = p[0]");
	for (uint32_t i = 1; i < size; ++i)
	{
		o.ws(" + (p[").wu32(i).ws("] << ").wu32(i * 8).ws("u)");
	}
	o.ws(";");
}

void Skeleton::emit_prolog (OutputFile & o)
{
	o.ws("\n#include <stdio.h>");
	o.ws("\n#include <stdlib.h> /* malloc, free */");
	o.ws("\n");
	o.ws("\nstatic void *read_file");
	o.ws("\n").wind(1).ws("( const char *fname");
	o.ws("\n").wind(1).ws(", size_t unit");
	o.ws("\n").wind(1).ws(", size_t padding");
	o.ws("\n").wind(1).ws(", size_t *pfsize");
	o.ws("\n").wind(1).ws(")");
	o.ws("\n{");
	o.ws("\n").wind(1).ws("void *buffer = NULL;");
	o.ws("\n").wind(1).ws("size_t fsize = 0;");
	o.ws("\n");
	o.ws("\n").wind(1).ws("/* open file */");
	o.ws("\n").wind(1).ws("FILE *f = fopen(fname, \"rb\");");
	o.ws("\n").wind(1).ws("if(f == NULL) {");
	o.ws("\n").wind(2).ws("goto error;");
	o.ws("\n").wind(1).ws("}");
	o.ws("\n");
	o.ws("\n").wind(1).ws("/* get file size */");
	o.ws("\n").wind(1).ws("fseek(f, 0, SEEK_END);");
	o.ws("\n").wind(1).ws("fsize = (size_t) ftell(f) / unit;");
	o.ws("\n").wind(1).ws("fseek(f, 0, SEEK_SET);");
	o.ws("\n");
	o.ws("\n").wind(1).ws("/* allocate memory for file and padding */");
	o.ws("\n").wind(1).ws("buffer = malloc(unit * (fsize + padding));");
	o.ws("\n").wind(1).ws("if (buffer == NULL) {");
	o.ws("\n").wind(2).ws("goto error;");
	o.ws("\n").wind(1).ws("}");
	o.ws("\n");
	o.ws("\n").wind(1).ws("/* read the whole file in memory */");
	o.ws("\n").wind(1).ws("if (fread(buffer, unit, fsize, f) != fsize) {");
	o.ws("\n").wind(2).ws("goto error;");
	o.ws("\n").wind(1).ws("}");
	o.ws("\n");
	o.ws("\n").wind(1).ws("fclose(f);");
	o.ws("\n").wind(1).ws("*pfsize = fsize;");
	o.ws("\n").wind(1).ws("return buffer;");
	o.ws("\n");
	o.ws("\nerror:");
	o.ws("\n").wind(1).ws("fprintf(stderr, \"error: cannot read file '%s'\\n\", fname);");
	o.ws("\n").wind(1).ws("free(buffer);");
	o.ws("\n").wind(1).ws("if (f != NULL) {");
	o.ws("\n").wind(2).ws("fclose(f);");
	o.ws("\n").wind(1).ws("}");
	o.ws("\n").wind(1).ws("return NULL;");
	o.ws("\n}");
	o.ws("\n");
}

void Skeleton::emit_start
	( OutputFile & o
	, size_t maxfill
	, bool backup
	, bool backupctx
	, bool accept
	) const
{
	const size_t sizeof_cunit = opts->encoding.szCodeUnit();
	const uint32_t default_rule = rule2key (rule_rank_t::none ());

	o.ws("\n#define YYCTYPE ");
	exact_uint (o, sizeof_cunit);
	o.ws("\n#define YYKEYTYPE ");
	exact_uint (o, sizeof_key);
	o.ws("\n#define YYPEEK() *cursor");
	o.ws("\n#define YYSKIP() ++cursor");
	if (backup)
	{
		o.ws("\n#define YYBACKUP() marker = cursor");
		o.ws("\n#define YYRESTORE() cursor = marker");
	}
	if (backupctx)
	{
		o.ws("\n#define YYBACKUPCTX() ctxmarker = cursor");
		o.ws("\n#define YYRESTORECTX() cursor = ctxmarker");
	}
	o.ws("\n#define YYLESSTHAN(n) (limit - cursor) < n");
	o.ws("\n#define YYFILL(n) { break; }");
	o.ws("\n");
	o.ws("\nstatic int action_").wstring(name);
	o.ws("\n").wind(1).ws("( unsigned int i");
	o.ws("\n").wind(1).ws(", const YYKEYTYPE *keys");
	o.ws("\n").wind(1).ws(", const YYCTYPE *start");
	o.ws("\n").wind(1).ws(", const YYCTYPE *token");
	o.ws("\n").wind(1).ws(", const YYCTYPE **cursor");
	o.ws("\n").wind(1).ws(", YYKEYTYPE rule_act");
	o.ws("\n").wind(1).ws(")");
	o.ws("\n{");
	o.ws("\n").wind(1).ws("const long pos = token - start;");
	o.ws("\n").wind(1).ws("const long len_act = *cursor - token;");
	o.ws("\n").wind(1).ws("const long len_exp = (long) keys [3 * i + 1];");
	o.ws("\n").wind(1).ws("const YYKEYTYPE rule_exp = keys [3 * i + 2];");
	o.ws("\n").wind(1).ws("if (rule_exp == ").wu32(default_rule).ws(") {");
	o.ws("\n").wind(2).ws("fprintf");
	o.ws("\n").wind(3).ws("( stderr");
	o.ws("\n").wind(3).ws(", \"warning: lex_").wstring(name).ws(": control flow is undefined for input\"");
	o.ws("\n").wind(4).ws("\" at position %ld, rerun re2c with '-W'\\n\"");
	o.ws("\n").wind(3).ws(", pos");
	o.ws("\n").wind(3).ws(");");
	o.ws("\n").wind(1).ws("}");
	o.ws("\n").wind(1).ws("if (len_act == len_exp && rule_act == rule_exp) {");
	o.ws("\n").wind(2).ws("const YYKEYTYPE offset = keys[3 * i];");
	o.ws("\n").wind(2).ws("*cursor = token + offset;");
	o.ws("\n").wind(2).ws("return 0;");
	o.ws("\n").wind(1).ws("} else {");
	o.ws("\n").wind(2).ws("fprintf");
	o.ws("\n").wind(3).ws("( stderr");
	o.ws("\n").wind(3).ws(", \"error: lex_").wstring(name).ws(": at position %ld (iteration %u):\\n\"");
	o.ws("\n").wind(4).ws("\"\\texpected: match length %ld, rule %u\\n\"");
	o.ws("\n").wind(4).ws("\"\\tactual:   match length %ld, rule %u\\n\"");
	o.ws("\n").wind(3).ws(", pos");
	o.ws("\n").wind(3).ws(", i");
	o.ws("\n").wind(3).ws(", len_exp");
	o.ws("\n").wind(3).ws(", rule_exp");
	o.ws("\n").wind(3).ws(", len_act");
	o.ws("\n").wind(3).ws(", rule_act");
	o.ws("\n").wind(3).ws(");");
	o.ws("\n").wind(2).ws("return 1;");
	o.ws("\n").wind(1).ws("}");
	o.ws("\n}");
	o.ws("\n");
	o.ws("\nint lex_").wstring(name).ws("()");
	o.ws("\n{");
	o.ws("\n").wind(1).ws("const size_t padding = ").wu64(maxfill).ws("; /* YYMAXFILL */");
	o.ws("\n").wind(1).ws("int status = 0;");
	o.ws("\n").wind(1).ws("size_t input_len = 0;");
	o.ws("\n").wind(1).ws("size_t keys_count = 0;");
	o.ws("\n").wind(1).ws("YYCTYPE *input = NULL;");
	o.ws("\n").wind(1).ws("YYKEYTYPE *keys = NULL;");
	o.ws("\n").wind(1).ws("const YYCTYPE *cursor = NULL;");
	o.ws("\n").wind(1).ws("const YYCTYPE *limit = NULL;");
	o.ws("\n").wind(1).ws("const YYCTYPE *token = NULL;");
	o.ws("\n").wind(1).ws("const YYCTYPE *eof = NULL;");
	o.ws("\n").wind(1).ws("unsigned int i = 0;");
	o.ws("\n");
	o.ws("\n").wind(1).ws("input = (YYCTYPE *) read_file");
	o.ws("\n").wind(2).ws("(\"").wstring(o.file_name).ws(".").wstring(name).ws(".input\"");
	o.ws("\n").wind(2).ws(", sizeof (YYCTYPE)");
	o.ws("\n").wind(2).ws(", padding");
	o.ws("\n").wind(2).ws(", &input_len");
	o.ws("\n").wind(2).ws(");");
	o.ws("\n").wind(1).ws("if (input == NULL) {");
	o.ws("\n").wind(2).ws("status = 1;");
	o.ws("\n").wind(2).ws("goto end;");
	o.ws("\n").wind(1).ws("}");
	o.ws("\n");
	if (sizeof_cunit > 1)
	{
		o.ws("\n").wind(1).ws("for (i = 0; i < input_len; ++i) {");
		from_le(o, 2, sizeof_cunit, "input[i]");
		o.ws("\n").wind(1).ws("}");
		o.ws("\n");
	}
	o.ws("\n").wind(1).ws("keys = (YYKEYTYPE *) read_file");
	o.ws("\n").wind(2).ws("(\"").wstring(o.file_name).ws(".").wstring(name).ws(".keys\"");
	o.ws("\n").wind(2).ws(", 3 * sizeof (YYKEYTYPE)");
	o.ws("\n").wind(2).ws(", 0");
	o.ws("\n").wind(2).ws(", &keys_count");
	o.ws("\n").wind(2).ws(");");
	o.ws("\n").wind(1).ws("if (keys == NULL) {");
	o.ws("\n").wind(2).ws("status = 1;");
	o.ws("\n").wind(2).ws("goto end;");
	o.ws("\n").wind(1).ws("}");
	o.ws("\n");
	if (sizeof_key > 1)
	{
		o.ws("\n").wind(1).ws("for (i = 0; i < 3 * keys_count; ++i) {");
		from_le(o, 2, sizeof_key, "keys[i]");
		o.ws("\n").wind(1).ws("}");
		o.ws("\n");
	}
	o.ws("\n").wind(1).ws("cursor = input;");
	o.ws("\n").wind(1).ws("limit = input + input_len + padding;");
	o.ws("\n").wind(1).ws("eof = input + input_len;");
	o.ws("\n");
	o.ws("\n").wind(1).ws("for (i = 0; status == 0 && i < keys_count; ++i) {");
	o.ws("\n").wind(2).ws("token = cursor;");
	if (backup)
	{
		o.ws("\n").wind(2).ws("const YYCTYPE *marker = NULL;");
	}
	if (backupctx)
	{
		o.ws("\n").wind(2).ws("const YYCTYPE *ctxmarker = NULL;");
	}
	o.ws("\n").wind(2).ws("YYCTYPE yych;");
	if (accept)
	{
		o.ws("\n").wind(2).ws("unsigned int yyaccept = 0;");
	}
	o.ws("\n");
	if (opts->bFlag && BitMap::first)
	{
		BitMap::gen (o, 2, 0, std::min (0x100u, opts->encoding.nCodeUnits ()));
	}
	o.ws("\n");
}

void Skeleton::emit_end
	( OutputFile & o
	, bool backup
	, bool backupctx
	) const
{
	o.ws("\n").wind(1).ws("}");
	o.ws("\n").wind(1).ws("if (status == 0) {");
	o.ws("\n").wind(2).ws("if (cursor != eof) {");
	o.ws("\n").wind(3).ws("status = 1;");
	o.ws("\n").wind(3).ws("const long pos = token - input;");
	o.ws("\n").wind(3).ws("fprintf(stderr, \"error: lex_").wstring(name).ws(": unused input strings left at position %ld\\n\", pos);");
	o.ws("\n").wind(2).ws("}");
	o.ws("\n").wind(2).ws("if (i != keys_count) {");
	o.ws("\n").wind(3).ws("status = 1;");
	o.ws("\n").wind(3).ws("fprintf(stderr, \"error: lex_").wstring(name).ws(": unused keys left after %u iterations\\n\", i);");
	o.ws("\n").wind(2).ws("}");
	o.ws("\n").wind(1).ws("}");
	o.ws("\n");
	o.ws("\nend:");
	o.ws("\n").wind(1).ws("free(input);");
	o.ws("\n").wind(1).ws("free(keys);");
	o.ws("\n");
	o.ws("\n").wind(1).ws("return status;");
	o.ws("\n}");
	o.ws("\n");
	o.ws("\n#undef YYCTYPE");
	o.ws("\n#undef YYKEYTYPE");
	o.ws("\n#undef YYPEEK");
	o.ws("\n#undef YYSKIP");
	if (backup)
	{
		o.ws("\n#undef YYBACKUP");
		o.ws("\n#undef YYRESTORE");
	}
	if (backupctx)
	{
		o.ws("\n#undef YYBACKUPCTX");
		o.ws("\n#undef YYRESTORECTX");
	}
	o.ws("\n#undef YYLESSTHAN");
	o.ws("\n#undef YYFILL");
	o.ws("\n");
}

void Skeleton::emit_epilog (OutputFile & o, const std::set<std::string> & names)
{
	o.ws("\n").ws("int main()");
	o.ws("\n").ws("{");

	for (std::set<std::string>::const_iterator i = names.begin (); i != names.end (); ++i)
	{
		o.ws("\n").wind(1).ws("if(lex_").wstring(*i).ws("() != 0) {");
		o.ws("\n").wind(2).ws("return 1;");
		o.ws("\n").wind(1).ws("}");
	}

	o.ws("\n").wind(1).ws("return 0;");
	o.ws("\n}");
	o.ws("\n");
}

void Skeleton::emit_action (OutputFile & o, uint32_t ind, rule_rank_t rank) const
{
	o.wind(ind).ws("status = action_").wstring(name).ws("(i, keys, input, token, &cursor, ").wu32(rule2key (rank)).ws(");\n");
	o.wind(ind).ws("continue;\n");
}

} // namespace re2c
