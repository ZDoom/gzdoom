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
