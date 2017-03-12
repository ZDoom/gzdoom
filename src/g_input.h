#pragma once

// These were in i_input.h, which differed between platforms and on Windows caused problems with its 
// inclusion of system specific data, so it has been separated into this platform independent file.
void I_PutInClipboard (const char *str);
FString I_GetFromClipboard (bool use_primary_selection);
void I_SetMouseCapture();
void I_ReleaseMouseCapture();

