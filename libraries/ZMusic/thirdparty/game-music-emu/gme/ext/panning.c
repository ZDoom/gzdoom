#include <stdlib.h>
#include <math.h>
#include "panning.h"

#ifndef PI
#define PI 3.14159265359
#endif
#ifndef SQRT2
#define SQRT2 1.414213562
#endif
#define RANGE 512

//-----------------------------------------------------------------
// Set the panning values for the two stereo channels (L,R)
// for a position -256..0..256 L..C..R
//-----------------------------------------------------------------
void calc_panning(float channels[2], int position)
{
	if ( position > RANGE / 2 )
		position = RANGE / 2;
	else if ( position < -RANGE / 2 )
		position = -RANGE / 2;
	position += RANGE / 2;	// make -256..0..256 -> 0..256..512

	// Equal power law: equation is
	// right = sin( position / range * pi / 2) * sqrt( 2 )
	// left is equivalent to right with position = range - position
	// position is in the range 0 .. RANGE
	// RANGE / 2 = centre, result = 1.0f
	channels[1] = (float)( sin( (double)position / RANGE * PI / 2 ) * SQRT2 );
	position = RANGE - position;
	channels[0] = (float)( sin( (double)position / RANGE * PI / 2 ) * SQRT2 );
}

//-----------------------------------------------------------------
// Reset the panning values to the centre position
//-----------------------------------------------------------------
void centre_panning(float channels[2])
{
	channels[0] = channels[1] = 1.0f;
}

/*//-----------------------------------------------------------------
// Generate a stereo position in the range 0..RANGE
// with Gaussian distribution, mean RANGE/2, S.D. RANGE/5
//-----------------------------------------------------------------
int random_stereo()
{
	int n = (int)(RANGE/2 + gauss_rand() * (RANGE * 0.2) );
	if ( n > RANGE ) n = RANGE;
	if ( n < 0 ) n = 0;
	return n;
}

//-----------------------------------------------------------------
// Generate a Gaussian random number with mean 0, variance 1
// Copied from an ancient C newsgroup FAQ
//-----------------------------------------------------------------
double gauss_rand()
{
	static double V1, V2, S;
	static int phase = 0;
	double X;

	if(phase == 0) {
		do {
			double U1 = (double)rand() / RAND_MAX;
			double U2 = (double)rand() / RAND_MAX;

			V1 = 2 * U1 - 1;
			V2 = 2 * U2 - 1;
			S = V1 * V1 + V2 * V2;
			} while(S >= 1 || S == 0);

		X = V1 * sqrt(-2 * log(S) / S);
	} else
		X = V2 * sqrt(-2 * log(S) / S);

	phase = 1 - phase;

	return X;
}*/
