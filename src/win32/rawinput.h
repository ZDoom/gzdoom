// Pointers to raw-input related functions. They were introduced with XP,
// so we can't use static linking with them.
#ifndef RIF
#define RIF(name,ret,args) \
	typedef ret (WINAPI *name##Proto)args; \
	extern name##Proto My##name;
#endif

RIF(DefRawInputProc, LRESULT, (PRAWINPUT *paRawInput, INT nInput, UINT cbSizeHeader))
RIF(GetRawInputBuffer, UINT, (PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader))
RIF(GetRawInputData, UINT, (HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader))
RIF(GetRawInputDeviceInfoA, UINT, (HANDLE hDevice, UINT uiCommand, LPVOID pData, PUINT pcbSize))
RIF(GetRawInputDeviceInfoW, UINT, (HANDLE hDevice, UINT uiCommand, LPVOID pData, PUINT pcbSize))
RIF(GetRawInputDeviceList, UINT, (PRAWINPUTDEVICELIST pRawInputDeviceList, PUINT puiNumDevices, UINT cbSize))
RIF(GetRegisteredRawInputDevices, UINT, (PRAWINPUTDEVICE pRawInputDevices, PUINT puiNumDevices, UINT cbSize))
RIF(RegisterRawInputDevices, BOOL, (PCRAWINPUTDEVICE pRawInputDevices, UINT uiNumDevices, UINT cbSize))

#undef RIF
