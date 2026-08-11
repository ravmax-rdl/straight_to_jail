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

/* Returns false if Rent.csv could not be loaded (D27). board_init has
   already explained why on stderr; the caller must abandon the run rather
   than play on a board whose properties have no prices. */
bool game_init(GameState *g, const char *csvPath)
{
    int i;

    memset(g, 0, sizeof *g);
    if (!board_init(g, csvPath)) {
        return false;
    }

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
    return true;
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

/* ------------------------------------------------------- block output ---- */

/* Rule-line widths, measured from the section 5 templates rather than
   guessed: the round summary uses 45 characters and the market conditions
   block uses 41. Extracting the PDF's text confirmed both, and confirmed
   that the round summary's field lines are consecutive -- the vertical gaps
   in the rendered PDF are LaTeX spacing, not content. That is D26. */
#define SUMMARY_RULE 45
#define MARKET_RULE  41

static void rule_line(char c, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        putchar(c);
    }
    putchar('\n');
}

/* Counted directly off the board rather than cached on the Player, so the
   figure cannot drift from reality after a foreclosure or an auction. */
static int count_properties(const GameState *g, int p)
{
    int i, n = 0;
    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].type == SQ_PROPERTY && g->board[i].owner == p) {
            n++;
        }
    }
    return n;
}

static int count_hotels(const GameState *g, int p)
{
    int i, n = 0;
    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].owner == p && g->board[i].hotel) {
            n++;
        }
    }
    return n;
}

/* Milestone 2 swaps the stored price for square_value once the choke point
   exists; until then nothing is owned and both read zero. */
static int total_property_value(const GameState *g, int p)
{
    int i, total = 0;
    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].owner == p) {
            total += g->board[i].price;
        }
    }
    return total;
}

/* Section 5 "End of Every Round". Players print in turn order, and the
   separator falls between players only -- there is none after the last one,
   because the closing rule already terminates the block. */
void round_summary(const GameState *g)
{
    char b[FMT_BUF];
    int  i;

    rule_line('=', SUMMARY_RULE);
    printf("Round %d Summary\n", g->round);
    rule_line('=', SUMMARY_RULE);

    for (i = 0; i < NUM_PLAYERS; i++) {
        const Player *pl = &g->players[g->order[i]];
        int           p  = g->order[i];

        printf("%s\n", pl->name);
        printf("Cash : LKR %s\n", fmt_lkr(b, pl->cash));
        printf("Net Worth : LKR %s\n", fmt_lkr(b, net_worth(g, p)));
        printf("Properties : %d\n", count_properties(g, p));
        printf("Hotels : %d\n", count_hotels(g, p));

        /* "None", never "LKR 0" -- the template is explicit about this and
           a zero would also be a lie, since a settled loan is not a loan. */
        if (pl->loan.active) {
            printf("Outstanding Loan : LKR %s\n", fmt_lkr(b, pl->loan.principal));
        } else {
            printf("Outstanding Loan : None\n");
        }

        if (i < NUM_PLAYERS - 1) {
            rule_line('-', SUMMARY_RULE);
        }
    }

    rule_line('=', SUMMARY_RULE);
}

/* Rule-LK 36, printed at the end of every round immediately after the
 * summary.
 *
 * The underline widths are literal. They are not label length plus a
 * constant -- the five run 13, 16, 23, 12 and 23 against labels of 11, 14,
 * 20, 9 and 21 -- so they are transcribed from the template rather than
 * computed, and a comment says so to stop a later reader "fixing" them.
 *
 * Milestone 4 fills the first three sections from the effect registry. Until
 * then only the two scalar rates have anything to report, and a section with
 * nothing active prints nothing at all.
 */
void market_conditions(const GameState *g)
{
    rule_line('=', MARKET_RULE);
    printf("Current Market Conditions\n");
    rule_line('=', MARKET_RULE);
    printf("\n");

    /* Market Boom, Market Decline and Regional Development arrive with the
       effect registry in milestone 4. */

    printf("Inflation\n");
    rule_line('-', 12);
    printf("%+d%%\n", g->econ.inflationPct);
    printf("\n");

    printf("Current Loan Interest\n");
    rule_line('-', 23);
    printf("%d%%\n", g->econ.interestRatePct);
    printf("\n");

    rule_line('=', MARKET_RULE);
}

/* Section 5 "End of Game". Note this block puts labels and values on
   separate lines, unlike the round summary's "Label : value" pairs. */
void final_report(const GameState *g)
{
    char b[FMT_BUF];
    int  i, winner = g->order[0];

    /* Rule 15 has two endings and one tiebreak between them. If a single
       player is still solvent they win outright; otherwise 500 rounds have
       elapsed and the highest net worth takes it. Scanning for the best net
       worth among the solvent covers both cases in one pass. */
    for (i = 0; i < NUM_PLAYERS; i++) {
        if (g->players[i].bankrupt) {
            continue;
        }
        if (g->players[winner].bankrupt || net_worth(g, i) > net_worth(g, winner)) {
            winner = i;
        }
    }

    rule_line('=', SUMMARY_RULE);
    printf("GAME OVER\n");
    printf("Winner\n");
    printf("%s\n", g->players[winner].name);
    printf("Total Cash\n");
    printf("LKR %s\n", fmt_lkr(b, g->players[winner].cash));
    printf("Total Property Value\n");
    printf("LKR %s\n", fmt_lkr(b, total_property_value(g, winner)));
    printf("Outstanding Loans\n");
    if (g->players[winner].loan.active) {
        printf("LKR %s\n", fmt_lkr(b, g->players[winner].loan.principal));
    } else {
        printf("None\n");
    }
    printf("Net Worth\n");
    printf("LKR %s\n", fmt_lkr(b, net_worth(g, winner)));
    rule_line('=', SUMMARY_RULE);
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

    /* D13 puts these last and in this order. The economic cadences slot in
       above them as the milestones introduce them. */
    round_summary(g);
    market_conditions(g);
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

    final_report(g);
}
