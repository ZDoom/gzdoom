GplZDoom
========

**WARNING: Not yet under the GPL.**

Licenses
----------------------------

### GPL Compatible

* **GZDoom License:** Probably compatible. The "LGPL if not a GZDoom fork"
  part is the only thing that might be a problem.
* **Paul Hsieh Derivative License:** AFAIK compatible, double check later on.
* **ZDoom License:** The 3-point BSD license. Compatible.
* **zlib License, Lesser General Public License, DUMB License:** Compatible.

### GPL Incompatible

* **Build License:** Incompatible. The following needs to be removed or
  replaced:
  * inline fixed point multiplication functions (`basicinlines.h`)
  * assembly code converted to C (`m_fixed.h`)
* **Doom Source License:** Most code under the Doom Source License is also
  released under the GPL.

Changes
-------

* The software renderer has been removed.
* All OPL players have been removed.
* FMOD Ex has been removed.
