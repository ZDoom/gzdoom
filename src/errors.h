#ifndef __ERRORS_H__
#define __ERRORS_H__

#include <string.h>

#define MAX_ERRORTEXT	1024

class CDoomError
{
public:
	CDoomError ()
	{
		m_Message[0] = '\0';
	}
	CDoomError (const char *message)
	{
		strncpy (m_Message, message, MAX_ERRORTEXT-1);
		m_Message[MAX_ERRORTEXT-1] = '\0';
	}
	const char *GetMessage (void) const
	{
		if (m_Message[0] != '\0')
			return (const char *)m_Message;
		else
			return NULL;
	}

private:
	char m_Message[MAX_ERRORTEXT];
};

class CRecoverableError : public CDoomError
{
public:
	CRecoverableError() : CDoomError() {}
	CRecoverableError(const char *message) : CDoomError(message) {}
};

class CFatalError : public CDoomError
{
public:
	CFatalError() : CDoomError() {}
	CFatalError(const char *message) : CDoomError(message) {}
};

#endif //__ERRORS_H__
