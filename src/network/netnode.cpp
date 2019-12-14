
#include "netnode.h"
#include "i_net.h"

void NetNodeOutput::WriteMessage(const void* data, size_t size, bool unreliable)
{
	mMessages.push_back(std::make_unique<Message>(data, (int)size, mSerial, unreliable));
}

void NetNodeOutput::Send(doomcom_t* comm, int nodeIndex)
{
	if (mMessages.empty())
		return;

	NetOutputPacket packet(nodeIndex);
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

bool NetNodeInput::IsMessageAvailable()
{
	return !ReadMessage(true).IsAtEnd();
}

ByteInputStream NetNodeInput::ReadMessage(bool peek)
{
	if (mMessagePeeked)
	{
		mMessagePeeked = false;
		return mPeekMessage;
	}

	while (true)
	{
		AdvanceToNextPacket();

		if (mPacketStream.IsAtEnd())
			break;

		uint16_t serial = mPacketStream.ReadShort();
		uint16_t size = mPacketStream.ReadShort();
		ByteInputStream body = mPacketStream.ReadSubstream(size);

		if (SerialDiff(mLastSeenSerial, serial) > 0)
		{
			mPeekMessage = body;
			mMessagePeeked = peek;
			return body;
		}
	}
	return {};
}

void NetNodeInput::AdvanceToNextPacket()
{
	if (!mPacketStream.IsAtEnd() || mPackets.empty())
		return;

	if (!mCurrentPacket)
	{
		mCurrentPacket = FindFirstPacket();
		mPacketStream = { mCurrentPacket->data, mCurrentPacket->size };
		mLastSeenSerial = mCurrentPacket->serial - 1;
	}
	else
	{
		Packet* nextPacket = FindNextPacket(mCurrentPacket);
		if (nextPacket)
		{
			mLastSeenSerial = mCurrentPacket->serial;
			RemovePacket(mCurrentPacket);
			mCurrentPacket = nextPacket;
			mPacketStream = { mCurrentPacket->data, mCurrentPacket->size };
		}
	}
}

NetNodeInput::Packet* NetNodeInput::FindFirstPacket() const
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

NetNodeInput::Packet* NetNodeInput::FindNextPacket(Packet* current) const
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
		if ((*it).get() == packet)
		{
			mPackets.erase(it);
			break;
		}
	}
}

void NetNodeInput::ReceivedPacket(NetInputPacket& packet, NetNodeOutput& outputStream)
{
	uint8_t headerFlags = packet.stream.ReadByte();
	uint16_t ack = packet.stream.ReadShort();
	uint16_t serial = packet.stream.ReadShort();

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

	mPackets.push_back(std::make_unique<Packet>(packet.stream.GetDataLeft(), packet.stream.BytesLeft(), serial));
}

int NetNodeInput::SerialDiff(uint16_t serialA, uint16_t serialB)
{
	int delta = static_cast<int>(serialB) - static_cast<int>(serialA);
	if (delta < -0x7fff)
		delta += 0x10000;
	else if (delta > 0x7fff)
		delta -= 0x10000;
	return delta;
}
