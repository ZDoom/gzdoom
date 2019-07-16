
#include "netnode.h"
#include "i_net.h"

void NetNodeOutput::WriteMessage(const void* data, size_t size, bool unreliable)
{
	mMessages.push_back(std::make_unique<Message>(data, (int)size, mSerial, unreliable));
}

void NetNodeOutput::Send(doomcom_t* comm, int nodeIndex)
{
	NetOutputPacket packet(nodeIndex);
	packet.stream.WriteByte(0);
	packet.stream.WriteByte(mHeaderFlags);
	packet.stream.WriteShort(mAck);
	packet.stream.WriteShort(mSerial);

	auto it = mMessages.begin();
	while (it != mMessages.end())
	{
		const auto& msg = *it;

		packet.stream.WriteShort(msg->serial);
		packet.stream.WriteShort(msg->size);
		packet.stream.WriteBuffer(msg->data, msg->size);

		if (msg->unreliable)
		{
			it = mMessages.erase(it);
		}
		else
		{
			++it;
		}
	}

	comm->PacketSend(packet);
	mSerial++;
}

void NetNodeOutput::AckPacket(uint8_t headerFlags, uint16_t serial, uint16_t ack)
{
	mAck = ack;
	mHeaderFlags |= 1;

	if (headerFlags & 1)
	{
		while (!mMessages.empty() && IsLessEqual(serial, mMessages.front()->serial))
		{
			mMessages.erase(mMessages.begin());
		}
	}
}

bool NetNodeOutput::IsLessEqual(uint16_t serialA, uint16_t serialB)
{
	if (serialA <= serialB)
		return serialB - serialA < 0x7fff;
	else
		return serialA - serialB > 0x7fff;
}

/////////////////////////////////////////////////////////////////////////////

ByteInputStream NetNodeInput::ReadMessage()
{
	if (!mCurrentPacket || mCurrentOffset == mCurrentPacket->size)
		AdvanceToNextPacket();

	if (!mCurrentPacket || mCurrentOffset == mCurrentPacket->size)
		return {};

	ByteInputStream header(mCurrentPacket->data + mCurrentOffset, mCurrentPacket->size - mCurrentOffset);
	uint16_t serial = header.ReadShort();
	uint16_t size = header.ReadShort();
	if (mCurrentPacket->size - mCurrentOffset - size < 0)
		return {};

	ByteInputStream body(mCurrentPacket->data + mCurrentOffset + 4, size);
	mCurrentOffset += 4 + size;
	return body;
}

void NetNodeInput::AdvanceToNextPacket()
{
	if (!mCurrentPacket)
	{
		mCurrentPacket = FindFirstPacket();
		mCurrentOffset = 0;
	}
	else
	{
		Packet* nextPacket = FindNextPacket(mCurrentPacket);
		if (nextPacket)
		{
			RemovePacket(mCurrentPacket);
			mCurrentPacket = nextPacket;
			mCurrentOffset = 0;
		}
	}
}

NetNodeInput::Packet* NetNodeInput::FindFirstPacket()
{
	if (mPackets.empty()) return nullptr;

	Packet* first = mPackets.front().get();
	for (const auto& packet : mPackets)
	{
		int delta = SerialDiff(first->serial, packet->serial);
		if (delta < 0)
			first = packet.get();
	}
	return first;
}

NetNodeInput::Packet* NetNodeInput::FindNextPacket(Packet* current)
{
	Packet* nextPacket = nullptr;
	int nextDelta = 0xffff;
	for (const auto& packet : mPackets)
	{
		int delta = SerialDiff(current->serial, packet->serial);
		if (delta > 0 && delta < nextDelta)
		{
			nextDelta = delta;
			nextPacket = packet.get();
		}
	}
	return nextPacket;
}

void NetNodeInput::RemovePacket(Packet* packet)
{
	for (auto it = mPackets.begin(); it != mPackets.end(); ++it)
	{
		if ((*it).get() == mCurrentPacket)
		{
			mPackets.erase(it);
			break;
		}
	}
}

void NetNodeInput::ReceivedPacket(NetInputPacket& packet, NetNodeOutput& outputStream)
{
	const int packetHeaderSize = 6;
	uint8_t headerFlags = packet.stream.ReadByte();
	uint16_t serial = packet.stream.ReadShort();
	uint16_t ack = packet.stream.ReadShort();

	outputStream.AckPacket(headerFlags, ack, serial);

	if (mCurrentPacket)
	{
		// Is this a packet from the past? If so, it arrived too late.
		if (SerialDiff(mCurrentPacket->serial, serial) <= 0)
			return;

		// Check for duplicates
		for (const auto& packet : mPackets)
		{
			if (packet->serial == serial)
				return;
		}
	}

	mPackets.push_back(std::make_unique<Packet>(static_cast<const uint8_t*>(packet.stream.GetData()) + packetHeaderSize, packet.stream.GetSize() - packetHeaderSize, serial));
}

int NetNodeInput::SerialDiff(uint16_t serialA, uint16_t serialB)
{
	int delta = static_cast<int>(serialB) - static_cast<int>(serialA);
	if (delta < -0x7fff)
		delta += 0xffff;
	else if (delta > 0x7fff)
		delta -= 0xffff;
	return delta;
}
