// Fuzz effect as rendered by the software renderer

#define FUZZTABLE 50
#define FUZZ_RANDOM_X_SIZE 100
#define FRACBITS 16
#define fixed_t int

int fuzz_random_x_offset[FUZZ_RANDOM_X_SIZE] = int[]
(
	75, 76, 21, 91, 56, 33, 62, 99, 61, 79,
	95, 54, 41, 18, 69, 43, 49, 59, 10, 84,
	94, 17, 57, 46,  9, 39, 55, 34,100, 81,
	73, 88, 92,  3, 63, 36,  7, 28, 13, 80,
	16, 96, 78, 29, 71, 58, 89, 24,  1, 35,
	52, 82,  4, 14, 22, 53, 38, 66, 12, 72,
	90, 44, 77, 83,  6, 27, 48, 30, 42, 32,
	65, 15, 97, 20, 67, 74, 98, 85, 60, 68,
	19, 26,  8, 87, 86, 64, 11, 37, 31, 47,
	25,  5, 50, 51, 23,  2, 93, 70, 40, 45
);

int fuzzoffset[FUZZTABLE] = int[]
(
	 6, 11,  6, 11,  6,  6, 11,  6,  6, 11,
	 6,  6,  6, 11,  6,  6,  6, 11, 15, 18,
	21,  6, 11, 15,  6,  6,  6,  6, 11, 6,
	11,  6,  6, 11, 15,  6,  6, 11, 15, 18,
	21,  6,  6,  6,  6, 11,  6,  6, 11, 6
);

vec4 ProcessTexel()
{
	vec2 texCoord = vTexCoord.st;
	vec4 basicColor = getTexel(texCoord);

	// Ideally fuzzpos would be an uniform and differ from each sprite so that overlapping demons won't get the same shade for the same pixel
	int next_random = int(abs(mod(timer * 35.0, float(FUZZ_RANDOM_X_SIZE))));
	int fuzzpos = (/*fuzzpos +*/ fuzz_random_x_offset[next_random] * FUZZTABLE / 100) % FUZZTABLE;

	int x = int(gl_FragCoord.x);
	int y = int(gl_FragCoord.y);

	fixed_t fuzzscale = (200 << FRACBITS) / uViewHeight;
	int scaled_x = (x * fuzzscale) >> FRACBITS;
	int fuzz_x = fuzz_random_x_offset[scaled_x % FUZZ_RANDOM_X_SIZE] + fuzzpos;
	fixed_t fuzzcount = FUZZTABLE << FRACBITS;
	fixed_t fuzz = ((fuzz_x << FRACBITS) + y * fuzzscale) % fuzzcount;
	float alpha = float(fuzzoffset[fuzz >> FRACBITS]) / 32.0;

	return vec4(0.0, 0.0, 0.0, basicColor.a * alpha);
}
