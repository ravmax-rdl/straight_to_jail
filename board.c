/* board.c -- the board itself: its layout, its randomness, and the movement
 * and valuation queries every other module asks of it.
 */

#include <stdlib.h>
#include <string.h>

#include "types.h"

/* ------------------------------------------------------------ the tables --
 *
 * D18 splits the two value sources, and keeping them in separate tables is
 * what stops them being confused at a call site:
 *
 *   GROUP_VALUES     Appendix B. Supplies construction costs and mortgage
 *                    value. Its basePrice column is the loan-calculation
 *                    basis named by the clarification -- no buyer is ever
 *                    charged it. Note it equals the cheapest member of each
 *                    group, which is a useful cross-check on the CSV.
 *
 *   PROPERTY_VALUES  Rent.csv. Supplies the INDIVIDUAL purchase price and
 *                    base rent actually charged. This is D7': base rent is
 *                    not 10% of anything -- Pettah's 100 on a 1,500 price is
 *                    6.7%, Galle Face's 1,200 on 12,000 is 10%.
 */

typedef struct { int basePrice, house, hotel, mortgage; } GroupValues;

static const GroupValues GROUP_VALUES[GRP_COUNT] = {
    /* BROWN     */ {  1500,  500,  2000,  750 },
    /* LIGHTBLUE */ {  2500,  750,  3000, 1250 },
    /* PINK      */ {  3500, 1000,  4000, 1750 },
    /* ORANGE    */ {  4500, 1250,  5000, 2250 },
    /* RED       */ {  5500, 1500,  6000, 2750 },
    /* YELLOW    */ {  6500, 2000,  8000, 3250 },
    /* GREEN     */ {  8000, 2500, 10000, 4000 },
    /* DARKBLUE  */ { 10000, 3000, 12000, 5000 }
};

typedef struct { int sq, price, baseRent; } PropertyValues;

static const PropertyValues PROPERTY_VALUES[NUM_PROPERTIES] = {
    {  1,  1500,  100 }, {  3,  1800,  120 },
    {  6,  2500,  180 }, {  8,  2700,  200 }, {  9,  3000,  220 },
    { 11,  3500,  260 }, { 13,  3800,  280 }, { 14,  4000,  300 },
    { 16,  4500,  350 }, { 18,  4700,  370 }, { 19,  5000,  400 },
    { 21,  5500,  450 }, { 23,  5800,  480 }, { 24,  6000,  500 },
    { 26,  6500,  600 }, { 27,  6800,  620 }, { 29,  7000,  650 },
    { 31,  8000,  750 }, { 32,  8300,  780 }, { 34,  8500,  800 },
    { 37, 10000, 1000 }, { 39, 12000, 1200 }
};

/* Table 1, plus the D14 region tags the economic events need. The tags are a
   bitmask because squares belong to several regions at once: Trincomalee is
   northern, eastern and coastal, and a Heavy Monsoon hits it as coastal
   while a Northern Development Programme hits it as northern. */
typedef struct {
    SquareType    type;
    const char   *name;
    PropertyGroup group;
    unsigned      regions;
} SquareLayout;

static const SquareLayout LAYOUT[NUM_SQUARES] = {
/*  0 */ { SQ_GO,        "GO",                                       GRP_NONE,      0u },
/*  1 */ { SQ_PROPERTY,  "Pettah",                                   GRP_BROWN,     REGION_WESTERN | REGION_COMMERCIAL },
/*  2 */ { SQ_COMMUNITY, "Community Development Fund",               GRP_NONE,      0u },
/*  3 */ { SQ_PROPERTY,  "Maradana",                                 GRP_BROWN,     REGION_WESTERN | REGION_COMMERCIAL },
/*  4 */ { SQ_TAX,       "Income Tax",                               GRP_NONE,      0u },
/*  5 */ { SQ_RAILWAY,   "Colombo Fort Railway Station",             GRP_NONE,      REGION_COMMERCIAL },
/*  6 */ { SQ_PROPERTY,  "Bambalapitiya",                            GRP_LIGHTBLUE, REGION_WESTERN | REGION_COASTAL },
/*  7 */ { SQ_EVENT,     "National Event Card",                      GRP_NONE,      0u },
/*  8 */ { SQ_PROPERTY,  "Wellawatte",                               GRP_LIGHTBLUE, REGION_WESTERN | REGION_COASTAL },
/*  9 */ { SQ_PROPERTY,  "Mount Lavinia",                            GRP_LIGHTBLUE, REGION_WESTERN | REGION_COASTAL },
/* 10 */ { SQ_JAIL,      "Jail / Just Visiting",                     GRP_NONE,      0u },
/* 11 */ { SQ_PROPERTY,  "Nugegoda",                                 GRP_PINK,      REGION_WESTERN },
/* 12 */ { SQ_UTILITY,   "Ceylon Electricity Board",                 GRP_NONE,      0u },
/* 13 */ { SQ_PROPERTY,  "Maharagama",                               GRP_PINK,      REGION_WESTERN },
/* 14 */ { SQ_PROPERTY,  "Kottawa",                                  GRP_PINK,      REGION_WESTERN },
/* 15 */ { SQ_RAILWAY,   "Kandy Railway Station",                    GRP_NONE,      REGION_COMMERCIAL },
/* 16 */ { SQ_PROPERTY,  "Negombo",                                  GRP_ORANGE,    REGION_WESTERN | REGION_COASTAL },
/* 17 */ { SQ_INSURANCE, "Sri Lanka Insurance",                      GRP_NONE,      0u },
/* 18 */ { SQ_PROPERTY,  "Katunayake",                               GRP_ORANGE,    REGION_WESTERN },
/* 19 */ { SQ_PROPERTY,  "Ja-Ela",                                   GRP_ORANGE,    REGION_WESTERN },
/* 20 */ { SQ_PARKING,   "Free Parking",                             GRP_NONE,      0u },
/* 21 */ { SQ_PROPERTY,  "Kandy City",                               GRP_RED,       REGION_CENTRAL },
/* 22 */ { SQ_EVENT,     "National Event Card",                      GRP_NONE,      0u },
/* 23 */ { SQ_PROPERTY,  "Peradeniya",                               GRP_RED,       REGION_CENTRAL },
/* 24 */ { SQ_PROPERTY,  "Katugastota",                              GRP_RED,       REGION_CENTRAL },
/* 25 */ { SQ_RAILWAY,   "Galle Railway Station",                    GRP_NONE,      REGION_COMMERCIAL },
/* 26 */ { SQ_PROPERTY,  "Galle Fort",                               GRP_YELLOW,    REGION_SOUTHERN_COASTAL | REGION_COASTAL | REGION_NWSDB_ADJACENT },
/* 27 */ { SQ_PROPERTY,  "Unawatuna",                                GRP_YELLOW,    REGION_SOUTHERN_COASTAL | REGION_COASTAL | REGION_NWSDB_ADJACENT },
/* 28 */ { SQ_UTILITY,   "National Water Supply and Drainage Board", GRP_NONE,      0u },
/* 29 */ { SQ_PROPERTY,  "Hikkaduwa",                                GRP_YELLOW,    REGION_SOUTHERN_COASTAL | REGION_COASTAL | REGION_NWSDB_ADJACENT },
/* 30 */ { SQ_GOTOJAIL,  "Go To Jail",                               GRP_NONE,      0u },
/* 31 */ { SQ_PROPERTY,  "Jaffna Town",                              GRP_GREEN,     REGION_NORTHERN },
/* 32 */ { SQ_PROPERTY,  "Nallur",                                   GRP_GREEN,     REGION_NORTHERN },
/* 33 */ { SQ_INSURANCE, "Ceylinco Insurance",                       GRP_NONE,      0u },
/* 34 */ { SQ_PROPERTY,  "Trincomalee",                              GRP_GREEN,     REGION_NORTHERN | REGION_EASTERN | REGION_COASTAL },
/* 35 */ { SQ_RAILWAY,   "Jaffna Railway Station",                   GRP_NONE,      REGION_COMMERCIAL },
/* 36 */ { SQ_EVENT,     "National Event Card",                      GRP_NONE,      0u },
/* 37 */ { SQ_PROPERTY,  "Nuwara Eliya",                             GRP_DARKBLUE,  REGION_CENTRAL },
/* 38 */ { SQ_BANK,      "Bank of Ceylon",                           GRP_NONE,      0u },
/* 39 */ { SQ_PROPERTY,  "Galle Face",                               GRP_DARKBLUE,  REGION_WESTERN | REGION_COMMERCIAL }
};

/* Uniform integer in [lo, hi].
 *
 * The rejection loop is not decoration. The naive lo + rand() % span is
 * biased whenever span does not divide RAND_MAX + 1: the low residues occur
 * once more often than the high ones. For a die that skews every roll in the
 * game, and every downstream statistic with it. Discarding the short tail
 * above the largest exact multiple of span removes the bias entirely.
 *
 * The loop terminates with probability 1 and in practice almost always on
 * the first draw -- the rejected window is at most span-1 values out of
 * RAND_MAX + 1.
 */
int rng_range(int lo, int hi)
{
    int span  = hi - lo + 1;
    int limit = RAND_MAX - (RAND_MAX % span);
    int r;

    do {
        r = rand();
    } while (r >= limit);

    return lo + (r % span);
}

int roll_die(void)
{
    return rng_range(1, 6);
}

/* Fills both dice and returns their total. Callers need the individual dice
   for Rule 13's doubles check and the total for movement and utility rent,
   so both are handed back rather than recomputed. */
int roll_dice(int *d1, int *d2)
{
    *d1 = roll_die();
    *d2 = roll_die();
    return *d1 + *d2;
}

/* ------------------------------------------------------- initialisation -- */

/* Rent.csv is keyed by square index, so this is a lookup rather than a
   parallel array: the CSV order and the board order are not the same thing,
   and pairing them positionally would be a silent hazard the first time
   either is edited. Twenty-two entries scanned once at startup. */
static const PropertyValues *property_values_for(int sq)
{
    int i;
    for (i = 0; i < NUM_PROPERTIES; i++) {
        if (PROPERTY_VALUES[i].sq == sq) {
            return &PROPERTY_VALUES[i];
        }
    }
    return NULL;
}

/* Populate all 40 squares. Called once, from game_init. */
void board_init(GameState *g)
{
    int i;

    memset(g->board, 0, sizeof g->board);

    for (i = 0; i < NUM_SQUARES; i++) {
        Square *s = &g->board[i];

        s->type    = LAYOUT[i].type;
        s->name    = LAYOUT[i].name;
        s->group   = LAYOUT[i].group;
        s->regions = LAYOUT[i].regions;

        /* Rule 1: everything starts with the Bank and unowned. -1 rather
           than 0 for purchasedRound, because round 0 is a real round index
           and D19 derives age from this field. */
        s->owner          = -1;
        s->purchasedRound = -1;
        s->conditionPct   = 100;    /* LK 25: buildings begin sound          */
        s->policy         = INS_NONE;

        if (s->type == SQ_PROPERTY) {
            const PropertyValues *pv = property_values_for(i);
            const GroupValues    *gv = &GROUP_VALUES[s->group];

            s->price     = pv->price;      /* D7': individual, from Rent.csv */
            s->baseRent  = pv->baseRent;   /* D7': individual, not a ratio   */
            s->mortgageValue = gv->mortgage;  /* D18: group, loans only      */
            s->houseCost = gv->house;
            s->hotelCost = gv->hotel;
        } else if (s->type == SQ_RAILWAY || s->type == SQ_UTILITY) {
            /* The PDF prices neither; the clarification sets both at 1,500
               with a 750 mortgage. Rent comes from Tables 7 and 8 rather
               than a stored baseRent, and neither can be developed. */
            s->price         = STATION_PRICE;
            s->mortgageValue = STATION_MORTGAGE;
        }
    }
}
