
#pragma once

#include "ssa_ubyte.h"
#include "ssa_ubyte_ptr.h"
#include "ssa_float.h"
#include "ssa_float_ptr.h"
#include "ssa_int.h"
#include "ssa_pixeltype.h"
//#include "ssa_pixelformat1f.h"
//#include "ssa_pixelformat2f.h"
//#include "ssa_pixelformat3f.h"
#include "ssa_pixelformat4f.h"
//#include "ssa_pixelformat1ub.h"
//#include "ssa_pixelformat2ub.h"
//#include "ssa_pixelformat3ub.h"
//#include "ssa_pixelformat3ub_rev.h"
#include "ssa_pixelformat4ub.h"
//#include "ssa_pixelformat4ub_argb.h"
#include "ssa_pixelformat4ub_rev.h"
#include "ssa_pixelformat4ub_argb_rev.h"
//#include "ssa_pixelformat4ub_channel.h"

//typedef SSAPixelType<SSAPixelFormat1f, SSAFloatPtr> SSAPixels1f;
//typedef SSAPixelType<SSAPixelFormat2f, SSAFloatPtr> SSAPixels2f;
//typedef SSAPixelType<SSAPixelFormat3f, SSAFloatPtr> SSAPixels3f;
typedef SSAPixelType<SSAPixelFormat4f, SSAFloatPtr> SSAPixels4f;

//typedef SSAPixelType<SSAPixelFormat1ub, SSAUBytePtr> SSAPixels1ub;
//typedef SSAPixelType<SSAPixelFormat2ub, SSAUBytePtr> SSAPixels2ub;
//typedef SSAPixelType<SSAPixelFormat3ub, SSAUBytePtr> SSAPixels3ub;
typedef SSAPixelType<SSAPixelFormat4ub, SSAUBytePtr> SSAPixels4ub;
//typedef SSAPixelType<SSAPixelFormat4ub_argb, SSAUBytePtr> SSAPixels4ub_argb;

//typedef SSAPixelType<SSAPixelFormat3ub_rev, SSAUBytePtr> SSAPixels3ub_rev;
typedef SSAPixelType<SSAPixelFormat4ub_rev, SSAUBytePtr> SSAPixels4ub_rev;
typedef SSAPixelType<SSAPixelFormat4ub_argb_rev, SSAUBytePtr> SSAPixels4ub_argb_rev;

//typedef SSAPixelType<SSAPixelFormat4ub_channel, SSAUBytePtr> SSAPixels4ub_channel;
