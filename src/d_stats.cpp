
#ifdef NO_SEND_STATS

void D_DoAnonStats()
{
}

#else // !NO_SEND_STATS

#if defined(_WIN32)
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
extern int sys_ostype;
#else
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

EXTERN_CVAR(Bool, vid_glswfb)
extern int currentrenderer;
CVAR(String, sys_statshost, "gzstats.drdteam.org", CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOSET)
CVAR(Int, sys_statsport, 80, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOSET)

// Each machine will only send two  reports, one when started with hardware rendering and one when started with software rendering.
#define CHECKVERSION 330
#define CHECKVERSIONSTR "330"
CVAR(Int, sentstats_swr_done, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET)
CVAR(Int, sentstats_hwr_done, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET)

std::pair<double, bool> gl_getInfo();


#ifdef _WIN32

bool I_HTTPRequest(const char* request)
{
	if (sys_statshost.GetHumanString() == NULL)
		return false; // no host, disable

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		DPrintf(DMSG_ERROR, "WSAStartup failed.\n");
		return false;
	}
	SOCKET Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct hostent *host;
	host = gethostbyname(sys_statshost.GetHumanString());
	SOCKADDR_IN SockAddr;
	SockAddr.sin_port = htons(sys_statsport);
	SockAddr.sin_family = AF_INET;
	SockAddr.sin_addr.s_addr = *((uint32_t*)host->h_addr);
	DPrintf(DMSG_NOTIFY, "Connecting to host %s\n", sys_statshost.GetHumanString());
	if (connect(Socket, (SOCKADDR*)(&SockAddr), sizeof(SockAddr)) != 0)
	{
		DPrintf(DMSG_ERROR, "Connection to host %s failed!\n", sys_statshost.GetHumanString());
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
	if (sys_statshost.GetHumanString() == NULL || sys_statshost.GetHumanString()[0] == 0)
		return false; // no host, disable

	int sockfd, portno, n;
		struct sockaddr_in serv_addr;
		struct hostent *server;

		portno = sys_statsport;
		sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0)
	{
		DPrintf(DMSG_ERROR, "Error opening TCP socket.\n");
		return false;
	}

	server = gethostbyname(sys_statshost.GetHumanString());
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

	DPrintf(DMSG_NOTIFY, "Connecting to host %s\n", sys_statshost.GetHumanString());
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		DPrintf(DMSG_ERROR, "Connection to host %s failed!\n", sys_statshost.GetHumanString());
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

static int GetOSVersion()
{
#ifdef _WIN32
	if (sys_ostype == 1) return 1;
	if (sizeof(void*) == 4)	// 32 bit
	{
		BOOL res;
		if (IsWow64Process(GetCurrentProcess(), &res) && res)
		{
			return 6;
		}
		if (sys_ostype == 2) return 2;
		else return 4;
	}
	else
	{
		if (sys_ostype == 2) return 3;
		else return 5;
	}

#elif defined __APPLE__

	if (sizeof(void*) == 4)	// 32 bit
	{
		return 7;
	}
	else
	{
		return 8;
	}

#else

// fall-through linux stuff here
#ifdef __arm__
	return 10;
#elif __ppc__
	return 9;
#else
	if (sizeof(void*) == 4)	// 32 bit
	{
		return 11;
	}
	else
	{
		return 12;
	}
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
	return cores < 2 ? 0 : cores < 4 ? 1 : cores < 6 ? 2 : cores < 8 ? 3 : 4;
}

#else
static int GetCoreInfo()
{
	int cores = std::thread::hardware_concurrency();
	if (CPU.HyperThreading) cores /= 2;
	return cores < 2? 0 : cores < 4? 1 : cores < 6? 2 : cores < 8? 3 : 4;
}
#endif

static int GetRenderInfo()
{
	if (currentrenderer == 0)
	{
		if (!screen->Accel2D) return 0;
		if (vid_glswfb) return 2;
		if (screen->LegacyHardware()) return 6;
		return 1;
	}
	else
	{
		auto info = gl_getInfo();
		if (info.first < 3.3) return 3;	// Legacy OpenGL. Don't care about Intel HD 3000 on Windows being run in 'risky' mode.
		if (!info.second) return 4;
		return 5;
	}
}

static void D_DoHTTPRequest(const char *request)
{
	if (I_HTTPRequest(request))
	{
		if (currentrenderer == 0)
		{
			cvar_forceset("sentstats_swr_done", CHECKVERSIONSTR);
		}
		else
		{
			cvar_forceset("sentstats_hwr_done", CHECKVERSIONSTR);
		}
	}
}

void D_DoAnonStats()
{
	static bool done = false;	// do this only once per session.
	if (done) return;
	done = true;

	// Do not repeat if already sent.
	if (currentrenderer == 0 && sentstats_swr_done >= CHECKVERSION) return;
	if (currentrenderer == 1 && sentstats_hwr_done >= CHECKVERSION) return;

	static char requeststring[1024];
	mysnprintf(requeststring, sizeof requeststring, "GET /stats.php?render=%i&cores=%i&os=%i&renderconfig=%i HTTP/1.1\nHost: %s\nConnection: close\nUser-Agent: %s %s\n\n",
		GetRenderInfo(), GetCoreInfo(), GetOSVersion(), currentrenderer, sys_statshost.GetHumanString(), GAMENAME, VERSIONSTR);
	DPrintf(DMSG_NOTIFY, "Sending %s", requeststring);
	std::thread t1(D_DoHTTPRequest, requeststring);
	t1.detach();
}

#endif // NO_SEND_STATS
