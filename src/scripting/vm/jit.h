
#pragma once

#include "vmintern.h"

JitFuncPtr JitCompile(VMScriptFunction *func);
void JitDumpLog(FILE *file, VMScriptFunction *func);
FString JitCaptureStackTrace(int framesToSkip, bool includeNativeFrames);
