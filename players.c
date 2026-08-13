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
    /* LK 24's Anti-Speculation Act, and the one regulation a strategy has to
     * read for itself -- the other seven are percentages that a choke point
     * applies without anyone asking.
     *
     * D25 implements the cap alone and drops the rule's second clause, that
     * additional purchases require development within five rounds. Enforcing
     * the cap strictly makes that clause unreachable: the additional purchase
     * can never happen, so there is nothing to develop and no five rounds to
     * count. One of the two has to give, and the cap is the half the rule
     * leads with.
     *
     * effect_modifier sums, which would be wrong for a ceiling -- but LK 24
     * runs one regulation at a time, so at most one such record exists and
     * the sum is that record.
     */
    int cap = effect_modifier(g, EFF_MAX_PROPERTIES, sq, p);

    if (cap > 0 && count_undeveloped(g, p) >= cap) {
        return false;
    }

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

    /* Appendix A's Labour Strike stops this player building for two rounds.
       A flag rather than a percentage, so it is read for presence. */
    if (effect_active(g, EFF_CONSTRUCTION_SUSPENDED, -1, p)) {
        return -1;
    }

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

/* Above this much wear, the placeholder pays to reverse it. Section 3 gives
   the Conservative Banker 10% and the Opportunistic Trader 15%, so this sits
   at the lower of the two and milestone 6 splits them. */
#define RENOVATE_ABOVE_PCT 10

/* PLACEHOLDER (milestone 6). LK 17: renovation is offered only on a square
 * the player is standing on and already owns, so the square comes in rather
 * than being searched for -- the caller in land_on already knows it.
 *
 * Worth doing at all only when there is something to reverse. A renovation
 * costs a tenth of market value and buys back the depreciation and the age;
 * on an undepreciated property it buys nothing.
 */
bool decide_renovate(GameState *g, int p, int sq)
{
    const Square *s = &g->board[sq];

    if (s->owner != p || s->type != SQ_PROPERTY) {
        return false;
    }

    /* LK 29 first. Structural damage is the worse of the two -- it costs
       value, rent and upkeep all at once, where depreciation costs only
       value -- so it is worth clearing before the wear is, and at a
       different price against a different base. */
    if (s->structDamaged) {
        return g->players[p].cash >= structural_renovation_cost(g, sq);
    }

    if (s->depreciationPct <= RENOVATE_ABOVE_PCT) {
        return false;
    }

    return g->players[p].cash >= pct_of(square_value(g, sq), RENOVATE_PCT);
}

/* PLACEHOLDER (milestone 6). R1.9 and S1.2: landing on an insurance square
 * buys or renews ONE policy on ONE property, so this returns a single square
 * or -1 and writes the tier it wants to *tier.
 *
 * Only developed property is worth insuring, and that is not a strategy
 * preference but what the rules make true: LK 10's disasters strike developed
 * properties, D1 prices the repair off the buildings, and a vacant lot has
 * neither. Insuring one would be paying a premium against a peril that cannot
 * reach it.
 *
 * Basic everywhere, which milestone 6 replaces -- section 3 wants Basic on
 * houses and Comprehensive on hotels for the Aggressive Investor, and nothing
 * at all for the Risk Taker until it has already lost something.
 */
int decide_insurance(GameState *g, int p, InsuranceType *tier)
{
    int i;

    *tier = INS_BASIC;

    for (i = 0; i < NUM_SQUARES; i++) {
        const Square *s = &g->board[i];

        if (s->owner != p || s->policy != INS_NONE) {
            continue;
        }
        if (development_level(g, i) == 0) {
            continue;
        }
        if (g->players[p].cash < premium(g, i, INS_BASIC)) {
            continue;
        }
        return i;
    }

    return -1;
}

/* How close to maturity a loan must be before the placeholder buys time
   rather than hoping for another Bank landing. */
#define EXTEND_WITHIN_ROUNDS 5

/* PLACEHOLDER (milestone 6). R1.8 and LK 5: the Bank square offers five
 * actions and grants exactly one per landing, so this returns the first that
 * applies and the order below IS the policy.
 *
 * Settle before part-paying, part-pay before buying time, buy time before
 * borrowing more. That ranking is not arbitrary: under D4 the principal
 * compounds every round at the rate it was issued at, and R1.8 makes this
 * square the only place it can ever be paid down. A player who passes up a
 * chance to reduce the balance may not get another for twenty rounds, by
 * which time 8% per round has multiplied it by four and a half.
 *
 * Borrowing is deliberately reactive -- only when cash has fallen below half
 * the starting stake, and only for the shortfall rather than the maximum.
 * Taking the LK 2 ceiling on principle is the Risk Taker's move and belongs
 * in milestone 6; here it would simply bankrupt all four players the same
 * way at the same time and hide everything else this milestone added.
 */
BankAction decide_bank(GameState *g, int p, int *amount)
{
    const Player *pl = &g->players[p];
    int           capacity;

    *amount = 0;

    if (pl->loan.active) {
        if (pl->cash >= pl->loan.principal) {
            *amount = pl->loan.principal;
            return BANK_REPAY_FULL;
        }
        if (pl->cash >= pl->loan.principal / 2 && pl->loan.principal > 1) {
            *amount = pl->loan.principal / 2;
            return BANK_REPAY_PART;
        }
        if (g->round + EXTEND_WITHIN_ROUNDS >= pl->loan.issuedRound + pl->loan.termRounds) {
            return BANK_EXTEND;
        }

        capacity = loan_capacity(g, p);
        if (capacity > 0 && pl->cash < START_CASH / 4) {
            *amount = capacity;
            return BANK_INCREASE;
        }
        return BANK_NONE;
    }

    if (pl->cash < START_CASH / 2) {
        int shortfall = START_CASH - pl->cash;
        int cap       = max_loan(g, p);

        if (cap > 0) {
            *amount = (shortfall < cap) ? shortfall : cap;
            return BANK_OBTAIN;
        }
    }

    return BANK_NONE;
}
