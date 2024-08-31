/*
**  OpenGL debugging support functions
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
*/


#include "gl_system.h"
#include "gl_debug.h"
#include "stats.h"
#include "printf.h"
#include <set>
#include <string>
#include <vector>

CUSTOM_CVAR(Int, gl_debug_level, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (!OpenGLRenderer::FGLDebug::HasDebugApi())
	{
		Printf("No OpenGL debug support detected.\n");
	}
}

CVAR(Bool, gl_debug_breakpoint, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

extern bool gpuStatActive;
extern bool keepGpuStatActive;
extern FString gpuStatOutput;

namespace OpenGLRenderer
{

namespace
{
	std::vector<std::pair<FString, GLuint>> timeElapsedQueries;
}

//-----------------------------------------------------------------------------
//
// Updates OpenGL debugging state
//
//-----------------------------------------------------------------------------

void FGLDebug::Update()
{
	gpuStatOutput = "";
	for (auto &query : timeElapsedQueries)
	{
		GLuint timeElapsed = 0;
		glGetQueryObjectuiv(query.second, GL_QUERY_RESULT, &timeElapsed);
		glDeleteQueries(1, &query.second);

		FString out;
		out.Format("%s=%04.2f ms\n", query.first.GetChars(), timeElapsed / 1000000.0f);
		gpuStatOutput += out;
	}
	timeElapsedQueries.clear();

	gpuStatActive = keepGpuStatActive;
	keepGpuStatActive = false;

	if (!HasDebugApi())
		return;

	SetupBreakpointMode();
	UpdateLoggingLevel();
	OutputMessageLog();
}

//-----------------------------------------------------------------------------
//
// Label objects so they are referenced by name in debug messages and in
// OpenGL debuggers (renderdoc)
//
//-----------------------------------------------------------------------------

void FGLDebug::LabelObject(GLenum type, GLuint handle, const char *name)
{
	if (HasDebugApi() && gl_debug_level != 0)
	{
		glObjectLabel(type, handle, -1, name);
	}
}

void FGLDebug::LabelObjectPtr(void *ptr, const char *name)
{
	if (HasDebugApi() && gl_debug_level != 0)
	{
		glObjectPtrLabel(ptr, -1, name);
	}
}

//-----------------------------------------------------------------------------
//
// Marks which render pass/group is executing commands so that debuggers can
// display this information
//
//-----------------------------------------------------------------------------

void FGLDebug::PushGroup(const FString &name)
{
	if (HasDebugApi() && gl_debug_level != 0)
	{
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, (GLsizei)name.Len(), name.GetChars());
	}

	if (gpuStatActive)
	{
		GLuint queryHandle = 0;
		glGenQueries(1, &queryHandle);
		glBeginQuery(GL_TIME_ELAPSED, queryHandle);
		timeElapsedQueries.push_back({ name, queryHandle });
	}
}

void FGLDebug::PopGroup()
{
	if (HasDebugApi() && gl_debug_level != 0)
	{
		glPopDebugGroup();
	}

	if (gpuStatActive)
	{
		glEndQuery(GL_TIME_ELAPSED);
	}
}

//-----------------------------------------------------------------------------
//
// Turns on synchronous debugging on and off based on gl_debug_breakpoint
//
// Allows getting the debugger to break exactly at the OpenGL function emitting
// a message.
//
//-----------------------------------------------------------------------------

void FGLDebug::SetupBreakpointMode()
{
	if (mBreakpointMode != gl_debug_breakpoint)
	{
		if (gl_debug_breakpoint)
		{
			glDebugMessageCallback(&FGLDebug::DebugCallback, this);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		}
		else
		{
			glDebugMessageCallback(nullptr, nullptr);
			glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		}
		mBreakpointMode = gl_debug_breakpoint;
	}
}

//-----------------------------------------------------------------------------
//
// Tells OpenGL which debug messages we are interested in
//
//-----------------------------------------------------------------------------

void FGLDebug::UpdateLoggingLevel()
{
	const GLenum level = gl_debug_level;
	if (level != mCurrentLevel)
	{
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0, nullptr, level > 0);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, level > 1);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, nullptr, level > 2);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, level > 3);
		mCurrentLevel = level;
	}
}

//-----------------------------------------------------------------------------
//
// The log may already contain entries for a debug level we are no longer
// interested in..
//
//-----------------------------------------------------------------------------

bool FGLDebug::IsFilteredByDebugLevel(GLenum severity)
{
	int severityLevel = 0;
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH: severityLevel = 1; break;
	case GL_DEBUG_SEVERITY_MEDIUM: severityLevel = 2; break;
	case GL_DEBUG_SEVERITY_LOW: severityLevel = 3; break;
	case GL_DEBUG_SEVERITY_NOTIFICATION: severityLevel = 4; break;
	}
	return severityLevel > (int)gl_debug_level;
}

//-----------------------------------------------------------------------------
//
// Prints all logged messages to the console
//
//-----------------------------------------------------------------------------

void FGLDebug::OutputMessageLog()
{
	if (mCurrentLevel <= 0)
		return;

	GLint maxDebugMessageLength = 0;
	glGetIntegerv(GL_MAX_DEBUG_MESSAGE_LENGTH, &maxDebugMessageLength);

	const int maxMessages = 50;
	const int messageLogSize = maxMessages * maxDebugMessageLength;

	TArray<GLenum> sources, types, severities;
	TArray<GLuint> ids;
	TArray<GLsizei> lengths;
	TArray<GLchar> messageLog;

	sources.Resize(maxMessages);
	types.Resize(maxMessages);
	severities.Resize(maxMessages);
	ids.Resize(maxMessages);
	lengths.Resize(maxMessages);
	messageLog.Resize(messageLogSize);

	while (true)
	{
		GLuint numMessages = glGetDebugMessageLog(maxMessages, messageLogSize, &sources[0], &types[0], &ids[0], &severities[0], &lengths[0], &messageLog[0]);
		if (numMessages <= 0) break;

		GLsizei offset = 0;
		for (GLuint i = 0; i < numMessages; i++)
		{
			if (!IsFilteredByDebugLevel(severities[i]))
				PrintMessage(sources[i], types[i], ids[i], severities[i], lengths[i], &messageLog[offset]);
			offset += lengths[i];
		}
	}
}

//-----------------------------------------------------------------------------
//
// Print a single message to the console
//
//-----------------------------------------------------------------------------

void FGLDebug::PrintMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message)
{
	if (type == GL_DEBUG_TYPE_PUSH_GROUP || type == GL_DEBUG_TYPE_POP_GROUP)
		return;

	const int maxMessages = 50;

	static int messagesPrinted = 0;
	if (messagesPrinted > maxMessages)
		return;

	FString msg(message, length);

	static std::set<std::string> seenMessages;
	bool alreadySeen = !seenMessages.insert(msg.GetChars()).second;
	if (alreadySeen)
		return;

	messagesPrinted++;
	if (messagesPrinted == maxMessages)
	{
		Printf("Max OpenGL debug messages reached. Suppressing further output.\n");
	}
	else if (messagesPrinted < maxMessages)
	{
		FString sourceStr = SourceToString(source);
		FString typeStr = TypeToString(type);
		FString severityStr = SeverityToString(severity);
		if (type != GL_DEBUG_TYPE_OTHER)
			Printf("[%s] %s, %s: %s\n", sourceStr.GetChars(), severityStr.GetChars(), typeStr.GetChars(), msg.GetChars());
		else
			Printf("[%s] %s: %s\n", sourceStr.GetChars(), severityStr.GetChars(), msg.GetChars());
	}
}

//-----------------------------------------------------------------------------
//
// OpenGL callback function used when synchronous debugging is enabled
//
//-----------------------------------------------------------------------------

void FGLDebug::DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
	if (IsFilteredByDebugLevel(severity))
		return;

	PrintMessage(source, type, id, severity, length, message);
	assert(severity == GL_DEBUG_SEVERITY_NOTIFICATION);
}

//-----------------------------------------------------------------------------
//
// Enum to string helpers
//
//-----------------------------------------------------------------------------

FString FGLDebug::SourceToString(GLenum source)
{
	FString s;
	switch (source)
	{
	case GL_DEBUG_SOURCE_API: s = "api"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: s = "window system"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: s = "shader compiler"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY: s = "third party"; break;
	case GL_DEBUG_SOURCE_APPLICATION: s = "application"; break;
	case GL_DEBUG_SOURCE_OTHER: s = "other"; break;
	default: s.Format("%d", (int)source);
	}
	return s;
}

FString FGLDebug::TypeToString(GLenum type)
{
	FString s;
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR: s = "error"; break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: s = "deprecated"; break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: s = "undefined"; break;
	case GL_DEBUG_TYPE_PORTABILITY: s = "portability"; break;
	case GL_DEBUG_TYPE_PERFORMANCE: s = "performance"; break;
	case GL_DEBUG_TYPE_MARKER: s = "marker"; break;
	case GL_DEBUG_TYPE_PUSH_GROUP: s = "push group"; break;
	case GL_DEBUG_TYPE_POP_GROUP: s = "pop group"; break;
	case GL_DEBUG_TYPE_OTHER: s = "other"; break;
	default: s.Format("%d", (int)type);
	}
	return s;
}

FString FGLDebug::SeverityToString(GLenum severity)
{
	FString s;
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_LOW: s = "low severity"; break;
	case GL_DEBUG_SEVERITY_MEDIUM: s = "medium severity"; break;
	case GL_DEBUG_SEVERITY_HIGH: s = "high severity"; break;
	case GL_DEBUG_SEVERITY_NOTIFICATION: s = "notification"; break;
	default: s.Format("%d", (int)severity);
	}
	return s;
}

}