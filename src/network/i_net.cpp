//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Low-level networking code. Uses BSD sockets for UDP networking.
//
//-----------------------------------------------------------------------------


/* [Petteri] Check if compiling for Win32:	*/
#if defined(__WINDOWS__) || defined(__NT__) || defined(_MSC_VER) || defined(_WIN32)
#ifndef __WIN32__
#	define __WIN32__
#endif
#endif
/* Follow #ifdef __WIN32__ marks */

#include <stdlib.h>
#include <string.h>

/* [Petteri] Use Winsock for Win32: */
#ifdef __WIN32__
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	include <winsock.h>
#else
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <errno.h>
#	include <unistd.h>
#	include <netdb.h>
#	include <sys/ioctl.h>
#	ifdef __sun
#		include <fcntl.h>
#	endif
#endif

#include "doomtype.h"
#include "i_system.h"
#include "net.h"
#include "m_argv.h"
#include "m_crc32.h"
#include "d_player.h"
#include "st_start.h"
#include "m_misc.h"
#include "doomerrors.h"
#include "atterm.h"

#include "i_net.h"
#include "i_time.h"

// As per http://support.microsoft.com/kb/q192599/ the standard
// size for network buffers is 8k.
#define TRANSMIT_SIZE		8000

/* [Petteri] Get more portable: */
#ifndef __WIN32__
typedef int SOCKET;
#define SOCKET_ERROR		-1
#define INVALID_SOCKET		-1
#define closesocket			close
#define ioctlsocket			ioctl
#define Sleep(x)			usleep (x * 1000)
#define WSAEWOULDBLOCK		EWOULDBLOCK
#define WSAECONNRESET		ECONNRESET
#define WSAGetLastError()	errno
#endif

#ifndef IPPORT_USERRESERVED
#define IPPORT_USERRESERVED 5000
#endif

#ifdef __WIN32__
typedef int socklen_t;
#endif

#ifdef __WIN32__
const char *neterror (void);
#else
#define neterror() strerror(errno)
#endif

class DoomComImpl : public doomcom_t
{
public:
	DoomComImpl(int port);
	~DoomComImpl();

	void PacketSend(const NetOutputPacket &packet) override;
	void PacketGet(NetInputPacket &packet) override;

	int Connect(const char *name) override;
	void Close(int node) override;

private:
	void BuildAddress(sockaddr_in *address, const char *name);
	int FindNode(const sockaddr_in *address);

	SOCKET mSocket = INVALID_SOCKET;

	sockaddr_in mNodeEndpoints[MAXNETNODES];
	uint64_t mNodeLastUpdate[MAXNETNODES];

	uint8_t mTransmitBuffer[TRANSMIT_SIZE];
};

class InitSockets
{
public:
	InitSockets()
	{
#ifdef __WIN32__
		WSADATA wsad;

		if (WSAStartup(0x0101, &wsad))
		{
			I_FatalError("Could not initialize Windows Sockets");
		}
#endif
	}

	~InitSockets()
	{
#ifdef __WIN32__
		WSACleanup();
#endif
	}
};

std::unique_ptr<doomcom_t> I_InitNetwork(int port)
{
	static InitSockets initsockets;
	return std::unique_ptr<doomcom_t>(new DoomComImpl(port));
}

DoomComImpl::DoomComImpl(int port)
{
	memset(mNodeEndpoints, 0, sizeof(mNodeEndpoints));
	memset(mNodeLastUpdate, 0, sizeof(mNodeLastUpdate));

	mSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (mSocket == INVALID_SOCKET)
		I_FatalError("can't create socket: %s", neterror());

	if (port != 0)
	{
		sockaddr_in address;
		memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);

		int v = bind(mSocket, (sockaddr *)&address, sizeof(address));
		if (v == SOCKET_ERROR)
			I_FatalError("BindToPort: %s", neterror());
	}

#ifndef __sun
	u_long trueval = 1;
	ioctlsocket(mSocket, FIONBIO, &trueval);
#else
	u_long trueval = 1;
	fcntl(mysocket, F_SETFL, trueval | O_NONBLOCK);
#endif
}

DoomComImpl::~DoomComImpl()
{
	if (mSocket != INVALID_SOCKET)
	{
		closesocket(mSocket);
		mSocket = INVALID_SOCKET;
	}
}

int DoomComImpl::Connect(const char *name)
{
	sockaddr_in addr;
	BuildAddress(&addr, name);
	return FindNode(&addr);
}

void DoomComImpl::Close(int node)
{
	mNodeLastUpdate[node] = 0;
}

int DoomComImpl::FindNode(const sockaddr_in *address)
{
	int slot = -1;
	for (int i = 0; i < MAXNETNODES; i++)
	{
		if (mNodeLastUpdate[i] != 0 && address->sin_addr.s_addr == mNodeEndpoints[i].sin_addr.s_addr && address->sin_port == mNodeEndpoints[i].sin_port)
		{
			slot = i;
			break;
		}
		else if (mNodeLastUpdate[i] == 0)
		{
			if (slot == -1)
				slot = i;
		}
	}

	if (slot == -1)
		return -1;

	mNodeEndpoints[slot] = *address;
	mNodeLastUpdate[slot] = I_nsTime();
	return slot;
}

void DoomComImpl::PacketSend(const NetOutputPacket &packet)
{
	assert(!(packet.buffer[0] & NCMD_COMPRESSED));

	int packetSize = packet.stream.GetSize() + 1;
	if (packetSize >= 10)
	{
		mTransmitBuffer[0] = packet.buffer[0] | NCMD_COMPRESSED;

		uLong size = TRANSMIT_SIZE - 1;
		int c = compress2(mTransmitBuffer + 1, &size, packet.buffer + 1, packetSize - 1, 9);
		size += 1;

		if (c == Z_OK && size < (uLong)packetSize)
		{
			sendto(mSocket, (char *)mTransmitBuffer, size, 0, (sockaddr *)&mNodeEndpoints[packet.node], sizeof(mNodeEndpoints[packet.node]));
			return;
		}
	}

	if (packetSize <= TRANSMIT_SIZE)
	{
		sendto(mSocket, (char *)packet.buffer, packetSize, 0, (sockaddr *)&mNodeEndpoints[packet.node], sizeof(mNodeEndpoints[packet.node]));
	}
	else
	{
		I_Error("NetPacket is too large to be transmitted");
	}
}

void DoomComImpl::PacketGet(NetInputPacket &packet)
{
	// First check if anything timed out. Treat this as a close.
	uint64_t nowtime = I_nsTime();
	for (int i = 0; i < MAXNETNODES; i++)
	{
		if (mNodeLastUpdate[i] != 0 && nowtime - mNodeLastUpdate[i] > 5'000'000'000) // 5 second timeout
		{
			Close(i);
			packet.node = i;
			packet.stream.SetBuffer(nullptr, 0);
			return;
		}
	}

	while (true)
	{
		sockaddr_in fromaddress;
		socklen_t fromlen = sizeof(fromaddress);
		int size = recvfrom(mSocket, (char*)mTransmitBuffer, TRANSMIT_SIZE, 0, (sockaddr *)&fromaddress, &fromlen);
		if (size == SOCKET_ERROR)
		{
			int err = WSAGetLastError();

			if (err == WSAECONNRESET) // The remote node aborted unexpectedly. Treat this as a close.
			{
				int node = FindNode(&fromaddress);
				if (node == -1)
					continue;

				Close(node);
				packet.node = node;
				packet.stream.SetBuffer(nullptr, 0);
				return;
			}
			else if (err != WSAEWOULDBLOCK)
			{
				I_Error("GetPacket: %s", neterror());
			}
			else // no packet
			{
				packet.node = -1;
				packet.stream.SetBuffer(nullptr, 0);
				return;
			}
		}
		else if (size > 0)
		{
			int node = FindNode(&fromaddress);
			if (node == -1)
				continue;

			packet.buffer[0] = mTransmitBuffer[0] & ~NCMD_COMPRESSED;
			if ((mTransmitBuffer[0] & NCMD_COMPRESSED) && size > 1)
			{
				uLongf msgsize = MAX_MSGLEN - 1;
				int err = uncompress(packet.buffer + 1, &msgsize, mTransmitBuffer + 1, size - 1);
				if (err != Z_OK)
				{
					Printf("Net decompression failed (zlib error %s)\n", M_ZLibError(err).GetChars());
					continue;
				}
				size = msgsize + 1;
			}
			else
			{
				memcpy(packet.buffer + 1, mTransmitBuffer + 1, size - 1);
			}

			packet.node = node;
			packet.stream.SetBuffer(packet.buffer + 1, size - 1);
			return;
		}
	}
}

void DoomComImpl::BuildAddress(sockaddr_in *address, const char *name)
{
	hostent *hostentry;		// host information entry
	u_short port;
	const char *portpart;
	bool isnamed = false;
	int curchar;
	char c;
	FString target;

	address->sin_family = AF_INET;

	if ((portpart = strchr(name, ':')))
	{
		target = FString(name, portpart - name);
		port = atoi(portpart + 1);
		if (!port)
		{
			Printf("Weird port: %s (using %d)\n", portpart + 1, DOOMPORT);
			port = DOOMPORT;
		}
	}
	else
	{
		target = name;
		port = DOOMPORT;
	}
	address->sin_port = htons(port);

	for (curchar = 0; (c = target[curchar]); curchar++)
	{
		if ((c < '0' || c > '9') && c != '.')
		{
			isnamed = true;
			break;
		}
	}

	if (!isnamed)
	{
		address->sin_addr.s_addr = inet_addr(target);
		// Printf("Node number %d, address %s\n", numnodes, target.GetChars());
	}
	else
	{
		hostentry = gethostbyname(target);
		if (!hostentry)
			I_FatalError("gethostbyname: couldn't find %s\n%s", target.GetChars(), neterror());
		address->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];
		// Printf("Node number %d, hostname %s\n", numnodes, hostentry->h_name);
	}
}

#ifdef __WIN32__
const char *neterror (void)
{
	static char neterr[16];
	int			code;

	switch (code = WSAGetLastError ()) {
		case WSAEACCES:				return "EACCES";
		case WSAEADDRINUSE:			return "EADDRINUSE";
		case WSAEADDRNOTAVAIL:		return "EADDRNOTAVAIL";
		case WSAEAFNOSUPPORT:		return "EAFNOSUPPORT";
		case WSAEALREADY:			return "EALREADY";
		case WSAECONNABORTED:		return "ECONNABORTED";
		case WSAECONNREFUSED:		return "ECONNREFUSED";
		case WSAECONNRESET:			return "ECONNRESET";
		case WSAEDESTADDRREQ:		return "EDESTADDRREQ";
		case WSAEFAULT:				return "EFAULT";
		case WSAEHOSTDOWN:			return "EHOSTDOWN";
		case WSAEHOSTUNREACH:		return "EHOSTUNREACH";
		case WSAEINPROGRESS:		return "EINPROGRESS";
		case WSAEINTR:				return "EINTR";
		case WSAEINVAL:				return "EINVAL";
		case WSAEISCONN:			return "EISCONN";
		case WSAEMFILE:				return "EMFILE";
		case WSAEMSGSIZE:			return "EMSGSIZE";
		case WSAENETDOWN:			return "ENETDOWN";
		case WSAENETRESET:			return "ENETRESET";
		case WSAENETUNREACH:		return "ENETUNREACH";
		case WSAENOBUFS:			return "ENOBUFS";
		case WSAENOPROTOOPT:		return "ENOPROTOOPT";
		case WSAENOTCONN:			return "ENOTCONN";
		case WSAENOTSOCK:			return "ENOTSOCK";
		case WSAEOPNOTSUPP:			return "EOPNOTSUPP";
		case WSAEPFNOSUPPORT:		return "EPFNOSUPPORT";
		case WSAEPROCLIM:			return "EPROCLIM";
		case WSAEPROTONOSUPPORT:	return "EPROTONOSUPPORT";
		case WSAEPROTOTYPE:			return "EPROTOTYPE";
		case WSAESHUTDOWN:			return "ESHUTDOWN";
		case WSAESOCKTNOSUPPORT:	return "ESOCKTNOSUPPORT";
		case WSAETIMEDOUT:			return "ETIMEDOUT";
		case WSAEWOULDBLOCK:		return "EWOULDBLOCK";
		case WSAHOST_NOT_FOUND:		return "HOST_NOT_FOUND";
		case WSANOTINITIALISED:		return "NOTINITIALISED";
		case WSANO_DATA:			return "NO_DATA";
		case WSANO_RECOVERY:		return "NO_RECOVERY";
		case WSASYSNOTREADY:		return "SYSNOTREADY";
		case WSATRY_AGAIN:			return "TRY_AGAIN";
		case WSAVERNOTSUPPORTED:	return "VERNOTSUPPORTED";
		case WSAEDISCON:			return "EDISCON";

		default:
			mysnprintf (neterr, countof(neterr), "%d", code);
			return neterr;
	}
}
#endif
