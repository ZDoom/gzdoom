#ifndef __ANNOUNCER_H__
#define __ANNOUNCER_H__

#ifdef _MSC_VER
#pragma once
#endif

class AActor;

// These all return true if they generated a text message

bool AnnounceGameStart ();
bool AnnounceKill (AActor *killer, AActor *killee);
bool AnnounceTelefrag (AActor *killer, AActor *killee);
bool AnnounceSpree (AActor *who);
bool AnnounceSpreeLoss (AActor *who);
bool AnnounceMultikill (AActor *who);

#endif //__ANNOUNCER_H__