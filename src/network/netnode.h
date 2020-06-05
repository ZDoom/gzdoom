
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

	FString GetStats();
	
private:
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
	bool IsMessageAvailable();
	ByteInputStream ReadMessage(bool peek = false);
	void ReceivedPacket(NetInputPacket& packet, NetNodeOutput& outputStream);

private:
	void AdvanceToNextPacket();

	struct Packet
	{
		Packet(const void* initdata, int size, uint16_t serial) : data(new uint8_t[size]), size(size), serial(serial) { memcpy(data, initdata, size); }
		~Packet() { delete[] data; }

		uint8_t* data;
		int size;
		uint16_t serial;
	};

	Packet* FindFirstPacket() const;
	Packet* FindNextPacket(Packet *current) const;
	void RemovePacket(Packet* packet);

	ByteInputStream mPeekMessage;
	bool mMessagePeeked = false;

	std::list<std::unique_ptr<Packet>> mPackets;
	Packet* mCurrentPacket = nullptr;
	ByteInputStream mPacketStream;
	uint16_t mLastSeenSerial = 0;
};
