#ifndef LIBXMP_PAULA_H
#define LIBXMP_PAULA_H

/* 131072 to 0, 2048 entries */
#define PAULA_HZ 3546895
#define MINIMUM_INTERVAL 16
#define BLEP_SCALE 17
#define BLEP_SIZE 2048
#define MAX_BLEPS (BLEP_SIZE / MINIMUM_INTERVAL)

/* the structure that holds data of bleps */
struct blep_state {
	int16 level;
	int16 age;
};

struct paula_state {
	/* the instantenous value of Paula output */
	int16 global_output_level;

	/* count of simultaneous bleps to keep track of */
	unsigned int active_bleps;

	/* place to keep our bleps in. MAX_BLEPS should be
	 * defined as a BLEP_SIZE / MINIMUM_EVENT_INTERVAL.
	 * For Paula, minimum event interval could be even 1, but it makes
	 * sense to limit it to some higher value such as 16. */
	struct blep_state blepstate[MAX_BLEPS];

	double remainder;
	double fdiv;
};


void	libxmp_paula_init	(struct context_data *, struct paula_state *);

#endif /* !LIBXMP_PAULA_H */
