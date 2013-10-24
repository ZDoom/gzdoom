#define ZCCERR_ERROR	0x40000000

enum EZCCError
{
	ERR_const_def_not_constant			= 20000 | ZCCERR_ERROR,
	ERR_bad_const_def_type				= 20001 | ZCCERR_ERROR,
	ERR_symbol_redefinition				= 20002 | ZCCERR_ERROR,
};
