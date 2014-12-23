#ifndef __I_INPUT_H__
#define __I_INPUT_H__

void I_PutInClipboard (const char *str);
FString I_GetFromClipboard (bool use_primary_selection);
void I_SetMouseCapture();
void I_ReleaseMouseCapture();

#endif

