
#pragma once

#include <memory>
#include <list>
#include "vectors.h"
#include "netcommand.h"

struct doomcom_t;
class NetInputPacket;

class NetNodeOutput
{
public:
	void WriteMessage(const void *data, size_t size, bool unreliable);
	void Send(doomcom_t* comm, int nodeIndex);
	void AckPacket(uint8_t headerFlags, uint16_t serial, uint16_t ack);
	
private:
	static bool IsLessEqual(uint16_t serialA, uint16_t serialB);

	struct Message
	{
		Message(const void* initdata, int size, uint16_t serial, bool unreliable) : data(new uint8_t[size]), size(size), serial(serial), unreliable(unreliable) { memcpy(data, initdata, size); }
		~Message() { delete[] data; }

		uint8_t* data;
		int size;
		uint16_t serial;
		bool unreliable;
	};

	std::list<std::unique_ptr<Message>> mMessages;
	uint16_t mSerial = 0;
	uint16_t mAck = 0;
	uint8_t mHeaderFlags = 0;
};

class NetNodeInput
{
public:
	ByteInputStream ReadMessage();
	void ReceivedPacket(NetInputPacket& packet, NetNodeOutput& outputStream);

private:
	void AdvanceToNextPacket();
	static int SerialDiff(uint16_t serialA, uint16_t serialB);

	struct Packet
	{
		Packet(const void* initdata, int size, uint16_t serial) : data(new uint8_t[size]), size(size), serial(serial) { memcpy(data, initdata, size); }
		~Packet() { delete[] data; }

		uint8_t* data;
		int size;
		uint16_t serial;
	};

	Packet* FindFirstPacket();
	Packet* FindNextPacket(Packet *current);
	void RemovePacket(Packet* packet);

	std::list<std::unique_ptr<Packet>> mPackets;
	Packet* mCurrentPacket = nullptr;
	ByteInputStream mPacketStream;
	uint16_t mLastSeenSerial = 0;
};
