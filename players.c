/* players.c -- the four decision engines.
 *
 * Section 3 gives each player a personality, and every one of them is
 * expressed through the same small set of decisions: buy or not, bid or
 * withdraw, borrow or repay, insure, build, maintain, renovate. This file
 * owns all of them and nothing else owns any of them.
 *
 * The boundary is strict: these functions READ state and DECIDE. They never
 * assign to cash, ownership or buildings -- game.c and finance.c execute
 * what is decided. A strategy that wanted to quietly adjust its own balance
 * would have to change its signature, which would be conspicuous.
 *
 * MILESTONE 6 replaces every body here with the four real personalities,
 * each a switch on Player.strat. The signatures are already final, so that
 * milestone touches this file and no other. Until then the placeholders
 * below give every player the same plain behaviour -- enough for the rest of
 * the program to be exercised and observed.
 */

#include "types.h"

/* PLACEHOLDER (milestone 6). Rule 5's choice: take it at the asking price,
   or decline and send it to auction. Every strategy currently buys whatever
   it can afford, which is deliberately naive -- it keeps property moving so
   rent, monopolies and auctions all get exercised. */
bool decide_buy(GameState *g, int p, int sq)
{
    return g->players[p].cash >= square_value(g, sq);
}

/* PLACEHOLDER (milestone 6). Return the amount to bid, or 0 to withdraw
   permanently from this auction (LK 21).
 *
 * minBid is the smallest legal bid right now -- the opening price for the
 * first bidder, the standing bid plus LK 20's increment afterwards. Handing
 * over the floor rather than the current high bid is what lets a strategy
 * answer without recomputing the opening for itself.
 *
 * The placeholder bids the minimum while it stays under 60% of market value
 * and within cash, so auctions run several rounds and then terminate.
 */
int decide_bid(GameState *g, int p, int sq, int minBid)
{
    int cap = pct_of(square_value(g, sq), 60);

    if (minBid > g->players[p].cash || minBid > cap) {
        return 0;
    }
    return minBid;
}

/* Rule 8 plus the consequence Rule 9 has for a mortgaged member.
 *
 * A mortgaged square cannot be built on, and building on its groupmates would
 * push them further and further ahead of it -- so the whole group is barred
 * until the mortgage is lifted. Allowing the rest to build would break the
 * evenness requirement rather than satisfy it.
 */
static bool group_developable(const GameState *g, int p, PropertyGroup grp)
{
    int i;

    if (!group_monopoly(g, p, grp)) {
        return false;
    }
    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].group == grp && g->board[i].mortgaged) {
            return false;
        }
    }
    return true;
}

/* PLACEHOLDER (milestone 6). Rule 3 step 6: which square to build on, or -1
 * to build nothing. The caller decides house or hotel from the square's
 * current level and executes; this function only chooses.
 *
 * Always the LEAST developed square across every monopolised group. That one
 * rule delivers Rule 9's even building for free: a square can only ever be
 * one level ahead of its groupmates, because the moment it is, one of them
 * becomes the minimum and takes the next building. No explicit evenness check
 * is needed anywhere, and the DEBUG invariant in game.c confirms it holds.
 *
 * A square already at MAX_HOUSES is the minimum only once every other member
 * has four too, which is exactly Rule 10's precondition for a hotel -- so the
 * upgrade falls out of the same comparison rather than needing its own pass.
 *
 * Affordability is checked here rather than left to charge(): building is
 * voluntary, and a player who would have to sell buildings to fund a building
 * should simply not build. See build_step in game.c.
 */
int decide_build(GameState *g, int p)
{
    int i, best = -1, bestLevel = MAX_HOUSES + 1;

    for (i = 0; i < NUM_SQUARES; i++) {
        const Square *s = &g->board[i];
        int           level;

        if (s->type != SQ_PROPERTY || s->owner != p) {
            continue;
        }
        if (!group_developable(g, p, s->group)) {
            continue;
        }

        level = development_level(g, i);
        if (level > MAX_HOUSES || level >= bestLevel) {
            continue;               /* already a hotel, or not the emptiest  */
        }
        if (g->players[p].cash < building_cost(g, i, level == MAX_HOUSES)) {
            continue;
        }

        best      = i;
        bestLevel = level;
    }

    return best;
}

/* Table 3's second band edge. Above it a building still collects 90% or
   100% of its rent; at 74% the factor drops to 75%, which is the first cut
   worth paying to avoid. */
#define MAINTAIN_BELOW_PCT 75

/* PLACEHOLDER (milestone 6). Rule 3 step 1 and LK 27: which building to
 * restore to full condition, or -1 for none. One square per call; game.c
 * repeats until this stops offering, which is how LK 27's "any number of
 * buildings if affordable" is expressed without this function executing
 * anything.
 *
 * Maintaining at 75% rather than on the way down from 100 is the cheap
 * reading of Table 3: condition falls 2% a round, so a property serviced at
 * the band edge collects full rent for the seven rounds it takes to slip
 * back, and pays once for them. Servicing at 99% would pay the same price
 * for one round of benefit.
 */
int decide_maintenance(GameState *g, int p)
{
    int i;

    for (i = 0; i < NUM_SQUARES; i++) {
        const Square *s = &g->board[i];

        if (s->owner != p || development_level(g, i) == 0) {
            continue;
        }
        if (s->conditionPct >= MAINTAIN_BELOW_PCT) {
            continue;
        }
        if (g->players[p].cash < maintenance_cost(g, i)) {
            continue;
        }
        return i;
    }

    return -1;
}
