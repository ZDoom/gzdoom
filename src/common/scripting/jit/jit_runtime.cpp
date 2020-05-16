
#include <memory>
#include "jit.h"
#include "jitintern.h"

static JITRuntime* JITRuntimeVar = nullptr;

JITRuntime* GetJITRuntime()
{
	if (!JITRuntimeVar)
	{
		JITRuntimeVar = new JITRuntime();
	}
	return JITRuntimeVar;
}

FString JitCaptureStackTrace(int framesToSkip, bool includeNativeFrames)
{
	FString lines;

	for (const auto& frame : GetJITRuntime()->captureStackTrace(framesToSkip, includeNativeFrames))
	{
		FString s;
		if (frame.FileName.empty())
			s.Format("Called from %s\n", frame.PrintableName.c_str());
		else if (frame.LineNumber == -1)
			s.Format("Called from %s at %s\n", frame.PrintableName.c_str(), frame.FileName.c_str());
		else
			s.Format("Called from %s at %s, line %d\n", frame.PrintableName.c_str(), frame.FileName.c_str(), frame.LineNumber);

		lines += s;
	}

	return lines;
}

void JitRelease()
{
	delete JITRuntimeVar;
	JITRuntimeVar = nullptr;
}
