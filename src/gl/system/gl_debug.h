#ifndef __GL_DEBUG_H
#define __GL_DEBUG_H

#include <string.h>
#include "gl/system/gl_interface.h"
#include "c_cvars.h"
#include "r_defs.h"

class FGLDebug
{
public:
	void Update();

private:
	void SetupBreakpointMode();
	void UpdateLoggingLevel();
	void OutputMessageLog();

	static void PrintMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message);

	static void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);

	static FString SourceToString(GLenum source);
	static FString TypeToString(GLenum type);
	static FString SeverityToString(GLenum severity);

	GLenum mCurrentLevel = 0;
	bool mBreakpointMode = false;
};

#endif
