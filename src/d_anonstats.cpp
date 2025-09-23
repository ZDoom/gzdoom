#define NO_SEND_STATS
#ifdef NO_SEND_STATS

void D_DoAnonStats()
{
}

void D_ConfirmSendStats()
{
}

#else // !NO_SEND_STATS

#if defined(_WIN32)
#include "i_mainwindow.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
extern const char* sys_ostype;
#else
extern FString sys_ostype;
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#else // !__APPLE__
#include <SDL2/SDL.h>
#endif // __APPLE__
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <thread>
#include "c_cvars.h"
#include "x86.h"
#include "version.h"
#include "v_video.h"
#include "gl_interface.h"
#include "printf.h"

CVAR(Int, anonstats_enabled411, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET)
CVAR(String, anonstats_host, "gzstats.drdteam.org", CVAR_NOSET)
CVAR(Int, anonstats_port, 80, CVAR_NOSET)

#define CHECKVERSION 490
#define CHECKVERSIONSTR "490"
CVAR(Int, sentstats_hwr_done, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET)

std::pair<double, bool> gl_getInfo();
extern int vkversion;



FString URLencode(const char *s)
{
	const char * unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";

	FString out;
	for (size_t i = 0; s[i]; i++)
	{
		if (strchr(unreserved, s[i]))
		{
			out += s[i];
		}
		else
		{
			out.AppendFormat("%%%02X", s[i]&255);
		}
	}
	return out;
}

// accept FString inputs too
FString URLencode(FString s)
{
	return URLencode(s.GetChars());
}

#ifdef _WIN32

bool I_HTTPRequest(const char* request)
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		DPrintf(DMSG_ERROR, "WSAStartup failed.\n");
		return false;
	}
	SOCKET Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct hostent *host;
	host = gethostbyname(anonstats_host);
	if (host == nullptr)
	{
		DPrintf(DMSG_ERROR, "Error looking up hostname.\n");
		return false;
	}
	SOCKADDR_IN SockAddr;
	SockAddr.sin_port = htons(anonstats_port);
	SockAddr.sin_family = AF_INET;
	SockAddr.sin_addr.s_addr = *((uint32_t*)host->h_addr);
	DPrintf(DMSG_NOTIFY, "Connecting to host %s\n", *anonstats_host);
	if (connect(Socket, (SOCKADDR*)(&SockAddr), sizeof(SockAddr)) != 0)
	{
		DPrintf(DMSG_ERROR, "Connection to host %s failed!\n", *anonstats_host);
		return false;
	}
	send(Socket, request, (int)strlen(request), 0);
	char buffer[1024];
	int nDataLength;
	while ((nDataLength = recv(Socket, buffer, 1024, 0)) > 0)
	{
		int i = 0;
		while (buffer[i] >= 32 || buffer[i] == '\n' || buffer[i] == '\r')
		{
			i++;
		}
	}
	closesocket(Socket);
	WSACleanup();
	DPrintf(DMSG_NOTIFY, "Stats send successful.\n");
	return true;
}
#else
bool I_HTTPRequest(const char* request)
{
	if ((*anonstats_host)[0] == 0)
		return false; // no host, disable

	int sockfd, portno, n;
		struct sockaddr_in serv_addr;
		struct hostent *server;

		portno = anonstats_port;
		sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0)
	{
		DPrintf(DMSG_ERROR, "Error opening TCP socket.\n");
		return false;
	}

	server = gethostbyname(anonstats_host);
	if (server == NULL)
	{
		DPrintf(DMSG_ERROR, "Error looking up hostname.\n");
		return false;
	}
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,
		  (char *)&serv_addr.sin_addr.s_addr,
		  server->h_length);
	serv_addr.sin_port = htons(portno);

	DPrintf(DMSG_NOTIFY, "Connecting to host %s\n", *anonstats_host);
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		DPrintf(DMSG_ERROR, "Connection to host %s failed!\n", *anonstats_host);
		return false;
	}

	n = write(sockfd, request, strlen(request));
	if (n<0)
	{
		DPrintf(DMSG_ERROR, "Error writing to socket.\n");
		close(sockfd);
		return false;
	}

	char buffer[1024] = {};
	n = read(sockfd, buffer, 1023);
	close(sockfd);
	DPrintf(DMSG_NOTIFY, "Stats send successful.\n");
	return true;
}
#endif

static FString GetOSVersion()
{
#ifdef _WIN32
#ifndef _M_ARM64
	return FStringf("Windows %s", sys_ostype);
#else
	return FStringf("Windows %s ARM", sys_ostype);
#endif

#elif defined __APPLE__

#if defined(__aarch64__)
	return sys_ostype + " ARM";
#else
	return sys_ostype + " x64";
#endif

#else // fall-through linux stuff here
#ifdef __arm__
	return sys_ostype + " ARM";
#else
	return sys_ostype;
#endif


#endif
}


#ifdef _WIN32

static int  GetCoreInfo()
{
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
	DWORD returnLength = 0;
	int cores = 0;
	uint32_t byteOffset = 0;

	auto rc = GetLogicalProcessorInformation(buffer, &returnLength);

	if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);
		if (!GetLogicalProcessorInformation(buffer, &returnLength)) return 0;
	}
	else
	{
		return 0;
	}

	ptr = buffer;

	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength)
	{
		if (ptr->Relationship == RelationProcessorCore) cores++;
		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		ptr++;
	}
	free(buffer);
	return cores;
}

#else
static int GetCoreInfo()
{
	int cores = std::thread::hardware_concurrency();
	return cores;
}
#endif


static int GetRenderInfo()
{
	if (screen->Backend() == 2) return 1;
	if (screen->Backend() == 1) return 4;
	auto info = gl_getInfo();
	if (!info.second)
	{
		return 2;
	}
	return 3;
}

static int GetGLVersion()
{
	if (screen->Backend() == 1) return vkversion;
	auto info = gl_getInfo();
	return int(info.first * 10);
}

static void D_DoHTTPRequest(const char *request)
{
	if (I_HTTPRequest(request))
	{
		cvar_forceset("sentstats_hwr_done", CHECKVERSIONSTR);
	}
}


static FString GetDeviceName()
{
	FString device = screen->DeviceName();
	if (device.Compare("AMD Radeon(TM) Graphics") == 0 ||
		device.Compare("Intel(R) UHD Graphics") == 0 ||
		//device.Compare("Intel(R) HD Graphics") == 0 || these are not that interesting so leave them alone
		device.Compare("Intel(R) Iris(R) Plus Graphics") == 0 ||
		device.Compare("Intel(R) Iris(R) Xe Graphics") == 0 ||
		device.Compare("Radeon RX Vega") == 0 ||
		device.Compare("AMD Radeon Series") == 0)
	{
		// for these anonymous series names add the CPU name to get an idea what GPU we really have
		auto ci = DumpCPUInfo(&CPU, true);
		device.AppendFormat(" * %s", ci.GetChars());
	}
	// cleanse the GPU info string to allow better searches on the database.
	device.Substitute("/SSE2", "");
	device.Substitute("/PCIe", "");
	device.Substitute("/PCI", "");
	device.Substitute("(TM) ", "");
	device.Substitute("Mesa ", "");
	device.Substitute("DRI ", "");
	auto pos = device.IndexOf("Intel(R)");
	if (pos >= 0)
	{
		device.Substitute("(R) ", "");
		auto pos = device.IndexOf("(");
		if (pos >= 0) device.Truncate(pos);
	}

	pos = device.IndexOf("(LLVM");
	if (pos >= 0) device.Truncate(pos);
	pos = device.IndexOf("(DRM");
	if (pos >= 0) device.Truncate(pos);
	pos = device.IndexOf("(RADV");
	if (pos >= 0) device.Truncate(pos);
	pos = device.IndexOf(", LLVM");
	if (pos >= 0)
	{
		device.Truncate(pos);
		device << ')';
	}
	device.StripLeftRight();
	return device;
}
void D_DoAnonStats()
{
#ifndef _DEBUG
	// Do not repeat if already sent.
	if (anonstats_enabled411 != 1 || sentstats_hwr_done >= CHECKVERSION)
	{
		return;
	}
#endif

	static bool done = false;	// do this only once per session.
	if (done) return;
	done = true;


	static char requeststring[1024];
	mysnprintf(requeststring, sizeof requeststring, "GET /stats_202309.py?render=%i&cores=%i&os=%s&glversion=%i&vendor=%s&model=%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: %s %s\r\n\r\n",
		GetRenderInfo(), GetCoreInfo(), URLencode(GetOSVersion()).GetChars(), GetGLVersion(), URLencode(screen->vendorstring).GetChars(), URLencode(GetDeviceName()).GetChars(), *anonstats_host, GAMENAME, VERSIONSTR);
	DPrintf(DMSG_NOTIFY, "Sending %s", requeststring);
#if 1//ndef _DEBUG
	// Don't send info in debug builds
	std::thread t1(D_DoHTTPRequest, requeststring);
	t1.detach();
#endif
}



void D_ConfirmSendStats()
{
	if (anonstats_enabled411 >= 0)
	{
		return;
	}

	// TODO: texts
	static const char *const MESSAGE_TEXT = "In order to decide where to focus development, the GZDoom team would like to know a little bit about the hardware it is run on.\n" \
		"For this we would like to ask you if we may send three bits of information to gzstats.drdteam.org.\n" \
		"The three items we would like to know about are:\n" \
		"- Operating system\n" \
		"- Number of processor cores\n" \
		"- OpenGL version and your graphics card's name\n\n" \
		"All information sent will be collected anonymously. We will NOT be sending this information to any third party.\n" \
		"It will merely be used for decision-making about GZDoom's future development.\n" \
		"Data will only be sent once per system.\n" \
		"If you are getting this notice more than once, please let us know on the forums. Thanks!\n\n" \
		"May we send this data? If you click 'no', nothing will be sent and you will not be asked again.";

	static const char *const TITLE_TEXT = "GZDoom needs your help!";

	UCVarValue enabled = { 0 };

#ifdef _WIN32
	enabled.Int = MessageBoxA(mainwindow.GetHandle(), MESSAGE_TEXT, TITLE_TEXT, MB_ICONQUESTION | MB_YESNO) == IDYES;
#elif defined __APPLE__
	const CFStringRef messageString = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, MESSAGE_TEXT, kCFStringEncodingASCII, kCFAllocatorNull);
	const CFStringRef titleString = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, TITLE_TEXT, kCFStringEncodingASCII, kCFAllocatorNull);
	if (messageString != nullptr && titleString != nullptr)
	{
		CFOptionFlags response;
		const SInt32 result = CFUserNotificationDisplayAlert(0, kCFUserNotificationNoteAlertLevel, nullptr, nullptr, nullptr,
			titleString, messageString, CFSTR("Yes"), CFSTR("No"), nullptr, &response);
		enabled.Int = result == 0 && (response & 3) == kCFUserNotificationDefaultResponse;
		CFRelease(titleString);
		CFRelease(messageString);
	}
#else // !__APPLE__
	const SDL_MessageBoxButtonData buttons[] =
	{
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Yes" },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "No" },
	};
	const SDL_MessageBoxData messageboxdata =
	{
		SDL_MESSAGEBOX_INFORMATION,
		nullptr,
		TITLE_TEXT,
		MESSAGE_TEXT,
		SDL_arraysize(buttons),
		buttons,
		nullptr
	};
	int buttonid;
	enabled.Int = SDL_ShowMessageBox(&messageboxdata, &buttonid) == 0 && buttonid == 0;
#endif // _WIN32

	anonstats_enabled411->ForceSet(enabled, CVAR_Int);
}

#endif // NO_SEND_STATS
