//-----------------------------------------------------------------------------
//
// Copyright 2018 Benjamin Berkels
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

#include "d_player.h"
#include "d_netsync.h"
#include "c_dispatch.h"
#include "actor.h"
#include "doomstat.h"

extern bool netserver, netclient;

//*****************************************************************************
//	VARIABLES

// [BB] Are we measuring outbound traffic?
static	bool	g_MeasuringOutboundTraffic = false;
// [BB] Number of bytes sent by NETWORK_Write* since NETWORK_StartTrafficMeasurement() was called.
static	int		g_OutboundBytesMeasured = 0;

IDList<AActor> g_NetIDList;

//*****************************************************************************
//
void NETWORK_AdvanceByteStreamPointer( BYTESTREAM_s *pByteStream, const int NumBytes, const bool OutboundTraffic )
{
	pByteStream->pbStream += NumBytes;

	// [BB]
	if ( g_MeasuringOutboundTraffic && OutboundTraffic )
		g_OutboundBytesMeasured += NumBytes;
}

//*****************************************************************************
//
int NETWORK_ReadByte( BYTESTREAM_s *pByteStream )
{
	int	Byte = -1;

	if (( pByteStream->pbStream + 1 ) <= pByteStream->pbStreamEnd )
		Byte = *pByteStream->pbStream;

	// Advance the pointer.
	pByteStream->pbStream += 1;

	return ( Byte );
}

//*****************************************************************************
//
void NETWORK_WriteByte( BYTESTREAM_s *pByteStream, int Byte )
{
	if (( pByteStream->pbStream + 1 ) > pByteStream->pbStreamEnd )
	{
		Printf( "NETWORK_WriteByte: Overflow!\n" );
		return;
	}

	*pByteStream->pbStream = Byte;

	// Advance the pointer.
	NETWORK_AdvanceByteStreamPointer ( pByteStream, 1, true );
}

//*****************************************************************************
//
void NETWORK_WriteShort( BYTESTREAM_s *pByteStream, int Short )
{
	if (( pByteStream->pbStream + 2 ) > pByteStream->pbStreamEnd )
	{
		Printf( "NETWORK_WriteShort: Overflow!\n" );
		return;
	}

	pByteStream->pbStream[0] = Short & 0xff;
	pByteStream->pbStream[1] = Short >> 8;

	// Advance the pointer.
	NETWORK_AdvanceByteStreamPointer ( pByteStream, 2, true );
}

//*****************************************************************************
//
void NETWORK_WriteLong( BYTESTREAM_s *pByteStream, int Long )
{
	if (( pByteStream->pbStream + 4 ) > pByteStream->pbStreamEnd )
	{
		Printf( "NETWORK_WriteLong: Overflow!\n" );
		return;
	}

	pByteStream->pbStream[0] = Long & 0xff;
	pByteStream->pbStream[1] = ( Long >> 8 ) & 0xff;
	pByteStream->pbStream[2] = ( Long >> 16 ) & 0xff;
	pByteStream->pbStream[3] = ( Long >> 24 );

	// Advance the pointer.
	NETWORK_AdvanceByteStreamPointer ( pByteStream, 4, true );
}

//*****************************************************************************
//
void NETWORK_WriteBuffer( BYTESTREAM_s *pByteStream, const void *pvBuffer, int nLength )
{
	if (( pByteStream->pbStream + nLength ) > pByteStream->pbStreamEnd )
	{
		Printf( "NETWORK_WriteLBuffer: Overflow!\n" );
		return;
	}

	memcpy( pByteStream->pbStream, pvBuffer, nLength );

	// Advance the pointer.
	NETWORK_AdvanceByteStreamPointer ( pByteStream, nLength, true );
}

//*****************************************************************************
//
void NETWORK_WriteBit( BYTESTREAM_s *byteStream, bool bit )
{
	// Add a bit to this byte
	byteStream->EnsureBitSpace( 1, true );
	if ( bit )
		*byteStream->bitBuffer |= 1 << byteStream->bitShift;
	++byteStream->bitShift;
}

//*****************************************************************************
//
void NETWORK_WriteVariable( BYTESTREAM_s *byteStream, int value )
{
	int length;

	// Determine how long we need to send this value
	if ( value == 0 )
		length = 0; // 0 - don't bother sending it at all
	else if (( value <= 0xFF ) && ( value >= 0 ))
		length = 1; // Can be sent as a byte
	else if (( value <= 0x7FFF ) && ( value >= -0x8000 ))
		length = 2; // Can be sent as a short
	else
		length = 3; // Must be sent as a long

	// Write this length as two bits
	NETWORK_WriteBit( byteStream, !!( length & 1 ) );
	NETWORK_WriteBit( byteStream, !!( length & 2 ) );

	// Depending on the required length, write the value.
	switch ( length )
	{
	case 1: NETWORK_WriteByte( byteStream, value ); break;
	case 2: NETWORK_WriteShort( byteStream, value ); break;
	case 3: NETWORK_WriteLong( byteStream, value ); break;
	}
}

//*****************************************************************************
//
void NETWORK_WriteShortByte( BYTESTREAM_s *byteStream, int value, int bits )
{
	if (( bits < 1 ) || ( bits > 8 ))
	{
		Printf( "NETWORK_WriteShortByte: bits must be within range [1..8], got %d.\n", bits );
		return;
	}

	byteStream->EnsureBitSpace( bits, true );
	value &= (( 1 << bits ) - 1 ); // Form a mask from the bits and trim our value using it.
	value <<= byteStream->bitShift; // Shift the value to its proper position.
	*byteStream->bitBuffer |= value; // Add it to the byte.
	byteStream->bitShift += bits; // Bump the shift value accordingly.
}

//*****************************************************************************
//
void NetSyncData::AssignNetID ( AActor *pActor )
{
	if ( netserver )
	{
		NetID = g_NetIDList.getNewID( );
		g_NetIDList.useID ( NetID, pActor );
	}
	else
		NetID = -1;
}

//*****************************************************************************
//
void NetSyncData::FreeNetID ( )
{
	g_NetIDList.freeID ( NetID );
}

//*****************************************************************************
//
BYTESTREAM_s::BYTESTREAM_s() :
	bitBuffer( NULL ),
	bitShift( -1 ) {}

//*****************************************************************************
//
void BYTESTREAM_s::EnsureBitSpace( int bits, bool writing )
{
	if ( ( bitBuffer == NULL ) || ( bitShift < 0 ) || ( bitShift + bits > 8 ) )
	{
		if ( writing )
		{
			// Not enough bits left in our current byte, we need a new one.
			NETWORK_WriteByte( this, 0 );
			bitBuffer = pbStream - 1;
		}
		else
		{
			// No room for the value in this byte, so we need a new one.
			if ( NETWORK_ReadByte( this ) != -1 )
			{
				bitBuffer = pbStream - 1;
			}
			else
			{
				// Argh! No bytes left!
				Printf("BYTESTREAM_s::EnsureBitSpace: out of bytes to use\n");
				static uint8_t fallback = 0;
				bitBuffer = &fallback;
			}
		}

		bitShift = 0;
	}
}

//*****************************************************************************
//
NETBUFFER_s::NETBUFFER_s ( )
{
	this->pbData = NULL;
	this->ulMaxSize = 0;
	this->BufferType = (BUFFERTYPE_e)0;
	Clear();
}

//*****************************************************************************
//
NETBUFFER_s::NETBUFFER_s ( const NETBUFFER_s &Buffer )
{
	Init ( Buffer.ulMaxSize, Buffer.BufferType );
	Clear();

	memcpy( this->pbData, Buffer.pbData, Buffer.ulMaxSize );
	this->ByteStream.pbStream = this->pbData + ( Buffer.ByteStream.pbStream - Buffer.pbData );
	this->ByteStream.pbStreamEnd = this->pbData + ( Buffer.ByteStream.pbStreamEnd - Buffer.pbData );
	this->ByteStream.bitShift = Buffer.ByteStream.bitShift;
	if ( Buffer.ByteStream.bitBuffer != NULL )
		this->ByteStream.bitBuffer = this->ByteStream.pbStream + ( Buffer.ByteStream.bitBuffer - Buffer.ByteStream.pbStream );

	this->ulCurrentSize = Buffer.ulCurrentSize;
}

//*****************************************************************************
//
void NETBUFFER_s::Init( unsigned int ulLength, BUFFERTYPE_e BufferType )
{
	memset( this, 0, sizeof( *this ));
	this->ulMaxSize = ulLength;
	this->pbData = new uint8_t[ulLength];
	this->BufferType = BufferType;
}

//*****************************************************************************
//
void NETBUFFER_s::Free()
{
	if ( this->pbData )
	{
		delete[] ( this->pbData );
		this->pbData = NULL;
	}

	this->ulMaxSize = 0;
	this->BufferType = (BUFFERTYPE_e)0;
}

//*****************************************************************************
//
void NETBUFFER_s::Clear()
{
	this->ulCurrentSize = 0;
	this->ByteStream.pbStream = this->pbData;
	this->ByteStream.bitBuffer = NULL;
	this->ByteStream.bitShift = -1;
	if ( this->BufferType == BUFFERTYPE_READ )
		this->ByteStream.pbStreamEnd = this->ByteStream.pbStream;
	else
		this->ByteStream.pbStreamEnd = this->ByteStream.pbStream + this->ulMaxSize;
}

//*****************************************************************************
//
int NETBUFFER_s::CalcSize() const
{
	if ( this->BufferType == BUFFERTYPE_READ )
		return ( int( this->ByteStream.pbStreamEnd - this->ByteStream.pbStream ));
	else
		return ( int( this->ByteStream.pbStream - this->pbData ));
}

//*****************************************************************************
//
int NETBUFFER_s::WriteTo( BYTESTREAM_s &ByteStream ) const
{
	int bufferSize = CalcSize();
	if ( bufferSize > 0 )
		NETWORK_WriteBuffer( &ByteStream, this->pbData, bufferSize );
	return bufferSize;
}

//*****************************************************************************
//
NetCommand::NetCommand ( const NetPacketType Header ) :
	_unreliable( false )
{
	_buffer.Init( MAX_UDP_PACKET, BUFFERTYPE_WRITE );
	_buffer.Clear();
	addByte( static_cast<int>(Header) );
}

//*****************************************************************************
//
NetCommand::~NetCommand ( )
{
	_buffer.Free();
}

//*****************************************************************************
//
void NetCommand::addInteger( const int IntValue, const int Size )
{
	if ( ( _buffer.ByteStream.pbStream + Size ) > _buffer.ByteStream.pbStreamEnd )
	{
		Printf( "NetCommand::AddInteger: Overflow! Header: %d\n", _buffer.pbData[0] );
		return;
	}

	for ( int i = 0; i < Size; ++i )
		_buffer.ByteStream.pbStream[i] = ( IntValue >> ( 8*i ) ) & 0xff;

	_buffer.ByteStream.pbStream += Size;
	_buffer.ulCurrentSize = _buffer.CalcSize();
}

//*****************************************************************************
//
void NetCommand::addByte ( const int ByteValue )
{
	addInteger( static_cast<uint8_t> ( ByteValue ), sizeof( uint8_t ) );
}

//*****************************************************************************
//
void NetCommand::addShort ( const int ShortValue )
{
	addInteger( static_cast<int16_t> ( ShortValue ), sizeof( int16_t ) );
}

//*****************************************************************************
//
void NetCommand::addLong ( const int32_t LongValue )
{
	addInteger( LongValue, sizeof( int32_t ) );
}

//*****************************************************************************
//
void NetCommand::addFloat ( const float FloatValue )
{
	union
	{
		float	f;
		int32_t	l;
	} dat;
	dat.f = FloatValue;
	addInteger ( dat.l, sizeof( int32_t ) );
}

//*****************************************************************************
//
void NetCommand::addBit( const bool value )
{
	NETWORK_WriteBit( &_buffer.ByteStream, value );
	_buffer.ulCurrentSize = _buffer.CalcSize();
}

//*****************************************************************************
//
void NetCommand::addVariable( const int value )
{
	NETWORK_WriteVariable( &_buffer.ByteStream, value );
	_buffer.ulCurrentSize = _buffer.CalcSize();
}

//*****************************************************************************
// [TP]
//
void NetCommand::addShortByte ( int value, int bits )
{
	NETWORK_WriteShortByte( &_buffer.ByteStream, value, bits );
	_buffer.ulCurrentSize = _buffer.CalcSize();
}

//*****************************************************************************
//
void NetCommand::addString ( const char *pszString )
{
	const int len = ( pszString != NULL ) ? strlen( pszString ) : 0;

	if ( len > MAX_NETWORK_STRING )
	{
		Printf( "NETWORK_WriteString: String exceeds %d characters! Header: %d\n", MAX_NETWORK_STRING, _buffer.pbData[0] );
		return;
	}

	for ( int i = 0; i < len; ++i )
		addByte( pszString[i] );
	addByte( 0 );
}

//*****************************************************************************
//
void NetCommand::addName ( FName name )
{
	if ( name.IsPredefined() )
	{
		addShort( name );
	}
	else
	{
		addShort( -1 );
		addString( name );
	}
}

//*****************************************************************************
//
void NetCommand::writeCommandToStream ( BYTESTREAM_s &ByteStream ) const
{
	// [BB] This also handles the traffic counting (NETWORK_StartTrafficMeasurement/NETWORK_StopTrafficMeasurement).
	_buffer.WriteTo ( ByteStream );
}

//*****************************************************************************
// [TP]
//
bool NetCommand::isUnreliable() const
{
	return _unreliable;
}

//*****************************************************************************
// [TP]
//
void NetCommand::setUnreliable ( bool a )
{
	_unreliable = a;
}

//*****************************************************************************
// [TP] Returns the size of this net command.
//
int NetCommand::calcSize() const
{
	return _buffer.CalcSize();
}

//*****************************************************************************
//
template <typename T>
void IDList<T>::clear( void )
{
	for ( unsigned int ulIdx = 0; ulIdx < MAX_NETID; ulIdx++ )
		freeID ( ulIdx );

	_firstFreeID = 1;
}

//*****************************************************************************
//
template <typename T>
void IDList<T>::rebuild( void )
{
	clear();

	T *pActor;

	TThinkerIterator<T> it;

	while ( (pActor = it.Next()) )
	{
		if (( pActor->syncdata.NetID > 0 ) && ( pActor->syncdata.NetID < MAX_NETID ))
			useID ( pActor->syncdata.NetID, pActor );
	}
}

//*****************************************************************************
//
template <typename T>
void IDList<T>::useID ( const int lNetID, T *pActor )
{
	if ( isIndexValid ( lNetID ) )
	{
		if ( ( _entries[lNetID].bFree == false ) && ( _entries[lNetID].pActor != pActor ) )
			Printf ( "IDList<T>::useID is using an already used ID.\n" );

		_entries[lNetID].bFree = false;
		_entries[lNetID].pActor = pActor;
	}
}

//*****************************************************************************
//
void CountActors ( )
{
	TMap<FName, int> actorCountMap;
	AActor * mo;
	int numActors = 0;
	int numActorsWithNetID = 0;

	TThinkerIterator<AActor> it;

	while ( (mo = it.Next()) )
	{
		numActors++;
		if ( mo->syncdata.NetID > 0 )
			numActorsWithNetID++;
		const FName curName = mo->GetClass()->TypeName.GetChars();
		if ( actorCountMap.CheckKey( curName ) == NULL )
			actorCountMap.Insert( curName, 1 );
		else
			actorCountMap [ curName ] ++;
	}

	const TMap<FName, int>::Pair *pair;

	Printf ( "%d actors in total found, %d have a NetID. Detailed listing:\n", numActors, numActorsWithNetID );

	TMap<FName, int>::ConstIterator mapit(actorCountMap);
	while (mapit.NextPair (pair))
	{
		Printf ( "%s %d\n", pair->Key.GetChars(), pair->Value );
	}
}

CCMD(countactors)
{
	CountActors ();
}

//*****************************************************************************
//
template <typename T>
unsigned int IDList<T>::getNewID( void )
{
	// Actor's network ID is the first availible net ID.
	unsigned int ulID = _firstFreeID;

	do
	{
		_firstFreeID++;
		if ( _firstFreeID >= MAX_NETID )
			_firstFreeID = 1;

		if ( _firstFreeID == ulID )
		{
			// [BB] In case there is no free netID, the server has to abort the current game.
			if ( netserver )
			{
				// [BB] We can only spawn (MAX_NETID-2) actors with netID, because ID zero is reserved and
				// we already check that a new ID for the next actor is available when assign a net ID.
				Printf( "ACTOR_GetNewNetID: Network ID limit reached (>=%d actors)\n", MAX_NETID - 1 );
				CountActors ( );
				I_Error ("Network ID limit reached (>=%d actors)!\n", MAX_NETID - 1 );
			}

			return ( 0 );
		}
	} while ( _entries[_firstFreeID].bFree == false );

	return ( ulID );
}

template class IDList<AActor>;
