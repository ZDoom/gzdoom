/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf -tCET -LANSI-C -Hweaponhash -Nis_weaponprop thingdef_weaponprops.gperf  */
/* Computed positions: -k'1-2' */

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

/* maximum key range = 26, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
weaponhash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27,  5, 27,  0, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27,  5, 27, 27,
      27,  0, 27,  9, 15,  0, 27, 15, 27,  5,
      27, 27,  0, 27,  0, 27,  0,  0, 27, 27,
      27,  0, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
      27, 27, 27, 27, 27, 27
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[1]];
      /*FALLTHROUGH*/
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
is_weaponprop (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 14,
      MIN_WORD_LENGTH = 1,
      MAX_WORD_LENGTH = 13,
      MIN_HASH_VALUE = 1,
      MAX_HASH_VALUE = 26
    };

  static const struct ActorProps wordlist[] =
    {
      {""},
#line 26 "thingdef_weaponprops.gperf"
      {"-",			WeaponFlagSetOrClear},
      {""}, {""}, {""}, {""},
#line 25 "thingdef_weaponprops.gperf"
      {"+",			WeaponFlagSetOrClear},
#line 23 "thingdef_weaponprops.gperf"
      {"upsound",		WeaponUpSound},
#line 13 "thingdef_weaponprops.gperf"
      {"pufftype",		WeaponPuffType},
      {""},
#line 22 "thingdef_weaponprops.gperf"
      {"readysound",		WeaponReadySound},
#line 15 "thingdef_weaponprops.gperf"
      {"pickupsound",		WeaponPickupSound},
#line 21 "thingdef_weaponprops.gperf"
      {"yadjust",		WeaponYAdjust},
#line 16 "thingdef_weaponprops.gperf"
      {"pickupmessage",		WeaponPickupMessage},
      {""}, {""},
#line 24 "thingdef_weaponprops.gperf"
      {"attacksound",		WeaponAttackSound},
#line 19 "thingdef_weaponprops.gperf"
      {"giveammo",		WeaponGiveAmmo},
#line 17 "thingdef_weaponprops.gperf"
      {"ammotype",		WeaponAmmoType},
      {""}, {""},
#line 18 "thingdef_weaponprops.gperf"
      {"ammopershot",		WeaponAmmoPerShot},
      {""},
#line 20 "thingdef_weaponprops.gperf"
      {"kickback",		WeaponKickback},
      {""}, {""},
#line 14 "thingdef_weaponprops.gperf"
      {"hitpufftype",		WeaponHitPuffType}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = weaponhash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
