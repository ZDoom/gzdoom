#include <sstream>

#include "src/codegen/input_api.h"
#include "src/codegen/indent.h"
#include "src/conf/opt.h"
#include "src/globals.h"

namespace re2c
{

InputAPI::InputAPI ()
	: type_ (DEFAULT)
{}

InputAPI::type_t InputAPI::type () const
{
	return type_;
}

void InputAPI::set (type_t t)
{
	type_ = t;
}

std::string InputAPI::expr_peek () const
{
	std::string s;
	switch (type_)
	{
		case DEFAULT:
			s = "*" + opts->yycursor;
			break;
		case CUSTOM:
			s = opts->yypeek + " ()";
			break;
	}
	return s;
}

std::string InputAPI::expr_peek_save () const
{
	return opts->yych + " = " + opts.yychConversion () + expr_peek ();
}

std::string InputAPI::stmt_peek (uint32_t ind) const
{
	return indent (ind) + expr_peek_save () + ";\n";
}

std::string InputAPI::stmt_skip (uint32_t ind) const
{
	std::string s;
	switch (type_)
	{
		case DEFAULT:
			s = "++" + opts->yycursor;
			break;
		case CUSTOM:
			s = opts->yyskip + " ()";
			break;
	}
	return indent (ind) + s + ";\n";
}

std::string InputAPI::stmt_backup (uint32_t ind) const
{
	std::string s;
	switch (type_)
	{
		case DEFAULT:
			s = opts->yymarker + " = " + opts->yycursor;
			break;
		case CUSTOM:
			s = opts->yybackup + " ()";
			break;
	}
	return indent (ind) + s + ";\n";
}

std::string InputAPI::stmt_backupctx (uint32_t ind) const
{
	std::string s;
	switch (type_)
	{
		case DEFAULT:
			s = opts->yyctxmarker + " = " + opts->yycursor;
			break;
		case CUSTOM:
			s = opts->yybackupctx + " ()";
			break;
	}
	return indent (ind) + s + ";\n";
}

std::string InputAPI::stmt_restore (uint32_t ind) const
{
	std::string s;
	switch (type_)
	{
		case DEFAULT:
			s = opts->yycursor + " = " + opts->yymarker;
			break;
		case CUSTOM:
			s = opts->yyrestore + " ()";
			break;
	}
	return indent (ind) + s + ";\n";
}

std::string InputAPI::stmt_restorectx (uint32_t ind) const
{
	std::string s;
	switch (type_)
	{
		case DEFAULT:
			s = indent (ind) + opts->yycursor + " = " + opts->yyctxmarker + ";\n";
			break;
		case CUSTOM:
			s = indent (ind) + opts->yyrestorectx + " ();\n";
			break;
	}
	return s;
}

std::string InputAPI::stmt_skip_peek (uint32_t ind) const
{
	return type_ == DEFAULT
		? indent (ind) + opts->yych + " = " + opts.yychConversion () + "*++" + opts->yycursor + ";\n"
		: stmt_skip (ind) + stmt_peek (ind);
}

std::string InputAPI::stmt_skip_backup (uint32_t ind) const
{
	return type_ == DEFAULT
		? indent (ind) + opts->yymarker + " = ++" + opts->yycursor + ";\n"
		: stmt_skip (ind) + stmt_backup (ind);
}

std::string InputAPI::stmt_backup_peek (uint32_t ind) const
{
	return type_ == DEFAULT
		? indent (ind) + opts->yych + " = " + opts.yychConversion () + "*(" + opts->yymarker + " = " + opts->yycursor + ");\n"
		: stmt_backup (ind) + stmt_peek (ind);
}

std::string InputAPI::stmt_skip_backup_peek (uint32_t ind) const
{
	return type_ == DEFAULT
		? indent (ind) + opts->yych + " = " + opts.yychConversion () + "*(" + opts->yymarker + " = ++" + opts->yycursor + ");\n"
		: stmt_skip (ind) + stmt_backup (ind) + stmt_peek (ind);
}

std::string InputAPI::expr_lessthan_one () const
{
	return type_ == DEFAULT
		? opts->yylimit + " <= " + opts->yycursor
		: expr_lessthan (1);
}

std::string InputAPI::expr_lessthan (size_t n) const
{
	std::ostringstream s;
	switch (type_)
	{
		case DEFAULT:
			s << "(" << opts->yylimit << " - " << opts->yycursor << ") < " << n;
			break;
		case CUSTOM:
			s << opts->yylessthan << " (" << n << ")";
			break;
	}
	return s.str ();
}

} // end namespace re2c
