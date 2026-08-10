/* board.c -- the board itself: its layout, its randomness, and the movement
 * and valuation queries every other module asks of it.
 */

#include <stdlib.h>

#include "types.h"

/* Uniform integer in [lo, hi].
 *
 * The rejection loop is not decoration. The naive lo + rand() % span is
 * biased whenever span does not divide RAND_MAX + 1: the low residues occur
 * once more often than the high ones. For a die that skews every roll in the
 * game, and every downstream statistic with it. Discarding the short tail
 * above the largest exact multiple of span removes the bias entirely.
 *
 * The loop terminates with probability 1 and in practice almost always on
 * the first draw -- the rejected window is at most span-1 values out of
 * RAND_MAX + 1.
 */
int rng_range(int lo, int hi)
{
    int span  = hi - lo + 1;
    int limit = RAND_MAX - (RAND_MAX % span);
    int r;

    do {
        r = rand();
    } while (r >= limit);

    return lo + (r % span);
}

int roll_die(void)
{
    return rng_range(1, 6);
}

/* Fills both dice and returns their total. Callers need the individual dice
   for Rule 13's doubles check and the total for movement and utility rent,
   so both are handed back rather than recomputed. */
int roll_dice(int *d1, int *d2)
{
    *d1 = roll_die();
    *d2 = roll_die();
    return *d1 + *d2;
}
