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
#include <string.h>   /* strcmp(), in the DEBUG table-order guard only */

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

    /* D34. A player-scoped record ages by its owner's laps, and the tick at
       the end of this round will subtract every lap the owner completed in
       it -- including the ones before this record existed. Crediting those
       back here makes it age only from the moment it was created, which is
       the same courtesy the global records get from the tick running ahead
       of the cadences. */
    if (owner >= 0) {
        rounds += g->players[owner].laps - g->players[owner].lapsPrev;
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

/* Is any effect of this kind in force here?
 *
 * Three of the effect kinds -- EFF_CLOSED, EFF_CONSTRUCTION_SUSPENDED and the
 * risk weights -- carry no percentage at all. Their presence is their whole
 * meaning, so summing magnitudes would report a two-round shutdown as zero,
 * which reads identically to no shutdown. Callers of those ask this instead.
 */
bool effect_active(const GameState *g, EffectKind kind, int square, int player)
{
    int i;

    for (i = 0; i < g->econ.effectCount; i++) {
        const Effect *e = &g->econ.effects[i];

        if (e->kind != kind) {
            continue;
        }
        if (e->owner >= 0 && e->owner != player) {
            continue;
        }
        if (scope_matches(g, e, square, player)) {
            return true;
        }
    }

    return false;
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
        Effect *e   = &g->econ.effects[i];
        int     age = 1;

        /* D34. A record carrying an owner is that player's alone -- an
           Appendix A card attaches to whoever drew it -- so it ages by what
           that player actually did this round, not by the round itself. A
           global record ages by one, the game round being its clock.
           A bankrupt owner's records are dropped: their laps stop, and an
           effect measured against a stopped clock would never expire. */
        if (e->owner >= 0) {
            if (g->players[e->owner].bankrupt) {
                continue;
            }
            age = g->players[e->owner].laps - g->players[e->owner].lapsPrev;
        }

        e->roundsLeft -= age;
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

    /* D2'. The loan rate used to drift here alongside the tax rate. It no
       longer exists: D21 now reads the rate straight off Table 9 by the
       prevailing condition, and inflation reaches it through that table's
       Moderate and High rows rather than by scaling a stored figure. LK 13's
       "existing loan rates remain unchanged" is still what makes Loan own its
       own frozen ratePct -- this line could never reach it either. */
    g->econ.incomeTaxPct = apply_pct(g->econ.incomeTaxPct, pct);

    /* Section 5 gives no template; this matches the economic-event voice of
       its neighbours. */
    printf("Inflation Rate : %+d%%\n", pct);
    printf("All property values, costs and rents have been recalculated.\n");
    end_block();
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
static int market_group(const GameState *g, bool boom, int *magnitudePct, int *roundsLeft)
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
        *magnitudePct = e->magnitudePct;
        *roundsLeft   = e->roundsLeft;
        return e->scope;
    }

    *magnitudePct = 0;
    *roundsLeft   = 0;
    return GRP_NONE;
}

int boom_group(const GameState *g, int *magnitudePct, int *roundsLeft)
{
    return market_group(g, true, magnitudePct, roundsLeft);
}

int decline_group(const GameState *g, int *magnitudePct, int *roundsLeft)
{
    return market_group(g, false, magnitudePct, roundsLeft);
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

/* D21 keys Table 9 on two of these by identity. Named here, beside the table
   they index, with a DEBUG check in national_event that the rows have not
   been reordered underneath them. */
#define EVENT_RECESSION      3
#define EVENT_STOCK_BOOM     4

/* The inflation draw above which LK 12's reading is High rather than Moderate
   Inflation. Strictly above: a draw of exactly 5 is Moderate (D21). */
#define INFLATION_HIGH_PCT   5

/* The event in force, or -1. Ages exactly as active_card does -- off D19's
   single clock rather than a second counter that could drift from it. */
static int active_national_event(const GameState *g)
{
    if (g->econ.activeEvent < 0 ||
        g->round - g->econ.activeEventRound >= EVENT_ROUNDS) {
        return -1;
    }
    return g->econ.activeEvent;
}

/* D21: which Table 9 row governs a loan written right now.
 *
 * The precedence is the clarification's. Economic Recession and Stock Market
 * Boom are economy-wide conditions and outrank the inflation reading; Table
 * 9's "Economic Boom" is the LK 18 event, not LK 33's per-group Market Boom,
 * which is a property condition and would otherwise be in force most rounds.
 * With neither event running, LK 12's latest draw decides, and a draw at or
 * below zero is not inflation at all -- that is Stable Economy.
 *
 * Every row is reachable because LK 12 draws from a fixed six: -3 and 0 are
 * Stable, 2 and 5 Moderate, 8 and 12 High.
 */
EconomicCondition prevailing_condition(const GameState *g)
{
    int ev = active_national_event(g);

    if (ev == EVENT_RECESSION) {
        return ECON_RECESSION;
    }
    if (ev == EVENT_STOCK_BOOM) {
        return ECON_BOOM;
    }
    if (g->econ.inflationPct > INFLATION_HIGH_PCT) {
        return ECON_HIGH_INFLATION;
    }
    if (g->econ.inflationPct > 0) {
        return ECON_MODERATE_INFLATION;
    }
    return ECON_STABLE;
}

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
    end_block();
}

/* LK 18, every fifteen rounds, affecting everyone.
 *
 * The row is remembered for the same reason regional_card remembers its own:
 * D21 keys Table 9 on the condition, and a condition is an identity that an
 * Effect record cannot carry. INTEREST_MUL stays on Recession and Stock
 * Market Boom because under D21 those two shifts now reach EXISTING loans,
 * whose frozen rate the table never touches.
 */
void national_event(GameState *g)
{
    int idx = rng_range(0, NATIONAL_EVENT_COUNT - 1);

#ifdef DEBUG
    /* The two named indices are the whole basis of prevailing_condition;
       a reordered table would misread the economy in silence. */
    if (strcmp(NATIONAL_EVENTS[EVENT_RECESSION].name, "Economic Recession") != 0 ||
        strcmp(NATIONAL_EVENTS[EVENT_STOCK_BOOM].name, "Stock Market Boom") != 0) {
        fprintf(stderr, "NATIONAL_EVENTS reordered: D21's indices are stale\n");
        abort();
    }
#endif

    g->econ.activeEvent      = idx;
    g->econ.activeEventRound = g->round;
    fire_event(g, &NATIONAL_EVENTS[idx], "Economic Event", "", -1, EVENT_ROUNDS);
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
        end_block();
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

/* Pick a square that matches a predicate over the whole board, or -1. */
static int random_square_where(GameState *g, unsigned regionMask, bool developedOnly)
{
    int i, pick = -1, seen = 0;

    for (i = 0; i < NUM_SQUARES; i++) {
        const Square *s = &g->board[i];

        if (s->owner < 0) {
            continue;
        }
        if (regionMask != 0u && (s->regions & regionMask) == 0u) {
            continue;
        }
        if (developedOnly && development_level(g, i) == 0) {
            continue;
        }
        seen++;
        if (rng_range(1, seen) == 1) {
            pick = i;
        }
    }
    return pick;
}

/* --------------------------------------------- disasters, claims, repairs -- */
/*
 * LK 10-11 and Appendix E. Every ten rounds one developed property is struck,
 * the policy on it pays out if it covers that peril, and the owner pays the
 * repair bill if it does not. Either way the building earns nothing until it
 * is put right.
 */

static const char *DISASTER_NAMES[DIS_COUNT] = {
    "Fire", "Flood", "Riot", "Building Collapse", "Electrical Failure"
};

/* D3, as a matrix, because the spec's two statements of it disagree and a
 * matrix is the only shape in which both can be read at once.
 *
 * LK 10's peril list and Appendix E's coverage table do not line up: Building
 * Collapse and Electrical Failure appear as perils but are covered by no tier
 * below Business Interruption. This follows both literally rather than
 * quietly extending Comprehensive to cover them -- an uninsurable peril is a
 * real thing, and inventing coverage the table does not grant would be the
 * larger liberty. Appendix E's "Earthquake" never occurs, so it has no row.
 *
 * Vandalism is listed under Comprehensive but is not a Disaster this
 * simulation ever rolls, so Comprehensive's fourth column is unreachable.
 * Returns the percentage of the repair the policy meets, or 0 for none.
 */
static int covers(InsuranceType tier, Disaster peril, bool isHotel)
{
    switch (tier) {
    case INS_BASIC:
        return (peril == DIS_FIRE || peril == DIS_FLOOD) ? 80 : 0;

    case INS_COMPREHENSIVE:
        return (peril == DIS_FIRE || peril == DIS_FLOOD || peril == DIS_RIOT)
               ? 100 : 0;

    case INS_BUSINESS:
        /* All perils, but hotel properties only -- the tier insures a
           trading business, and D3 reads a hotel as the thing being traded
           from. A policy on a house-only property covers nothing. */
        return isHotel ? 100 : 0;

    case INS_NONE:
        return 0;
    }
    return 0;
}

/* Choose the peril. Base weight is equal across the five, then Heavy Monsoon
 * and Political Unrest tilt it through the two risk effects.
 *
 * Those two are pushed with +100, which here means "twice as likely" rather
 * than "+100% of something" -- the only place in the program where an effect
 * magnitude is a weight rather than a price. Reading them through
 * effect_modifier rather than as flags on Economy is what makes them expire
 * on their own when the event does.
 */
static Disaster pick_peril(const GameState *g)
{
    int weight[DIS_COUNT];
    int i, total = 0, roll;

    for (i = 0; i < DIS_COUNT; i++) {
        weight[i] = 100;
    }
    weight[DIS_FLOOD] += effect_modifier(g, EFF_FLOOD_RISK, -1, -1);
    weight[DIS_RIOT]  += effect_modifier(g, EFF_RIOT_RISK, -1, -1);

    for (i = 0; i < DIS_COUNT; i++) {
        if (weight[i] < 0) {
            weight[i] = 0;
        }
        total += weight[i];
    }
    if (total <= 0) {
        return DIS_FIRE;
    }

    roll = rng_range(1, total);
    for (i = 0; i < DIS_COUNT; i++) {
        roll -= weight[i];
        if (roll <= 0) {
            return (Disaster)i;
        }
    }
    return DIS_FIRE;
}

/* LK 10, every ten rounds. Strikes one developed property at random.
 *
 * D20 is the clause easiest to miss and hardest to spot afterwards: a payout
 * CONSUMES the policy whatever rounds remain on it. A property struck twice
 * is insured for the first only. The policy is cleared before the claim block
 * prints, so there is no path through this function that pays twice.
 */
void fire_disaster(GameState *g)
{
    char     b[FMT_BUF];
    int      sq = random_square_where(g, 0u, true);
    Square  *s;
    Disaster peril;
    int      cost, coverPct, payout;

    if (sq < 0) {
        return;                    /* nothing developed yet to strike       */
    }

    s     = &g->board[sq];
    peril = pick_peril(g);
    cost  = repair_cost(g, sq);

    s->damaged = true;                       /* LK 11: earns nothing now   */
    g->players[s->owner].sufferedLoss = true; /* the Risk Taker's trigger  */

    printf("%s occurred.\n", DISASTER_NAMES[peril]);
    printf("\n");
    printf("Affected Property :\n");
    printf("%s.\n", s->name);
    printf("\n");

    coverPct = covers(s->policy, peril, s->hotel);
    if (coverPct == 0) {
        /* Uninsured, or insured against something else. The owner meets the
           bill themselves, through charge -- so an owner who cannot afford
           it goes down the D11 ladder like any other debt. */
        printf("No claim is payable.\n");
        if (charge(g, s->owner, cost, -1)) {
            printf("Repair Cost :\n");
            printf("LKR %s.\n", fmt_lkr(b, cost));
        }
        end_block();
        return;
    }

    payout = pct_of(cost, coverPct);

    /* D3: Business Interruption also pays five rounds of lost hotel rent, as
       an immediate lump sum. Read before the policy is cleared, and computed
       off the undamaged rent -- square_rent would return zero now that the
       damaged flag is set, which is the loss being compensated. */
    if (s->policy == INS_BUSINESS) {
        s->damaged = false;
        payout += square_rent(g, sq, 0) * BI_RENT_ROUNDS;
        s->damaged = true;
    }

    /* D20, before the block prints: the policy is spent. */
    s->policy       = INS_NONE;
    s->policyWarned = false;

    credit(g, s->owner, payout);
    printf("Insurance Claim Approved.\n");
    printf("Compensation Paid :\n");
    printf("LKR %s.\n", fmt_lkr(b, payout));
    end_block();
}

/* LK 11, once per round. Damage is a pause on the income, not the end of it:
 * any owner who can cover the bill pays it and the building earns again.
 *
 * Cash is tested first rather than left to charge, on the usual principle --
 * this is the owner choosing to repair, and a player who would have to sell
 * buildings to fix a building is not in a position to fix it. The square
 * simply stays damaged until they are.
 */
void auto_repairs(GameState *g)
{
    char b[FMT_BUF];
    int  i;

    for (i = 0; i < NUM_SQUARES; i++) {
        Square *s = &g->board[i];
        int     cost;

        if (!s->damaged || s->owner < 0) {
            continue;
        }

        cost = repair_cost(g, i);
        if (g->players[s->owner].cash < cost || !charge(g, s->owner, cost, -1)) {
            continue;
        }

        s->damaged = false;
        printf("%s repaired %s.\n", g->players[s->owner].name, s->name);
        printf("Repair Cost : LKR %s.\n", fmt_lkr(b, cost));
        end_block();
    }
}

/* ------------------------------------------- the National Event Card deck -- */
/*
 * Appendix A. Twenty cards, drawn only by landing on square 7, 22 or 36 --
 * square 2 levies rather than draws (D17), so there are three card squares
 * and not four. This is a separate system from LK 18's national events
 * despite the similar name: those fire on a clock and reach everyone, these
 * are drawn by one player and attach to that player.
 *
 * "Returned to the bottom of the deck" is a circular queue over a fixed
 * array: nothing moves, the head index advances, and the twentieth draw wraps
 * to the first card again. O(1) per draw, no shifting, and no allocation
 * (R0.5). Because the array is shuffled once and never reordered, every card
 * appears exactly once before any repeats -- which is what "returned to the
 * bottom" is for.
 */

typedef enum {
    CARD_TOURISM_HYPE, CARD_FUEL_SHORTAGE, CARD_HEAVY_FLOODS, CARD_POLITICAL_RALLY,
    CARD_STOCK_RISE, CARD_DOWNTURN, CARD_HOUSING_SUBSIDY, CARD_RATE_CUT,
    CARD_RATE_RISE, CARD_TAX_AMNESTY, CARD_POWER_FAILURE, CARD_FOREIGN_FUNDING,
    CARD_PORT_EXPANSION, CARD_FESTIVAL, CARD_LABOUR_STRIKE, CARD_INSURANCE_DISCOUNT,
    CARD_REVALUATION, CARD_CURRENCY_DEPRECIATION, CARD_GOVERNMENT_GRANT,
    CARD_NATIONAL_DISASTER,
    CARD_COUNT
} EventCardId;

static const char *CARD_NAMES[CARD_COUNT] = {
    "Tourism Hype", "Fuel Shortage", "Heavy Floods", "Political Rally",
    "Stock Market Rise", "Economic Downturn", "Housing Subsidy", "Interest Rate Cut",
    "Interest Rate Increase", "Tax Amnesty", "Power Failure", "Foreign Funding",
    "Port Expansion", "Festival Season", "Labour Strike", "Insurance Discount",
    "Property Revaluation", "Currency Depreciation", "Government Grant",
    "National Disaster"
};

/* Every card announces itself in the same three-line shape as an economic
   event: heading, name, what it does. The cards that pick a target when they
   fire add a fourth line naming it, since "a random coastal property" is not
   something a static string can say. */
static const char *CARD_DETAILS[CARD_COUNT] = {
    "Your hotel rents double for five rounds.",
    "Your railway rents double for five rounds.",
    "Floods strike the coast.",
    "One of your properties is shut by a rally.",
    "Your property values rise by 10%.",
    "Your property values fall by 15%.",
    "Your construction costs fall by 30%.",
    "Your loan interest falls by 2 percentage points.",
    "Your loan interest rises by 2 percentage points.",
    "Every player receives a payment from the treasury.",
    "Your utility rents halve for three rounds.",
    "Your commercial property values rise by 15%.",
    "Your railway values rise by 20%.",
    "Your hotel rents rise by 50%.",
    "Your construction is suspended for two rounds.",
    "Your premiums fall by 20%.",
    "One colour group rises in value by 15%.",
    "Your construction costs rise by 10%.",
    "A government grant is awarded.",
    "Disaster strikes a developed property."
};

#define CARD_GRANT_AMOUNT   5000
#define CARD_AMNESTY_AMOUNT 2000

/* Fisher-Yates over the fixed array, called once from game_init.
 *
 * Drawing an index at random on every draw would be simpler and wrong: it
 * would let the same card come up twice before others had appeared at all,
 * and Appendix A's return-to-the-bottom rule exists precisely to stop that.
 * Shuffling once and walking the order preserves it.
 */
void deck_init(GameState *g)
{
    int i;

    for (i = 0; i < DECK_SIZE; i++) {
        g->deck.cards[i] = i;
    }
    for (i = DECK_SIZE - 1; i > 0; i--) {
        int j   = rng_range(0, i);
        int tmp = g->deck.cards[i];

        g->deck.cards[i] = g->deck.cards[j];
        g->deck.cards[j] = tmp;
    }
    g->deck.head = 0;
}

/* Pick a square p owns, or -1. Reservoir choice again, for the two cards that
   strike somewhere of the drawer's rather than somewhere of the board's. */
static int random_owned_square(GameState *g, int p)
{
    int i, pick = -1, seen = 0;

    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].owner != p) {
            continue;
        }
        seen++;
        if (rng_range(1, seen) == 1) {
            pick = i;
        }
    }
    return pick;
}

/* Mark a property damaged (LK 11).
 *
 * Deliberately only the flag. Milestone 5 owns what damage MEANS -- the rent
 * guard that stops a damaged building earning, and the automatic repair that
 * lifts it again -- and those two have to arrive together. Honouring the flag
 * here, before repairs exist, would kill the property for the rest of the
 * game rather than until its owner could afford the work.
 */
static void damage_square(GameState *g, int sq)
{
    if (sq < 0) {
        return;                    /* nothing developed to strike   */
    }

    /* Damage attaches to BUILDINGS, never to a bare lot. LK 11 is the
       operative sentence -- "damaged buildings cannot collect rent" --
       and LK 10 says the same from the other side, striking only
       developed property. A vacant lot has nothing to damage and
       nothing to repair: D1 prices repair off the buildings standing,
       so admitting one produced a repair that cost nothing and
       restored nothing. Both callers select developed squares; this
       guard makes that a property of the function rather than of the
       two call sites. */
#ifdef DEBUG
    if (!g->board[sq].hotel && g->board[sq].houses == 0) {
        fprintf(stderr, "R%d: damage on undeveloped square %d\n", g->round, sq);
        abort();
    }
#endif

    g->board[sq].damaged = true;
    printf("%s has been damaged.\n", g->board[sq].name);
}

/* Appendix A's twenty, as a switch rather than a table.
 *
 * The board-wide systems above are table rows because they differ only in
 * their numbers. These do not: five of them act immediately instead of
 * pushing anything, three choose a target at random when they fire, and the
 * durations range from two rounds to fifteen. A table would need a column per
 * exception and a switch to read them, which is the switch below plus a
 * table.
 *
 * Everything pushed here carries owner = p. That is what makes a card the
 * drawer's rather than the board's: effect_modifier will only apply it when
 * reading a value for that player, so Tourism Hype lifts the hotel rents this
 * player collects and nobody else's.
 */
static void apply_card(GameState *g, int p, int card)
{
    char b[FMT_BUF];
    int  i, sq;

    switch ((EventCardId)card) {
    case CARD_TOURISM_HYPE:
        effect_push(g, EFF_HOTEL_RENT_MUL, SCOPE_GLOBAL, 0, +100, p, 5);
        break;
    case CARD_FUEL_SHORTAGE:
        effect_push(g, EFF_RAILWAY_RENT_MUL, SCOPE_GLOBAL, 0, +100, p, 5);
        break;
    case CARD_HEAVY_FLOODS:
        damage_square(g, random_square_where(g, REGION_COASTAL, true));
        break;
    case CARD_POLITICAL_RALLY:
        sq = random_owned_square(g, p);
        if (sq >= 0) {
            effect_push(g, EFF_CLOSED, SCOPE_SQUARE, sq, 0, p, 2);
            printf("%s is closed for two rounds.\n", g->board[sq].name);
        }
        break;
    case CARD_STOCK_RISE:
        effect_push(g, EFF_VALUE_MUL, SCOPE_GLOBAL, 0, +10, p, CARD_ROUNDS);
        break;
    case CARD_DOWNTURN:
        effect_push(g, EFF_VALUE_MUL, SCOPE_GLOBAL, 0, -15, p, CARD_ROUNDS);
        break;
    case CARD_HOUSING_SUBSIDY:
        effect_push(g, EFF_BUILD_COST_MUL, SCOPE_GLOBAL, 0, -30, p, CARD_ROUNDS);
        break;
    case CARD_RATE_CUT:
        /* D21: percentage points, not a relative shift. */
        effect_push(g, EFF_INTEREST_ADD, SCOPE_GLOBAL, 0, -2, p, CARD_ROUNDS);
        break;
    case CARD_RATE_RISE:
        effect_push(g, EFF_INTEREST_ADD, SCOPE_GLOBAL, 0, +2, p, CARD_ROUNDS);
        break;
    case CARD_TAX_AMNESTY:
        for (i = 0; i < NUM_PLAYERS; i++) {
            if (!g->players[i].bankrupt) {
                credit(g, i, CARD_AMNESTY_AMOUNT);
            }
        }
        printf("Every player receives LKR %s.\n", fmt_lkr(b, CARD_AMNESTY_AMOUNT));
        break;
    case CARD_POWER_FAILURE:
        effect_push(g, EFF_UTILITY_RENT_MUL, SCOPE_GLOBAL, 0, -50, p, 3);
        break;
    case CARD_FOREIGN_FUNDING:
        effect_push(g, EFF_VALUE_MUL, SCOPE_REGION, (int)REGION_COMMERCIAL, +15, p,
                    CARD_ROUNDS);
        break;
    case CARD_PORT_EXPANSION:
        /* The four railways, named individually. D14's COMMERCIAL tag also
           covers Pettah, Maradana and Galle Face, so it is the wrong reach
           for a card about stations. */
        for (i = 0; i < NUM_SQUARES; i++) {
            if (g->board[i].type == SQ_RAILWAY) {
                effect_push(g, EFF_VALUE_MUL, SCOPE_SQUARE, i, +20, p, CARD_ROUNDS);
            }
        }
        break;
    case CARD_FESTIVAL:
        effect_push(g, EFF_HOTEL_RENT_MUL, SCOPE_GLOBAL, 0, +50, p, CARD_ROUNDS);
        break;
    case CARD_LABOUR_STRIKE:
        effect_push(g, EFF_CONSTRUCTION_SUSPENDED, SCOPE_GLOBAL, 0, 0, p, 2);
        break;
    case CARD_INSURANCE_DISCOUNT:
        effect_push(g, EFF_PREMIUM_MUL, SCOPE_GLOBAL, 0, -20, p, CARD_ROUNDS);
        break;
    case CARD_REVALUATION:
        effect_push(g, EFF_VALUE_MUL, SCOPE_GROUP, rng_range(0, GRP_COUNT - 1), +15, p,
                    CARD_ROUNDS);
        break;
    case CARD_CURRENCY_DEPRECIATION:
        effect_push(g, EFF_BUILD_COST_MUL, SCOPE_GLOBAL, 0, +10, p, CARD_ROUNDS);
        break;
    case CARD_GOVERNMENT_GRANT:
        i = rng_range(0, NUM_PLAYERS - 1);
        credit(g, i, CARD_GRANT_AMOUNT);
        printf("%s receives LKR %s.\n", g->players[i].name,
               fmt_lkr(b, CARD_GRANT_AMOUNT));
        break;
    case CARD_NATIONAL_DISASTER:
        damage_square(g, random_square_where(g, 0u, true));
        break;
    case CARD_COUNT:
        break;
    }
}

/* Squares 7, 22 and 36. Read the top card, advance the head, execute. */
void draw_event_card(GameState *g, int p)
{
    int card = g->deck.cards[g->deck.head];

    g->deck.head = (g->deck.head + 1) % DECK_SIZE;

    printf("National Event Card\n");
    printf("%s\n", CARD_NAMES[card]);
    printf("%s\n", CARD_DETAILS[card]);
    apply_card(g, p, card);
    end_block();
}
