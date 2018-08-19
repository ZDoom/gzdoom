#ifndef __D_NETSYNC_H__
#define __D_NETSYNC_H__

#include "vectors.h"
#include "r_data/renderstyle.h"

// Maximum size of the packets sent out by the server.
#define	MAX_UDP_PACKET				8192

// This is the longest possible string we can pass over the network.
#define	MAX_NETWORK_STRING			2048

enum class NetPacketType
{
	ConnectRequest,
	ConnectResponse,
	Disconnect,
	Tic
};

class AActor;

struct NetSyncData {
	DVector3		Pos;
	DVector3		Vel;
	DAngle			SpriteAngle;
	DAngle			SpriteRotation;
	DRotator		Angles;
	DVector2		Scale;				// Scaling values; 1 is normal size
	double			Alpha;				// Since P_CheckSight makes an alpha check this can't be a float. It has to be a double.
	int				sprite;				// used to find patch_t and flip value
	uint8_t			frame;				// sprite frame to draw
	uint8_t			effects;			// [RH] see p_effect.h
	FRenderStyle	RenderStyle;		// Style to draw this actor with
	uint32_t		Translation;
	uint32_t		RenderRequired;		// current renderer must have this feature set
	uint32_t		RenderHidden;		// current renderer must *not* have any of these features
	uint32_t		renderflags;		// Different rendering flags
	double			Floorclip;		// value to use for floor clipping
	DAngle			VisibleStartAngle;
	DAngle			VisibleStartPitch;
	DAngle			VisibleEndAngle;
	DAngle			VisibleEndPitch;
	double			Speed;
	double			FloatSpeed;
	double			CameraHeight;	// Height of camera when used as such
	double			CameraFOV;
	double			StealthAlpha;	// Minmum alpha for MF_STEALTH.
	int				NetID;

	void AssignNetID ( AActor *pActor );
	void FreeNetID ();
};

//*****************************************************************************
struct BYTESTREAM_s
{
	BYTESTREAM_s();
	void EnsureBitSpace( int bits, bool writing );

	int	ReadByte();
	int ReadShort();
	int	ReadLong();
	float ReadFloat();
	const char* ReadString();
	bool ReadBit();
	int ReadVariable();
	int ReadShortByte( int bits );
	void ReadBuffer( void* buffer, size_t length );

	void WriteByte( int Byte );
	void WriteShort( int Short );
	void WriteLong( int Long );
	void WriteFloat( float Float );
	void WriteString( const char *pszString );
	void WriteBit( bool bit );
	void WriteVariable( int value );
	void WriteShortByte( int value, int bits );
	void WriteBuffer( const void *pvBuffer, int nLength );

	void WriteHeader( int Byte );

	bool IsAtEnd() const;

	// Pointer to our stream of data.
	uint8_t		*pbStream;

	// Pointer to the end of the stream. When pbStream > pbStreamEnd, the
	// entire stream has been read.
	uint8_t		*pbStreamEnd;

	uint8_t		*bitBuffer;
	int			bitShift;

	void AdvancePointer( const int NumBytes, const bool OutboundTraffic );
};

//*****************************************************************************
enum BUFFERTYPE_e
{
	BUFFERTYPE_READ,
	BUFFERTYPE_WRITE,

};

//*****************************************************************************
struct NETBUFFER_s
{
	// This is the data in our packet.
	uint8_t			*pbData;

	// The maximum amount of data this packet can hold.
	unsigned int	ulMaxSize;

	// How much data is currently in this packet?
	unsigned int	ulCurrentSize;

	// Byte stream for this buffer for managing our current position and where
	// the end of the stream is.
	BYTESTREAM_s	ByteStream;

	// Is this a buffer that we write to, or read from?
	BUFFERTYPE_e	BufferType;

	NETBUFFER_s ( );
	NETBUFFER_s ( const NETBUFFER_s &Buffer );

	void			Init( unsigned int ulLength, BUFFERTYPE_e BufferType );
	void			Free();
	void			Clear();
	int				CalcSize() const;
	int				WriteTo( BYTESTREAM_s &ByteStream ) const;
};

struct NetPacket;

/**
 * \author Benjamin Berkels
 */
class NetCommand {
	NETBUFFER_s	_buffer;
	bool		_unreliable;

public:
	NetCommand ( const NetPacketType Header );
	~NetCommand ( );

	void addInteger( const int IntValue, const int Size );
	void addByte ( const int ByteValue );
	void addShort ( const int ShortValue );
	void addLong ( const int32_t LongValue );
	void addFloat ( const float FloatValue );
	void addString ( const char *pszString );
	void addName ( FName name );
	void addBit ( const bool value );
	void addVariable ( const int value );
	void addShortByte ( int value, int bits );
	void addBuffer ( const void *pvBuffer, int nLength );
	void writeCommandToStream ( BYTESTREAM_s &ByteStream ) const;
	void writeCommandToPacket ( NetPacket &response ) const;
	bool isUnreliable() const;
	void setUnreliable ( bool a );
	int calcSize() const;
};

//==========================================================================
//
// IDList
//
// Manages IDs to reference a certain type of objects over the network.
// Since it still mimics the old Actor ID mechanism, 0 is never assigned as
// ID.
//
// @author Benjamin Berkels
//
//==========================================================================

template <typename T>
class IDList
{
public:
	const static int MAX_NETID = 32768;

private:
	// List of all possible network ID's for an actor. Slot is true if it available for use.
	typedef struct
	{
		// Is this node occupied, or free to be used by a new actor?
		bool	bFree;

		// If this node is occupied, this is the actor occupying it.
		T	*pActor;

	} IDNODE_t;

	IDNODE_t _entries[MAX_NETID];
	unsigned int _firstFreeID;

	inline bool isIndexValid ( const int lNetID ) const
	{
		return ( lNetID >= 0 ) && ( lNetID < MAX_NETID );
	}
public:
	void clear ( );

	// [BB] Rebuild the global list of used / free NetIDs from scratch.
	void rebuild ( );

	IDList ( )
	{
		clear ( );
	}

	void useID ( const int lNetID, T *pActor );

	void freeID ( const int lNetID )
	{
		if ( isIndexValid ( lNetID ) )
		{
			_entries[lNetID].bFree = true;
			_entries[lNetID].pActor = NULL;
		}
	}

	unsigned int getNewID ( );

	T* findPointerByID ( const int lNetID ) const
	{
		if ( isIndexValid ( lNetID ) == false )
			return ( NULL );

		if (( _entries[lNetID].bFree == false ) && ( _entries[lNetID].pActor ))
			return ( _entries[lNetID].pActor );

		return ( NULL );
	}
};

#endif //__D_NETSYNC_H__