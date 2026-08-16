/* main.c -- program entry point.
 *
 * Owns three things and nothing else: the seed, the GameState on the stack,
 * and the handoff to game.c. All simulation logic lives elsewhere.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "types.h"

int main(int argc, char **argv)
{
    /* R0.4 forbids globals, so the entire mutable state of the simulation
       lives here, on main's stack, and is threaded by pointer from now on.
       About 7 KB -- comfortably inside any default stack. */
    GameState g;
    int       i;

    /* D27: argv[2] overrides where Rent.csv is looked for. NULL means search
       the candidate list, which is what happens in every ordinary run. */
    const char *csvPath = (argc > 2) ? argv[2] : NULL;

    /* R0.7: a seed on the command line makes a run reproducible, which is
       the whole verification strategy -- there is no test binary, because a
       glob build cannot tolerate a second main. Without one, vary by clock. */
    srand(argc > 1 ? (unsigned)atoi(argv[1]) : (unsigned)time(NULL));

    /* Initialise before printing anything. R5.7 requires the banner to be
       the first line of output, so a failed CSV load must not have printed a
       partial pre-game block ahead of its own error -- and the error itself
       goes to stderr, which never pollutes the graded stdout stream. */
    if (!game_init(&g, csvPath)) {
        return 1;
    }

    /* Section 5 "Before the Game Begins", graded character-for-character.
       R5.7: this must be the first line of output -- nothing may precede it.
       The roster is read back out of the state game_init just filled, so the
       names have one definition (game.c's PLAYER_NAMES) rather than two. */
    printf("MONOPOLY-LK Simulation\n");
    printf("\n");
    for (i = 0; i < NUM_PLAYERS; i++) {
        printf("Player %d : %s\n", i + 1, g.players[i].name);
    }
    printf("\n");
    printf("Each player begins with LKR 30,000.\n");
    printf("\n");

    game_run(&g);

    return 0;
}
