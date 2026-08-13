/* events.c -- the economy: timed effects, cadenced systems, the card deck.
 *
 * Six systems fire on four different clocks and none of them knows about the
 * others. What makes that tractable is D12: every timed modifier in the game,
 * whatever created it, reduces to one Effect record in one array, and every
 * reader of a modified value goes through one of board.c's four choke points.
 * A boom does not have to know that a national event is also running, and
 * square_value does not have to know that either exists.
 *
 * The alternative -- a flat set of fields on Economy, one per rule -- fails on
 * two of the spec's own requirements. LK 34 makes concurrent effects
 * cumulative, so two systems touching property value must stack rather than
 * overwrite; and Appendix A's cards attach to the player who drew them, so the
 * same kind of modifier can be live for one player and not another. Both fall
 * out of a list of records carrying a scope and an owner.
 */

#include <stdio.h>
#include <stdlib.h>   /* abort(), in the DEBUG overflow guard only */

#include "types.h"

/* ------------------------------------------------------------ the registry */

/* Append a timed modifier. Everything in this file ends up here.
 *
 * A full registry drops the effect rather than corrupting the array. That is
 * the safe failure, but it is still a failure -- an economy quietly missing a
 * boom is harder to diagnose than one that stops -- so the debug build aborts
 * instead. MAX_EFFECTS carries several times the headroom a real game uses:
 * eight from a market review, six from the two 15-round systems, one from a
 * regulation, and a handful of player-scoped cards.
 */
void effect_push(GameState *g, EffectKind kind, EffectScope scopeKind, int scope,
                 int magnitudePct, int owner, int rounds)
{
    Effect *e;

    if (rounds <= 0) {
        return;
    }
    if (g->econ.effectCount >= MAX_EFFECTS) {
#ifdef DEBUG
        fprintf(stderr, "R%d: effect registry full (%d records)\n", g->round, MAX_EFFECTS);
        abort();
#else
        return;
#endif
    }

    e = &g->econ.effects[g->econ.effectCount++];
    e->kind         = kind;
    e->scopeKind    = (int)scopeKind;
    e->scope        = scope;
    e->magnitudePct = magnitudePct;
    e->owner        = owner;
    e->roundsLeft   = rounds;
}

/* Does this record reach this square?
 *
 * square is -1 for the two readings that belong to the economy rather than to
 * any square -- the rate a new loan is written at, and the income tax rate.
 * Only global and player-scoped records can reach those, which is what the
 * switch below already says without needing a special case.
 */
static bool scope_matches(const GameState *g, const Effect *e, int square, int player)
{
    switch ((EffectScope)e->scopeKind) {
    case SCOPE_GLOBAL:
        return true;
    case SCOPE_GROUP:
        return square >= 0 && g->board[square].group == (PropertyGroup)e->scope;
    case SCOPE_REGION:
        return square >= 0 && (g->board[square].regions & (unsigned)e->scope) != 0u;
    case SCOPE_SQUARE:
        return square >= 0 && square == e->scope;
    case SCOPE_PLAYER:
        return player >= 0 && player == e->scope;
    }
    return false;
}

/* Sum the magnitudes of every active effect of this kind that reaches this
 * square and this player, as a signed percentage for apply_pct.
 *
 * LK 34 requires concurrent effects to be cumulative, and a sum is what that
 * means: one rounding at the choke point rather than one per effect, and
 * expiry subtracting exactly what activation added. Chained multiplication
 * would do neither -- +20% followed by -20% would not return to where it
 * started, and the residue would be permanent.
 *
 * player is the participant the value is being read *for* -- the owner of the
 * square, in practice. An effect with owner == -1 reaches everyone; one
 * carrying a real owner reaches only that player's holdings, which is how
 * Appendix A's cards stay attached to whoever drew them.
 */
int effect_modifier(const GameState *g, EffectKind kind, int square, int player)
{
    int i, total = 0;

    for (i = 0; i < g->econ.effectCount; i++) {
        const Effect *e = &g->econ.effects[i];

        if (e->kind != kind) {
            continue;
        }
        if (e->owner >= 0 && e->owner != player) {
            continue;
        }
        if (!scope_matches(g, e, square, player)) {
            continue;
        }
        total += e->magnitudePct;
    }

    return total;
}

/* Age every record by one round and compact the expired out of the array.
 *
 * Removal is the whole of LK 35. "Values revert to the market-adjusted
 * baseline" needs no restore step and no saved copy, because nothing was ever
 * written down: the choke points recompute from the stored value and whatever
 * records are live at the moment they are asked. Dropping the record is the
 * revert.
 *
 * Compacting in place over the same array is safe -- the write index never
 * overtakes the read index, since a record is only ever written to a slot at
 * or before the one it came from.
 *
 * Deviation from D13's written order, which puts this AFTER the round's
 * cadenced systems. Its stated intent is that an effect created this round
 * lives its full stated duration, and running the tick after the cadences
 * defeats exactly that: a 15-round effect pushed and immediately aged is
 * readable for 14 rounds, and would leave the LK 36 block 14 rounds after
 * appearing rather than 15. Running the tick first ages only records that
 * already existed, which is what the note asks for. It is the same rule as
 * "do not age what was created this round", expressed without threading a
 * boundary index through the function.
 */
void tick_effects(GameState *g)
{
    int i, kept = 0;

    for (i = 0; i < g->econ.effectCount; i++) {
        Effect *e = &g->econ.effects[i];

        e->roundsLeft--;
        if (e->roundsLeft > 0) {
            g->econ.effects[kept++] = *e;
        }
    }

    g->econ.effectCount = kept;
}

/* LK 33's 30-round bar on re-selecting a group. A plain countdown rather than
   a stored round number, because the only question ever asked of it is
   "is this group available yet", and zero answers it. */
void tick_cooldowns(GameState *g)
{
    int i;

    for (i = 0; i < GRP_COUNT; i++) {
        if (g->econ.groupCooldown[i] > 0) {
            g->econ.groupCooldown[i]--;
        }
    }
}

/* ------------------------------------------------------------- inflation -- */

/* LK 12's six outcomes, drawn uniformly. A table rather than a formula
   because the set is not an interval -- it skips 1, 3, 4 and everything
   between 8 and 12 -- and because it can be checked against the rule by eye. */
static const int INFLATION_DRAWS[] = { -3, 0, 2, 5, 8, 12 };
#define INFLATION_DRAW_COUNT ((int)(sizeof INFLATION_DRAWS / sizeof INFLATION_DRAWS[0]))

/* LK 12-14 and D12's permanent half.
 *
 * This is the one system in the file that does NOT push a record. LK 14 says
 * the new value replaces the old, so inflation is written into the stored
 * fields and the registry never hears about it -- which is also what makes it
 * compound correctly across draws, since each one scales what the last one
 * left rather than being one more term in a sum.
 *
 * Premiums, repair costs, maintenance and both tax bases need no handling
 * here. They are all derived from square_value and building_cost, so they
 * inflate the moment their inputs do. That is the choke-point pattern paying
 * for itself; the alternative is five more lines here and a sixth forgotten.
 */
void draw_inflation(GameState *g)
{
    int pct = INFLATION_DRAWS[rng_range(0, INFLATION_DRAW_COUNT - 1)];
    int i;

    g->econ.inflationPct = pct;

    for (i = 0; i < NUM_SQUARES; i++) {
        Square *s = &g->board[i];

        /* The 18 non-property squares carry zeros in every one of these and
           stay at zero, so no type test is needed. Railways and utilities
           have a price and a mortgage value and correctly move with them. */
        s->price         = apply_pct(s->price, pct);
        s->baseRent      = apply_pct(s->baseRent, pct);
        s->houseCost     = apply_pct(s->houseCost, pct);
        s->hotelCost     = apply_pct(s->hotelCost, pct);
        s->mortgageValue = apply_pct(s->mortgageValue, pct);
    }

    /* D21 and D2'. Both economy-wide rates move by the same factor. The loan
       rate governs NEW loans only -- every live loan froze its own ratePct at
       issue under LK 13, and the fact that this line cannot reach it is the
       whole reason Loan owns that field. This is the most commonly
       mis-implemented rule in the spec, and here it is correct by
       construction rather than by remembering. */
    g->econ.interestRatePct = apply_pct(g->econ.interestRatePct, pct);
    g->econ.incomeTaxPct    = apply_pct(g->econ.incomeTaxPct, pct);

    /* Section 5 gives no template; this matches the economic-event voice of
       its neighbours. */
    printf("Inflation Rate : %+d%%\n", pct);
    printf("All property values, costs and rents have been recalculated.\n");
}

/* -------------------------------------------------- booms and declines -- */

/* LK 31 and LK 32, as effect specifications rather than code.
 *
 * LK 31 lists "+15% purchase prices" separately from "+20% property values".
 * In this model those are the same number -- a buyer pays square_value, which
 * is what VALUE_MUL scales -- so the two collapse into one record. Listing
 * both would double the effect on the one figure they describe.
 */
static const struct { EffectKind kind; int pct; } BOOM_EFFECTS[] = {
    { EFF_VALUE_MUL,    +20 },
    { EFF_RENT_MUL,     +25 },
    { EFF_MORTGAGE_MUL, +15 },
    { EFF_BUILD_COST_MUL, +10 }
};
#define BOOM_EFFECT_COUNT ((int)(sizeof BOOM_EFFECTS / sizeof BOOM_EFFECTS[0]))

static const struct { EffectKind kind; int pct; } DECLINE_EFFECTS[] = {
    { EFF_VALUE_MUL,        -15 },
    { EFF_RENT_MUL,         -20 },
    { EFF_MORTGAGE_MUL,     -10 },
    { EFF_AUCTION_OPEN_MUL, -25 }
};
#define DECLINE_EFFECT_COUNT ((int)(sizeof DECLINE_EFFECTS / sizeof DECLINE_EFFECTS[0]))

/* Pick a group that LK 33's cooldown allows, that LK 30 did not give this
 * same treatment last review, and that is not already spoken for this review.
 * Returns GRP_NONE when nothing qualifies, which is a real outcome rather
 * than an error: with eight groups and a thirty-round bar on a ten-round
 * cadence, the eligible set genuinely empties from time to time.
 *
 * Reservoir choice over one pass rather than building a candidate array: the
 * nth qualifying group replaces the running pick with probability 1/n, which
 * leaves every candidate equally likely without a second loop or a count.
 */
static PropertyGroup pick_group(GameState *g, PropertyGroup avoidRepeat,
                                PropertyGroup alreadyTaken)
{
    PropertyGroup pick = GRP_NONE;
    int           seen = 0, i;

    for (i = 0; i < GRP_COUNT; i++) {
        PropertyGroup grp = (PropertyGroup)i;

        if (g->econ.groupCooldown[i] > 0) {
            continue;                       /* LK 33                        */
        }
        if (grp == avoidRepeat || grp == alreadyTaken) {
            continue;                       /* LK 30                        */
        }
        seen++;
        if (rng_range(1, seen) == 1) {
            pick = grp;
        }
    }

    return pick;
}

/* LK 30-33, every ten rounds: one group rises, another falls, for ten rounds
 * each. Both are barred from re-selection for thirty (LK 33) and neither may
 * repeat the same treatment it had last time (LK 30).
 *
 * lastBoomGroup and lastDeclineGroup are read before they are written, which
 * is what makes the LK 30 test mean "last review" rather than "this one".
 * They start at GRP_NONE, which is -1 and matches no group, so the first
 * review is unconstrained without needing a special case.
 */
void market_review(GameState *g)
{
    PropertyGroup boom    = pick_group(g, g->econ.lastBoomGroup, GRP_NONE);
    PropertyGroup decline = pick_group(g, g->econ.lastDeclineGroup, boom);
    int           i;

    if (boom != GRP_NONE) {
        for (i = 0; i < BOOM_EFFECT_COUNT; i++) {
            effect_push(g, BOOM_EFFECTS[i].kind, SCOPE_GROUP, (int)boom,
                        BOOM_EFFECTS[i].pct, -1, MARKET_ROUNDS);
        }
        g->econ.groupCooldown[boom] = MARKET_COOLDOWN;
    }

    if (decline != GRP_NONE) {
        for (i = 0; i < DECLINE_EFFECT_COUNT; i++) {
            effect_push(g, DECLINE_EFFECTS[i].kind, SCOPE_GROUP, (int)decline,
                        DECLINE_EFFECTS[i].pct, -1, MARKET_ROUNDS);
        }
        g->econ.groupCooldown[decline] = MARKET_COOLDOWN;
    }

    g->econ.lastBoomGroup    = boom;
    g->econ.lastDeclineGroup = decline;
}

/* ------------------------------------------------- LK 36 block queries -- */
/*
 * game.c owns every block of formatted output, so these answer questions
 * rather than printing. The answers come out of the live registry rather than
 * a set of fields kept alongside it -- there is exactly one place a boom is
 * recorded, so there is no second copy to drift.
 *
 * A SCOPE_GROUP value effect can only have come from a market review: the
 * regional cards scope by square or region and the national events are global
 * or regional, so the sign of the magnitude distinguishes a boom from a
 * decline unambiguously.
 */
static int market_group(const GameState *g, bool boom, int *roundsLeft)
{
    int i;

    for (i = 0; i < g->econ.effectCount; i++) {
        const Effect *e = &g->econ.effects[i];

        if (e->kind != EFF_VALUE_MUL || e->scopeKind != SCOPE_GROUP) {
            continue;
        }
        if ((e->magnitudePct > 0) != boom) {
            continue;
        }
        *roundsLeft = e->roundsLeft;
        return e->scope;
    }

    *roundsLeft = 0;
    return GRP_NONE;
}

int boom_group(const GameState *g, int *roundsLeft)
{
    return market_group(g, true, roundsLeft);
}

int decline_group(const GameState *g, int *roundsLeft)
{
    return market_group(g, false, roundsLeft);
}
