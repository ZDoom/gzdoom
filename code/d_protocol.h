#ifndef __D_PROTOCOL_H__
#define __D_PROTOCOL_H__

#include "doomtype.h"
#include "m_fixed.h"

#define ZDEMO_ID	(('Z'<<24)|('D'<<16)|('E'<<8)|('M'))

#define MAX_QPATH			64
#define MAX_MSGLEN			1400

#define	ANGLE2SHORT(x)	((((x)/360) & 65535)
#define	SHORT2ANGLE(x)	((x)*360)


struct zdemoheader_s {
	int		magic;
	byte	demovermajor;
	byte	demoverminor;
	byte	minvermajor;
	byte	minverminor;
	byte	demoplayers;
	byte	skill;
	byte	episode;
	byte	map;
	byte	deathmatch;
	byte	respawnparm;
	byte	fastparm;
	byte	nomonsters;
	byte	consoleplayer;
	byte	playeringame[1];
};
typedef struct zdemoheader_s zdemoheader_t;

struct message_s {
	short	len;
	byte	id;
};
typedef struct message_s message_t;

#define ZDEMO_STOP			(-1)	// stored in the len field of the message

#define SVC_BAD				0

#define SVC_USERCMD			1
struct usercmd_s {
	byte	msec;			// not sure how to use this yet...
	byte	buttons;
	short	pitch;			// up/down. currently just a y-sheering amount
	short	yaw;			// left/right	// If you haven't guessed, I just
	short	roll;			// tilt			// ripped these from Quake2's usercmd.
	short	forwardmove;
	short	sidemove;
	short	upmove;
	byte	impulse;
	byte	lightlevel;
};
typedef struct usercmd_s usercmd_t;

// When transmitted, the above message is preceded by a byte
// indicating which fields are actually present in the message.
// (buttons is always sent)
#define UCMDF_PITCH			0x01
#define UCMDF_YAW			0x02
#define UCMDF_ROLL			0x04
#define UCMDF_FORWARDMOVE	0x08
#define UCMDF_SIDEMOVE		0x10
#define UCMDF_UPMOVE		0x20
#define UCMDF_IMPULSE		0x40
#define UCMDF_LIGHTLEVEL	0x80

#define SVC_STUFFTEXT		2
// This message is a string for the console to execute

#define SVC_MUSICCHANGE		3
// This message is a string containing the name of the new music

#define SVC_PRINT			4
// Prints a string at the top of the screen

#define SVC_CENTERPRINT		5
// Prints a string in the middle of the screen

#define SVC_NOP				6



int UnpackUserCmd (usercmd_t *ucmd, byte **stream);
int PackUserCmd (usercmd_t *ucmd, byte **stream);
int WriteUserCmdMessage (usercmd_t *ucmd, byte **stream);

int ReadByte (byte **stream);
int ReadWord (byte **stream);
int ReadLong (byte **stream);
char *ReadString (byte **stream);
void WriteByte (byte val, byte **stream);
void WriteWord (short val, byte **stream);
void WriteLong (int val, byte **stream);
void WriteString (char *string, byte **stream);

#endif //__D_PROTOCOL_H__