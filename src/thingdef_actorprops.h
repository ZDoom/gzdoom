/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf -tET -LANSI-C -Hactorhash -Nis_actorprop -C thingdef_actorprops.gperf  */
/* Computed positions: -k'1,5,10' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

/* maximum key range = 141, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
actorhash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142,   1, 142,   0, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142,  25, 142,  35,  45,  55,
        0,  15, 142,  35,   0,   0, 142, 142,  20,   0,
       40,   5,  10, 142,  20,  25,  10,   5,   0, 142,
       10,  30, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
      142, 142, 142, 142, 142, 142
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[9]];
      /*FALLTHROUGH*/
      case 9:
      case 8:
      case 7:
      case 6:
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
      case 3:
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

#ifdef __GNUC__
__inline
#endif
const struct ActorProps *
is_actorprop (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 50,
      MIN_WORD_LENGTH = 1,
      MAX_WORD_LENGTH = 15,
      MIN_HASH_VALUE = 1,
      MAX_HASH_VALUE = 141
    };

  static const struct ActorProps wordlist[] =
    {
      {""},
#line 61 "thingdef_actorprops.gperf"
      {"-",			ActorFlagSetOrReset},
#line 60 "thingdef_actorprops.gperf"
      {"+",			ActorFlagSetOrReset},
#line 39 "thingdef_actorprops.gperf"
      {"ice",			ActorIceState},
#line 21 "thingdef_actorprops.gperf"
      {"mass",			ActorMass},
#line 36 "thingdef_actorprops.gperf"
      {"death",			ActorDeathState},
#line 20 "thingdef_actorprops.gperf"
      {"height",			ActorHeight},
#line 34 "thingdef_actorprops.gperf"
      {"missile",		ActorMissileState},
#line 30 "thingdef_actorprops.gperf"
      {"dropitem",		ActorDropItem},
      {""},
#line 28 "thingdef_actorprops.gperf"
      {"deathsound",		ActorDeathSound},
#line 50 "thingdef_actorprops.gperf"
      {"deathheight",		ActorDeathHeight},
      {""},
#line 55 "thingdef_actorprops.gperf"
      {"missileheight",		ActorMissileHeight},
#line 35 "thingdef_actorprops.gperf"
      {"pain",			ActorPainState},
#line 47 "thingdef_actorprops.gperf"
      {"donthurtshooter",	ActorDontHurtShooter},
#line 14 "thingdef_actorprops.gperf"
      {"health",			ActorHealth},
#line 58 "thingdef_actorprops.gperf"
      {"monster",		ActorMonster},
#line 45 "thingdef_actorprops.gperf"
      {"obituary",		ActorObituary},
      {""},
#line 33 "thingdef_actorprops.gperf"
      {"melee",			ActorMeleeState},
#line 54 "thingdef_actorprops.gperf"
      {"missiletype",		ActorMissileType},
      {""}, {""}, {""},
#line 53 "thingdef_actorprops.gperf"
      {"meleesound",		ActorMeleeSound},
#line 37 "thingdef_actorprops.gperf"
      {"xdeath",			ActorXDeathState},
      {""},
#line 32 "thingdef_actorprops.gperf"
      {"see",			ActorSeeState},
      {""},
#line 18 "thingdef_actorprops.gperf"
      {"speed",			ActorSpeed},
#line 19 "thingdef_actorprops.gperf"
      {"radius",			ActorRadius},
      {""}, {""}, {""},
#line 49 "thingdef_actorprops.gperf"
      {"explosiondamage",	ActorExplosionDamage},
#line 22 "thingdef_actorprops.gperf"
      {"xscale",			ActorXScale},
      {""},
#line 25 "thingdef_actorprops.gperf"
      {"seesound",		ActorSeeSound},
      {""},
#line 40 "thingdef_actorprops.gperf"
      {"raise",			ActorRaiseState},
#line 17 "thingdef_actorprops.gperf"
      {"damage",			ActorDamage},
#line 15 "thingdef_actorprops.gperf"
      {"reactiontime",		ActorReactionTime},
      {""},
#line 27 "thingdef_actorprops.gperf"
      {"painsound",		ActorPainSound},
#line 24 "thingdef_actorprops.gperf"
      {"scale",			ActorScale},
#line 42 "thingdef_actorprops.gperf"
      {"states",			ActorStates},
      {""}, {""},
#line 38 "thingdef_actorprops.gperf"
      {"burn",			ActorBurnState},
#line 59 "thingdef_actorprops.gperf"
      {"projectile",		ActorProjectile},
#line 56 "thingdef_actorprops.gperf"
      {"translation",		ActorTranslation},
      {""}, {""}, {""},
#line 48 "thingdef_actorprops.gperf"
      {"explosionradius",	ActorExplosionRadius},
#line 23 "thingdef_actorprops.gperf"
      {"yscale",			ActorYScale},
      {""}, {""}, {""},
#line 41 "thingdef_actorprops.gperf"
      {"crash",			ActorCrashState},
#line 52 "thingdef_actorprops.gperf"
      {"meleedamage",		ActorMeleeDamage},
      {""}, {""}, {""},
#line 51 "thingdef_actorprops.gperf"
      {"burnheight",		ActorBurnHeight},
#line 43 "thingdef_actorprops.gperf"
      {"renderstyle",		ActorRenderStyle},
      {""}, {""}, {""},
#line 31 "thingdef_actorprops.gperf"
      {"spawn",			ActorSpawnState},
      {""},
#line 13 "thingdef_actorprops.gperf"
      {"spawnid",		ActorSpawnID},
      {""}, {""},
#line 44 "thingdef_actorprops.gperf"
      {"alpha",			ActorAlpha},
#line 46 "thingdef_actorprops.gperf"
      {"hitobituary",		ActorHitObituary},
      {""}, {""}, {""},
#line 12 "thingdef_actorprops.gperf"
      {"skip_super",		ActorSkipSuper},
      {""}, {""}, {""}, {""}, {""},
#line 29 "thingdef_actorprops.gperf"
      {"activesound",		ActorActiveSound},
      {""}, {""}, {""},
#line 16 "thingdef_actorprops.gperf"
      {"painchance",		ActorPainChance},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""},
#line 57 "thingdef_actorprops.gperf"
      {"clearflags",		ActorClearFlags},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""},
#line 26 "thingdef_actorprops.gperf"
      {"attacksound",		ActorAttackSound}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = actorhash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
