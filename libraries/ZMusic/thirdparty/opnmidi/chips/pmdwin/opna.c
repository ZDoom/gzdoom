/* FIXME: move ugly-ass legalese somewhere where it won't be seen
// by anyone other than lawyers. (/dev/null would be ideal but sadly
// we live in an imperfect world). */
/* Copyright (c) 2012/2013, Peter Barfuss
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>
#include "op.h"
#include "psg.h"
#include "opna.h"

static const uint8_t notetab[128] =
{
     0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  3,  3,  3,  3,  3,  3,
     4,  4,  4,  4,  4,  4,  4,  5,  6,  7,  7,  7,  7,  7,  7,  7,
     8,  8,  8,  8,  8,  8,  8,  9, 10, 11, 11, 11, 11, 11, 11, 11,
    12, 12, 12, 12, 12, 12, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15,
    16, 16, 16, 16, 16, 16, 16, 17, 18, 19, 19, 19, 19, 19, 19, 19,
    20, 20, 20, 20, 20, 20, 20, 21, 22, 23, 23, 23, 23, 23, 23, 23,
    24, 24, 24, 24, 24, 24, 24, 25, 26, 27, 27, 27, 27, 27, 27, 27,
    28, 28, 28, 28, 28, 28, 28, 29, 30, 31, 31, 31, 31, 31, 31, 31,
};

static const int8_t dttab[256] =
{
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  2,  2,  2,  2,  2,  2,  2,  2,  4,  4,  4,  4,
      4,  6,  6,  6,  8,  8,  8, 10, 10, 12, 12, 14, 16, 16, 16, 16,
      2,  2,  2,  2,  4,  4,  4,  4,  4,  6,  6,  6,  8,  8,  8, 10,
     10, 12, 12, 14, 16, 16, 18, 20, 22, 24, 26, 28, 32, 32, 32, 32,
      4,  4,  4,  4,  4,  6,  6,  6,  8,  8,  8, 10, 10, 12, 12, 14,
     16, 16, 18, 20, 22, 24, 26, 28, 32, 34, 38, 40, 44, 44, 44, 44,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0, -2, -2, -2, -2, -2, -2, -2, -2, -4, -4, -4, -4,
     -4, -6, -6, -6, -8, -8, -8,-10,-10,-12,-12,-14,-16,-16,-16,-16,
     -2, -2, -2, -2, -4, -4, -4, -4, -4, -6, -6, -6, -8, -8, -8,-10,
    -10,-12,-12,-14,-16,-16,-18,-20,-22,-24,-26,-28,-32,-32,-32,-32,
     -4, -4, -4, -4, -4, -6, -6, -6, -8, -8, -8,-10,-10,-12,-12,-14,
    -16,-16,-18,-20,-22,-24,-26,-28,-32,-34,-38,-40,-44,-44,-44,-44,
};

static const uint8_t gaintab[64] = {
    0xff, 0xea, 0xd7, 0xc5, 0xb5, 0xa6, 0x98, 0x8b, 0x80, 0x75, 0x6c, 0x63, 0x5a, 0x53, 0x4c, 0x46,
    0x40, 0x3b, 0x36, 0x31, 0x2d, 0x2a, 0x26, 0x23, 0x20, 0x1d, 0x1b, 0x19, 0x17, 0x15, 0x13, 0x12,
    0x10, 0x0f, 0x0e, 0x0c, 0x0b, 0x0a, 0x0a, 0x09, 0x08, 0x07, 0x07, 0x06, 0x06, 0x05, 0x05, 0x04,
    0x04, 0x04, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01,
};

/* sinf(M_PI*(2*i+1)/1024.0f), i=0,...,511.
// Should make this twice as large (so a duplicate of the top 512, but with the other half of the
// interval [0,2*M_PI], therefore the negative of the first half), and then get rid of the
// silly hack in Sinetable(). However, I'm not actually sure which will use less gates on an FPGA,
// and there's really no speed difference on any machine newer than a 6502, probably. */
static const int16_t sinetable[1024] = {
     1,  2,  4,  5,  7,  9,  10,  12,  13,  15,  16,  18,  20,  21,  23,  24,
     26,  27,  29,  31,  32,  34,  35,  37,  38,  40,  41,  43,  45,  46,  48,  49,
     51,  52,  54,  55,  57,  58,  60,  61,  63,  64,  66,  68,  69,  71,  72,  74,
     75,  77,  78,  80,  81,  83,  84,  86,  87,  88,  90,  91,  93,  94,  96,  97,
     99,  100,  102,  103,  104,  106,  107,  109,  110,  112,  113,  114,  116,  117,  119,  120,
     121,  123,  124,  125,  127,  128,  130,  131,  132,  134,  135,  136,  138,  139,  140,  142,
     143,  144,  145,  147,  148,  149,  151,  152,  153,  154,  156,  157,  158,  159,  161,  162,
     163,  164,  165,  167,  168,  169,  170,  171,  173,  174,  175,  176,  177,  178,  179,  180,
     182,  183,  184,  185,  186,  187,  188,  189,  190,  191,  192,  193,  194,  195,  196,  197,
     198,  199,  200,  201,  202,  203,  204,  205,  206,  207,  208,  209,  210,  211,  212,  212,
     213,  214,  215,  216,  217,  218,  218,  219,  220,  221,  222,  222,  223,  224,  225,  225,
     226,  227,  228,  228,  229,  230,  230,  231,  232,  232,  233,  234,  234,  235,  236,  236,
     237,  237,  238,  239,  239,  240,  240,  241,  241,  242,  242,  243,  243,  244,  244,  245,
     245,  246,  246,  247,  247,  247,  248,  248,  249,  249,  249,  250,  250,  250,  251,  251,
     251,  252,  252,  252,  252,  253,  253,  253,  253,  254,  254,  254,  254,  254,  255,  255,
     255,  255,  255,  255,  255,  255,  256,  256,  256,  256,  256,  256,  256,  256,  256,  256,
     256,  256,  256,  256,  256,  256,  256,  256,  256,  256,  255,  255,  255,  255,  255,  255,
     255,  255,  254,  254,  254,  254,  254,  253,  253,  253,  253,  252,  252,  252,  252,  251,
     251,  251,  250,  250,  250,  249,  249,  249,  248,  248,  247,  247,  247,  246,  246,  245,
     245,  244,  244,  243,  243,  242,  242,  241,  241,  240,  240,  239,  239,  238,  237,  237,
     236,  236,  235,  234,  234,  233,  232,  232,  231,  230,  230,  229,  228,  228,  227,  226,
     225,  225,  224,  223,  222,  222,  221,  220,  219,  218,  218,  217,  216,  215,  214,  213,
     212,  212,  211,  210,  209,  208,  207,  206,  205,  204,  203,  202,  201,  200,  199,  198,
     197,  196,  195,  194,  193,  192,  191,  190,  189,  188,  187,  186,  185,  184,  183,  182,
     180,  179,  178,  177,  176,  175,  174,  173,  171,  170,  169,  168,  167,  165,  164,  163,
     162,  161,  159,  158,  157,  156,  154,  153,  152,  151,  149,  148,  147,  145,  144,  143,
     142,  140,  139,  138,  136,  135,  134,  132,  131,  130,  128,  127,  125,  124,  123,  121,
     120,  119,  117,  116,  114,  113,  112,  110,  109,  107,  106,  104,  103,  102,  100,  99,
     97,  96,  94,  93,  91,  90,  88,  87,  86,  84,  83,  81,  80,  78,  77,  75,
     74,  72,  71,  69,  68,  66,  64,  63,  61,  60,  58,  57,  55,  54,  52,  51,
     49,  48,  46,  45,  43,  41,  40,  38,  37,  35,  34,  32,  31,  29,  27,  26,
     24,  23,  21,  20,  18,  16,  15,  13,  12,  10,  9,  7,  6,  4,  2,  1,
    -1, -2, -4, -5, -7, -9, -10, -12, -13, -15, -16, -18, -20, -21, -23, -24,
    -26, -27, -29, -31, -32, -34, -35, -37, -38, -40, -41, -43, -45, -46, -48, -49,
    -51, -52, -54, -55, -57, -58, -60, -61, -63, -64, -66, -68, -69, -71, -72, -74,
    -75, -77, -78, -80, -81, -83, -84, -86, -87, -88, -90, -91, -93, -94, -96, -97,
    -99, -100, -102, -103, -104, -106, -107, -109, -110, -112, -113, -114, -116, -117, -119, -120,
    -121, -123, -124, -125, -127, -128, -130, -131, -132, -134, -135, -136, -138, -139, -140, -142,
    -143, -144, -145, -147, -148, -149, -151, -152, -153, -154, -156, -157, -158, -159, -161, -162,
    -163, -164, -165, -167, -168, -169, -170, -171, -173, -174, -175, -176, -177, -178, -179, -180,
    -182, -183, -184, -185, -186, -187, -188, -189, -190, -191, -192, -193, -194, -195, -196, -197,
    -198, -199, -200, -201, -202, -203, -204, -205, -206, -207, -208, -209, -210, -211, -212, -212,
    -213, -214, -215, -216, -217, -218, -218, -219, -220, -221, -222, -222, -223, -224, -225, -225,
    -226, -227, -228, -228, -229, -230, -230, -231, -232, -232, -233, -234, -234, -235, -236, -236,
    -237, -237, -238, -239, -239, -240, -240, -241, -241, -242, -242, -243, -243, -244, -244, -245,
    -245, -246, -246, -247, -247, -247, -248, -248, -249, -249, -249, -250, -250, -250, -251, -251,
    -251, -252, -252, -252, -252, -253, -253, -253, -253, -254, -254, -254, -254, -254, -255, -255,
    -255, -255, -255, -255, -255, -255, -256, -256, -256, -256, -256, -256, -256, -256, -256, -256,
    -256, -256, -256, -256, -256, -256, -256, -256, -256, -256, -255, -255, -255, -255, -255, -255,
    -255, -255, -254, -254, -254, -254, -254, -253, -253, -253, -253, -252, -252, -252, -252, -251,
    -251, -251, -250, -250, -250, -249, -249, -249, -248, -248, -247, -247, -247, -246, -246, -245,
    -245, -244, -244, -243, -243, -242, -242, -241, -241, -240, -240, -239, -239, -238, -237, -237,
    -236, -236, -235, -234, -234, -233, -232, -232, -231, -230, -230, -229, -228, -228, -227, -226,
    -225, -225, -224, -223, -222, -222, -221, -220, -219, -218, -218, -217, -216, -215, -214, -213,
    -212, -212, -211, -210, -209, -208, -207, -206, -205, -204, -203, -202, -201, -200, -199, -198,
    -197, -196, -195, -194, -193, -192, -191, -190, -189, -188, -187, -186, -185, -184, -183, -182,
    -180, -179, -178, -177, -176, -175, -174, -173, -171, -170, -169, -168, -167, -165, -164, -163,
    -162, -161, -159, -158, -157, -156, -154, -153, -152, -151, -149, -148, -147, -145, -144, -143,
    -142, -140, -139, -138, -136, -135, -134, -132, -131, -130, -128, -127, -125, -124, -123, -121,
    -120, -119, -117, -116, -114, -113, -112, -110, -109, -107, -106, -104, -103, -102, -100, -99,
    -97, -96, -94, -93, -91, -90, -88, -87, -86, -84, -83, -81, -80, -78, -77, -75,
    -74, -72, -71, -69, -68, -66, -64, -63, -61, -60, -58, -57, -55, -54, -52, -51,
    -49, -48, -46, -45, -43, -41, -40, -38, -37, -35, -34, -32, -31, -29, -27, -26,
    -24, -23, -21, -20, -18, -16, -15, -13, -12, -10, -9, -7, -6, -4, -2, -1,
};

static const uint8_t cltab[512] = {
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6,
   0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4,
   0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97, 0x97,
   0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
   0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b,
   0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
   0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b, 0x4b,
   0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
   0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35,
   0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
   0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25, 0x25,
   0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
   0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a,
   0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
   0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
   0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
   0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
   0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
   0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
   0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
   0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
   0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
   0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
   0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
   0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
   0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
   0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
   0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
   0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
   0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
   0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
};

static const uint8_t fbtab[8] = { 31, 7, 6, 5, 4, 3, 2, 1 };

/* Amplitude/Phase modulation tables. */
static const uint8_t amt[4] = { 29, 4, 2, 1 }; /* OPNA */

/* libOPNMIDI: pan law table */
static const uint16_t panlawtable[] = {
  65535, 65529, 65514, 65489, 65454, 65409, 65354, 65289,
  65214, 65129, 65034, 64929, 64814, 64689, 64554, 64410,
  64255, 64091, 63917, 63733, 63540, 63336, 63123, 62901,
  62668, 62426, 62175, 61914, 61644, 61364, 61075, 60776,
  60468, 60151, 59825, 59489, 59145, 58791, 58428, 58057,
  57676, 57287, 56889, 56482, 56067, 55643, 55211, 54770,
  54320, 53863, 53397, 52923, 52441, 51951, 51453, 50947,
  50433, 49912, 49383, 48846, 48302, 47750, 47191,
  46340, /* Center left */
  46340, /* Center right */
  45472, 44885, 44291, 43690, 43083, 42469, 41848, 41221,
  40588, 39948, 39303, 38651, 37994, 37330, 36661, 35986,
  35306, 34621, 33930, 33234, 32533, 31827, 31116, 30400,
  29680, 28955, 28225, 27492, 26754, 26012, 25266, 24516,
  23762, 23005, 22244, 21480, 20713, 19942, 19169, 18392,
  17613, 16831, 16046, 15259, 14469, 13678, 12884, 12088,
  11291, 10492, 9691, 8888, 8085, 7280, 6473, 5666,
  4858, 4050, 3240, 2431, 1620, 810, 0
};

/* --------------------------------------------------------------------------- */
static inline void LFO(OPNA *opna)
{
    uint8_t c = (opna->lfocount >> FM_LFOCBITS) & 0xff;
    opna->lfocount += opna->lfodcount;
    if (c < 0x80)       opna->aml = (c << 1);
    else                opna->aml = ~(c << 1);
}

/* ---------------------------------------------------------------------------
// Magic. No, really.
// In reality this just initialises some tables used by everything else,
// that are dependent on both the chip clock and the "DAC" samplerate.
// The hilarious thing though is that this is really the only place where
// the chip clock value gets actually *used*, and even then it's indirectly
// via the ratio parameter.
*/
static void MakeTimeTable(OPNA *opna, uint32_t ratio)
{
    int h, l;
    uint32_t *ratetable = opna->ratetable;

    if (ratio != opna->currentratio)
    {
        opna->currentratio = ratio;

        /* EG */
        for (h=1; h<16; h++)
        {
            for (l=0; l<4; l++)
            {
                int m = h == 15 ? 8 : l+4;
                ratetable[h*4+l] =
                    ((ratio << (FM_EGBITS - 3 - FM_RATIOBITS)) << Min(h, 11)) * m;
            }
        }
        ratetable[0] = ratetable[1] = ratetable[2] = ratetable[3] = 0;
        ratetable[5] = ratetable[4],  ratetable[7] = ratetable[6];
    }
}

static void SetEGRate(FMOperator *op, uint32_t r)
{
    Channel4 *ch = op->master;
    OPNA *opna = ch->master;
    op->egstepd = opna->ratetable[r];
    op->egtransa = Limit(15 - (r>>2), 4, 1);
    op->egtransd = 16 >> op->egtransa;
}

/* Standard operator init routine. Zeros out some more stuff
// than OperatorReset() does, then calls OperatorReset().
*/
void OperatorInit(Channel4 *ch4, FMOperator *op)
{
    op->master = ch4;

    /* EG Part */
    op->ar = op->dr = op->sr = op->rr = op->ksr = 0;
    op->ams = 0;
    op->mute = 0;
    op->keyon = 0;

    /* PG Part */
    op->multiple = 0;
    op->detune = 0;

    /* LFO */
    op->ms = 0;

    OperatorReset(op);
}

/* Standard operator reset routine. Init EG/PG to defaults,
// clear any stored samples, then force a reinit of EG/PG
// in OperatorPrepare() below by setting paramchanged to 1.
*/
void OperatorReset(FMOperator *op)
{
    /* EG part */
    op->tl = op->tll = 127;
    op->eglevel = 0xff;
    op->eglvnext = 0x100;
    SetEGRate(op, 0);
    op->phase = off;
    op->egstep = 0;

    /* PG part */
    op->pgcount = 0;

    /* OP part */
    op->out = op->out2 = 0;
    op->paramchanged = 1;
}

/* Init EG, PG.
// PG init is trivial, simply set pgdcount (phase counter increment)
// based on multiple, detune and bn.
// See Pages 24-26 of the OPNA manual for details.
// EG init is your standard ADSR state machine. Should (hopefully!)
// be self-explanatory, especially if you've ever seen a software implementation
// of ADSR before (seriously, they're all the damn same).
*/
void OperatorPrepare(FMOperator *op)
{
    Channel4 *ch = op->master;
    OPNA *opna = ch->master;

    if (op->paramchanged)
    {
        uint8_t l = ((op->multiple) ? 2*op->multiple : 1);
        op->paramchanged = 0;
        /*  PG Part */
        op->pgdcount = (op->dp + dttab[op->detune + op->bn]) * (uint32_t)(l * opna->rr);
        op->pgdcountl = op->pgdcount >> 11;

        /* EG Part */
        op->ksr = op->bn >> (3-op->ks);

        switch (op->phase)
        {
        case attack:
            SetEGRate(op, op->ar ? Min(63, op->ar+op->ksr) : 0);
            break;
        case decay:
            SetEGRate(op, op->dr ? Min(63, op->dr+op->ksr) : 0);
            op->eglvnext = op->sl * 8;
            break;
        case sustain:
            SetEGRate(op, op->sr ? Min(63, op->sr+op->ksr) : 0);
            break;
        case release:
            SetEGRate(op, Min(63, op->rr+op->ksr));
            break;
        case next: /* temporal */
            break;
        case off:  /* temporal */
            break;
        }
        /* LFO */
        op->ams = (op->amon ? (op->ms >> 4) & 3 : 0);
    }
}

/* FIXME: Rename. "Phase" here refers to ADSR DFA state,
// not PG/sine table phase. Also, yeah, this does the
// ADSR DFA state transitions.
*/
static void ShiftPhase(FMOperator *op, EGPhase nextphase)
{
    switch (nextphase)
    {
    case attack:        /* Attack Phase */
        op->tl = op->tll;
        if ((op->ar+op->ksr) < 62) {
            SetEGRate(op, op->ar ? Min(63, op->ar+op->ksr) : 0);
            op->phase = attack;
            break;
        }
        /* fall through */
    case decay:         /* Decay Phase */
        if (op->sl) {
            op->eglevel = 0;
            op->eglvnext = op->sl*8;
            SetEGRate(op, op->dr ? Min(63, op->dr+op->ksr) : 0);
            op->phase = decay;
            break;
        }
        /* fall through */
    case sustain:       /* Sustain Phase */
        op->eglevel = op->sl*8;
        op->eglvnext = 0x100;
        SetEGRate(op, op->sr ? Min(63, op->sr+op->ksr) : 0);
        op->phase = sustain;
        break;

    case release:       /* Release Phase */
        if (op->phase == attack || (op->eglevel < 0x100/* && phase != off*/)) {
            op->eglvnext = 0x100;
            SetEGRate(op, Min(63, op->rr+op->ksr));
            op->phase = release;
            break;
        }
        /* fall through */
    case off:           /* off */
    default:
        op->eglevel = 0xff;
        op->eglvnext = 0x100;
        SetEGRate(op, 0);
        op->phase = off;
        break;
    }
}

/*  Block/F-Num */
static inline void SetFNum(FMOperator *op, uint32_t f)
{
    op->dp = (f & 2047) << ((f >> 11) & 7);
    op->bn = notetab[(f >> 7) & 127];
    op->paramchanged = 1;
}

/* Clock the EG for one operator.
// Essentially just a call to ShiftPhase,
// but decrements the output EG level if starting
// from the attack phase, otherwise incrementing it.
// Should probably integrate the special case for attack
// from ShiftPhase() directly into here at some point. */
void EGCalc(FMOperator *op)
{
    op->egstep += 3L << (11 + FM_EGBITS);
    if (op->phase == attack)
    {
        op->eglevel -= 1 + (op->eglevel >> op->egtransa);
        if (op->eglevel <= 0)
            ShiftPhase(op, decay);
    }
    else
    {
        op->eglevel += op->egtransd;
        if (op->eglevel >= op->eglvnext)
            ShiftPhase(op, (EGPhase)(op->phase+1));
    }
}

/* KeyOn, hopefully obvious. */
static void KeyOn(FMOperator *op)
{
    /*if (!op->keyon && ((op->ar = 31) || (op->ar == 62))) {*/
    if (!op->keyon) {
        op->keyon = 1;
        if (!op->sl) {
            ShiftPhase(op, sustain);
            op->out = op->out2 = 0;
            op->pgcount = 0;
        } else {
            if (op->phase == off || op->phase == release) {
                ShiftPhase(op, attack);
                op->out = op->out2 = 0;
                op->pgcount = 0;
            }
        }
    }
}

/* KeyOff, hopefully obvious. */
static void KeyOff(FMOperator *op)
{
    if (op->keyon) {
        op->keyon = 0;
        ShiftPhase(op, release);
    }
}

/* PG uses 9 bits, with the table itsself using another 10 bits.
// The top bits are the actually relevant ones, given that the PG increment will basically set
// the lowest few bits to nonsense.
// The hack there that checks for bit 10 in the right place and if yes, does some strange xor magic
// makes the value of Sine() negative if we're in the top half of the [0,2*M_PI] interval.
// It is, of course, one/two's complement specific, but I have yet to hear of an integer arithmetic implementation
// on any modern machine that isn't at least one of those two. (In fact, I think they're all two's complement, even). */
/*#define Sine(s) sinetable[((s) >> (20+FM_PGBITS-FM_OPSINBITS))&(FM_OPSINENTS/2-1)]^(-(((s) & 0x10000000) >> 27))*/
#define Sine(s) sinetable[((s) >> (20+FM_PGBITS-FM_OPSINBITS))&(FM_OPSINENTS-1)]

static inline uint32_t LogToLin(uint32_t x) {
    if(x >= 0xff) {
        return 0;
    }
    return cltab[x];
}

/*  PG clock routine.
//  Does this really need to be in its own function anymore?
//  It's literally just a trivial increment of a counter now, nothing more.
//  Its output, btw, is 2^(20+PGBITS) / cycle, with PGBITS=9 in this implementation. */
static inline uint32_t PGCalc(FMOperator *op)
{
    uint32_t ret = op->pgcount;
    op->pgcount += op->pgdcount;
    return ret;
}

/* Clock one FM operator. Does a lookup in the sine table
// for the waveform to output, possibly frequency-modulating
// that with the contents of in, then clocks the Phase Generator
// for that operator, stores the output sample and returns.
// Should probably integrate PGCalc() into this function,
// at some point at least. */
static inline int32_t Calc(FMOperator *op, int32_t in)
{
    int32_t tmp = Sine(op->pgcount + (in << 7));
    op->out = op->egout*tmp;
    PGCalc(op);
    return op->out;
}

/* Clock operator 0. OP0 is special as it does not take an input from
// another operator, rather it can frequency-modulate itsself via the
// fb parameter (which specifies feedback amount). This is incredibly
// useful, and makes it possible to define a lot more instruments
// for the OPNA than you'd be able to otherwise. */
#define FM_PRECISEFEEDBACK 1
static inline void CalcFB(FMOperator *op, uint32_t fb)
{
    int32_t tmp;
    int32_t in = op->out + op->out2;
    op->out2 = op->out;
    if (FM_PRECISEFEEDBACK && fb == 31)
        tmp = Sine(op->pgcount);
    else
        tmp = Sine(op->pgcount + ((in << 6) >> fb));

    op->out = op->egout*tmp;
    PGCalc(op);
}

/* ---------------------------------------------------------------------------
//  4-op Channel
//  Sets the "algorithm", i.e. the connections between individual operators
//  in a channel. See Page 22 of the manual for pretty drawings of all of the
//  different algorithms supported by the OPNA.
*/
static void SetAlgorithm(Channel4 *ch4, uint32_t algo)
{
    static const uint8_t table1[8][6] =
    {
        { 0, 1, 1, 2, 2, 3 },   { 1, 0, 0, 1, 1, 2 },
        { 1, 1, 1, 0, 0, 2 },   { 0, 1, 2, 1, 1, 2 },
        { 0, 1, 2, 2, 2, 1 },   { 0, 1, 0, 1, 0, 1 },
        { 0, 1, 2, 1, 2, 1 },   { 1, 0, 1, 0, 1, 0 },
    };

    ch4->idx[0] = table1[algo][0]; /* in[0]; */
    ch4->idx[1] = table1[algo][2]; /* in[1]; */
    ch4->idx[2] = table1[algo][4]; /* in[2]; */
    ch4->idx[3] = table1[algo][1]; /* out[0]; */
    ch4->idx[4] = table1[algo][3]; /* out[1]; */
    ch4->idx[5] = table1[algo][5]; /* out[2]; */
    ch4->op[0].out2 = ch4->op[0].out = 0;
}

static inline void Ch4Init(OPNA *opna, Channel4 *ch4)
{
    int i;
    ch4->master = opna;
    for(i=0; i<4; i++) {
        OperatorInit(ch4, &ch4->op[i]);
    }
    SetAlgorithm(ch4, 0);
}

/* Reinit all operators on a given channel if paramchanged=1
// for that channel, set the PM table for that channel, then determine
// if there is any output from this channel, based on:
// - mute state of each operator
// - keyon state of each operator
// - AM (Tremolo) enable for each operator.
// Bit 0 of the return value is set if there is any output,
// Bit 1 is set if tremolo is enabled for any of the operators on this
// channel. */
static inline int Ch4Prepare(Channel4 *ch4)
{
    OperatorPrepare(&ch4->op[0]);
    OperatorPrepare(&ch4->op[1]);
    OperatorPrepare(&ch4->op[2]);
    OperatorPrepare(&ch4->op[3]);

    if(ch4->op[0].mute && ch4->op[1].mute && ch4->op[2].mute && ch4->op[3].mute)
        return 0;
    else {
        int key = (IsOn(&ch4->op[0]) | IsOn(&ch4->op[1]) | IsOn(&ch4->op[2]) | IsOn(&ch4->op[3])) ? 1 : 0;
        int lfo = ch4->op[0].ms & (ch4->op[0].amon | ch4->op[1].amon | ch4->op[2].amon | ch4->op[3].amon ? 0x37 : 7) ? 2 : 0;
        return key | lfo;
    }
}

/* Clock one channel. Clocks all the Envelope Generators in parallel
// (well, okay, in sequence, but a hardware implementation *should*
//  clock them in parallel as they are completely independent tasks,
//  all that is important is that you don't execute Calc{L,FB,FBL}
//  until all of the EGs are done clocking - but that should be, again,
//  straightforward to implement in hardware).
*/
int32_t Ch4Calc(Channel4 *ch4)
{
    int i, o;
    OPNA *opna = ch4->master;
    ch4->buf[1] = ch4->buf[2] = ch4->buf[3] = 0;
    for(i=0; i<4; i++) {
        if ((ch4->op[i].egstep -= ch4->op[i].egstepd) < 0)
            EGCalc(&ch4->op[i]);
        ch4->op[i].egout = (LogToLin(ch4->op[i].eglevel + (opna->aml >> amt[ch4->op[i].ams]))*gaintab[ch4->op[i].tl]);
    }

    ch4->buf[0] = ch4->op[0].out; CalcFB(&ch4->op[0], ch4->fb);
    if (!(ch4->idx[0] | ch4->idx[2] | ch4->idx[4])) {
        o  = Calc(&ch4->op[1], ch4->buf[0]);
        o += Calc(&ch4->op[2], ch4->buf[0]);
        o += Calc(&ch4->op[3], ch4->buf[0]);
        return (o >> 8);
    } else {
        ch4->buf[ch4->idx[3]] += Calc(&ch4->op[1], ch4->buf[ch4->idx[0]]);
        ch4->buf[ch4->idx[4]] += Calc(&ch4->op[2], ch4->buf[ch4->idx[1]]);
        o = ch4->op[3].out;
        Calc(&ch4->op[3], ch4->buf[ch4->idx[2]]);
        return ((ch4->buf[ch4->idx[5]] + o) >> 8);
    }
}

/* This essentially initializes a couple constant tables
// and chip-specific parameters based on what the chip clock and "DAC" samplerate
// were set to in OPNAInit(). psgrate is always equal to the user-requested samplerate,
// whereas rate is only equal to that in the interpolation=0 case, otherwise
// it's set to whatever value is needed to downsample 55466Hz to the user-requested
// samplerate, which will (almost?) always be either 44100Hz or 48000Hz.
// TODO: better-quality resampling may be of use here, possibly.
*/
static void SetPrescaler(OPNA *opna, uint32_t p)
{
    static const char table[3][2] = { { 6, 4 }, { 3, 2 }, { 2, 1 } };
    static const uint8_t table2[8] = { 109,  78,  72,  68,  63,  45,  9,  6 };
    /* 512 */
    if (opna->prescale != p)
    {
        uint32_t i, fmclock;
        uint32_t ratio;

        opna->prescale = p;
        fmclock = opna->clock / table[p][0] / 12;

        if (opna->interpolation) {
            opna->rate = fmclock * 2;
            do {
                opna->rate >>= 1;
                opna->mpratio = opna->psgrate * 16384 / opna->rate;
            } while (opna->mpratio <= 8192);
        } else {
            opna->rate = opna->psgrate;
        }
        ratio = ((fmclock << FM_RATIOBITS) + opna->rate/2) / opna->rate;
        opna->timer_step = (int32_t)(1000000.0f * 65536.0f/fmclock);
        /* PG Part */
        opna->rr = (float)ratio / (1 << (2 + FM_RATIOBITS - FM_PGBITS));
        MakeTimeTable(opna, ratio);
        PSGSetClock(&opna->psg, opna->clock / table[p][1], opna->psgrate);

        for (i=0; i<8; i++) {
            opna->lfotab[i] = (ratio << (1+FM_LFOCBITS-FM_RATIOBITS)) / table2[i];
        }
    }
}

static inline void RebuildTimeTable(OPNA *opna)
{
    int p = opna->prescale;
    opna->prescale = -1;
    SetPrescaler(opna, p);
}

/* Chip-internal TimerA() handler. All it does is implement CSM, i.e.
// channel 3 will get keyed on and off whenever the TimerA() interrupt fires.
// To the best of my knowledge, CSM was intended to be used to implement
// primitive formant synthesis (which Yamaha later repackaged in a much more
// elaborate and featured implementation in their FS1R), and used by
// approximately nobody. It's also been removed from the YMF288/OPN3.
*/
static void TimerA(OPNA *opna)
{
    int i;
    if (opna->regtc & 0x80)
    {
        for(i=0; i<4; i++)
            KeyOn(&opna->csmch->op[i]);
        for(i=0; i<4; i++)
            KeyOff(&opna->csmch->op[i]);
    }
}

/* ---------------------------------------------------------------------------
// Clock timers. TimerA has a resolution of 9 microseconds (assuming standard
// chip clockspeed of 8MHz, which all of this code of course does), and
// on the Speak Board for the PC-9801 is used only for the purpose of sound effects.
// TimerB, on the other hand, has a resolution of 144 microseconds, and is basically
// used as the main chip clock. Also, binding "sound-effects" to TimerB (needed as
// ZUN uses the sound-effects feature to implement PSG percussion) results in tiny
// changes to the output file, precisely none of them audible, making TimerA
// all but useless in this case. Note that TimerA is also used internally in the chip
// to implement CSM-mode (see comment above).
*/
uint8_t OPNATimerCount(OPNA *opna, int32_t us)
{
    uint8_t event = 0;

    if (opna->timera_count) {
        opna->timera_count -= us << 16;
        if (opna->timera_count <= 0) {
            event = 1;
            TimerA(opna);

            while (opna->timera_count <= 0)
                opna->timera_count += opna->timera;

            if (opna->regtc & 4) {
                if (!(opna->status & 1)) {
                    opna->status |= 1;
                }
            }
        }
    }
    if (opna->timerb_count) {
        opna->timerb_count -= us << 12;
        if (opna->timerb_count <= 0) {
            event = 1;
            while (opna->timerb_count <= 0)
                opna->timerb_count += opna->timerb;

            if (opna->regtc & 8) {
                if (!(opna->status & 2)) {
                    opna->status |= 2;
                }
            }
        }
    }
    return event;
}

/* Rhythm source samples. pcm_s8 (*not* u8!), and found in rhythmdata.h,
// which is included in rhythmdata.c in order to keep the size of the
// object file that you get from compiling this file at a reasonable size,
// for debugging/testing/sanity purposes.
*/
extern const unsigned char* rhythmdata[6];
static const unsigned int rhythmdatalen[6] = {
    9013, 10674, 66610, 7259, 18562, 3042
};

/* ---------------------------------------------------------------------------
// Main chip init routine.
// c is the chip clock, which should never be set to anything other than 8MHz.
// r is the chip samplerate, set to 44100 typically.
// ipflag - if 1, ignore the value of r, clock the "DAC" at the OPNA-internal
// samplerate of 55466Hz, then downsample to whatever the actual value of r is.
*/
uint8_t OPNAInit(OPNA *opna, uint32_t c, uint32_t r, uint8_t ipflag)
{
    int i;
    opna->devmask = 0x7;
    opna->prescale = 0;
    opna->rate = 44100;
    opna->mixl = 0;
    opna->mixr = 0;
    opna->mixdelta = 16383;
    opna->interpolation = 0;

    for (i=0; i<8; i++)
        opna->lfotab[i] = 0;

    opna->aml = 0;

    opna->currentratio = ~0;
    opna->rr = 0;
    for (i=0; i<64; i++)
        opna->ratetable[i] = 0;

    for (i=0; i<6; i++) {
        Ch4Init(opna, &opna->ch[i]);
        opna->rhythm[i].sample = 0;
        opna->rhythm[i].pos = 0;
        opna->rhythm[i].size = 0;
        opna->rhythm[i].volume = 0;
    }
    opna->rhythmtvol = 0;
    opna->csmch = &opna->ch[2];
    for (i=0; i<6; i++)
        opna->rhythm[i].pos = ~0;

    for (i=0; i<6; i++)
    {
        uint8_t *file_buf = (uint8_t*)0;
        uint32_t fsize;
        file_buf = (uint8_t*)rhythmdata[i];
        fsize = rhythmdatalen[i];
        file_buf += 44;
        fsize -= 44;
        fsize /= 2;
        opna->rhythm[i].sample = (int8_t*)file_buf;
        opna->rhythm[i].rate = 44100;
        opna->rhythm[i].step = opna->rhythm[i].rate * 1024 / opna->rate;
        opna->rhythm[i].pos = opna->rhythm[i].size = fsize * 1024;
    }

    c /= 2;
    opna->clock = c;
    if (!OPNASetRate(opna, r, ipflag))
        return 0;
    RebuildTimeTable(opna);
    OPNAReset(opna);
    PSGInit(&opna->psg);

    OPNASetChannelMask(opna, ~0);
    return 1;
}

/* ---------------------------------------------------------------------------
// Reset chip. Your standard routine, basically zeros everything in sight.
*/
void OPNAReset(OPNA *opna)
{
    int i, j;

    opna->status = 0;
    SetPrescaler(opna, 0);
    opna->timera_count = 0;
    opna->timerb_count = 0;
    PSGReset(&opna->psg);
    opna->reg29 = 0x1f;
    opna->rhythmkey = 0;
    for (i=0x20; i<0x28; i++) OPNASetReg(opna, i, 0);
    for (i=0x30; i<0xc0; i++) OPNASetReg(opna, i, 0);
    for (i=0x130; i<0x1c0; i++) OPNASetReg(opna, i, 0);
    for (i=0x100; i<0x110; i++) OPNASetReg(opna, i, 0);
    for (i=0x10; i<0x20; i++) OPNASetReg(opna, i, 0);
    for (i=0; i<6; i++) {
        Channel4 *ch = &opna->ch[i];
        ch->panl = 46340;
        ch->panr = 46340;
        for(j=0; j<4; j++) {
            FMOperator *op = &opna->ch[i].op[j];
            OperatorReset(op);
        }
    }

    opna->statusnext = 0;
    opna->lfocount = 0;
    opna->status = 0;
}

/* ---------------------------------------------------------------------------
// Change OPNA "DAC" samplerate.
// r and ipflag are as in OPNAInit(), above.
*/
uint8_t OPNASetRate(OPNA *opna, uint32_t r, uint8_t ipflag)
{
    int i, j;
    opna->interpolation = ipflag;
    opna->psgrate = r;
    RebuildTimeTable(opna);
    opna->lfodcount = opna->reg22 & 0x08 ? opna->lfotab[opna->reg22 & 7] : 0;

    for (i=0; i<6; i++) {
        for (j=0; j<4; j++)
            opna->ch[i].op[j].paramchanged = 1;
    }
    for (i=0; i<6; i++) {
        opna->rhythm[i].step = opna->rhythm[i].rate * 1024 / r;
    }
    return 1;
}

void SetVolumeRhythm(OPNA *opna, unsigned int index, int db)
{
    db = Min(db, 20);
    opna->rhythm[index].volume = 16-(db * 2 / 3);
}

/* ---------------------------------------------------------------------------
// Set OPNA channel mask. The 6 LSBs of mask are 0 to disable that FM channel,
// and 1 to enable it. The next 3 LSBs are passed to PSGSetChannelMask() to,
// well, set the PSG channel mask (which behaves the same way: 0 disables
// a given channel and 1 enables it).
*/
void OPNASetChannelMask(OPNA *opna, uint32_t mask)
{
    int i, j;
    for (i=0; i<6; i++) {
        for (j=0; j<4; j++) {
            opna->ch[i].op[j].mute = (!(mask & (1 << i)));
            opna->ch[i].op[j].paramchanged = 1;
        }
    }
    PSGSetChannelMask(&opna->psg, (mask >> 6));
    /*if (!(mask & 0x200)) opna->devmask = 3;*/
}

#include <stdio.h>

/* libOPNMIDI: allow to disable the console messages */
#if defined(OPNA_VERBOSE)
static void message(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
#else
static void message(const char *fmt, ...)
{
    (void)fmt;
}
#endif

/* ---------------------------------------------------------------------------
// Main OPNA register-set routine. Really long and boring switch-case.
// Basically taken directly from the manual - the only parts of the spec
// that were even the least bit tricky to implement were the f-number tables,
// everything else is basically obvious.
*/
void OPNASetReg(OPNA *opna, uint32_t addr, uint32_t data)
{
    uint32_t j, _dp = 0;
    int c = addr & 3;
    switch (addr)
    {
        uint32_t modified;
        uint32_t tmp;

    /* Timer ----------------------------------------------------------------- */
        case 0x24: case 0x25:
            opna->regta[addr & 1] = (uint8_t)data;
            tmp = (opna->regta[0] << 2) + (opna->regta[1] & 3);
            opna->timera = (1024-tmp) * opna->timer_step;
            break;

        case 0x26:
            opna->timerb = (256-data) * opna->timer_step;
            break;

        case 0x27:
            tmp = opna->regtc ^ data;
            opna->regtc = (uint8_t)data;
            if (data & 0x10)
                opna->status &= ~1;
            if (data & 0x20)
                opna->status &= ~2;
            if (tmp & 0x01)
                opna->timera_count = (data & 1) ? opna->timera : 0;
            if (tmp & 0x02)
                opna->timerb_count = (data & 2) ? opna->timerb : 0;
            break;

    /* Misc ------------------------------------------------------------------ */
    case 0x28:      /* Key On/Off */
        if ((data & 3) < 3)
        {
            uint32_t key = (data >> 4);
            c = (data & 3) + (data & 4 ? 3 : 0);
            if (key & 0x1) KeyOn(&opna->ch[c].op[0]); else KeyOff(&opna->ch[c].op[0]);
            if (key & 0x2) KeyOn(&opna->ch[c].op[1]); else KeyOff(&opna->ch[c].op[1]);
            if (key & 0x4) KeyOn(&opna->ch[c].op[2]); else KeyOff(&opna->ch[c].op[2]);
            if (key & 0x8) KeyOn(&opna->ch[c].op[3]); else KeyOff(&opna->ch[c].op[3]);
        }
        break;

    /* Status Mask ----------------------------------------------------------- */
    case 0x29:
        opna->reg29 = data;
        break;

    /* Prescaler ------------------------------------------------------------- */
    case 0x2d: case 0x2e: case 0x2f:
        SetPrescaler(opna, (addr-0x2d));
        break;

    /* F-Number -------------------------------------------------------------- */
    case 0x1a0: case 0x1a1: case 0x1a2:
        c += 3;
        /* fall through */
    case 0xa0:  case 0xa1: case 0xa2:
        opna->fnum[c] = data + opna->fnum2[c] * 0x100;
        _dp = (opna->fnum[c] & 2047) << ((opna->fnum[c] >> 11) & 7);
        for(j=0; j<4; j++) {
            opna->ch[c].op[j].dp = _dp;
            opna->ch[c].op[j].bn = notetab[(opna->fnum[c] >> 7) & 127];
            opna->ch[c].op[j].paramchanged = 1;
        }
        break;

    case 0x1a4: case 0x1a5: case 0x1a6:
        c += 3;
        /* fall through */
    case 0xa4 : case 0xa5: case 0xa6:
        opna->fnum2[c] = (uint8_t)data;
        break;

    case 0xa8:  case 0xa9: case 0xaa:
        opna->fnum3[c] = data + opna->fnum2[c+6] * 0x100;
        break;

    case 0xac : case 0xad: case 0xae:
        opna->fnum2[c+6] = (uint8_t)data;
        break;

    /* Algorithm ------------------------------------------------------------- */
    case 0x1b0: case 0x1b1:  case 0x1b2:
        c += 3;
        /* fall through */
    case 0xb0:  case 0xb1:  case 0xb2:
        opna->ch[c].fb = fbtab[((data >> 3) & 7)];
        SetAlgorithm(&opna->ch[c], data & 7);
        message("OP%u: Algorithm: %u, FB: %u\n", c, data & 7, opna->ch[c].fb);
        break;

    case 0x1b4: case 0x1b5: case 0x1b6:
        c += 3;
        /* fall through */
    case 0xb4: case 0xb5: case 0xb6:
        /*opna->pan[c] = (data >> 6) & 3;*/
        for(j=0; j<4; j++) {
            opna->ch[c].op[j].ms = data;
            opna->ch[c].op[j].paramchanged = 1;
        }
        break;

    /* Rhythm ---------------------------------------------------------------- */
    case 0x10:          /* DM/KEYON */
        if (!(data & 0x80))  /* KEY ON */
        {
            opna->rhythmkey |= data & 0x3f;
            if (data & 0x01) opna->rhythm[0].pos = 0;
            if (data & 0x02) opna->rhythm[1].pos = 0;
            if (data & 0x04) opna->rhythm[2].pos = 0;
            if (data & 0x08) opna->rhythm[3].pos = 0;
            if (data & 0x10) opna->rhythm[4].pos = 0;
            if (data & 0x20) opna->rhythm[5].pos = 0;
        }
        else
        {                   /* DUMP */
            opna->rhythmkey &= ~data;
        }
        break;

    case 0x11:
        opna->rhythmtl = ~data & 63;
        break;

    case 0x1a:      /* Top Cymbal */
        break;
    case 0x18:      /* Bass Drum */
    case 0x19:      /* Snare Drum */
    case 0x1b:      /* Hihat */
    case 0x1c:      /* Tom-tom */
    case 0x1d:      /* Rim shot */
        opna->rhythm[addr & 7].pan   = (data >> 6) & 3;
        opna->rhythm[addr & 7].level = ~data & 31;
        break;

    /* LFO ------------------------------------------------------------------- */
    case 0x22:
        modified = opna->reg22 ^ data;
        opna->reg22 = data;
        if (modified & 0x8)
            opna->lfocount = 0;
        opna->lfodcount = opna->reg22 & 8 ? opna->lfotab[opna->reg22 & 7] : 0;
        message("LFO: reg22: %u, lfodcount: %u\n", opna->reg22, opna->lfodcount);
        break;

    /* PSG ------------------------------------------------------------------- */
    case  0: case  1: case  2: case  3: case  4: case  5: case  6: case  7:
    case  8: case  9: case 10: case 11: case 12: case 13: case 14: case 15:
        PSGSetReg(&opna->psg, addr, data);
        break;

    /* ADSR ------------------------------------------------------------------ */
    default:
        if (c < 3)
        {
            if (addr & 0x100)
                c += 3;
            {
                /*uint8_t slottable[4] = { 0, 2, 1, 3 };*/
                /*uint32_t slot = slottable[(addr >> 2) & 3];*/
                uint32_t slottable = 216;
                uint32_t l, slot = ((slottable >> (((addr >> 2) & 3) << 1)) & 3);
                FMOperator* op = &opna->ch[c].op[slot];

                switch ((addr >> 4) & 15)
                {
                case 3: /* 30-3E DT/MULTI */
                    op->detune = (((data >> 4) & 0x07) * 0x20);
                    op->multiple = (data & 0x0f);
                    l = ((op->multiple) ? 2*op->multiple : 1);
                    /*  PG Part */
                    op->pgdcount = (op->dp + dttab[op->detune + op->bn]) * (uint32_t)(l * opna->rr);
                    op->pgdcountl = op->pgdcount >> 11;
                    /*op->paramchanged = 1;*/
                    if(!op->mute)
                    message("OP%u DT: %u, Mult: %u\n", c, op->detune, op->multiple);
                    break;

                case 4: /* 40-4E TL */
                    if(!((opna->regtc & 0x80) && (opna->csmch == &opna->ch[c]))) {
                        op->tl = (data & 0x7f);
                        op->paramchanged = 1;
                    }
                    op->tll = (data & 0x7f);
                    break;

                case 5: /* 50-5E KS/AR */
                    op->ks = ((data >> 6) & 3);
                    op->ar = ((data & 0x1f) * 2);
                    op->paramchanged = 1;
                    if(!op->mute)
                    message("OP%u KS: %u, AR: %u\n", c, op->ks, op->ar);
                    break;

                case 6: /* 60-6E DR/AMON */
                    op->dr = ((data & 0x1f) * 2);
                    op->amon = ((data & 0x80) != 0);
                    op->paramchanged = 1;
                    if(!op->mute)
                    message("OP%u DR: %u, AM: %u\n", c, op->dr, op->amon);
                    break;

                case 7: /* 70-7E SR */
                    op->sr = ((data & 0x1f) * 2);
                    op->paramchanged = 1;
                    if(!op->mute)
                    message("OP%u SR: %u\n", c, op->sr);
                    break;

                case 8: /* 80-8E SL/RR */
                    op->sl = (((data >> 4) & 15) * 4);
                    op->rr = ((data & 0x0f) * 4 + 2);
                    op->paramchanged = 1;
                    if(!op->mute)
                    message("OP%u SL: %u, RR: %u\n", c, op->sl, op->rr);
                    break;

                case 9: /* 90-9E SSG-EC */
                    op->ssgtype = (data & 0x0f);
                    message("OP%u SSG-EG: %u\n", c, op->ssgtype);
                    break;
                }
            }
        }
        break;
    }
}

/* libOPNMIDI: soft panning */
void OPNASetPan(OPNA *opna, uint32_t chan, uint32_t data)
{
    assert(chan < 6);
    assert(data < 128);
    opna->ch[chan].panl = panlawtable[data & 0x7F];
    opna->ch[chan].panr = panlawtable[0x7F - (data & 0x7F)];
}

/* ---------------------------------------------------------------------------
// Read OPNA register. Pointless. Only SSG registers can be read, and of those
// the only one anyone seems to be interested in reading is register 7,
// which as I explain in detail in psg.c, is completely superfluous.
*/
uint32_t OPNAGetReg(OPNA *opna, uint32_t addr)
{
    if (addr < 0x10)
        return PSGGetReg(&opna->psg, addr);
    if (addr == 0xff)
        return 1;
    return 0;
}

/* --------------------------------------------------------------------------- */

static inline void MixSubS(Channel4 ch[6], int activech, int32_t *dest)
{
    unsigned int c;
    int32_t l = 0;
    int32_t r = 0;

    for (c = 0; c < 6; ++c) {
        if (activech & (1 << (c << 1))) {
            int32_t s = Ch4Calc(&ch[c]);
            s >>= 2; /* libOPNMIDI: prevent FM channel clipping (TODO: also adjust PSG and rhythm) */
            l += s * ch[c].panl / 65536;
            r += s * ch[c].panr / 65536;
        }
    }

    dest[0] = l;
    dest[1] = r;
}

/* ---------------------------------------------------------------------------
// Mix FM channels and output. Mix6 runs at user-specified samplerate,
// Mix6I runs at the chip samplerate of 55466Hz and then downsamples
// to the user-specified samplerate. It is an open problem as to determining
// if one of these sounds better than the other.
*/
#define IStoSample(s)   (Limit16((s)))

static void Mix6(OPNA *opna, int32_t *buffer, uint32_t nsamples, int activech)
{
    /* Mix */
    int32_t ibuf[2];
    unsigned int i;

    for (i = 0; i < nsamples; i++) {
        ibuf[0] = 0;
        ibuf[1] = 0;
        if (activech & 0xaaa)
            LFO(opna), MixSubS(opna->ch, activech, ibuf);
        else
            MixSubS(opna->ch, activech, ibuf);
        buffer[i * 2 + 0] += IStoSample(ibuf[0]);
        buffer[i * 2 + 1] += IStoSample(ibuf[1]);
    }
}

/* ---------------------------------------------------------------------------
// See comment above Mix6(), above.
*/
static void Mix6I(OPNA *opna, int32_t *buffer, uint32_t nsamples, int activech)
{
    /* Mix */
    int32_t ibuf[2], delta = opna->mixdelta;
    unsigned int i;

    if (opna->mpratio < 16384) {
        for (i = 0; i < nsamples; i++) {
            int32_t l = 0, r = 0, d = 0;
            while (delta > 0) {
                ibuf[0] = 0;
                ibuf[1] = 0;
                if (activech & 0xaaa)
                    LFO(opna), MixSubS(opna->ch, activech, ibuf);
                else
                    MixSubS(opna->ch, activech, ibuf);

                l = IStoSample(ibuf[0]);
                r = IStoSample(ibuf[1]);
                d = Min(opna->mpratio, delta);
                opna->mixl += l * d;
                opna->mixr += r * d;
                delta -= opna->mpratio;
            }
            buffer[i * 2 + 0] += opna->mixl >> 14;
            buffer[i * 2 + 1] += opna->mixr >> 14;
            opna->mixl = l * (16384-d);
            opna->mixr = r * (16384-d);
            delta += 16384;
        }
    } else {
        int impr = 16384 * 16384 / opna->mpratio;
        for (i = 0; i < nsamples; i++) {
            int32_t l, r;
            if (delta < 0) {
                delta += 16384;
                opna->mixl = opna->mixl1;
                opna->mixr = opna->mixr1;

                ibuf[0] = 0;
                ibuf[1] = 0;
                if (activech & 0xaaa)
                    LFO(opna), MixSubS(opna->ch, activech, ibuf);
                else
                    MixSubS(opna->ch, activech, ibuf);

                opna->mixl1 = IStoSample(ibuf[0]);
                opna->mixr1 = IStoSample(ibuf[1]);
            }
            l = (delta * opna->mixl + (16384 - delta) * opna->mixl1) / 16384;
            r = (delta * opna->mixr + (16384 - delta) * opna->mixr1) / 16384;
            buffer[i * 2 + 0] += l;
            buffer[i * 2 + 1] += r;
            delta -= impr;
        }
    }
    opna->mixdelta = delta;
}

/* ---------------------------------------------------------------------------
// Main FM output routine. Clocks all of the operators on the chip, then mixes
// together the output using one of Mix6() or Mix6I() above, and then outputs
// the result to OPNAMix, which is what the calling routine will actually use.
// buffer should be a pointer to a buffer of type Sample (int32_t in this
// implementation, though another used float and in principle int16_t *should*
// be sufficient), and be of size at least equal to nsamples.
*/
static void FMMix(OPNA *opna, int32_t *buffer, uint32_t nsamples)
{
    uint32_t j;
    {
        /* Set F-Number */
        if (!(opna->regtc & 0xc0)) {
            uint32_t _dp = (opna->fnum[opna->csmch-opna->ch] & 2047) << ((opna->fnum[opna->csmch-opna->ch] >> 11) & 7);
            for(j=0; j<4; j++) {
                opna->csmch->op[j].dp = _dp;
                opna->csmch->op[j].bn = notetab[(opna->fnum[opna->csmch-opna->ch] >> 7) & 127];
                opna->csmch->op[j].paramchanged = 1;
            }
        } else {
            SetFNum(&opna->csmch->op[0], opna->fnum3[1]); SetFNum(&opna->csmch->op[1], opna->fnum3[2]);
            SetFNum(&opna->csmch->op[2], opna->fnum3[0]); SetFNum(&opna->csmch->op[3], opna->fnum[2]);
        }
    }

    {
        int act = (((Ch4Prepare(&opna->ch[2]) << 2) | Ch4Prepare(&opna->ch[1])) << 2) | Ch4Prepare(&opna->ch[0]);
        if (opna->reg29 & 0x80)
            act |= (Ch4Prepare(&opna->ch[3]) | ((Ch4Prepare(&opna->ch[4]) | (Ch4Prepare(&opna->ch[5]) << 2)) << 2)) << 6;
        if (!(opna->reg22 & 0x08))
            act &= 0x555;

        if (act & 0x555) {
            if (opna->interpolation)
                Mix6I(opna, buffer, nsamples, act);
            else
                Mix6(opna, buffer, nsamples, act);
        } else {
            opna->mixl = 0, opna->mixr = 0, opna->mixdelta = 16383;
        }
    }
}

/* ---------------------------------------------------------------------------
// Mix Rhythm generator output. Boring, just takes the PCM samples,
// multiplies them by the volume set for that rhythm sample, and then outputs
// the appropriate length of sample for that given samplerate to buffer.
// The same restrictions on buffer as in FMMix() above apply.
*/
static void RhythmMix(OPNA *opna, int32_t *buffer, uint32_t count)
{
    unsigned int i, j;
    if (opna->rhythmtvol < 128 && opna->rhythm[0].sample && (opna->rhythmkey & 0x3f)) {
        for (i=0; i<6; i++) {
            Rhythm *r = &opna->rhythm[i];
            if ((opna->rhythmkey & (1 << i)) && r->level >= 0) {
                int db = Limit(opna->rhythmtl+r->level+r->volume, 127, 0);
                int vol = cltab[db];

                for (j = 0; j < count && r->pos < r->size; j++) {
                    int sample = Limit16(((r->sample[r->pos >> 10] << 8) * vol) >> 10);
                    r->pos += r->step;
                    buffer[j * 2 + 0] += sample;
                    buffer[j * 2 + 1] += sample;
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------------
// Main OPNA output routine. See FMMix(), RhythmMix() above and PSGMix()
// in psg.c for details.
*/
void OPNAMix(OPNA *opna, int16_t *buf, uint32_t nframes)
{
    int32_t buffer[16384];
    unsigned int i, clips = 0;
    for (i = 0; i < 2 * nframes; i++) buffer[i] = 0;
    if(opna->devmask & 1) FMMix(opna, buffer, nframes);
    if(opna->devmask & 2) PSGMix(&opna->psg, buffer, nframes);
    if(opna->devmask & 4) RhythmMix(opna, buffer, nframes);
    for (i = 0; i < 2 * nframes; i++) {
        int32_t k = (buffer[i] >> 2);
        if (k > 32767 || k < -32767) clips++;
        buf[i] = Limit16(k);
    }
    if (clips) message("clipped %u samples\n", clips);
}
