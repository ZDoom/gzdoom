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

	static void LabelObject(GLenum type, GLuint handle, const FString &name);
	static void LabelObjectPtr(void *ptr, const FString &name);

	static void PushGroup(const FString &name);
	static void PopGroup();

private:
	void SetupBreakpointMode();
	void UpdateLoggingLevel();
	void OutputMessageLog();

	static bool IsFilteredByDebugLevel(GLenum severity);
	static void PrintMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message);

	static void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);

	static FString SourceToString(GLenum source);
	static FString TypeToString(GLenum type);
	static FString SeverityToString(GLenum severity);

	GLenum mCurrentLevel = 0;
	bool mBreakpointMode = false;
};

#endif
