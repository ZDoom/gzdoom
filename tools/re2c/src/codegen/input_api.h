#ifndef _RE2C_CODEGEN_INPUT_API_
#define _RE2C_CODEGEN_INPUT_API_

#include "src/util/c99_stdint.h"
#include <string>

namespace re2c
{

class InputAPI
{
public:
	enum type_t
		{ DEFAULT
		, CUSTOM
		};

private:
	type_t type_;

public:
	InputAPI ();
	type_t type () const;
	void set (type_t t);
	std::string expr_peek () const;
	std::string expr_peek_save () const;
	std::string stmt_peek (uint32_t ind) const;
	std::string stmt_skip (uint32_t ind) const;
	std::string stmt_backup (uint32_t ind) const;
	std::string stmt_backupctx (uint32_t ind) const;
	std::string stmt_restore (uint32_t ind) const;
	std::string stmt_restorectx (uint32_t ind) const;
	std::string stmt_skip_peek (uint32_t ind) const;
	std::string stmt_skip_backup (uint32_t ind) const;
	std::string stmt_backup_peek (uint32_t ind) const;
	std::string stmt_skip_backup_peek (uint32_t ind) const;
	std::string expr_lessthan_one () const;
	std::string expr_lessthan (size_t n) const;
};

} // end namespace re2c

#endif // _RE2C_CODEGEN_INPUT_API_
