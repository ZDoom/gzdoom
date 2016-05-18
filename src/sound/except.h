#ifndef __EXCEPT_H
#define __EXCEPT_H

#ifdef _MSC_VER
//==========================================================================
//
// CheckException
//
//==========================================================================

#ifndef FACILITY_VISUALCPP
#define FACILITY_VISUALCPP  ((LONG)0x6d)
#endif
#define VcppException(sev,err)  ((sev) | (FACILITY_VISUALCPP<<16) | err)

inline int CheckException(DWORD code)
{
	if (code == VcppException(ERROR_SEVERITY_ERROR,ERROR_MOD_NOT_FOUND) ||
		code == VcppException(ERROR_SEVERITY_ERROR,ERROR_PROC_NOT_FOUND))
	{
		return EXCEPTION_EXECUTE_HANDLER;
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

#endif

#endif
