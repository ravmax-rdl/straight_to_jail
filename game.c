/* game.c -- the simulation engine.
 *
 * The only module that orchestrates. It owns the round and turn loops, the
 * landing dispatch, the end-of-round scheduler, and every block-format
 * output. Everything else answers questions; this file asks them.
 */

#include <stdio.h>
#include <string.h>

#include "types.h"

/* Player 1-4 order, fixed by the section 5 pre-game block. This is the
   order they are *introduced* in, not the order they play in -- turn order
   is decided by the roll-off below. */
static const char *PLAYER_NAMES[NUM_PLAYERS] = {
    "Aggressive Investor",
    "Conservative Banker",
    "Risk Taker",
    "Opportunistic Trader"
};

static const Strategy PLAYER_STRATS[NUM_PLAYERS] = {
    STRAT_AGGRESSIVE, STRAT_CONSERVATIVE, STRAT_RISKTAKER, STRAT_OPPORTUNIST
};

/* ------------------------------------------------------- initialisation -- */

void game_init(GameState *g)
{
    int i;

    memset(g, 0, sizeof *g);
    board_init(g);

    for (i = 0; i < NUM_PLAYERS; i++) {
        Player *p = &g->players[i];

        p->name  = PLAYER_NAMES[i];
        p->strat = PLAYER_STRATS[i];
        p->cash  = START_CASH;      /* Rule 1                               */
        p->pos   = SQ_IDX_GO;

        g->order[i] = i;            /* replaced by determine_order          */
    }

    /* Table 9's Stable Economy is where the economy starts (D21). Both rates
       drift from here: the loan rate for new loans, the tax rate for
       Income Tax (D2'). */
    g->econ.interestRatePct = BASE_INTEREST_PCT;
    g->econ.incomeTaxPct    = INCOME_TAX_PCT;

    /* -1 is "none yet" for all three. GRP_NONE happens to be -1 too, which
       is what makes the LK 30 consecutive-repeat check work on round 10. */
    g->econ.activeRegulation  = -1;
    g->econ.lastBoomGroup     = GRP_NONE;
    g->econ.lastDeclineGroup  = GRP_NONE;

    g->round = 0;
}

/* --------------------------------------------------------- turn order --- */

/* Selection sort of order[lo..hi) into descending score. Four elements at
   most, so the algorithm is irrelevant; what matters is that order[] and
   score[] stay in lockstep, since score[k] is the roll of player order[k]
   rather than of player k. */
static void sort_slice(int *order, int *score, int lo, int hi)
{
    int i, j, best, tmp;

    for (i = lo; i < hi - 1; i++) {
        best = i;
        for (j = i + 1; j < hi; j++) {
            if (score[j] > score[best]) {
                best = j;
            }
        }
        if (best != i) {
            tmp = order[i]; order[i] = order[best]; order[best] = tmp;
            tmp = score[i]; score[i] = score[best]; score[best] = tmp;
        }
    }
}

/* D8': resolve a run of equal rolls.
 *
 * The subtlety the rule turns on is that a reroll decides only the
 * positions of the tied players *among themselves*. Untied players keep
 * their ranks. Because this only ever touches order[lo..hi), a player who
 * was not tied cannot be displaced by someone else's reroll -- which is
 * precisely what the lecturer's worked example requires when players 2 and
 * 3 reroll and players 1 and 4 stay put.
 *
 * A reroll can tie again, so this recurses on any run that survives. Depth
 * is bounded in practice by probability and in code by the guard, which
 * cannot fire in a real game but stops a pathological RNG from hanging the
 * simulation.
 */
static void resolve_ties(GameState *g, int *order, int *score, int lo, int hi, int depth)
{
    int k, runStart, runEnd;

    if (hi - lo < 2 || depth > 32) {
        return;
    }

    for (k = lo; k < hi; k++) {
        int d1, d2;
        score[k] = roll_dice(&d1, &d2);
        printf("%s rolls %d.\n", g->players[order[k]].name, score[k]);
    }

    sort_slice(order, score, lo, hi);

    runStart = lo;
    while (runStart < hi) {
        runEnd = runStart + 1;
        while (runEnd < hi && score[runEnd] == score[runStart]) {
            runEnd++;
        }
        resolve_ties(g, order, score, runStart, runEnd, depth + 1);
        runStart = runEnd;
    }
}

/* Rule 2 and section 5 "Determining the First Player". */
void determine_order(GameState *g)
{
    int score[NUM_PLAYERS];
    int i, runStart, runEnd;

    for (i = 0; i < NUM_PLAYERS; i++) {
        int d1, d2;
        g->order[i] = i;
        score[i]    = roll_dice(&d1, &d2);
        printf("%s rolls %d.\n", g->players[i].name, score[i]);
    }

    sort_slice(g->order, score, 0, NUM_PLAYERS);

    runStart = 0;
    while (runStart < NUM_PLAYERS) {
        runEnd = runStart + 1;
        while (runEnd < NUM_PLAYERS && score[runEnd] == score[runStart]) {
            runEnd++;
        }
        resolve_ties(g, g->order, score, runStart, runEnd, 0);
        runStart = runEnd;
    }

    printf("\n");
    printf("%s will begin the game.\n", g->players[g->order[0]].name);
    printf("\n");
    printf("Turn order:\n");
    for (i = 0; i < NUM_PLAYERS; i++) {
        printf("%s\n", g->players[g->order[i]].name);
    }
}

/* --------------------------------------------------------- the loops ----- */

/* Rule 3. Steps 2 and 3 only for now; the remaining six arrive with the
   systems they depend on, in the order the milestones introduce them. */
void play_turn(GameState *g, int p)
{
    int d1, d2, total;

    total = roll_dice(&d1, &d2);
    printf("%s rolled %d.\n", g->players[p].name, total);

    move_player(g, p, total);
}

/* One round is one turn for every solvent player, in order[] sequence --
   which is the clarification's definition, and the reason D19 needs only a
   single clock: a player's own round count and the game's advance together,
   diverging only when the player leaves the game entirely. */
void play_round(GameState *g)
{
    int i;

    g->round++;

    for (i = 0; i < NUM_PLAYERS; i++) {
        int p = g->order[i];
        if (!g->players[p].bankrupt) {
            play_turn(g, p);
        }
    }
}

/* Rule 15's first ending: the game stops when only one player is still
   solvent. Nobody can go bankrupt yet, so this is always false until
   milestone 3 -- but game_run is written against it now so the ending does
   not need retrofitting later. */
bool game_over(const GameState *g)
{
    int i, solvent = 0;

    for (i = 0; i < NUM_PLAYERS; i++) {
        if (!g->players[i].bankrupt) {
            solvent++;
        }
    }
    return solvent < 2;
}

void game_run(GameState *g)
{
    determine_order(g);

    while (g->round < MAX_ROUNDS && !game_over(g)) {
        play_round(g);
    }
}
