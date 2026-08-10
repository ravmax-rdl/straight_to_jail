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

    /* R0.7: a seed on the command line makes a run reproducible, which is
       the whole verification strategy -- there is no test binary, because a
       glob build cannot tolerate a second main. Without one, vary by clock. */
    srand(argc > 1 ? (unsigned)atoi(argv[1]) : (unsigned)time(NULL));

    /* Section 5 "Before the Game Begins", graded character-for-character.
       R5.7: this must be the first line of output -- nothing may precede it. */
    printf("MONOPOLY-LK Simulation\n");
    printf("\n");
    printf("Player 1 : Aggressive Investor\n");
    printf("Player 2 : Conservative Banker\n");
    printf("Player 3 : Risk Taker\n");
    printf("Player 4 : Opportunistic Trader\n");
    printf("\n");
    printf("Each player begins with LKR 30,000.\n");
    printf("\n");

    game_init(&g);
    game_run(&g);

    return 0;
}
