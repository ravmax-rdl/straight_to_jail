/* board.c -- the board itself: its layout, its randomness, and the movement
 * and valuation queries every other module asks of it.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

/* ------------------------------------------------------------ the tables --
 *
 * D18 splits the two value sources, and keeping them apart is what stops
 * them being confused at a call site:
 *
 *   GROUP_VALUES   Appendix B, compiled in. Supplies construction costs and
 *                  mortgage value. Its basePrice column is the
 *                  loan-calculation basis named by the clarification -- no
 *                  buyer is ever charged it. Note it equals the cheapest
 *                  member of each group, which is a useful cross-check on
 *                  the CSV.
 *
 *   Rent.csv       Read at runtime (R1.3, D7', D27). Supplies the INDIVIDUAL
 *                  purchase price and base rent actually charged. Base rent
 *                  is not 10% of anything -- Pettah's 100 on a 1,500 price
 *                  is 6.7%, Galle Face's 1,200 on 12,000 is 10%.
 *
 * The individual values are deliberately NOT compiled in. The lecturer's
 * file is the single source of truth; a transcribed copy would be a second
 * one, free to drift out of step the first time either is edited.
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

/* The CSV's "Property Group" column, in PropertyGroup order. Matching on
   these lets a mistyped group in the file be caught rather than ignored. */
static const char *GROUP_NAMES[GRP_COUNT] = {
    "Brown", "Light Blue", "Pink", "Orange",
    "Red", "Yellow", "Green", "Dark Blue"
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

/* -------------------------------------------------------------- Rent.csv --
 *
 * D27. The 22 individual purchase prices and base rents are data, not code,
 * and are read from the lecturer's CSV at startup with plain stdio.
 *
 * Everything here works in fixed buffers on the stack. R0.5 forbids dynamic
 * allocation, and nothing about reading this file needs it: the row count is
 * known (22 properties), the longest real line is about 45 bytes, and the
 * destination -- g->board -- already exists. This is the one place in the
 * program where a reader might reach for malloc, so it is worth saying why
 * it is absent. POSIX getline() is out for the same reason; it allocates,
 * and it is not in C99 besides.
 */

#define CSV_LINE_MAX  256   /* the longest real line is ~45 bytes           */
#define CSV_FIELDS      4   /* Property Group, Property, Price, Base Rent   */

/* Searched in order when no path is given on the command line. Covers being
   run from the repository root and from the source directory, which is the
   difference between a marker typing ./monopoly and ../monopoly. */
static const char *CSV_CANDIDATES[] = {
    "assets/Rent.csv",
    "Rent.csv",
    "../assets/Rent.csv"
};
#define CSV_CANDIDATE_COUNT ((int)(sizeof CSV_CANDIDATES / sizeof CSV_CANDIDATES[0]))

/* Drop the line terminator. The file may arrive with either ending: it was
   authored on Windows, while .gitattributes normalises to LF on checkout, so
   tolerating both is not optional. fgets keeps whatever it found. */
static void chomp(char *s)
{
    size_t n = strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

/* Trim surrounding blanks in place, returning the new start. Interior spaces
   survive, which matters -- "Light Blue" is one field. */
static char *trim(char *s)
{
    char *end;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '\0') {
        return s;
    }
    for (end = s + strlen(s) - 1; end > s && (*end == ' ' || *end == '\t'); end--) {
        *end = '\0';
    }
    return s;
}

/* strtol rather than atoi: atoi cannot distinguish "0" from "not a number",
   which would turn a corrupt cell into a free property. */
static bool parse_int(const char *s, int *out)
{
    char *end;
    long  v;

    if (*s == '\0') {
        return false;
    }
    v = strtol(s, &end, 10);
    if (*end != '\0' || v <= 0 || v > INT_MAX) {
        return false;
    }
    *out = (int)v;
    return true;
}

static PropertyGroup group_from_name(const char *name)
{
    int i;

    for (i = 0; i < GRP_COUNT; i++) {
        if (strcmp(GROUP_NAMES[i], name) == 0) {
            return (PropertyGroup)i;
        }
    }
    return GRP_NONE;
}

/* Join the CSV to the board on the property NAME rather than on row order.
   Positional pairing would be a silent hazard the first time either the file
   or the layout is reordered; a name that does not match is an error we can
   report. */
static int property_square_named(const char *name)
{
    int i;

    for (i = 0; i < NUM_SQUARES; i++) {
        if (LAYOUT[i].type == SQ_PROPERTY && strcmp(LAYOUT[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static FILE *open_rent_csv(const char *override, const char **usedPath)
{
    FILE *f;
    int   i;

    /* An explicitly supplied path is never second-guessed: if the caller
       named a file, failing to open that file is the error to report. */
    if (override != NULL) {
        *usedPath = override;
        return fopen(override, "r");
    }

    for (i = 0; i < CSV_CANDIDATE_COUNT; i++) {
        f = fopen(CSV_CANDIDATES[i], "r");
        if (f != NULL) {
            *usedPath = CSV_CANDIDATES[i];
            return f;
        }
    }

    *usedPath = NULL;
    return NULL;
}

static void report_missing_csv(const char *override)
{
    int i;

    if (override != NULL) {
        fprintf(stderr, "straight_to_jail: cannot open %s\n", override);
        return;
    }

    fprintf(stderr, "straight_to_jail: cannot open Rent.csv\n");
    fprintf(stderr, "  tried: %s\n", CSV_CANDIDATES[0]);
    for (i = 1; i < CSV_CANDIDATE_COUNT; i++) {
        fprintf(stderr, "         %s\n", CSV_CANDIDATES[i]);
    }
    fprintf(stderr, "  pass a path as the 2nd argument:\n");
    fprintf(stderr, "    monopoly <seed> <path-to-Rent.csv>\n");
}

/* Overlay the individual values onto an already-laid-out board. Returns
   false, having explained itself on stderr, if the file is missing, short,
   or disagrees with the board. */
static bool load_rent_csv(GameState *g, const char *override)
{
    char        line[CSV_LINE_MAX];
    const char *path   = NULL;
    FILE       *f      = open_rent_csv(override, &path);
    int         lineNo = 0;
    int         loaded = 0;
    int         i;

    if (f == NULL) {
        report_missing_csv(override);
        return false;
    }

    while (fgets(line, (int)sizeof line, f) != NULL) {
        char         *field[CSV_FIELDS];
        char         *tok;
        int           n = 0;
        int           sq, price, baseRent;
        PropertyGroup grp;

        lineNo++;
        chomp(line);

        if (line[0] == '\0') {
            continue;                                   /* blank line       */
        }
        if (lineNo == 1 && strncmp(line, "Property Group", 14) == 0) {
            continue;                                   /* header row       */
        }

        /* Count every field, not just the first four, so both a short row
           and an over-long one are caught. strtok would fold two adjacent
           commas into one delimiter and silently shorten the row; the count
           check is what turns that into a reported error. */
        for (tok = strtok(line, ","); tok != NULL; tok = strtok(NULL, ",")) {
            if (n < CSV_FIELDS) {
                field[n] = trim(tok);
            }
            n++;
        }
        if (n != CSV_FIELDS) {
            fprintf(stderr, "straight_to_jail: %s:%d: expected %d fields, found %d\n",
                    path, lineNo, CSV_FIELDS, n);
            fclose(f);
            return false;
        }

        sq = property_square_named(field[1]);
        if (sq < 0) {
            fprintf(stderr, "straight_to_jail: %s:%d: \"%s\" is not a property on the board\n",
                    path, lineNo, field[1]);
            fclose(f);
            return false;
        }

        grp = group_from_name(field[0]);
        if (grp != LAYOUT[sq].group) {
            fprintf(stderr, "straight_to_jail: %s:%d: %s is group \"%s\" here but %s on the board\n",
                    path, lineNo, field[1], field[0],
                    LAYOUT[sq].group == GRP_NONE ? "none" : GROUP_NAMES[LAYOUT[sq].group]);
            fclose(f);
            return false;
        }

        if (!parse_int(field[2], &price) || !parse_int(field[3], &baseRent)) {
            fprintf(stderr, "straight_to_jail: %s:%d: price \"%s\" and base rent \"%s\" "
                            "must both be positive integers\n",
                    path, lineNo, field[2], field[3]);
            fclose(f);
            return false;
        }

        if (g->board[sq].price > 0) {
            fprintf(stderr, "straight_to_jail: %s:%d: duplicate row for %s\n",
                    path, lineNo, field[1]);
            fclose(f);
            return false;
        }

        g->board[sq].price    = price;      /* D7': individual, not a ratio */
        g->board[sq].baseRent = baseRent;
        loaded++;
    }

    if (ferror(f)) {
        fprintf(stderr, "straight_to_jail: %s: read error\n", path);
        fclose(f);
        return false;
    }
    fclose(f);

    /* A file that opened and parsed can still be incomplete. Every property
       square must have been given a price, or the first player to land on
       the gap would buy it for nothing. */
    if (loaded != NUM_PROPERTIES) {
        for (i = 0; i < NUM_SQUARES; i++) {
            if (LAYOUT[i].type == SQ_PROPERTY && g->board[i].price <= 0) {
                fprintf(stderr, "straight_to_jail: %s: no row for \"%s\" (square %d)\n",
                        path, LAYOUT[i].name, i);
            }
        }
        fprintf(stderr, "straight_to_jail: %s: read %d properties, expected %d\n",
                path, loaded, NUM_PROPERTIES);
        return false;
    }

    return true;
}

/* ------------------------------------------------------- initialisation -- */

/* Populate all 40 squares. Called once, from game_init.
 *
 * Two passes by necessity: the layout and the group-derived values are
 * compiled in and go down first, then Rent.csv overlays the individual price
 * and base rent onto the property squares. csvPath is argv[2] when the user
 * supplied one and NULL otherwise, in which case D27's candidate list is
 * searched.
 *
 * Returns false if the CSV could not be loaded. The caller must not continue
 * -- a board whose properties have no prices is not a game.
 */
bool board_init(GameState *g, const char *csvPath)
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
            const GroupValues *gv = &GROUP_VALUES[s->group];

            /* D18: the group supplies construction cost and mortgage value
               only. price and baseRent arrive from the CSV below. */
            s->mortgageValue = gv->mortgage;
            s->houseCost     = gv->house;
            s->hotelCost     = gv->hotel;
        } else if (s->type == SQ_RAILWAY || s->type == SQ_UTILITY) {
            /* The PDF prices neither; the clarification sets both at 1,500
               with a 750 mortgage. Rent comes from Tables 7 and 8 rather
               than a stored baseRent, and neither can be developed. */
            s->price         = STATION_PRICE;
            s->mortgageValue = STATION_MORTGAGE;
        }
    }

    return load_rent_csv(g, csvPath);
}

/* --------------------------------------------------------- the tables 2 -- */

/* Table 6, indexed by house count. A table rather than an if-chain so it can
   be checked against the spec by eye, and so it cannot grow an unreachable
   branch. Hotels are separate because Rule 10 makes them mutually exclusive
   with houses rather than a fifth house. */
static const int RENT_MULT[MAX_HOUSES + 1] = { 1, 2, 3, 5, 7 };
#define HOTEL_RENT_MULT 10

/* Table 7, indexed by (stations owned - 1). Index -1 never happens: a
   railway with no owner charges no rent and returns before the lookup. */
static const int RAILWAY_RENT[4] = { 250, 500, 1000, 2000 };

/* Table 8. */
#define UTILITY_MULT_ONE   4
#define UTILITY_MULT_BOTH 10

/* ---------------------------------------------------------- ownership -- */

/* Rule 5's "purchasable square". Everything else is either unowned forever
   (GO, Jail, tax squares) or not a thing anyone can hold. */
bool is_purchasable(const GameState *g, int sq)
{
    SquareType t = g->board[sq].type;

    return t == SQ_PROPERTY || t == SQ_RAILWAY || t == SQ_UTILITY;
}

/* How many squares of one type p owns. Railway and utility rent both key off
   this, and so does the monopoly test in milestone 3. */
int count_owned(const GameState *g, int p, SquareType type)
{
    int i, n = 0;

    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].type == type && g->board[i].owner == p) {
            n++;
        }
    }
    return n;
}

/* The colour group's printable name, for the LK 36 block. The table already
   exists to validate the CSV's group column; exposing a reader is cheaper and
   safer than a second copy of the eight names in game.c. */
const char *group_name(PropertyGroup grp)
{
    if (grp <= GRP_NONE || grp >= GRP_COUNT) {
        return "None";
    }
    return GROUP_NAMES[grp];
}

/* Rule 8: p holds every square of the colour group, which is the only thing
 * that permits construction.
 *
 * GRP_NONE is false rather than an error. Railways, utilities and the special
 * squares all carry it, so a caller sweeping the board asks this question of
 * them too and must get a plain "no" -- Rule 8 is about colour groups, and
 * owning all four railways is not a monopoly for building purposes.
 */
bool group_monopoly(const GameState *g, int p, PropertyGroup grp)
{
    int i, members = 0;

    if (grp == GRP_NONE) {
        return false;
    }

    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].group != grp) {
            continue;
        }
        if (g->board[i].owner != p) {
            return false;
        }
        members++;
    }

    return members > 0;
}

/* How many purchasable squares p holds that carry no buildings.
 *
 * The Anti-Speculation Act's base (LK 24, D25). Railways and utilities count:
 * neither can ever be developed, so both are permanently undeveloped holdings
 * and the rule is aimed at exactly that -- accumulating board without
 * improving it.
 */
int count_undeveloped(const GameState *g, int p)
{
    int i, n = 0;

    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].owner == p && is_purchasable(g, i)
            && development_level(g, i) == 0) {
            n++;
        }
    }
    return n;
}

/* How far a square is developed, on one scale: 0-4 houses, or MAX_HOUSES + 1
 * for a hotel.
 *
 * Rule 10 makes a hotel a replacement for four houses rather than a fifth
 * one, so board state stores them in separate fields -- which leaves a hotel
 * reading houses == 0, indistinguishable from an empty lot by that field
 * alone. Every caller that ranks development therefore asks this instead:
 * the builder picking the least-developed square in a group, and the Rule 9
 * evenness check. Without it the builder would see a fresh hotel as the
 * emptiest property in its group and immediately start it over with houses.
 */
int development_level(const GameState *g, int sq)
{
    const Square *s = &g->board[sq];

    return s->hotel ? MAX_HOUSES + 1 : s->houses;
}

/* ------------------------------------------------------- choke points -- */
/*
 * Every modifier in the game is read in one of the four functions below, and
 * nowhere else. A property's value is wanted by net worth, insurance
 * premiums, auction openings, renovation, the tax base and three of the four
 * strategies; its rent by roughly twenty call sites. Inlining the modifier
 * arithmetic at each would mean dozens of places to keep correct, and a
 * missed one does not crash -- it quietly reports a wrong number.
 *
 * The player argument to effect_modifier is the square's OWNER: an effect
 * that attaches to a player (Appendix A) reaches the holdings of that
 * player. An unowned square has owner -1, so only global effects reach it,
 * which is what we want for an auction opening on a Bank-held property.
 */

/* D18: market value is built on the INDIVIDUAL price from Rent.csv. Used for
 * purchase, net worth, the Community Development Fund base, renovation and
 * auction openings.
 *
 * Milestone 5 inserts depreciation (LK 16) and structural damage (LK 28)
 * between the stored price and the effect multiplier -- inside this function
 * and nowhere else.
 */
int square_value(const GameState *g, int sq)
{
    const Square *s     = &g->board[sq];

    /* LK 16, applied before the market. Depreciation is a property of the
       building itself -- what age has done to it -- so a boom lifts the
       depreciated property rather than the property it used to be. */
    int           value = apply_pct(s->price, -s->depreciationPct);

    return apply_pct(value, effect_modifier(g, EFF_VALUE_MUL, sq, s->owner));
}

/* LK 16, on the five-round cadence. One percentage point per tick once a
 * property has been held longer than DEPREC_START_AGE, capped at
 * DEPREC_CAP_PCT.
 *
 * D19's single clock is what makes this correct without a second counter:
 * age is round - purchasedRound, so it starts at purchase rather than at the
 * start of the game and resets when LK 17's renovation resets the field.
 * Unowned property carries purchasedRound == -1 and never ages -- a vacant
 * lot has nothing to wear out, and without this test every unsold square
 * would arrive at the cap by round 80.
 */
void depreciation_tick(GameState *g)
{
    char b[FMT_BUF];
    int  i;

    for (i = 0; i < NUM_SQUARES; i++) {
        Square *s = &g->board[i];

        if (s->type != SQ_PROPERTY || s->owner < 0 || s->purchasedRound < 0) {
            continue;
        }
        if (g->round - s->purchasedRound <= DEPREC_START_AGE) {
            continue;
        }
        if (s->depreciationPct >= DEPREC_CAP_PCT) {
            continue;
        }

        s->depreciationPct++;
        printf("Property\n");
        printf("%s\n", s->name);
        printf("has depreciated by %d%%.\n", s->depreciationPct);
        printf("Current Value\n");
        printf("LKR %s.\n", fmt_lkr(b, square_value(g, i)));
    }
}

/* D18 again, and the reason this is not square_value: mortgage value comes
 * from the GROUP table in Appendix B, not from the individual price. It
 * governs loan capacity and debt-recovery proceeds only. Keeping the two in
 * separate functions is what stops them being confused at a call site. */
int mortgage_value(const GameState *g, int sq)
{
    const Square *s = &g->board[sq];

    return apply_pct(s->mortgageValue, effect_modifier(g, EFF_MORTGAGE_MUL, sq, s->owner));
}

/* D18: construction cost comes from the GROUP table in Appendix B, so every
 * property in a colour group builds at the same price regardless of what its
 * individual purchase price is.
 *
 * The fourth choke point, and it earns the name several times over: house and
 * hotel prices are read by the builder, by LK 27's maintenance charge (a
 * percentage of them), by D1's repair cost, by the D11 ladder's 50% sale
 * price, and by net worth's building book value. The Housing Subsidy
 * regulation and the Fuel Crisis both move construction costs, and they move
 * all five of those readings by moving this one function.
 */
int building_cost(const GameState *g, int sq, bool hotel)
{
    const Square *s    = &g->board[sq];
    int           cost = hotel ? s->hotelCost : s->houseCost;

    return apply_pct(cost, effect_modifier(g, EFF_BUILD_COST_MUL, sq, s->owner));
}

/* LK 27: the cost of restoring one property to 100% condition -- 5% of
 * construction cost per house, 8% for a hotel.
 *
 * Reading building_cost rather than the stored field is what makes
 * maintenance track inflation and the Housing Subsidy regulation without a
 * line of its own: the upkeep of a building is a fixed fraction of what that
 * building currently costs to put up.
 */
int maintenance_cost(const GameState *g, int sq)
{
    const Square *s = &g->board[sq];

    if (s->hotel) {
        return pct_of(building_cost(g, sq, true), MAINT_HOTEL_PCT);
    }
    return pct_of(building_cost(g, sq, false) * s->houses, MAINT_HOUSE_PCT);
}

/* D1: what it costs to put right the buildings standing on a square -- half
 * of what they currently cost to put up.
 *
 * The spec never quantifies a repair, so D1 anchors it to construction. That
 * makes damage scale with development, which is the behaviour LK 10 clearly
 * intends (a hotel is a bigger loss than one house), and makes it track
 * inflation for free through building_cost.
 *
 * Note this is the whole square, unlike the D11 ladder's demolition refund,
 * which prices only the topmost building. A fire does not burn down one
 * house of four.
 */
int repair_cost(const GameState *g, int sq)
{
    const Square *s = &g->board[sq];

    if (s->hotel) {
        return pct_of(building_cost(g, sq, true), REPAIR_PCT);
    }
    return pct_of(building_cost(g, sq, false) * s->houses, REPAIR_PCT);
}

/* Table 3 as a band lookup rather than a formula. The spec gives five bands
 * with hard edges, not a curve, and 89% collecting the same 90% as 75% is
 * the rule rather than an approximation of one.
 *
 * The bottom band is LK 26's Closed: below 25% the building collects nothing
 * at all, which is why this returns 0 rather than some small percentage.
 */
static int condition_rent_pct(int conditionPct)
{
    if (conditionPct >= 90) { return 100; }
    if (conditionPct >= 75) { return  90; }
    if (conditionPct >= 50) { return  75; }
    if (conditionPct >= 25) { return  50; }
    return 0;                                   /* LK 26: Closed            */
}

/* LK 25, called at the end of every round. Buildings decay; land does not.
 *
 * The guard is what makes the rule sensible: an undeveloped property has no
 * building to fall into disrepair, so it neither loses condition nor counts
 * unmaintained rounds towards LK 28's structural damage. Decaying every
 * square would quietly close half the board's undeveloped properties by
 * round 40, none of which the rules ever intended.
 */
void condition_tick(GameState *g)
{
    int i;

    for (i = 0; i < NUM_SQUARES; i++) {
        Square *s = &g->board[i];

        if (development_level(g, i) == 0) {
            continue;
        }

        s->conditionPct -= COND_DECAY_PCT;
        if (s->conditionPct < 0) {
            s->conditionPct = 0;
        }
        s->unmaintainedRounds++;
    }
}

/* Rule 7 and Tables 6-8. Returns what the visitor owes the owner.
 *
 * Zero in three cases: nobody owns it, it is mortgaged (Rule 7 says a
 * mortgaged property collects nothing), or it is not a rent-bearing square
 * at all. Landing on your own property is the caller's business, not this
 * function's -- it reports what the square is worth, not who is standing on
 * it.
 *
 * diceTotal is used only by utilities, which is why it is a parameter here
 * rather than something square_rent could look up.
 */
int square_rent(const GameState *g, int sq, int diceTotal)
{
    const Square *s = &g->board[sq];
    int rent;

    if (s->owner < 0 || s->mortgaged) {
        return 0;
    }

    /* Appendix A's Political Rally shuts a square for two rounds. Read before
       the type switch because it applies whatever the square is, and read
       through effect_active rather than effect_modifier because EFF_CLOSED
       carries no magnitude to sum -- it is a flag, and its presence is the
       whole of its meaning. (LK 26's separate closure, for a building decayed
       below 25%, already falls out of condition_rent_pct returning zero.) */
    if (effect_active(g, EFF_CLOSED, sq, s->owner)) {
        return 0;
    }

    /* LK 11: a damaged building collects nothing until it is repaired. Only
       buildings can be damaged, so an undeveloped square is unaffected --
       and auto_repairs runs every round, so this is a pause on the income
       rather than the end of it. */
    if (s->damaged) {
        return 0;
    }

    switch (s->type) {
    case SQ_PROPERTY:
        if (s->hotel) {
            rent = s->baseRent * HOTEL_RENT_MULT;
            rent = apply_pct(rent, effect_modifier(g, EFF_HOTEL_RENT_MUL, sq, s->owner));
        } else {
            rent = s->baseRent * RENT_MULT[s->houses];
        }
        /* LK 25-26, and only where there is something to be in disrepair.
           An undeveloped property collects its full base rent forever: the
           condition percentage describes buildings, and a vacant lot has
           none. Applied before the market multipliers because a boom lifts
           what the property actually earns, not what it would earn if it
           were maintained. */
        if (development_level(g, sq) > 0) {
            rent = pct_of(rent, condition_rent_pct(s->conditionPct));
        }
        return apply_pct(rent, effect_modifier(g, EFF_RENT_MUL, sq, s->owner));

    case SQ_RAILWAY:
        rent = RAILWAY_RENT[count_owned(g, s->owner, SQ_RAILWAY) - 1];
        return apply_pct(rent, effect_modifier(g, EFF_RAILWAY_RENT_MUL, sq, s->owner));

    case SQ_UTILITY:
        rent = diceTotal * (count_owned(g, s->owner, SQ_UTILITY) == 2
                            ? UTILITY_MULT_BOTH : UTILITY_MULT_ONE);
        return apply_pct(rent, effect_modifier(g, EFF_UTILITY_RENT_MUL, sq, s->owner));

    case SQ_GO:    case SQ_BANK:      case SQ_INSURANCE: case SQ_TAX:
    case SQ_COMMUNITY: case SQ_EVENT: case SQ_JAIL:      case SQ_PARKING:
    case SQ_GOTOJAIL:
        return 0;
    }

    return 0;
}

/* ------------------------------------------------------------- movement -- */

/* Rule 3 step 3 and Rule 4. Moves clockwise, wrapping, and pays the GO
 * salary on a pass or a landing.
 *
 * The wrap test is the whole rule: with dice in [2,12] a move can never
 * return a player to where they started, so to < from means exactly one
 * thing -- the index wrapped past 39, which is passing GO. to == 0 catches
 * landing on it squarely. Starting a turn on GO and moving off it satisfies
 * neither, which is correct: Rule 4 pays for passing or landing, not for
 * standing there.
 *
 * Rule 12's Go To Jail deliberately does not come through here. A player
 * sent to jail is transferred, not walked, and collects nothing.
 */
void move_player(GameState *g, int p, int steps)
{
    char    b[FMT_BUF];
    Player *pl   = &g->players[p];
    int     from = pl->pos;
    int     to   = (from + steps) % NUM_SQUARES;

    printf("%s moves from Square %d to Square %d.\n", pl->name, from, to);
    pl->pos = to;

    if (to < from || to == SQ_IDX_GO) {
        pl->cash += GO_SALARY;
        printf("%s passed GO.\n", pl->name);
        printf("Collected LKR %s.\n", fmt_lkr(b, GO_SALARY));
        printf("Current Balance : LKR %s.\n", fmt_lkr(b, pl->cash));
    }
}
