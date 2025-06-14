#pragma once

typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef unsigned long VisualID;
typedef XID Window;

class X11NativeHandle
{
public:
	Display* display = nullptr;
	Window window = 0;
};
