
#pragma once

#include <memory>
#include "netcommand.h"

#define MAX_MSGLEN 14000
#define DOOMPORT 5029

class NetOutputPacket
{
public:
	NetOutputPacket(int node) : node(node), stream(buffer + 1, MAX_MSGLEN - 1) { buffer[0] = 0; }

	int node = 0;
	ByteOutputStream stream;

private:
	uint8_t	buffer[MAX_MSGLEN];

	NetOutputPacket(const NetOutputPacket &) = delete;
	NetOutputPacket &operator=(const NetOutputPacket &) = delete;
	friend class DoomComImpl;
};

class NetInputPacket
{
public:
	NetInputPacket() = default;

	int node = -1; // -1 = no packet available
	ByteInputStream stream;

private:
	uint8_t	buffer[MAX_MSGLEN];

	NetInputPacket(const NetInputPacket &) = delete;
	NetInputPacket &operator=(const NetInputPacket &) = delete;
	friend class DoomComImpl;
};

// Network packet data.
struct doomcom_t
{
	virtual ~doomcom_t() { }

	virtual void PacketSend(const NetOutputPacket &packet) = 0;
	virtual void PacketGet(NetInputPacket &packet) = 0;

	virtual int Connect(const char *name) = 0;
	virtual void Close(int node) = 0;
};

std::unique_ptr<doomcom_t> I_InitNetwork(int port);
