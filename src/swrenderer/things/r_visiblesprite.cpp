//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "p_lnspec.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "i_system.h"
#include "w_wad.h"
#include "g_levellocals.h"
#include "p_maputl.h"
#include "swrenderer/things/r_visiblesprite.h"
#include "swrenderer/things/r_voxel.h"
#include "swrenderer/things/r_particle.h"
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/things/r_wallsprite.h"
#include "swrenderer/things/r_playersprite.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/r_memory.h"

namespace swrenderer
{
}
