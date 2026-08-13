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

/* ---------------------------------------- national events, Table 4 cards -- */
/*
 * Two systems on the same fifteen-round clock, and the reason they are two
 * rather than one is scope. LK 18's national events are the weather over the
 * whole board; Table 4's regional cards name particular squares and stand for
 * a development somewhere specific. They fire together, national first.
 *
 * Both reduce to the same shape -- a name, a line of prose, and up to three
 * effect specifications -- so both use the table below and neither needs code
 * of its own beyond choosing a row.
 */

typedef struct {
    EffectKind kind;
    int        scopeKind;      /* an EffectScope                            */
    int        scope;          /* group, region mask, or square index       */
    int        magnitudePct;
} EffectSpec;

typedef struct {
    const char *name;
    const char *detail;        /* the third line of the announcement        */
    EffectSpec  eff[3];
    int         effCount;
} EconomicEvent;

/* LK 18. FLOOD_RISK and RIOT_RISK carry no percentage anyone reads yet --
   milestone 5's disaster roll takes them as peril weights, which is why they
   are effects rather than flags on Economy: they expire on their own. */
static const EconomicEvent NATIONAL_EVENTS[] = {
    { "Tourism Boom",
      "Southern Province properties increase in value by 15%.",
      { { EFF_HOTEL_RENT_MUL, SCOPE_GLOBAL, 0, +100 },
        { EFF_VALUE_MUL, SCOPE_REGION, (int)REGION_SOUTHERN_COASTAL, +15 } }, 2 },

    { "Fuel Crisis",
      "Railway rents double and construction costs rise by 20%.",
      { { EFF_RAILWAY_RENT_MUL, SCOPE_GLOBAL, 0, +100 },
        { EFF_BUILD_COST_MUL, SCOPE_GLOBAL, 0, +20 } }, 2 },

    { "Heavy Monsoon",
      "Coastal property values fall by 10% and premiums rise by 20%.",
      { { EFF_PREMIUM_MUL, SCOPE_GLOBAL, 0, +20 },
        { EFF_VALUE_MUL, SCOPE_REGION, (int)REGION_COASTAL, -10 },
        { EFF_FLOOD_RISK, SCOPE_GLOBAL, 0, +100 } }, 3 },

    { "Economic Recession",
      "Property values fall by 15% and rents by 10%.",
      { { EFF_VALUE_MUL, SCOPE_GLOBAL, 0, -15 },
        { EFF_RENT_MUL, SCOPE_GLOBAL, 0, -10 },
        { EFF_INTEREST_MUL, SCOPE_GLOBAL, 0, +15 } }, 3 },

    { "Stock Market Boom",
      "Property values rise by 10% and borrowing gets cheaper.",
      { { EFF_VALUE_MUL, SCOPE_GLOBAL, 0, +10 },
        { EFF_INTEREST_MUL, SCOPE_GLOBAL, 0, -10 } }, 2 },

    { "Government Housing Programme",
      "Construction costs fall by 25%.",
      { { EFF_BUILD_COST_MUL, SCOPE_GLOBAL, 0, -25 } }, 1 },

    { "Foreign Investment",
      "Commercial property values rise by 20%.",
      { { EFF_VALUE_MUL, SCOPE_REGION, (int)REGION_COMMERCIAL, +20 } }, 1 },

    { "Political Unrest",
      "Hotel rents fall by half.",
      { { EFF_HOTEL_RENT_MUL, SCOPE_GLOBAL, 0, -50 },
        { EFF_RIOT_RISK, SCOPE_GLOBAL, 0, +100 } }, 2 }
};
#define NATIONAL_EVENT_COUNT ((int)(sizeof NATIONAL_EVENTS / sizeof NATIONAL_EVENTS[0]))

/* Table 4's twelve regional cards. A card naming several squares pushes one
   SCOPE_SQUARE record per square; one whose reach a D14 tag captures exactly
   pushes a single SCOPE_REGION record instead. Both readings are the same to
   effect_modifier, so the choice is only about how many records it takes. */
static const EconomicEvent REGIONAL_CARDS[] = {
    { "Southern Tourism Boom", "Rents in the deep south rise by 40%.",
      { { EFF_RENT_MUL, SCOPE_SQUARE, 26, +40 },
        { EFF_RENT_MUL, SCOPE_SQUARE, 27, +40 },
        { EFF_RENT_MUL, SCOPE_SQUARE, 29, +40 } }, 3 },

    { "Port City Expansion", "Colombo values rise by 25%.",
      { { EFF_VALUE_MUL, SCOPE_SQUARE, 1, +25 },
        { EFF_VALUE_MUL, SCOPE_SQUARE, 3, +25 },
        { EFF_VALUE_MUL, SCOPE_SQUARE, 5, +25 } }, 3 },

    { "IT Industry Growth", "Suburban values rise by 20%.",
      { { EFF_VALUE_MUL, SCOPE_SQUARE, 13, +20 },
        { EFF_VALUE_MUL, SCOPE_SQUARE, 11, +20 },
        { EFF_VALUE_MUL, SCOPE_SQUARE, 14, +20 } }, 3 },

    { "Northern Development Programme", "Northern values rise by 30%.",
      { { EFF_VALUE_MUL, SCOPE_SQUARE, 31, +30 },
        { EFF_VALUE_MUL, SCOPE_SQUARE, 32, +30 },
        { EFF_VALUE_MUL, SCOPE_SQUARE, 34, +30 } }, 3 },

    { "Tea Export Boom", "Nuwara Eliya rises in value by 35%.",
      { { EFF_VALUE_MUL, SCOPE_SQUARE, 37, +35 } }, 1 },

    { "Airport Expansion", "Rents around the airport rise by 30%.",
      { { EFF_RENT_MUL, SCOPE_SQUARE, 16, +30 },
        { EFF_RENT_MUL, SCOPE_SQUARE, 18, +30 },
        { EFF_RENT_MUL, SCOPE_SQUARE, 19, +30 } }, 3 },

    { "University City Growth", "Kandy values rise by 20%.",
      { { EFF_VALUE_MUL, SCOPE_SQUARE, 23, +20 },
        { EFF_VALUE_MUL, SCOPE_SQUARE, 21, +20 } }, 2 },

    { "Beach Pollution", "Southern coastal rents fall by 30%.",
      { { EFF_RENT_MUL, SCOPE_REGION, (int)REGION_SOUTHERN_COASTAL, -30 } }, 1 },

    { "Flood Damage", "Coastal values fall by 20%.",
      { { EFF_VALUE_MUL, SCOPE_REGION, (int)REGION_COASTAL, -20 } }, 1 },

    { "Transport Strike", "Railway rents fall by 40%.",
      { { EFF_RAILWAY_RENT_MUL, SCOPE_GLOBAL, 0, -40 } }, 1 },

    { "Electricity Tariff Increase", "Utility rents rise by 25%.",
      { { EFF_UTILITY_RENT_MUL, SCOPE_GLOBAL, 0, +25 } }, 1 },

    { "Water Shortage", "Water board rents rise while nearby values fall.",
      { { EFF_UTILITY_RENT_MUL, SCOPE_SQUARE, 28, +20 },
        { EFF_VALUE_MUL, SCOPE_REGION, (int)REGION_NWSDB_ADJACENT, -10 } }, 2 }
};
#define REGIONAL_CARD_COUNT ((int)(sizeof REGIONAL_CARDS / sizeof REGIONAL_CARDS[0]))

/* Push a row's whole effect list and print its three-line announcement.
 *
 * owner is -1 for the board-wide systems and a player for Appendix A's cards,
 * which reuse this in 4.6. suffix is section 5's " Introduced." on a
 * regulation and "" everywhere else -- a difference in the template rather
 * than in the row, so it belongs at the print rather than in the table.
 */
static void fire_event(GameState *g, const EconomicEvent *ev, const char *heading,
                       const char *suffix, int owner, int rounds)
{
    int i;

    for (i = 0; i < ev->effCount; i++) {
        effect_push(g, ev->eff[i].kind, (EffectScope)ev->eff[i].scopeKind,
                    ev->eff[i].scope, ev->eff[i].magnitudePct, owner, rounds);
    }

    printf("%s\n", heading);
    printf("%s%s\n", ev->name, suffix);
    printf("%s\n", ev->detail);
}

/* LK 18, every fifteen rounds, affecting everyone.
 *
 * D21 is why Economic Recession carries INTEREST_MUL rather than an additive
 * shift: the rule's "+15%" is relative, so 8% becomes 9%, which is the figure
 * section 5's own sample block prints.
 */
void national_event(GameState *g)
{
    const EconomicEvent *ev = &NATIONAL_EVENTS[rng_range(0, NATIONAL_EVENT_COUNT - 1)];

    fire_event(g, ev, "Economic Event", "", -1, EVENT_ROUNDS);
}

/* Table 4, every fifteen rounds. The chosen row is remembered because the
   LK 36 block has to name it and an Effect record carries no name; the rounds
   remaining are recomputed from D19's clock rather than counted twice. */
void regional_card(GameState *g)
{
    int idx = rng_range(0, REGIONAL_CARD_COUNT - 1);

    g->econ.activeCard      = idx;
    g->econ.activeCardRound = g->round;
    fire_event(g, &REGIONAL_CARDS[idx], "Regional Development", "", -1, CARD_ROUNDS);
}

/* The LK 36 block's third section. NULL when nothing is in force.
 *
 * The magnitude returned is the first effect's, which is the one the card is
 * named for -- every multi-effect row here either repeats one magnitude
 * across several squares or leads with its headline figure.
 */
const char *active_card(const GameState *g, int *magnitudePct, int *roundsLeft)
{
    int left;

    if (g->econ.activeCard < 0) {
        return NULL;
    }

    left = CARD_ROUNDS - (g->round - g->econ.activeCardRound);
    if (left <= 0) {
        return NULL;
    }

    *magnitudePct = REGIONAL_CARDS[g->econ.activeCard].eff[0].magnitudePct;
    *roundsLeft   = left;
    return REGIONAL_CARDS[g->econ.activeCard].name;
}

/* ------------------------------------------------ government regulations -- */

/* LK 24's eight, every twenty rounds, each lasting twenty.
 *
 * "Replacing the previous one" needs no code. The cadence and the duration
 * are the same number, so tick_effects has already dropped the outgoing
 * regulation's record by the time this runs -- the tick fires ahead of the
 * cadences. That coincidence is worth naming rather than relying on silently,
 * which is why REGULATION_EVERY and REGULATION_ROUNDS are separate constants
 * that happen to be equal: if either moved, this would need a real removal.
 *
 * Luxury Property Tax has no row here because D24 makes it a one-off charge
 * on activation rather than a standing modifier. It is handled below.
 */
static const EconomicEvent REGULATIONS[] = {
    { "Increase Property Tax", "Income tax rises by half.",
      { { EFF_TAX_MUL, SCOPE_GLOBAL, 0, +50 } }, 1 },

    { "Reduce Loan Interest", "Loan interest falls by 2 percentage points.",
      { { EFF_INTEREST_ADD, SCOPE_GLOBAL, 0, -2 } }, 1 },

    { "Housing Subsidy", "Construction costs reduced by 30%.",
      { { EFF_BUILD_COST_MUL, SCOPE_GLOBAL, 0, -30 } }, 1 },

    /* D24: charged once on activation, not a standing modifier at all. The
       zero effect count is what says so. */
    { "Luxury Property Tax", "Hotel properties are levied 25% of their value.",
      { { EFF_VALUE_MUL, SCOPE_GLOBAL, 0, 0 } }, 0 },

    { "Railway Modernization", "Railway rents rise by 25%.",
      { { EFF_RAILWAY_RENT_MUL, SCOPE_GLOBAL, 0, +25 } }, 1 },

    { "Electricity Tariff Revision", "Utility rents rise by 20%.",
      { { EFF_UTILITY_RENT_MUL, SCOPE_GLOBAL, 0, +20 } }, 1 },

    { "Insurance Regulation", "Premiums fall by 15%, coverage unchanged.",
      { { EFF_PREMIUM_MUL, SCOPE_GLOBAL, 0, -15 } }, 1 },

    { "Anti-Speculation Act", "No player may hold more than three undeveloped properties.",
      { { EFF_MAX_PROPERTIES, SCOPE_GLOBAL, 0, ANTI_SPEC_CAP } }, 1 }
};
#define REGULATION_COUNT ((int)(sizeof REGULATIONS / sizeof REGULATIONS[0]))
#define REG_LUXURY_TAX 3

/* D24. LK 24 calls this tax annual but gives no per-round cadence to work
 * from, unlike LK 4, which is why D4 could take that rule literally and this
 * one cannot. A regulation runs about twenty rounds, so charging once on
 * activation is the closest available reading of "annual".
 *
 * The base is the property's value INCLUDING its buildings, which is the only
 * reading under which "luxury" means anything -- a hotel is the luxury being
 * taxed, and square_value alone would ignore it.
 */
static void levy_luxury_tax(GameState *g)
{
    char b[FMT_BUF];
    int  sq;

    for (sq = 0; sq < NUM_SQUARES; sq++) {
        const Square *s = &g->board[sq];
        int           base, due;

        if (!s->hotel || s->owner < 0) {
            continue;
        }

        base = square_value(g, sq) + building_cost(g, sq, true);
        due  = pct_of(base, 25);

        printf("%s is levied on %s.\n", g->players[s->owner].name, s->name);
        if (charge(g, s->owner, due, -1)) {
            printf("Luxury Tax Paid : LKR %s.\n", fmt_lkr(b, due));
        }
    }
}

/* LK 24, every twenty rounds. */
void government_regulation(GameState *g)
{
    int idx = rng_range(0, REGULATION_COUNT - 1);

    g->econ.activeRegulation = idx;
    fire_event(g, &REGULATIONS[idx], "Government Regulation", " Introduced.", -1, REGULATION_ROUNDS);

    if (idx == REG_LUXURY_TAX) {
        levy_luxury_tax(g);
    }
}
