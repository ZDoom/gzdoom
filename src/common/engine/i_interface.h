#pragma once


struct SystemCallbacks
{
	bool (*WantGuiCapture)();
	bool (*WantLeftButton)();
	bool (*NetGame)();
	bool (*WantNativeMouse)();
	bool (*CaptureModeInGame)();
};

extern SystemCallbacks *sysCallbacks;
