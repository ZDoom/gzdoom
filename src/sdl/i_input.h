#ifndef __I_INPUT_H__
#define __I_INPUT_H__

void I_PutInClipboard (const char *str);
FString I_GetFromClipboard ();

struct GUIDName
{
        GUID ID;
        char *Name;
};

extern TArray<GUIDName> JoystickNames;
extern char *JoyAxisNames[8];

extern void DI_EnumJoy ();

#endif

