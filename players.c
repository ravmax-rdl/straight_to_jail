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

#include "types.h"

/* ------------------------------------------------------------- profiles -- */

/* One row per section 3 personality, indexed by Strategy. Every column cites
 * the bullet it implements, so the table can be checked against the spec by
 * eye rather than by reading four functions.
 *
 * Behaviour that is structural rather than scalar -- what a strategy buys,
 * how it banks -- cannot live here and is a switch further down.
 */
typedef struct {
    int  bidCapPct;          /* R4.*: ceiling on a bid, as % of value       */
    int  maintainBelowPct;   /* R4.*: condition band that triggers upkeep   */
    int  renovateAbovePct;   /* R4.*: depreciation that triggers renovation */
    bool hotelsWhileIndebted;/* R4.2: no hotels while a loan is outstanding */
    bool insureOnlyAfterLoss;/* R4.3: insures only after suffering one      */
    InsuranceType houseTier; /* R4.*: cover bought on a house property      */
    InsuranceType hotelTier; /* R4.*: cover bought on a hotel property      */
} Profile;

static const Profile PROFILE[] = {
    /* STRAT_AGGRESSIVE   */ { 120, 75, 10, true,  false, INS_BASIC, INS_COMPREHENSIVE },
    /* STRAT_CONSERVATIVE */ {  90, 90, 10, false, false, INS_COMPREHENSIVE, INS_COMPREHENSIVE },
    /* STRAT_RISKTAKER    */ {  60, 75, 10, true,  false, INS_BASIC, INS_BASIC },
    /* STRAT_OPPORTUNIST  */ {  60, 75, 10, true,  false, INS_BASIC, INS_BASIC }
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

/* ------------------------------------------------------------- purchase -- */

/* Rule 5's choice: take it at the asking price, or decline and send it to
 * auction. The rules have already had their say by the time this is called;
 * what is left is whether this personality wants it at that price.
 */
/* R4.1. Buys whatever it can afford for as long as anyone is left to charge
 * rent to, and spends down to nothing to do it.
 *
 * completes_group and the two coveted squares do not appear as a separate
 * branch because they cannot change the answer: this arm already says yes to
 * everything affordable. They earn their keep in the auction, where there is
 * a ceiling to raise.
 */
static bool buy_aggressive(GameState *g, int p, int sq)
{
    return rent_still_payable(g, p) && g->players[p].cash >= square_value(g, sq);
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

static bool wants_to_buy(GameState *g, int p, int sq)
{
    switch (g->players[p].strat) {
    case STRAT_AGGRESSIVE:
        return buy_aggressive(g, p, sq);

    case STRAT_CONSERVATIVE:
        return buy_conservative(g, p, sq);

    case STRAT_RISKTAKER:
    case STRAT_OPPORTUNIST:
        return g->players[p].cash >= square_value(g, sq);
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

    case STRAT_CONSERVATIVE:
    case STRAT_RISKTAKER:
    case STRAT_OPPORTUNIST:
        break;
    }

    return pct_of(square_value(g, sq), pct);
}

int decide_bid(GameState *g, int p, int sq, int minBid)
{
    int ceiling = bid_ceiling(g, p, sq);

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
    (void)sq;

    if (hotel && !profile(g, p)->hotelsWhileIndebted && g->players[p].loan.active) {
        return false;                            /* R4.2                    */
    }

    switch (g->players[p].strat) {
    case STRAT_AGGRESSIVE:
    case STRAT_CONSERVATIVE:
    case STRAT_RISKTAKER:
    case STRAT_OPPORTUNIST:
        return true;
    }
    return false;
}

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
        if (!wants_to_build(g, p, i, level == MAX_HOUSES)) {
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
        if (g->round + EXTEND_WITHIN_ROUNDS >= pl->loan.issuedRound + pl->loan.termRounds) {
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

static BankAction bank_baseline(GameState *g, int p, int *amount)
{
    const Player *pl = &g->players[p];
    int           capacity;

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

BankAction decide_bank(GameState *g, int p, int *amount)
{
    *amount = 0;

    switch (g->players[p].strat) {
    case STRAT_AGGRESSIVE:
        return bank_aggressive(g, p, amount);

    case STRAT_CONSERVATIVE:
        return bank_conservative(g, p, amount);

    case STRAT_RISKTAKER:
    case STRAT_OPPORTUNIST:
        return bank_baseline(g, p, amount);
    }
    return BANK_NONE;
}
