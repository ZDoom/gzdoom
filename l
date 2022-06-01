Mainly a bugfix release addressing the following issues:
- Fixed: SBar_DrawTextureRotated was missing the angle parameter
- Compat patch for misplaced items in Hexen MAP08.
- fixed some imprecisions in font luminosity calculation. This was still going through the palette which could result in off-by-one errors.
- fixed palette initialization for single lump fonts (FON2 and BMF)
- push sprite shadows a little back for distance sorting
- avoid multiple Dehacked mappings to the same function to fix MBF21 lookup issues.
- disallow changing the line spacing for option menus.
- be a bit more aggressive with the GC when not running the game loop.
- fixed memory leak in DShape2DBufferInfo.
- allow "INDEXFON" as an alias for "INDEXFONT".
- fixed A_Punch having a short attack range (take 2).
- GLES: Fix texture MapBuffer and AllocateBuffer create client side memory.
- Fix "out of memory" errors when the vulkan backend is asked to create zero byte sized buffers
- resolved a VM abort upon morphing while the tome of power is active
- fixed particle processing in the main thinker loop. This may not be guarded by the dynamic light flags, only the light ticking may.
- fixed handling of states with the 'light' keyword.
- Fixed broken V_BreakLines.
- treat "materials" as a known folder name for eliminating root folders in Zips.

New Features:
- widescreen assets for all officially supported games. This version adds Hacx, Harmony and Chex Quest.
- Allow WorldUnloaded events to know the next map name (if any).
- Allow map markers to scale relative to automap zoom.
- Add cvars to control automap line alpha and thickness
- PreTravelled virtual for player and inventory.
- Allow customization of pause screen.
- ReadThisMenu / MenuDelegate overriding.
