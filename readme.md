GplZDoom
========

**WARNING: Not yet under the GPL.**

An attempt to remove all code from GZDoom that is incompatible with the GPL, in order that commercial games may be created with it. *This is not yet finished.* The long-term goal is to find or write cross-platform replacement libraries under the 3-point modified BSD license.

Preleminary License Auditing
----------------------------

If you feel any of the below is incorrect, please post an issue on the issue tracker.

### Compatible

* **Doom Source License:** Most code under the Doom Source License is also released under the GPL. An audit of the code to ensure any code under the DSL that is not also available under the GPL is removed or replaced.
* **Paul Hsieh Derivative License:** AFAIK compatible, double check later on.
* **ZDoom License:** The 3-point modified BSD license. This is the preferred license of the GplZDoom project, as it allows ZDoom and GZDoom to use any newly-added code.
* **zlib License, General Public License, LGPL, DUMB License, GZDoom License:** Compatible.

### Incompatible

* **Build License:** Incompatible. The following needs to be removed or replaced:
  * inline fixed point multiplication functions (`mscinlines.h` and `gccinlines.h`)
  * some assembly code (`asm_ia32/a.asm`)
  * the (unused) Polymost implementation (`r_polymost.cpp`)
  * voxels rendering (`r_things.cpp`)
  * part of the decal and wall rendering code (`r_segs.cpp`, `r_draw.cpp`)
* **FMOD Ex License:** ~~FMOD will need to be completely removed.~~ *Done.*
* **MAME License:** Incompatible. The MAME OPL2 emulator will need to be removed.
* **MUSLib License:** Vladimir Arnost's OPL library will need to be removed.
