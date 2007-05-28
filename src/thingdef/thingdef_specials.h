/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf -m10000 -tE -LANSI-C -Hspecialhash -Nis_special -C --null-strings thingdef_specials.gperf  */
/* Computed positions: -k'1,7,9,14,17' */

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

#line 10 "thingdef_specials.gperf"
struct ACSspecials { const char *name; unsigned char Special; unsigned char MinArgs; unsigned char MaxArgs; };
/* maximum key range = 326, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
specialhash (register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] =
    {
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356,  53, 356,  20,  85,  15,
      107,  13,  88,  10,  68,  95,  10,  10,  13, 136,
       72,  84,  44, 356,  10,  11,  12,  28,  53,  69,
       10,  75, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356, 356, 356, 356, 356,
      356, 356, 356, 356, 356, 356
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[16]];
      /*FALLTHROUGH*/
      case 16:
      case 15:
      case 14:
        hval += asso_values[(unsigned char)str[13]];
      /*FALLTHROUGH*/
      case 13:
      case 12:
      case 11:
      case 10:
      case 9:
        hval += asso_values[(unsigned char)str[8]];
      /*FALLTHROUGH*/
      case 8:
      case 7:
        hval += asso_values[(unsigned char)str[6]];
      /*FALLTHROUGH*/
      case 6:
      case 5:
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
const struct ACSspecials *
is_special (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 169,
      MIN_WORD_LENGTH = 8,
      MAX_WORD_LENGTH = 29,
      MIN_HASH_VALUE = 30,
      MAX_HASH_VALUE = 355
    };

  static const struct ACSspecials wordlist[] =
    {
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0},
#line 76 "thingdef_specials.gperf"
      {"teleport",70,1},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0},
#line 93 "thingdef_specials.gperf"
      {"teleportgroup",77,5},
#line 49 "thingdef_specials.gperf"
      {"light_strobe",116,5},
#line 160 "thingdef_specials.gperf"
      {"exit_secret",244,1},
#line 145 "thingdef_specials.gperf"
      {"thing_setgoal",229,3},
      {(char*)0},
#line 126 "thingdef_specials.gperf"
      {"generic_lift",203,5},
      {(char*)0}, {(char*)0}, {(char*)0},
#line 88 "thingdef_specials.gperf"
      {"thing_spawn",135,3,4},
#line 13 "thingdef_specials.gperf"
      {"acs_suspend",81,2},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0},
#line 127 "thingdef_specials.gperf"
      {"generic_stairs",204,5},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0},
#line 128 "thingdef_specials.gperf"
      {"generic_crusher",205,5},
      {(char*)0}, {(char*)0},
#line 82 "thingdef_specials.gperf"
      {"thing_activate",130,1},
#line 12 "thingdef_specials.gperf"
      {"acs_execute",80,1,5},
      {(char*)0}, {(char*)0}, {(char*)0},
#line 107 "thingdef_specials.gperf"
      {"line_alignceiling",183,2},
      {(char*)0},
#line 105 "thingdef_specials.gperf"
      {"thing_settranslation",180,2},
      {(char*)0}, {(char*)0}, {(char*)0},
#line 168 "thingdef_specials.gperf"
      {"ceiling_raisetonearest",252,2},
#line 170 "thingdef_specials.gperf"
      {"ceiling_lowertofloor",254,2},
      {(char*)0},
#line 99 "thingdef_specials.gperf"
      {"thing_spawnfacing",139,2,4},
#line 21 "thingdef_specials.gperf"
      {"ceiling_crushraiseandstay",45,3},
#line 118 "thingdef_specials.gperf"
      {"ceiling_crushraiseandstaya",195,4},
#line 135 "thingdef_specials.gperf"
      {"teleport_line",215,2},
#line 141 "thingdef_specials.gperf"
      {"scroll_floor",223,4},
#line 171 "thingdef_specials.gperf"
      {"ceiling_crushraiseandstaysila",255,4},
#line 16 "thingdef_specials.gperf"
      {"ceiling_crushandraise",42,3},
#line 119 "thingdef_specials.gperf"
      {"ceiling_crushandraisea",196,4},
#line 15 "thingdef_specials.gperf"
      {"acs_lockedexecute",83,5},
#line 18 "thingdef_specials.gperf"
      {"ceiling_lowerandcrush",43,3},
      {(char*)0}, {(char*)0},
#line 180 "thingdef_specials.gperf"
      {"acs_lockedexecutedoor",85,5},
#line 120 "thingdef_specials.gperf"
      {"ceiling_crushandraisesilenta",197,4},
#line 77 "thingdef_specials.gperf"
      {"teleport_nofog",71,1,2},
      {(char*)0},
#line 142 "thingdef_specials.gperf"
      {"scroll_ceiling",224,4},
#line 102 "thingdef_specials.gperf"
      {"thing_hate",177,2,3},
#line 70 "thingdef_specials.gperf"
      {"radius_quake",120,5},
#line 133 "thingdef_specials.gperf"
      {"sector_setfade",213,4},
#line 177 "thingdef_specials.gperf"
      {"noisealert",173,2},
#line 63 "thingdef_specials.gperf"
      {"polyobj_rotateright",3,3},
#line 175 "thingdef_specials.gperf"
      {"acs_executewithresult",84,1,4},
      {(char*)0},
#line 59 "thingdef_specials.gperf"
      {"plat_stop",61,1},
#line 78 "thingdef_specials.gperf"
      {"teleport_newmap",74,2},
#line 79 "thingdef_specials.gperf"
      {"teleport_endgame",75,0},
#line 17 "thingdef_specials.gperf"
      {"ceiling_crushstop",44,1},
#line 134 "thingdef_specials.gperf"
      {"sector_setdamage",214,3},
      {(char*)0}, {(char*)0},
#line 139 "thingdef_specials.gperf"
      {"sector_setcurrent",220,4},
#line 47 "thingdef_specials.gperf"
      {"light_glow",114,4},
#line 50 "thingdef_specials.gperf"
      {"light_stop",117,1},
#line 92 "thingdef_specials.gperf"
      {"teleportother",76,3},
      {(char*)0},
#line 91 "thingdef_specials.gperf"
      {"ceiling_waggle",38,5},
#line 112 "thingdef_specials.gperf"
      {"sector_setceilingscale",188,5},
      {(char*)0},
#line 110 "thingdef_specials.gperf"
      {"sector_setceilingpanning",186,5},
      {(char*)0},
#line 123 "thingdef_specials.gperf"
      {"generic_floor",200,5},
#line 124 "thingdef_specials.gperf"
      {"generic_ceiling",201,5},
#line 178 "thingdef_specials.gperf"
      {"thing_raise",17,1},
#line 106 "thingdef_specials.gperf"
      {"line_mirror",182,0},
#line 80 "thingdef_specials.gperf"
      {"thrustthing",72,2,4},
#line 98 "thingdef_specials.gperf"
      {"thrustthingz",128,4},
#line 104 "thingdef_specials.gperf"
      {"changeskill",179,1},
      {(char*)0},
#line 148 "thingdef_specials.gperf"
      {"light_strobedoom",232,3},
#line 108 "thingdef_specials.gperf"
      {"line_alignfloor",184,2},
      {(char*)0},
#line 51 "thingdef_specials.gperf"
      {"pillar_build",29,3},
#line 14 "thingdef_specials.gperf"
      {"acs_terminate",82,2},
      {(char*)0},
#line 131 "thingdef_specials.gperf"
      {"translucentline",208,2,3},
#line 23 "thingdef_specials.gperf"
      {"door_close",10,2},
#line 173 "thingdef_specials.gperf"
      {"clearforcefield",34,1},
#line 84 "thingdef_specials.gperf"
      {"thing_destroy",133,1,2},
#line 125 "thingdef_specials.gperf"
      {"generic_door",202,5},
#line 94 "thingdef_specials.gperf"
      {"teleportinsector",78,4,5},
#line 97 "thingdef_specials.gperf"
      {"thing_setspecial",127,5},
#line 89 "thingdef_specials.gperf"
      {"thing_spawnnofog",137,3,4},
#line 25 "thingdef_specials.gperf"
      {"door_raise",12,3},
      {(char*)0},
#line 73 "thingdef_specials.gperf"
      {"stairs_buildup",27,5},
      {(char*)0},
#line 53 "thingdef_specials.gperf"
      {"pillar_open",30,4},
      {(char*)0}, {(char*)0},
#line 169 "thingdef_specials.gperf"
      {"ceiling_lowertolowest",253,2},
      {(char*)0},
#line 101 "thingdef_specials.gperf"
      {"thing_changetid",176,2},
#line 143 "thingdef_specials.gperf"
      {"acs_executealways",226,1,5},
      {(char*)0},
#line 20 "thingdef_specials.gperf"
      {"ceiling_raisebyvalue",41,3},
      {(char*)0},
#line 117 "thingdef_specials.gperf"
      {"ceiling_raiseinstant",194,3},
#line 19 "thingdef_specials.gperf"
      {"ceiling_lowerbyvalue",40,3},
#line 45 "thingdef_specials.gperf"
      {"light_changetovalue",112,2},
#line 116 "thingdef_specials.gperf"
      {"ceiling_lowerinstant",193,3},
#line 121 "thingdef_specials.gperf"
      {"ceiling_raisebyvaluetimes8",198,3},
      {(char*)0}, {(char*)0},
#line 122 "thingdef_specials.gperf"
      {"ceiling_lowerbyvaluetimes8",199,3},
#line 87 "thingdef_specials.gperf"
      {"thing_remove",132,1},
      {(char*)0},
#line 115 "thingdef_specials.gperf"
      {"ceiling_lowertohighestfloor",192,2},
      {(char*)0},
#line 165 "thingdef_specials.gperf"
      {"door_closewaitopen",249,3},
#line 83 "thingdef_specials.gperf"
      {"thing_deactivate",131,1},
#line 132 "thingdef_specials.gperf"
      {"sector_setcolor",212,4,5},
      {(char*)0},
#line 153 "thingdef_specials.gperf"
      {"changecamera",237,3},
#line 90 "thingdef_specials.gperf"
      {"floor_waggle",138,5},
      {(char*)0},
#line 144 "thingdef_specials.gperf"
      {"plat_raiseandstaytx0",228,2},
#line 164 "thingdef_specials.gperf"
      {"healthing",248,1,2},
#line 62 "thingdef_specials.gperf"
      {"polyobj_rotateleft",2,3},
#line 68 "thingdef_specials.gperf"
      {"polyobj_or_rotateleft",90,3},
#line 69 "thingdef_specials.gperf"
      {"polyobj_or_rotateright",91,3},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
#line 57 "thingdef_specials.gperf"
      {"plat_upbyvalue",65,4},
      {(char*)0},
#line 72 "thingdef_specials.gperf"
      {"stairs_builddown",26,5},
#line 140 "thingdef_specials.gperf"
      {"scroll_texture_both",221,5},
#line 44 "thingdef_specials.gperf"
      {"light_lowerbyvalue",111,2},
#line 162 "thingdef_specials.gperf"
      {"elevator_movetofloor",246,2},
#line 113 "thingdef_specials.gperf"
      {"sector_setfloorscale",189,5},
      {(char*)0},
#line 138 "thingdef_specials.gperf"
      {"sector_setwind",218,4},
#line 109 "thingdef_specials.gperf"
      {"sector_setrotation",185,3},
      {(char*)0},
#line 114 "thingdef_specials.gperf"
      {"setplayerproperty",191,3},
#line 61 "thingdef_specials.gperf"
      {"polyobj_move",4,4},
#line 111 "thingdef_specials.gperf"
      {"sector_setfloorpanning",187,5},
#line 65 "thingdef_specials.gperf"
      {"polyobj_doorslide",8,5},
      {(char*)0},
#line 67 "thingdef_specials.gperf"
      {"polyobj_or_move",92,4},
#line 74 "thingdef_specials.gperf"
      {"stairs_builddownsync",31,4},
#line 56 "thingdef_specials.gperf"
      {"plat_upwaitdownstay",64,3},
#line 48 "thingdef_specials.gperf"
      {"light_flicker",115,3},
      {(char*)0},
#line 96 "thingdef_specials.gperf"
      {"thing_move",125,2},
      {(char*)0},
#line 42 "thingdef_specials.gperf"
      {"light_forcelightning",109,1},
      {(char*)0},
#line 31 "thingdef_specials.gperf"
      {"floor_lowertolowest",21,2},
#line 146 "thingdef_specials.gperf"
      {"plat_upbyvaluestaytx",230,3},
#line 43 "thingdef_specials.gperf"
      {"light_raisebyvalue",110,2},
#line 46 "thingdef_specials.gperf"
      {"light_fade",113,3},
#line 157 "thingdef_specials.gperf"
      {"floor_lowertolowesttxty",241,2},
      {(char*)0}, {(char*)0},
#line 136 "thingdef_specials.gperf"
      {"sector_setgravity",216,3},
      {(char*)0}, {(char*)0},
#line 81 "thingdef_specials.gperf"
      {"damagething",73,1},
#line 75 "thingdef_specials.gperf"
      {"stairs_buildupsync",32,4},
#line 52 "thingdef_specials.gperf"
      {"pillar_buildandcrush",94,4},
#line 58 "thingdef_specials.gperf"
      {"plat_perpetualraise",60,3},
#line 55 "thingdef_specials.gperf"
      {"plat_downbyvalue",63,4},
#line 41 "thingdef_specials.gperf"
      {"floor_crushstop",46,1},
#line 130 "thingdef_specials.gperf"
      {"plat_perpetualraiselip",207,4},
#line 24 "thingdef_specials.gperf"
      {"door_open",11,2},
#line 71 "thingdef_specials.gperf"
      {"sector_changesound",140,2},
#line 176 "thingdef_specials.gperf"
      {"plat_upnearestwaitdownstay",172,3},
      {(char*)0}, {(char*)0},
#line 156 "thingdef_specials.gperf"
      {"floor_raisebytexture",240,2},
#line 137 "thingdef_specials.gperf"
      {"stairs_buildupdoom",217,5},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0},
#line 159 "thingdef_specials.gperf"
      {"exit_normal",243,1},
#line 154 "thingdef_specials.gperf"
      {"floor_raisetolowestceiling",238,2},
#line 151 "thingdef_specials.gperf"
      {"floor_transfertrigger",235,1},
      {(char*)0}, {(char*)0},
#line 179 "thingdef_specials.gperf"
      {"startconversation",18,1,2},
      {(char*)0},
#line 85 "thingdef_specials.gperf"
      {"thing_projectile",134,5},
      {(char*)0}, {(char*)0},
#line 150 "thingdef_specials.gperf"
      {"light_maxneighbor",234,1},
#line 163 "thingdef_specials.gperf"
      {"elevator_lowertonearest",247,2},
      {(char*)0},
#line 64 "thingdef_specials.gperf"
      {"polyobj_doorswing",7,4},
#line 161 "thingdef_specials.gperf"
      {"elevator_raisetonearest",245,2},
#line 54 "thingdef_specials.gperf"
      {"plat_downwaitupstay",62,3},
#line 174 "thingdef_specials.gperf"
      {"teleport_zombiechanger",39,2},
      {(char*)0},
#line 129 "thingdef_specials.gperf"
      {"plat_downwaitupstaylip",206,4},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
#line 95 "thingdef_specials.gperf"
      {"thing_damage",119,2,3},
#line 86 "thingdef_specials.gperf"
      {"thing_projectilegravity",136,5},
#line 27 "thingdef_specials.gperf"
      {"floor_lowerbyvalue",20,3},
      {(char*)0},
#line 29 "thingdef_specials.gperf"
      {"floor_lowerinstant",66,3},
#line 32 "thingdef_specials.gperf"
      {"floor_lowertonearest",22,2},
      {(char*)0}, {(char*)0},
#line 28 "thingdef_specials.gperf"
      {"floor_lowerbyvaluetimes8",36,3},
#line 103 "thingdef_specials.gperf"
      {"thing_projectileaimed",178,4,5},
      {(char*)0},
#line 166 "thingdef_specials.gperf"
      {"floor_donut",250,3},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
#line 22 "thingdef_specials.gperf"
      {"ceiling_movetovaluetimes8",69,4},
      {(char*)0},
#line 40 "thingdef_specials.gperf"
      {"floorandceiling_raisebyvalue",96,3},
#line 167 "thingdef_specials.gperf"
      {"floorandceiling_lowerraise",251,3},
#line 152 "thingdef_specials.gperf"
      {"floor_transfernumeric",236,1},
#line 39 "thingdef_specials.gperf"
      {"floorandceiling_lowerbyvalue",95,3},
      {(char*)0}, {(char*)0}, {(char*)0},
#line 34 "thingdef_specials.gperf"
      {"floor_raisebyvalue",23,3},
      {(char*)0},
#line 36 "thingdef_specials.gperf"
      {"floor_raiseinstant",67,3},
#line 38 "thingdef_specials.gperf"
      {"floor_raisetonearest",25,2},
#line 155 "thingdef_specials.gperf"
      {"floor_raisebyvaluetxty",239,3},
      {(char*)0},
#line 35 "thingdef_specials.gperf"
      {"floor_raisebyvaluetimes8",35,3},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
#line 66 "thingdef_specials.gperf"
      {"polyobj_or_movetimes8",93,4},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
#line 26 "thingdef_specials.gperf"
      {"door_lockedraise",13,4},
      {(char*)0},
#line 60 "thingdef_specials.gperf"
      {"polyobj_movetimes8",6,4},
      {(char*)0},
#line 149 "thingdef_specials.gperf"
      {"light_minneighbor",233,1},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0},
#line 147 "thingdef_specials.gperf"
      {"plat_toggleceiling",231,1},
      {(char*)0}, {(char*)0},
#line 158 "thingdef_specials.gperf"
      {"floor_lowertohighest",242,3},
      {(char*)0},
#line 172 "thingdef_specials.gperf"
      {"door_animated",14,3},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
#line 30 "thingdef_specials.gperf"
      {"floor_movetovaluetimes8",68,4},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0},
#line 33 "thingdef_specials.gperf"
      {"floor_raiseandcrush",28,3},
      {(char*)0},
#line 37 "thingdef_specials.gperf"
      {"floor_raisetohighest",24,2},
      {(char*)0}, {(char*)0}, {(char*)0}, {(char*)0},
      {(char*)0},
#line 100 "thingdef_specials.gperf"
      {"thing_projectileintercept",175,5}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = specialhash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].name;

          if (s && *str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
