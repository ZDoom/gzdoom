#ifndef __STSTUFF_H__
#define __STSTUFF_H__

struct event_t;

bool ST_Responder(event_t* ev);

// [RH] Base blending values (for e.g. underwater)
extern int BaseBlendR, BaseBlendG, BaseBlendB;
extern float BaseBlendA;

#endif
