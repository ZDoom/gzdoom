
// Use in place of Ym2413_Emu.cpp and ym2413.c to disable support for this chip

// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#include "Ym2413_Emu.h"

Ym2413_Emu::Ym2413_Emu() { }

Ym2413_Emu::~Ym2413_Emu() { }

int Ym2413_Emu::set_rate( double, double ) { return 2; }

void Ym2413_Emu::reset() { }

void Ym2413_Emu::write( int, int ) { }

void Ym2413_Emu::mute_voices( int ) { }

void Ym2413_Emu::run( int, sample_t* ) { }

