
#pragma once

#include <memory>

#define MAX_MSGLEN 14000

struct NetPacket
{
	NetPacket() { memset(data, 0, sizeof(data)); }

	// packet data to be sent
	uint8_t	data[MAX_MSGLEN];

	// bytes in data to be sent
	int16_t	datalength = 0;

	// dest for send, set by get (-1 = no packet).
	int16_t	remotenode = 0;

	uint8_t &operator[](int i) { return data[i]; }
	const uint8_t &operator[](int i) const { return data[i]; }
};

// Network packet data.
struct doomcom_t
{
	virtual ~doomcom_t() { }

	virtual void PacketSend(const NetPacket &packet) = 0;
	virtual void PacketGet(NetPacket &packet) = 0;

	// info common to all nodes
	int16_t	numnodes = 0; // console is always node 0.

	// info specific to this node
	int16_t	consoleplayer = 0;
	int16_t	numplayers = 0;
};

std::unique_ptr<doomcom_t> I_InitNetwork();
