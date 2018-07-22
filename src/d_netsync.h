#ifndef __D_NETSYNC_H__
#define __D_NETSYNC_H__

#include "vectors.h"
#include "r_data/renderstyle.h"

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

	// Pointer to our stream of data.
	uint8_t		*pbStream;

	// Pointer to the end of the stream. When pbStream > pbStreamEnd, the
	// entire stream has been read.
	uint8_t		*pbStreamEnd;

	uint8_t		*bitBuffer;
	int			bitShift;
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