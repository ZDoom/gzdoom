/*
** skins.c: Player "skins" (actually alternate sprites/animations)
*/

/*

  NOTE: Please ignore this for now. I wrote this up and never bothered
		to write the code that implements it. Some of these things are
		already obsolete, such as the use of death1-death4. A single
		death sound with a random choice like in SNDINFO would be better
		and more flexible.

Each skin uses a single wad file with a SKINDEF lump and supporting
graphics and sounds. An S_SKIN lump is optional, for compatibilty
with Doom Legacy. If both a SKINDEF and S_SKIN lump are present,
SKINDEF is used, and S_SKIN is ignored. For information on the
content of an S_SKIN lump, refer to the Doom Legacy skin docs.

Every SKINDEF lump must contain an [Info] section with one or more
of the following entries:

  Name (req)			Name of the skin.
  Sprite (opt)			Default sprite to use for animations. If every
						animation you define also specifies a sprite,
						then you don't need this here.
  Face (opt)			Replaces the face graphics in the status bar. Since
						only Doom has a face in its status bar, this entry
						is only useful when playing Doom.
  Gender (opt)			The gender to use for deciding which player sounds
						to use for sounds not defined in the [Sounds] section.
						Can be male, female, or cyborg. The default is male.
  ColorRange (opt)		The range of palette entries to recolor to match
						the player's color. Specified as <first>,<last>.
						For Doom, this defaults to 112,127. For Heretic,
						225,240. For Hexen, 146,163.

Use a [Sounds] section to specify new sounds for the skin. In this section,
each entry is the name of a player sound to replace, and the value after
the equals sign is the lump to use for that sound. Currently defined player
sounds are:

  death1				Randomly chosen sounds when the player dies.
  death2
  death3
  death4
  xdeath1				Player had an "extreme" death.
  xdeath2 (Hexen)
  xdeath3 (Hexen)
  wimpydeath (Heretic)	Player dies from an attack of less than 10 damage.
  crazydeath (Heretic)	Player's health after death is in the range (-100,50].
  burndeath (Hexen)		Player was burnt to death.
  pain100_1				Pain sounds when health is in the range (75,100].
  pain100_2
  pain75_1				Pain sounds when health is in the range (50,75].
  pain75_2
  pain50_1				Pain sounds when health is in the range (25,50].
  pain50_2
  pain25_1				Pain sounds when health is in the range (0,25].
  pain25_2
  grunt1				Player runs into a wall too fast on an icy floor.
  land1					Player lands on the ground.
  jump1					Player jumps up.
  gibbed				Player is gibbed.
  fist (Doom)			Player uses her fist.
  usefail				Player presses +use on an non-usable line.
  puzzfail (Hexen)		Player tried to use a puzzle item where it didn't work.
  weaponlaugh (Heretic)	Player picks up a weapon.
  evillaugh (Heretic)	Player uses a Tome of Power or Chaos Device.
  poison				Player is being poisoned.
  falling (Hexen)		Player is falling far.
  splat					Player fell too far and was killed on impact.

As you can see, the sound names are the same as the player sounds defined in
zdoom.wad's SNDINFO lump, but with only the part after the last slash.

The remaining sections in the SKINDEF lump define animations for the player.
Only the Idle animation is required. The rest are all optional, and ZDoom will
try to use the best possible alternative for animations you do not specify.
Each animation is defined in a different section with the same name as the
animation. Valid entries in an animation section are:

  Sprite (opt)			Name of the sprite to use for this animation. If
						omitted, the sprite specified in the [Info] lump will
						be used.
  Rate (opt)			Speed at which to play through the frames in this
						animation (in tics). If omitted, a rate of four
						tics/frame will be used.
  Frames (req)			A list of frames to play for the animation.

At its simplest, Frames lists the frames of the sprite to play (A-Z, [, \, ],
^, and _), in the order they are to be played, with no spaces. There are some
optional character that can appear after a frame letter to do something
special for that frame:

  *		Sprite is drawn fullbright.
  !		Play pain sound.
  @		Play death sound.
  #		Set player to non-solid (death animations only).
  %		Pop off player's skull (needs HeadFlying and HeadOnGround animations).
  $		Plays the sound "misc/burn".
  ^		Play gibbed sound.
  (		Begin frozen death sequence.
  )		Finish frozen death sequence by bursting into shards of ice.

Possible animations are:

  Idle				Player is standing around doing nothing.
  Running			Player is running.
  Attacking			Player is attacking but not currently firing.
  Firing			Player is attacking and currently firing.
  Climbing			Player is climbing a ladder.
  Crouching			Player is crouching but not moving.
  Crawling			Player is crawling around.
  JumpUp			Player is jumping.
  Falling			Player is falling.
  Landing			Player just hit the ground after falling.
  Swimming			Player is swimming in water.
  Death				Player died normally.
  ExtremeDeath		Player died violently.
  BurnDeath			Player was burnt to death.
  FreezeDeath		Player was frozen to death.
  HeadFlying		Player's head after popping off body.
  HeadOnGround		Player's head after landing on ground.

*/
