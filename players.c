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
