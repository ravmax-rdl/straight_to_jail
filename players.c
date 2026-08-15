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
 * SHAPE. Each public decide_* below splits in two. The RULES come first and
 * apply to everyone -- Rule 8's monopoly test, D25's purchase cap, LK 27's
 * one-square-at-a-time -- because a personality is a preference and may not
 * override the rulebook. What is left is the preference, and that is where
 * the switch on Player.strat lives. Writing it the other way round would put
 * four copies of every rule in the file and invite three of them to drift.
 *
 * The scalar preferences live in PROFILE below rather than in four copies of
 * the same function, so section 3 can be read against a table.
 */

#include <limits.h>   /* INT_MAX, for the uncapped bid sentinel */

#include "types.h"

/* ------------------------------------------------------------- profiles -- */

/* One row per section 3 personality, indexed by Strategy. Every column cites
 * the bullet it implements, so the table can be checked against the spec by
 * eye rather than by reading four functions.
 *
 * Behaviour that is structural rather than scalar -- what a strategy buys,
 * how it banks -- cannot live here and is a switch further down.
 */
/* R4.3 alone has no bid ceiling -- section 3.3 says the Risk Taker "bids
   until available cash is exhausted", so LK 22's cash cap is its only limit.
   A negative percentage says that outright; encoding it as some large
   percentage would put a number that looks like a ceiling in a column of
   real ones, and invite the reading that this player will pay ten times
   market value. */
#define BID_UNCAPPED (-1)

typedef struct {
    int  bidCapPct;          /* R4.*: ceiling as % of value, or BID_UNCAPPED */
    int  maintainBelowPct;   /* R4.*: condition band that triggers upkeep   */
    int  renovateAbovePct;   /* R4.*: depreciation that triggers renovation */
    int  bailBelowPct;       /* Rule 13: bail paid when it costs no more    */
    bool hotelsWhileIndebted;/* R4.2: no hotels while a loan is outstanding */
    bool insureOnlyAfterLoss;/* R4.3: insures only after suffering one      */
    InsuranceType houseTier; /* R4.*: cover bought on a house property      */
    InsuranceType hotelTier; /* R4.*: cover bought on a hotel property      */
} Profile;

/* On the tier columns. Three of the four personalities have theirs dictated:
 * 3.1 buys "only Basic for houses and Comprehensive for hotels", 3.2 "always
 * Comprehensive for every developed property", 3.4 "Comprehensive only for
 * high-value developments". 3.3 is the exception -- it says when the Risk
 * Taker insures ("only after experiencing a financial loss") and never what
 * it buys, which is the one opening section 3 leaves.
 *
 * Business Interruption goes there. D3 confines that tier to hotel
 * properties, and the Risk Taker builds more hotels than anyone; a
 * speculative player already carrying a loss, choosing the dearest cover on
 * the assets most exposed to it, is the reading its section supports. Its
 * houses stay on Basic, since D3 gives Business Interruption no coverage at
 * all on a property without a hotel and the premium would buy nothing.
 */

static const Profile PROFILE[] = {
    /* STRAT_AGGRESSIVE   */ { 120, 75, 10, 100, true,  false, INS_BASIC, INS_COMPREHENSIVE },
    /* STRAT_CONSERVATIVE */ {  90, 90, 10,   0, false, false, INS_COMPREHENSIVE, INS_COMPREHENSIVE },
    /* STRAT_RISKTAKER    */ { BID_UNCAPPED, 25, 30, 100, true, true, INS_BASIC, INS_BUSINESS },
    /* STRAT_OPPORTUNIST  */ { 100, 75, 15,  10, true,  false, INS_NONE,  INS_COMPREHENSIVE }
};

static const Profile *profile(const GameState *g, int p)
{
    return &PROFILE[g->players[p].strat];
}

/* ---------------------------------------------------------- shared rules -- */

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

/* LK 24's Anti-Speculation Act, and the one regulation a strategy has to be
 * told about -- the other seven are percentages a choke point applies without
 * anyone asking.
 *
 * D25 implements the cap alone and drops the rule's second clause, that
 * additional purchases require development within five rounds. Enforcing the
 * cap strictly makes that clause unreachable: the additional purchase can
 * never happen, so there is nothing to develop and no five rounds to count.
 *
 * effect_modifier sums, which would be wrong for a ceiling -- but LK 24 runs
 * one regulation at a time, so at most one such record exists and the sum is
 * that record.
 */
static bool purchase_permitted(const GameState *g, int p, int sq)
{
    int cap = effect_modifier(g, EFF_MAX_PROPERTIES, sq, p);

    return !(cap > 0 && count_undeveloped(g, p) >= cap);
}

/* Is p holding more undeveloped colour property than LK 24 allows?
 *
 * The condition that "invokes the immediate development": above the cap,
 * building stops being a preference and becomes an obligation, so every
 * personality builds while it lasts -- including the Opportunistic Trader,
 * which would otherwise wait out an inflationary round, and the Conservative
 * Banker, which would otherwise refuse a hotel while indebted.
 */
static bool over_speculation_cap(const GameState *g, int p)
{
    int cap = effect_modifier(g, EFF_MAX_PROPERTIES, -1, p);

    return cap > 0 && count_undeveloped(g, p) > cap;
}

/* Would owning this square complete a colour group for p? R4.1 has the
   Aggressive Investor complete groups first, and a group is the only thing
   that unlocks Rule 8, so this is worth paying over the odds for. */
static bool completes_group(const GameState *g, int p, int sq)
{
    PropertyGroup grp = g->board[sq].group;
    int           i;

    if (grp == GRP_NONE) {
        return false;
    }
    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].group == grp && i != sq && g->board[i].owner != p) {
            return false;
        }
    }
    return true;
}

/* R4.1: is there anyone left who could still land on this and pay for it?
 *
 * The bullet reads "always buys if one future rent remains payable", which is
 * a solvency question rather than a property question -- a square nobody can
 * visit earns nothing however good it looks.
 */
static bool rent_still_payable(const GameState *g, int p)
{
    int i;

    for (i = 0; i < NUM_PLAYERS; i++) {
        if (i != p && !g->players[i].bankrupt) {
            return true;
        }
    }
    return false;
}

/* What it would cost p to raise every square of every monopoly it holds by
 * one level.
 *
 * R4.1 borrows "whenever the funds raise projected rent", and under Rule 8
 * that is the only condition in which borrowed money can raise rent at all --
 * so this figure is both the test (is it greater than zero) and the amount.
 * Borrowing the LK 2 ceiling instead would be a different bullet from a
 * different personality, and under D4's per-round compounding it is fatal:
 * with the maximum drawn on every monopoly, the Aggressive Investor went
 * bankrupt on all five plan seeds.
 */
static int development_shortfall(const GameState *g, int p)
{
    int i, needed = 0;

    for (i = 0; i < NUM_SQUARES; i++) {
        const Square *s     = &g->board[i];
        int           level;

        if (s->type != SQ_PROPERTY || s->owner != p) {
            continue;
        }
        if (!group_developable(g, p, s->group)) {
            continue;
        }
        level = development_level(g, i);
        if (level > MAX_HOUSES) {
            continue;                            /* already a hotel         */
        }
        needed += building_cost(g, i, level == MAX_HOUSES);
    }

    return needed;
}

/* ------------------------------------------------------------- purchase -- */

/* R4.1. Buys whatever it can afford for as long as anyone is left to charge
 * rent to -- but finishes what it started first.
 *
 * "Completes groups first" outranks "always buys" in the bullet list, and
 * that ordering is load-bearing rather than decorative. Spending down to
 * nothing on every square it lands on leaves a monopoly stuck at four houses
 * with no cash for the hotel: across the five plan seeds this personality
 * built 98 houses and not one hotel, while the Conservative Banker's 50%
 * reserve let it finish five. So a purchase that would leave the outstanding
 * development unfunded is declined -- which sends the square to auction,
 * where this player is still the highest bidder on anything it actually
 * wants.
 *
 * The exception is a square that completes a group. That is the one purchase
 * which creates more development than it defers, so it is never declined on
 * these grounds.
 */
static bool buy_aggressive(GameState *g, int p, int sq)
{
    int price = square_value(g, sq);
    int cash  = g->players[p].cash;

    if (!rent_still_payable(g, p) || cash < price) {
        return false;
    }
    if (completes_group(g, p, sq)) {
        return true;
    }

    return cash - price >= development_shortfall(g, p);
}

/* R4.2. Buys out of surplus rather than out of capital, and stops buying
 * altogether while the market is falling.
 *
 * "No investments during recessions" is read off the registry rather than
 * from any flag: a global negative VALUE_MUL is exactly what Economic
 * Recession, Economic Downturn and a market decline all push, so one query
 * covers every way the spec has of saying the market is down. Square -1
 * restricts it to board-wide effects, which is what "recession" means -- a
 * single group in decline is a bargain, not a recession.
 */
static bool buy_conservative(GameState *g, int p, int sq)
{
    int price = square_value(g, sq);
    int cash  = g->players[p].cash;

    if (effect_modifier(g, EFF_VALUE_MUL, -1, p) < 0) {
        return false;                            /* R4.2: not in a slump    */
    }

    /* R4.2: buys only if at least half the cash survives the purchase --
       except for railways and utilities, where it will go down to a quarter.
       That relaxation IS the "prefers railways and utilities" bullet: they
       are fixed income that can never be developed, so they never demand a
       second outlay the way a colour group does, and this personality will
       dig deeper for one. */
    if (g->board[sq].type != SQ_PROPERTY) {
        return cash - price >= cash / 4;
    }

    return cash - price >= cash / 2;
}

/* D9's projected appreciation: the square's value scaled by the net of every
 * modifier currently reaching it.
 *
 * The decision reads straight off effect_modifier rather than out of a
 * forecast of its own, which is what makes it a genuinely different way of
 * valuing the board -- this player buys what the economy is already lifting,
 * where the Aggressive Investor buys what completes a group.
 */
static int projected_appreciation(const GameState *g, int p, int sq)
{
    int net = effect_modifier(g, EFF_VALUE_MUL, sq, p)
            + effect_modifier(g, EFF_RENT_MUL, sq, p);

    return pct_of(square_value(g, sq), net);
}

/* R4.4. Buys only what the market is already rewarding, and would rather
 * wait for the auction than pay the asking price.
 *
 * D9 sets the bar at the group's construction cost: an appreciation smaller
 * than one house is not worth the capital, because the same money spent on a
 * group it already holds would earn more. Railways and utilities have no
 * house cost, so the bar for them is the one thing they can be compared
 * against -- their own mortgage value, the sum the board itself says they
 * are worth as security.
 */
static bool buy_opportunist(GameState *g, int p, int sq)
{
    int bar;

    if (g->players[p].cash < square_value(g, sq)) {
        return false;
    }

    bar = (g->board[sq].type == SQ_PROPERTY)
          ? building_cost(g, sq, false)
          : mortgage_value(g, sq);

    /* R4.4: prefers discounted auctions. Declining sends the square straight
       to auction under Rule 5, where LK 19 opens it at half of market value
       -- so a refusal here is not a decision to go without, it is a decision
       to bid for the same square at a discount. */
    return projected_appreciation(g, p, sq) > bar;
}

/* Rule 5's choice: take it at the asking price, or decline and send it to
 * auction. The rules have already had their say by the time this is called;
 * what is left is whether this personality wants it at that price.
 */
static bool wants_to_buy(GameState *g, int p, int sq)
{
    switch (g->players[p].strat) {
    case STRAT_AGGRESSIVE:
        return buy_aggressive(g, p, sq);

    case STRAT_CONSERVATIVE:
        return buy_conservative(g, p, sq);

    case STRAT_RISKTAKER:
        /* R4.3: buys every available property, and invests through
           downturns -- so unlike the Conservative Banker there is
           deliberately no market test here at all. */
        return g->players[p].cash >= square_value(g, sq);

    case STRAT_OPPORTUNIST:
        return buy_opportunist(g, p, sq);
    }
    return false;
}

bool decide_buy(GameState *g, int p, int sq)
{
    if (!purchase_permitted(g, p, sq)) {
        return false;
    }
    return wants_to_buy(g, p, sq);
}

/* -------------------------------------------------------------- auction -- */

/* Return the amount to bid, or 0 to withdraw permanently from this auction
 * (LK 21).
 *
 * minBid is the smallest legal bid right now -- the opening price for the
 * first bidder, the standing bid plus LK 20's increment afterwards. Handing
 * over the floor rather than the current high bid is what lets a strategy
 * answer without recomputing the opening for itself.
 *
 * Every personality bids the minimum and differs only in how far it will
 * follow, which is D9's reading of section 3: the ceiling is the personality,
 * the increment is the rule.
 */
/* The two squares R4.1 names outright. Board indices rather than names
   because Rent.csv can rename a property but Table 1 fixes where it sits. */
#define SQ_IDX_NUWARA_ELIYA 37
#define SQ_IDX_GALLE_FACE   39

static bool covets(int sq)
{
    return sq == SQ_IDX_GALLE_FACE || sq == SQ_IDX_NUWARA_ELIYA;
}

/* How far this personality will follow an auction, as a percentage of market
 * value. PROFILE holds the ceiling; the switch is for personalities whose
 * appetite depends on which square is under the hammer.
 */
static int bid_ceiling(GameState *g, int p, int sq)
{
    int pct = profile(g, p)->bidCapPct;

    switch (g->players[p].strat) {
    case STRAT_AGGRESSIVE:
        /* R4.1: D9's 120% is the ceiling it will ever reach, and it reaches
           it for the squares the bullet names -- one that completes a group,
           or Galle Face and Nuwara Eliya. Anything else it takes at market
           and no higher, which is what makes "completes groups first" and
           "covets" observable rather than decorative. */
        if (!completes_group(g, p, sq) && !covets(sq)) {
            pct = 100;
        }
        break;

    case STRAT_RISKTAKER:
        /* R4.3: bids until its cash is gone, so there is no ceiling to
           compute. LK 22 caps every bid at cash in run_auction regardless,
           which is what actually stops it. */
        break;

    case STRAT_OPPORTUNIST:
        /* R4.4: prefers discounted auctions, and the ceiling is what makes
           that a preference rather than a slogan. Its direct-purchase test
           is the strictest on the board, so the auction -- opened by LK 19
           at half of market value -- is how this player expects to acquire
           anything at all. A ceiling below market would lose every contested
           auction to the Aggressive Investor and leave it with nothing to be
           balanced about; market value is the point past which a discount is
           no longer a discount. */
        break;

    case STRAT_CONSERVATIVE:
        break;
    }

    if (pct == BID_UNCAPPED) {
        return INT_MAX;
    }
    return pct_of(square_value(g, sq), pct);
}

int decide_bid(GameState *g, int p, int sq, int minBid)
{
    int ceiling;

    /* LK 24 governs how much a player OWNS, not how they came to own it, so
       the Anti-Speculation cap gates the auction exactly as it gates the
       purchase. Bidding here was the way round it. */
    if (!purchase_permitted(g, p, sq)) {
        return 0;
    }

    ceiling = bid_ceiling(g, p, sq);
    if (minBid > g->players[p].cash || minBid > ceiling) {
        return 0;
    }
    return minBid;
}

/* --------------------------------------------------------- construction -- */

/* Rule 3 step 6: which square to build on, or -1 to build nothing.
 *
 * The SELECTION is Rule 9 and belongs to everyone: always the least developed
 * square across every monopolised group. That one rule delivers even building
 * for free -- a square can only ever be one level ahead of its groupmates,
 * because the moment it is, one of them becomes the minimum and takes the
 * next building. A square already at MAX_HOUSES is the minimum only once
 * every other member has four too, which is exactly Rule 10's precondition
 * for a hotel, so the upgrade falls out of the same comparison.
 *
 * The personality decides only whether to build the thing selected.
 *
 * Affordability is checked here rather than left to charge(): building is
 * voluntary, and a player who would have to sell buildings to fund a building
 * should simply not build. See build_step in game.c.
 */
static bool wants_to_build(GameState *g, int p, int sq, bool hotel)
{
    /* LK 24, and the one place a regulation outranks a personality. Above the
       Anti-Speculation cap the act requires immediate development, so every
       preference below is suspended until the holding is back within it --
       including R4.2's refusal to build hotels while indebted, since the
       regulation is the more specific instruction. */
    if (over_speculation_cap(g, p)) {
        return true;
    }

    if (hotel && !profile(g, p)->hotelsWhileIndebted && g->players[p].loan.active) {
        return false;                            /* R4.2                    */
    }

    switch (g->players[p].strat) {
    case STRAT_AGGRESSIVE:
    case STRAT_CONSERVATIVE:
    case STRAT_RISKTAKER:
        return true;

    case STRAT_OPPORTUNIST:
        /* R4.4: accelerates under a housing subsidy, delays while inflation
           is positive. Checked in that order, because a subsidy is a
           standing discount on this exact purchase while inflation is a
           reason to wait for one -- when both hold, the discount is already
           in hand and the reason to wait has gone. */
        if (effect_modifier(g, EFF_BUILD_COST_MUL, sq, p) < 0) {
            return true;
        }
        return g->econ.inflationPct <= 0;
    }
    return false;
}

/* Rule 9, and the only part of the selection that is a rule: is this square
 * at its own group's minimum development?
 *
 * Building anywhere else would put it two levels clear of a groupmate, which
 * is what "evenly" forbids. game.c's DEBUG guard asserts exactly this, so a
 * personality that got it wrong would abort rather than quietly cheat.
 */
static bool is_group_minimum(const GameState *g, int sq)
{
    PropertyGroup grp   = g->board[sq].group;
    int           level = development_level(g, sq);
    int           i;

    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].group == grp && development_level(g, i) < level) {
            return false;
        }
    }
    return true;
}

/* Given two legal candidates, does this personality prefer the newer one?
 *
 * Rule 9 fixes which square within a group may be built on; it says nothing
 * about which GROUP to push, and that gap is a real strategic choice. Pushing
 * the least developed group spreads houses across the board and reaches
 * hotels last; pushing the most developed finishes one group and starts
 * collecting hotel rent while the others are still empty lots.
 */
static bool prefers_candidate(GameState *g, int p, int level, int bestLevel)
{
    switch (g->players[p].strat) {
    case STRAT_AGGRESSIVE:
        /* R4.1: hotels as soon as legal. Concentrating on the group nearest
           completion is the only way to get there -- spreading evenly across
           every monopoly held reaches four houses everywhere and a hotel
           nowhere, which is what this arm was doing before. */
        return level > bestLevel;

    case STRAT_CONSERVATIVE:
    case STRAT_RISKTAKER:
    case STRAT_OPPORTUNIST:
        return level < bestLevel;
    }
    return false;
}

int decide_build(GameState *g, int p)
{
    int i, best = -1, bestLevel = -1;

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
        if (level > MAX_HOUSES) {
            continue;                            /* already a hotel         */
        }
        if (!is_group_minimum(g, i)) {
            continue;                            /* Rule 9                  */
        }
        if (g->players[p].cash < building_cost(g, i, level == MAX_HOUSES)) {
            continue;
        }
        if (!wants_to_build(g, p, i, level == MAX_HOUSES)) {
            continue;
        }
        if (best >= 0 && !prefers_candidate(g, p, level, bestLevel)) {
            continue;
        }

        best      = i;
        bestLevel = level;
    }

    return best;
}

/* --------------------------------------------------------- maintenance --- */

/* Rule 3 step 1 and LK 27: which building to restore to full condition, or
 * -1 for none. One square per call; game.c repeats until this stops offering,
 * which is how LK 27's "any number of buildings if affordable" is expressed
 * without this function executing anything.
 *
 * The scan is shared; the band each personality tolerates before paying is
 * PROFILE's, because that is the whole of the difference between an owner who
 * protects its rent and one that lets the buildings rot.
 */
int decide_maintenance(GameState *g, int p)
{
    int i, threshold = profile(g, p)->maintainBelowPct;

    for (i = 0; i < NUM_SQUARES; i++) {
        const Square *s = &g->board[i];

        if (s->owner != p || development_level(g, i) == 0) {
            continue;
        }
        if (s->conditionPct >= threshold) {
            continue;
        }
        if (g->players[p].cash < maintenance_cost(g, i)) {
            continue;
        }
        return i;
    }

    return -1;
}

/* ---------------------------------------------------------- renovation --- */

/* LK 17: renovation is offered only on a square the player is standing on and
 * already owns, so the square comes in rather than being searched for -- the
 * caller in land_on already knows it.
 */
bool decide_renovate(GameState *g, int p, int sq)
{
    const Square *s = &g->board[sq];

    if (s->owner != p || s->type != SQ_PROPERTY) {
        return false;
    }

    /* LK 29 first, and for everyone. Structural damage is the worse of the
       two -- it costs value, rent and upkeep at once, where depreciation
       costs only value -- so it is worth clearing before the wear is, and at
       a different price against a different base. */
    if (s->structDamaged) {
        return g->players[p].cash >= structural_renovation_cost(g, sq);
    }

    if (s->depreciationPct <= profile(g, p)->renovateAbovePct) {
        return false;
    }

    return g->players[p].cash >= pct_of(square_value(g, sq), RENOVATE_PCT);
}

/* ------------------------------------------------------------ insurance -- */

/* R1.9 and S1.2: landing on an insurance square buys or renews ONE policy on
 * ONE property, so this returns a single square or -1 and writes the tier it
 * wants to *tier.
 *
 * Only developed property is considered, and that is not a preference but
 * what the rules make true: LK 10's disasters strike developed properties and
 * D1 prices the repair off the buildings, so a vacant lot has neither
 * exposure nor a repair bill. Insuring one would be paying a premium against
 * a peril that cannot reach it.
 */
int decide_insurance(GameState *g, int p, InsuranceType *tier)
{
    const Profile *pr = profile(g, p);
    int            i;

    *tier = INS_NONE;

    /* R4.3: the Risk Taker buys nothing until something has already gone
       wrong. sufferedLoss is set by the disaster roll and never cleared. */
    if (pr->insureOnlyAfterLoss && !g->players[p].sufferedLoss) {
        return -1;
    }

    for (i = 0; i < NUM_SQUARES; i++) {
        const Square *s    = &g->board[i];
        InsuranceType want;

        if (s->owner != p || s->policy != INS_NONE) {
            continue;
        }
        if (development_level(g, i) == 0) {
            continue;
        }

        want = s->hotel ? pr->hotelTier : pr->houseTier;
        if (want == INS_NONE) {
            continue;
        }
        if (g->players[p].cash < premium(g, i, want)) {
            continue;
        }

        *tier = want;
        return i;
    }

    return -1;
}

/* --------------------------------------------------------------- banking -- */

/* How close to maturity a loan must be before a strategy buys time rather
   than hoping for another Bank landing. */
#define EXTEND_WITHIN_ROUNDS 5

/* R1.8 and LK 5: the Bank square offers five actions and grants exactly one
 * per landing, so each arm returns the first that applies and its order IS
 * that personality's policy.
 *
 * Under D4 the principal compounds every round at the rate it was issued at,
 * and R1.8 makes this square the only place it can ever be paid down. A
 * player who passes up a chance to reduce the balance may not get another for
 * twenty rounds, by which time 8% per round has multiplied it by four and a
 * half. That is what makes the ordering below a real strategic difference
 * rather than a cosmetic one.
 */
/* R4.1. Borrows to build and is in no hurry to repay.
 *
 * The repayment test is written cash/2 > principal rather than
 * cash > 2*principal because a saturated principal would overflow the
 * multiplication -- D29 makes balances that large reachable.
 */
static BankAction bank_aggressive(GameState *g, int p, int *amount)
{
    const Player *pl = &g->players[p];

    if (pl->loan.active) {
        if (pl->cash / 2 > pl->loan.principal) {
            *amount = pl->loan.principal;
            return BANK_REPAY_FULL;              /* R4.1: only when 2x      */
        }
        if (pl->laps + EXTEND_WITHIN_ROUNDS >= pl->loan.issuedLap + pl->loan.termLaps) {
            return BANK_EXTEND;
        }
        return BANK_NONE;
    }

    {
        int needed = development_shortfall(g, p) - pl->cash;
        int cap    = max_loan(g, p);

        if (needed > 0 && cap > 0) {
            *amount = (needed < cap) ? needed : cap;
            return BANK_OBTAIN;                  /* R4.1: funds the build   */
        }
    }

    return BANK_NONE;
}

/* R4.2. Treats a loan as an emergency measure and clears it at the first
 * opportunity.
 *
 * "Borrows only when bankruptcy is otherwise unavoidable" is a judgment call
 * of the kind D9 exists to pin down, and the proxy here is a cash cushion
 * gone: below a tenth of the starting stake this player cannot meet an
 * ordinary rent, and the D11 ladder is the only thing left between it and
 * Rule 14.
 *
 * "Repaid at every Bank visit" is taken literally, tempered by the same
 * bullet list's "largest cash reserve" -- in full when it can be afforded
 * outright, half the remaining cash when it cannot. Repaying down to nothing
 * would honour one bullet by breaking another.
 */
#define CONSERVATIVE_DISTRESS_DIV 10

static BankAction bank_conservative(GameState *g, int p, int *amount)
{
    const Player *pl = &g->players[p];

    if (pl->loan.active) {
        if (pl->cash >= pl->loan.principal) {
            *amount = pl->loan.principal;
            return BANK_REPAY_FULL;
        }
        if (pl->cash / 2 > 0) {
            *amount = pl->cash / 2;
            return BANK_REPAY_PART;
        }
        return BANK_NONE;
    }

    if (pl->cash < START_CASH / CONSERVATIVE_DISTRESS_DIV) {
        int cap = max_loan(g, p);

        if (cap > 0) {
            *amount = cap;
            return BANK_OBTAIN;                  /* R4.2: staving off ruin  */
        }
    }

    return BANK_NONE;
}

/* R4.3. Borrows the maximum and never voluntarily reduces it.
 *
 * There is no repayment arm at all, and that is the bullet rather than an
 * omission: "always borrows the max, refinances often" describes a player who
 * treats a loan as permanent capital. Under D4 and R1.8 that ends in
 * foreclosure, which is the point -- this is the personality that exercises
 * milestone 3's failure paths.
 */
static BankAction bank_risktaker(GameState *g, int p, int *amount)
{
    const Player *pl = &g->players[p];
    int           cap;

    if (pl->loan.active) {
        cap = loan_capacity(g, p);
        if (cap > 0) {
            *amount = cap;
            return BANK_INCREASE;                /* R4.3: refinances often  */
        }
        if (pl->laps + EXTEND_WITHIN_ROUNDS >= pl->loan.issuedLap + pl->loan.termLaps) {
            return BANK_EXTEND;
        }
        return BANK_NONE;
    }

    cap = max_loan(g, p);
    if (cap > 0) {
        *amount = cap;                           /* R4.3: always the max    */
        return BANK_OBTAIN;
    }

    return BANK_NONE;
}

/* R4.4. Borrows only when the return beats the cost of the money.
 *
 * The comparison is the one the bullet names: what the borrowed capital would
 * add, against what it would cost to hold. Projected appreciation across the
 * whole portfolio stands for the return, and current_loan_rate for the cost
 * -- both read live, so a Reduce Loan Interest regulation or a Stock Market
 * Boom changes this player's willingness to borrow without a line here.
 */
static BankAction bank_opportunist(GameState *g, int p, int *amount)
{
    const Player *pl = &g->players[p];
    int           i, upside = 0, cap;

    if (pl->loan.active) {
        if (pl->cash >= pl->loan.principal) {
            *amount = pl->loan.principal;
            return BANK_REPAY_FULL;
        }
        if (pl->laps + EXTEND_WITHIN_ROUNDS >= pl->loan.issuedLap + pl->loan.termLaps) {
            return BANK_EXTEND;
        }
        return BANK_NONE;
    }

    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].owner == p) {
            upside += projected_appreciation(g, p, i);
        }
    }

    cap = max_loan(g, p);
    if (cap > 0 && upside > pct_of(cap, current_loan_rate(g, p))) {
        *amount = cap;
        return BANK_OBTAIN;                      /* R4.4: return beats cost */
    }

    return BANK_NONE;
}

/* D31: which mortgage is worth lifting, or -1.
 *
 * Redeeming restores three things at once -- Rule 7's rent, the group's
 * eligibility to be developed under Rule 8, and the square's standing as
 * collateral -- so the square that unblocks a monopoly is worth more than a
 * cheaper one that does not. Failing that, the dearest is taken, since
 * mortgage value tracks the rent the square will resume earning.
 *
 * Only redeem what leaves something behind. Spending the last rupee to lift
 * a mortgage puts the player straight back down the D11 ladder on the next
 * rent they owe, and the ladder would re-mortgage the same square.
 */
static int redeemable(GameState *g, int p)
{
    int i, best = -1, bestValue = 0;

    for (i = 0; i < NUM_SQUARES; i++) {
        const Square *s = &g->board[i];
        int           due, value;

        if (s->owner != p || !s->mortgaged) {
            continue;
        }
        due = mortgage_value(g, i);
        if (g->players[p].cash < due * 2) {
            continue;                            /* leaves nothing behind   */
        }

        /* A square whose group p otherwise owns outright is the one holding
           Rule 8 hostage; weight it above any bare rent comparison. */
        value = completes_group(g, p, i) ? due * 2 : due;
        if (value > bestValue) {
            best      = i;
            bestValue = value;
        }
    }

    return best;
}

BankAction decide_bank(GameState *g, int p, int *amount, int *square)
{
    BankAction act = BANK_NONE;

    *amount = 0;
    *square = -1;

    switch (g->players[p].strat) {
    case STRAT_AGGRESSIVE:
        act = bank_aggressive(g, p, amount);
        break;
    case STRAT_CONSERVATIVE:
        act = bank_conservative(g, p, amount);
        break;
    case STRAT_RISKTAKER:
        act = bank_risktaker(g, p, amount);
        break;
    case STRAT_OPPORTUNIST:
        act = bank_opportunist(g, p, amount);
        break;
    }

    /* Loan business first, redemption second, and only ever one of the two
     * (R1.8). A loan compounds every round under D4 while a mortgage sits
     * still, so a round spent on the debt that is growing is worth more than
     * one spent on the debt that is not.
     *
     * The Risk Taker is excluded outright rather than by threshold: R4.3 has
     * it borrowing the maximum and refinancing at every opportunity, and a
     * personality that never repays a loan does not clear a mortgage either.
     */
    if (act == BANK_NONE && g->players[p].strat != STRAT_RISKTAKER) {
        int sq = redeemable(g, p);

        if (sq >= 0) {
            *square = sq;
            return BANK_REDEEM;
        }
    }

    return act;
}

/* ---------------------------------------------------------- liquidation -- */

/* Rule 3 step 7: which PROPERTY to sell outright, or -1 to sell nothing.
 *
 * Section 3 says properties, not buildings. Three of the four personalities
 * are given a position on it -- section 3.1 never sells one, section 3.3
 * "sells lower-value properties to finance premium developments", section 3.4
 * "sells properties expected to decrease in value following economic events"
 * -- and D32 prices the act at market value.
 *
 * An earlier version sold one level of development instead. That was a
 * misreading, and an expensive one: buildings come back at half what they
 * cost, so selling and rebuilding destroyed value on every cycle and the
 * board churned at roughly one demolition per construction.
 */

/* Never sell out from under a monopoly. Breaking a group to raise cash
   forfeits Rule 8, which is the only thing that makes the rest of the group
   worth holding -- so whatever the strategy, the group being developed is
   not the place to find money. */
static bool sellable(const GameState *g, int p, int sq)
{
    const Square *s = &g->board[sq];

    if (s->owner != p || !is_purchasable(g, sq)) {
        return false;
    }
    if (s->loanLocked || s->mortgaged) {
        return false;                            /* LK 3; D31 comes first   */
    }
    return !group_developable(g, p, s->group);
}

/* Rule 13's choice: pay the LKR 300 or sit the turn out.
 *
 * Section 3 gives no personality any jail behaviour, so the column is read
 * off what each one IS. 3.1 and 3.3 are both defined by needing to be on the
 * board -- "rather than maintaining large cash reserves", "purchases every
 * available property whenever legally possible" -- so they pay whenever they
 * can. 3.2 waits, and waiting is not a gap in that strategy but the whole of
 * it: "maintains the largest emergency cash reserve among all players" is the
 * one bullet the personality is named for. 3.4 "always evaluates expected
 * return before making any financial decision", so it pays only while the
 * bail is small against what it holds.
 *
 * bailBelowPct needs no sentinel at either end, unlike BID_UNCAPPED: 0 makes
 * the test unsatisfiable and 100 collapses it into the affordability check,
 * so both read as the percentages they are.
 */
bool decide_bail(GameState *g, int p)
{
    const Player *pl = &g->players[p];

    /* Never through the D11 ladder. Rule 13 gives release away for three
       turns, so no player should sell a building or mortgage a property to
       buy it -- cash is tested before the charge, exactly as it is for every
       other voluntary payment (D31's redemption, LK 5's repayment). */
    if (pl->cash < JAIL_BAIL) {
        return false;
    }

    return JAIL_BAIL <= pct_of(pl->cash, profile(g, p)->bailBelowPct);
}

int decide_liquidate(GameState *g, int p)
{
    int i, best = -1, bestValue = 0;

    switch (g->players[p].strat) {
    case STRAT_AGGRESSIVE:
        /* 3.1: "never voluntarily sells a property unless bankruptcy is
           unavoidable". The unavoidable case is not this decision -- it is
           the D11 ladder, which sells and mortgages without asking. */
        return -1;

    case STRAT_CONSERVATIVE:
        /* 3.2 gives this player no selling behaviour at all, and one whose
           defining trait is the largest cash reserve has no reason to
           liquidate at market value to raise more. */
        return -1;

    case STRAT_RISKTAKER:
        /* 3.3: "sells lower-value properties to finance premium
           developments" -- so only when a premium development is actually
           waiting, and the cheapest holding goes first. */
        if (development_shortfall(g, p) <= g->players[p].cash) {
            return -1;
        }
        for (i = 0; i < NUM_SQUARES; i++) {
            int value;

            if (!sellable(g, p, i)) {
                continue;
            }
            value = square_value(g, i);
            if (best < 0 || value < bestValue) {
                best      = i;
                bestValue = value;
            }
        }
        return best;

    case STRAT_OPPORTUNIST:
        /* 3.4: "sells properties expected to decrease in value following
           economic events". A negative VALUE_MUL reaching the square is the
           economic event saying so, and LK 31-32 give a decline ten rounds
           to run -- long enough that the cash is better held elsewhere. The
           worst-hit square goes first. */
        for (i = 0; i < NUM_SQUARES; i++) {
            int drop;

            if (!sellable(g, p, i)) {
                continue;
            }
            drop = effect_modifier(g, EFF_VALUE_MUL, i, p);
            if (drop >= 0) {
                continue;
            }
            if (best < 0 || drop < bestValue) {
                best      = i;
                bestValue = drop;
            }
        }
        return best;
    }

    return -1;
}
