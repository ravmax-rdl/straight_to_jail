#ifndef TYPES_H
#define TYPES_H

/* ------------------------------------------------------------------------
 * Straight to Jail -- an implementation of the MONOPOLY-LK specification
 * (SCS 1301 take-home). Spec: assets/Assignment_1_unlocked.pdf
 * Supplemental per-property values: assets/Rent.csv
 *
 * SPEC-GAP DECISIONS. Full rationale in docs/REQUIREMENTS.md section D.
 * Every one of these is implemented exactly once, at a choke point, and the
 * implementation site carries a comment citing its ID.
 *
 * The three marked SUPERSEDES override the PDF; they come from the
 * lecturer's clarification set, which is later and authoritative.
 *
 * D1  Repair cost     50% of the current construction cost of the buildings
 *                     standing on the property. Damage NEVER attaches to
 *                     a bare lot: LK 11 ties it to buildings and LK 10
 *                     strikes only developed property, so every path
 *                     that empties a square clears it       [LK 10, 11]
 * D2' Income Tax      SUPERSEDES the PDF. 15% of the player's CURRENT CASH.
 *                     The rate is held in econ.incomeTaxPct, seeded at 15,
 *                     moved by inflation the way the loan rate is (D21),
 *                     and further scaled at charge time by EFF_TAX_MUL --
 *                     x1.5 under Increase Property Tax          [Rule 11]
 * D3  Coverage        Basic {Fire,Flood} @80%; Comprehensive
 *                     {Fire,Flood,Riot,Vandalism} @100%; Business
 *                     Interruption all perils @100% + 5 rounds of hotel
 *                     rent. Building Collapse and Electrical Failure are
 *                     uncovered below Business Interruption. App E's
 *                     "Earthquake" never occurs             [LK 10, App E]
 * D4  Interest        The Table 9 rate applies EVERY ROUND; the "annual"
 *                     label is ignored as inconsistent with LK 4. A loan is
 *                     a single-player instrument, so the round is the
 *                     BORROWER's lap (D34) -- a 20-lap term therefore
 *                     compounds exactly 20 times. The issued rate is frozen
 *                     for the loan's life                       [LK 4, 13]
 * D5  Max loan        75% of the mortgage value of properties + railways +
 *                     utilities (LK 2 beats the narrower 1.1.4)      [LK 2]
 * D6' Rounding        SUPERSEDES the PDF. Money is STORED as int; ratio
 *                     and interest arithmetic is computed in double and
 *                     rounded to nearest by money_round(). No math.h
 * D7' Values          SUPERSEDES the PDF. Individual purchase price and
 *                     base rent come from Rent.csv; the group table
 *                     supplies construction cost and mortgage value
 * D8' Tie-break       Only tied players reroll, and the reroll permutes
 *                     only their own positions                      [Rule 2]
 * D9  Valuation proxy "estimated market value" = square_value()    [sec. 3]
 * D10 Jail            REVISED. Rule 13's three routes are a CHOICE, made
 *                     in players.c: doubles are free, bail is paid from
 *                     cash only, and the third turn releases WITHOUT a
 *                     charge -- serving the time is itself an exit. Doubles
 *                     have no effect outside jail                [Rule 13]
 * D11 Debt recovery   sell buildings @50% -> mortgage free assets ->
 *                     bankrupt, assets auctioned. Repaying a loan is NOT
 *                     a rung: that needs the Bank square      [Rule 11, 14]
 * D12 Effects         Permanent effects mutate stored values; temporary
 *                     effects live in the registry and are read at
 *                     access time                          [LK 14, 34, 35]
 * D13 Round order     interest -> default -> condition -> insurance ->
 *                     repairs -> cadences -> tick -> summary -> market
 * D14 Region tags     see REGION_* below                   [LK 18, Table 4]
 * D15 Claims recv.    always 0 -- compensation is credited immediately
 *                                                              [Rule 15]
 * D16 Total assets    the Community Development Fund's base, and its alone:
 *                     sum of square_value over the 22 coloured properties
 *                     owned. Buildings, railways and utilities excluded.
 *                     The Fund levies 10% of it. Income Tax uses cash (D2'),
 *                     so the two squares have genuinely different bases
 * D17 Square 2        its own SQ_COMMUNITY type -- it draws no card. Card
 *                     squares are 7, 22 and 36 only              [Table 1]
 * D18 Value bases     individual price -> purchase, market value, rent
 *                     basis, renovation, tax base, auction opening.
 *                     group base price -> mortgage value only, i.e. loan
 *                     capacity and debt-recovery proceeds
 * D19 Age from buy    property age is round - purchasedRound and starts at
 *                     purchase, never before                    [clarified]
 * D35 House subsidies Appendix B prices houses and hotels in separate
 *                     columns, so "HOUSE construction costs reduce" names
 *                     one of them. The Housing Programme, the Housing
 *                     Subsidy regulation and its card discount houses
 *                     only; LK 31's boom moves both      [LK 18, 24, 31]
 * D36 No renewal      LK 9 promises a reminder three rounds out but never
 *                     defines renewal -- no price, no term, no square. A
 *                     policy therefore lapses and is rebought like any
 *                     other. The reminder stands as section 5 shows it,
 *                     and the gap is the spec's, not an omission   [LK 9]
 * D39 Price vs value  LK 31 moves purchase prices +15% and property values
 *                     +20% as SEPARATE bullets, so purchase_price is its
 *                     own choke point reading EFF_PRICE_MUL. Every other
 *                     rule says "property values" and moves square_value
 *                     alone; LK 32 names no price line          [LK 31]
 * D40 Cumulative      LK 34: concurrent percentages ADD, never compound.
 *                     A hotel +100% under a +25% boom collects 225% of
 *                     base, not 250%. Kinds are summed and applied once,
 *                     which is also how the general rent shift finally
 *                     reaches railways and utilities            [LK 34]
 * D41 Risk doubling   "Riot probability doubles" is about the PROBABILITY.
 *                     One weight cannot double it -- the others must give
 *                     up the difference. k doubled of n: 2*(n-k) against
 *                     (n-2k), which is exactly 2/n each          [LK 18]
 * D38 Struct. damage  LK 28 needs no counter: LK 25's 2%-a-round decay
 *                     already counts, so "more than 20 rounds" IS a
 *                     condition below 60%. Only LK 29's renovation clears
 *                     it; LK 27 upkeep prevents, never undoes [LK 28, 29]
 * D37 Condition       LK 25 rates every BUILDING, so a property with four
 *                     houses has four ratings; Table 3 maps one. The
 *                     AVERAGE drives the band -- the spec is silent, it
 *                     was written as though there were one     [LK 25, 26]
 * D34 Two clocks      SUPERSEDES D19's single clock. A SINGLE-PLAYER effect
 *                     counts that player's own laps -- a loan matures 20 GO
 *                     passes after issue, a policy lapses 20 after purchase,
 *                     an App A card ages by its drawer. A GLOBAL effect and
 *                     the 500-round limit count game rounds       [clarified]
 * D33 Taxes due     income tax is ASSESSED every round at 15% of cash and
 *                     COLLECTED on square 4; the balance stands in
 *                     taxesDue, which is what Rule 15 subtracts. The rate
 *                     fluctuates with inflation and LK 24        [Rule 11, 15]
 * D32 Selling        section 3 requires it and names no price: sold to the
 *                     Bank at current square_value, buildings down first
 *                     at D11's 50%. Never breaks a developed group [sec. 3]
 * D31 Redemption      a mortgage is lifted by repaying the CURRENT mortgage
 *                     value at the Bank square, one action per landing like
 *                     LK 5's five. Rent and development resume; no interest
 *                     accrues meanwhile, the spec naming none    [Rule 7, R1.8]
 * D30 A round         SUPERSEDES the earlier reading. A round is one LAP of
 *                     the board: it ends when every solvent player has
 *                     passed GO since it began, not after one turn each.
 *                     Players keep taking turns in order[] throughout, so a
 *                     round is several turns each             [Rule 15, LK]
 * D20 Single claim    a payout consumes the policy, whatever rounds remain
 * D21 Interest rate   REVISED TWICE. Table 9 alone governs a NEW loan:
 *                     Recession 15%, Stock Market Boom 5%, else LK 12's
 *                     draw -- above 5% High 12%, above 0 Moderate 10%,
 *                     else Stable 8%. Nothing else touches the issued
 *                     rate. EVERY other adjustment moves LIVE loans: the
 *                     boom and recession shifts, LK 24's regulations and
 *                     Appendix A's rate cards alike            [App D]
 * D22 Loan pledge     only the minimum set of assets, highest mortgage
 *                     value first, whose 75% LTV covers the amount
 * D23 Auction order   starts with the player immediately after the current
 *                     player, then clockwise. The decliner may bid
 * D24 Luxury tax      charged once when the regulation activates, at 25%
 *                     of each hotel property's value including buildings
 * D25 Anti-Spec. Act  REVISED. The cap counts OWNED undeveloped COLOUR
 *                     properties, so it gates auctions as well as direct
 *                     purchases -- how a square arrives is irrelevant.
 *                     Holding more than three makes construction
 *                     compulsory, overriding personality        [LK 24]
 * D26 Output spacing  REVISED. Every message type ends with a blank line,
 *                     so two kinds of output never run together. Within a
 *                     block, a labelled category opens a new group; the
 *                     round summary and market table stay compact [sec. 5]
 * ------------------------------------------------------------------------ */

#include <stdbool.h>

/* ---------------------------------------------------------------- enums -- */

/* D17: SQ_COMMUNITY is distinct from SQ_EVENT. Table 1 types square 2 as
   "Event", but it levies a tax rather than drawing a card, so the three card
   squares are 7, 22 and 36 only. */
typedef enum {
    SQ_GO, SQ_PROPERTY, SQ_RAILWAY, SQ_UTILITY, SQ_BANK, SQ_INSURANCE,
    SQ_TAX, SQ_COMMUNITY, SQ_EVENT, SQ_JAIL, SQ_PARKING, SQ_GOTOJAIL
} SquareType;

/* GRP_NONE is -1 so that colour groups index arrays directly from zero.
   GRP_COUNT is the array-sizing sentinel. */
typedef enum {
    GRP_NONE = -1,
    GRP_BROWN = 0, GRP_LIGHTBLUE, GRP_PINK, GRP_ORANGE,
    GRP_RED, GRP_YELLOW, GRP_GREEN, GRP_DARKBLUE,
    GRP_COUNT
} PropertyGroup;

typedef enum { INS_NONE, INS_BASIC, INS_COMPREHENSIVE, INS_BUSINESS } InsuranceType;

typedef enum {
    STRAT_AGGRESSIVE, STRAT_CONSERVATIVE, STRAT_RISKTAKER, STRAT_OPPORTUNIST
} Strategy;

typedef enum {
    DIS_FIRE, DIS_FLOOD, DIS_RIOT, DIS_COLLAPSE, DIS_ELECTRICAL, DIS_COUNT
} Disaster;

/* Every timed economic modifier in the game reduces to one of these kinds.
   D21: both interest kinds exist deliberately. Large event shifts are
   relative (MUL); LK 24's and Appendix A's explicit +/-2% adjustments are
   percentage points (ADD), because a relative 2% rounds to a no-op. ADD is
   applied before MUL.
   EFF_MAX_PROPERTIES reuses Effect.magnitudePct as a plain count, not a
   percentage -- it carries the Anti-Speculation Act's cap of 3. */
typedef enum {
    EFF_VALUE_MUL, EFF_PRICE_MUL, EFF_RENT_MUL, EFF_HOTEL_RENT_MUL,
    EFF_RAILWAY_RENT_MUL,
    EFF_UTILITY_RENT_MUL, EFF_BUILD_COST_MUL, EFF_HOUSE_COST_MUL,
    EFF_PREMIUM_MUL, EFF_MORTGAGE_MUL,
    EFF_AUCTION_OPEN_MUL, EFF_INTEREST_MUL, EFF_INTEREST_ADD, EFF_TAX_MUL,
    EFF_CLOSED, EFF_CONSTRUCTION_SUSPENDED, EFF_MAX_PROPERTIES,
    EFF_FLOOD_RISK, EFF_RIOT_RISK,
    EFF_KIND_COUNT
} EffectKind;

typedef enum {
    SCOPE_GLOBAL, SCOPE_GROUP, SCOPE_REGION, SCOPE_SQUARE, SCOPE_PLAYER
} EffectScope;

/* LK 5's five loan actions, plus D31's mortgage redemption, plus doing
   nothing. Exactly one per landing on the Bank square, which is what makes
   this an enum rather than a set of flags: decide_bank picks one and game.c
   performs it. BANK_REDEEM is the only member that names a square. */
typedef enum {
    BANK_NONE, BANK_OBTAIN, BANK_REPAY_PART, BANK_REPAY_FULL,
    BANK_EXTEND, BANK_INCREASE, BANK_REDEEM
} BankAction;

/* D14 region tags. A bitmask rather than an enum because a square belongs to
   several regions at once -- Trincomalee is northern, eastern and coastal. */
#define REGION_WESTERN          0x01u
#define REGION_CENTRAL          0x02u
#define REGION_SOUTHERN_COASTAL 0x04u
#define REGION_NORTHERN         0x08u
#define REGION_EASTERN          0x10u
#define REGION_COMMERCIAL       0x20u
#define REGION_NWSDB_ADJACENT   0x40u
#define REGION_COASTAL          0x80u

/* ------------------------------------------------------------ constants -- */

#define NUM_SQUARES         40
#define NUM_PLAYERS          4
#define NUM_PROPERTIES      22
#define MAX_ROUNDS         500   /* Rule 15                                  */
#define START_CASH       30000   /* Rule 1                                   */
#define GO_SALARY         2000   /* Rule 4                                   */
#define JAIL_BAIL          300   /* Rule 13                                  */
#define JAIL_MAX_TURNS       3   /* Rule 13                                  */
#define AUCTION_INC        250   /* LK 20                                    */
#define AUCTION_OPEN_PCT    50   /* LK 19                                    */
#define LOAN_LTV_PCT        75   /* LK 2, D5                                 */
#define LOAN_ROUNDS         20   /* LK 4, D19                                */
#define INS_ROUNDS          20   /* LK 9                                     */
#define INS_WARN_ROUNDS      3   /* LK 9                                     */
#define INS_BASIC_PCT        5   /* App E: premium, of current value         */
#define INS_COMPREHENSIVE_PCT 10 /* App E                                    */
#define INS_BUSINESS_PCT    15   /* App E                                    */
#define BI_RENT_ROUNDS       5   /* D3: Business Interruption's lost rent    */
#define REPAIR_PCT          50   /* D1: of the buildings' construction cost  */
#define MAX_HOUSES           4   /* Rule 9                                   */
#define INCOME_TAX_PCT      15   /* D2': of cash, seeds econ.incomeTaxPct    */
#define COMMUNITY_PCT       10   /* D16: of total property assets            */
#define COND_DECAY_PCT       2   /* LK 25                                    */
#define MAINT_HOUSE_PCT      5   /* LK 27: per house, of construction cost   */
#define MAINT_HOTEL_PCT      8   /* LK 27: per hotel, of construction cost   */
#define DEPREC_START_AGE    50   /* LK 16                                    */
#define DEPREC_CAP_PCT      30   /* LK 16                                    */
#define DEPREC_EVERY         5   /* LK 16: one point per five rounds         */
#define RENOVATE_PCT        10   /* LK 17: of current market value           */
#define STRUCT_COND_PCT     60   /* LK 28 = 20 rounds of LK 25 decay          */
#define STRUCT_VALUE_PCT   -15   /* LK 28: value penalty                     */
#define STRUCT_RENT_PCT     75   /* LK 28: max rent, i.e. -25%               */
#define STRUCT_MAINT_PCT    50   /* LK 28: upkeep costs half again           */
#define STRUCT_RENOVATE_PCT 25   /* LK 29: of replacement value              */
#define MARKET_COOLDOWN     30   /* LK 33                                    */
#define DECK_SIZE           20   /* App A                                    */

/* Cadences and durations for the timed systems (D13). Every one of these is
   both how often a system fires and how long what it creates survives; where
   the two coincide, as they do for regulations, one replaces the previous
   with no code because the old record has already expired. */
#define INFLATION_EVERY     10   /* LK 12                                    */
#define MARKET_EVERY        10   /* LK 30                                    */
#define MARKET_ROUNDS       10   /* LK 31-32                                 */
#define EVENT_EVERY         15   /* LK 18, Table 4                           */
#define EVENT_ROUNDS        15   /* LK 18                                    */
#define CARD_ROUNDS         15   /* Table 4, App A                           */
#define REGULATION_EVERY    20   /* LK 24                                    */
#define REGULATION_ROUNDS   20   /* LK 24                                    */
#define ANTI_SPEC_CAP        3   /* LK 24, D25: undeveloped properties       */
#define STATION_PRICE     1500   /* clarification: railways and utilities    */
#define STATION_MORTGAGE   750   /* clarification: 50% of station price      */

/* Worst case: 4 players holding several card effects each, plus the four
   global cadenced systems, plus up to three square-scoped regional effects.
   Sized with headroom; a DEBUG assert catches overflow rather than dropping
   an effect silently. */
#define MAX_EFFECTS        128

/* Board indices that rules name directly. */
#define SQ_IDX_GO            0
#define SQ_IDX_JAIL         10
#define SQ_IDX_BANK         38

/* -------------------------------------------------------------- structs -- */

/* One board square. The property fields sit idle on the 18 non-property
   squares; a union would save under 1 KB and cost a discriminated-access
   dance on every read, which is a bad trade when simplicity is the graded
   quality. */
typedef struct {
    SquareType    type;
    const char   *name;
    PropertyGroup group;          /* GRP_NONE for non-properties             */
    unsigned      regions;        /* REGION_* bitmask, D14                   */

    /* Permanent-adjusted stored values. Only inflation mutates these (D12).
       D18: price and baseRent are INDIVIDUAL, from Rent.csv. mortgageValue,
       houseCost and hotelCost come from the group table in Appendix B. */
    int price, baseRent, mortgageValue, houseCost, hotelCost;

    int  owner;                   /* -1 = Bank                               */
    int  purchasedRound;          /* -1 = unowned. D19: age is derived from
                                     this, so it cannot disagree with owner. */
    int  houses;                  /* 0..4, mutually exclusive with hotel     */
    bool hotel;
    bool mortgaged, loanLocked, damaged, structDamaged;

    int  depreciationPct;         /* LK 16, capped at DEPREC_CAP_PCT         */
    /* LK 25: one Condition Rating per BUILDING, each starting at 100.
       cond[0] is the hotel's while hotel is true, since a hotel replaces
       the four houses rather than joining them (Rule 10); otherwise
       cond[0..houses-1] are the houses in the order they were built. */
    int  cond[MAX_HOUSES];

    InsuranceType policy;
    int           policyLap;      /* D34: OWNER's lap count when bought      */
    bool          policyWarned;   /* LK 9's reminder fires once              */
} Square;

typedef struct {
    bool active;
    int  principal;               /* grows every GAME round at ratePct       */
    int  ratePct;                 /* frozen at issue -- LK 13                */
    int  issuedLap;               /* D34: the BORROWER's lap count at issue  */
    int  termLaps;                /* 20 of the borrower's own laps, extended
                                     by the LK 5 extend action               */
} Loan;

typedef struct {
    const char *name;
    Strategy    strat;
    int  cash, pos, jailTurns, taxesDue;
    bool bankrupt, jailed;
    bool passedGo;                /* D30: lapped since the round began       */
    int  laps;                    /* D34: this player's own clock -- total
                                     times they have passed GO. Single-player
                                     effects are measured against this, not
                                     against the game round                  */
    int  lapsPrev;                /* laps at the start of the current round,
                                     so the registry can age a player-scoped
                                     effect by what that player actually did */
    bool sufferedLoss;            /* gates the Risk Taker's insurance, 3.3   */
    Loan loan;
} Player;

/* A single timed modifier. Every timed system in the game -- booms, declines,
   regional cards, national events, event cards, regulations -- reduces to
   pushing one of these. See the architecture document section 5 for why a
   flat set of Economy fields cannot work: Appendix A cards are per-player and
   overlapping, and LK 34 requires concurrent effects to stack rather than
   overwrite. */
/* Table 9's five rows, in the table's own order, so TABLE9_RATE indexes
   straight off this. D21 decides which one is prevailing. */
typedef enum {
    ECON_BOOM, ECON_STABLE, ECON_MODERATE_INFLATION,
    ECON_HIGH_INFLATION, ECON_RECESSION
} EconomicCondition;

typedef struct {
    EffectKind kind;
    int  scopeKind;               /* an EffectScope                          */
    int  scope;                   /* group index, region mask, square, player */
    int  magnitudePct;            /* signed: +25, -15                        */
    int  owner;                   /* -1 = everyone                           */
    int  roundsLeft;
} Effect;

typedef struct {
    int  inflationPct;            /* most recent draw, for the LK 36 block   */
    int  incomeTaxPct;            /* seeded at 15, inflation-adjusted, D2'   */
    int  groupCooldown[GRP_COUNT];/* LK 33: 30-round bar on re-selection     */
    int  lastBoomGroup, lastDeclineGroup;

    /* The Table 4 card currently in force, and the round it was drawn. The
       LK 36 block must name it, and a name is the one thing an Effect record
       cannot carry -- so the identity is stored and the rounds remaining are
       derived from D19's single clock rather than kept in a second counter
       that could drift from the registry. -1 = none. */
    int  activeCard, activeCardRound;

    /* D21: which LK 18 national event is in force, and when it fired. Stored
       for the same reason activeCard is -- Table 9 keys on the CONDITION,
       and a condition is an identity that an Effect record cannot carry.
       Only Economic Recession and Stock Market Boom are ever read back, but
       storing the index rather than two flags keeps one fact in one place.
       -1 = none. */
    int  activeEvent, activeEventRound;
    Effect effects[MAX_EFFECTS];
    int    effectCount;
} Economy;

/* App A's "returned to the bottom of the deck" over a fixed array: nothing
   moves, only the head index advances. No linked list, no allocation (R0.5). */
typedef struct {
    int cards[DECK_SIZE];
    int head;
} EventDeck;

/* The entire mutable state of the simulation. One of these lives on main's
   stack and is threaded by pointer through every function -- R0.4 forbids
   globals, and this is what replaces them. */
typedef struct {
    Square    board[NUM_SQUARES];
    Player    players[NUM_PLAYERS];
    int       order[NUM_PLAYERS];  /* player indices in turn order            */
    Economy   econ;
    EventDeck deck;
    int       round;               /* 1-based                                */
} GameState;

/* ----------------------------------------------------------- prototypes -- */

/* Every public function in the program is declared here and nowhere else, so
   a mismatch between what one module produces and another consumes fails the
   build rather than diverging silently. */

/* Buffers passed to fmt_lkr must be at least this large. */
#define FMT_BUF 20

/* finance.c -- the money boundary (D6'). These three are the only functions
   in the program permitted to contain a double. */
int         money_round(double v);
int         apply_pct(int value, int percent);
int         pct_of(int value, int percent);
const char *fmt_lkr(char *buf, int amount);
int         net_worth(const GameState *g, int p);

/* finance.c -- money movement. charge is the single place in the program
   where insolvency is detected: a short payer goes through the D11 ladder,
   and one the ladder cannot save is declared bankrupt on the spot. It
   returns false only in that case, with the block already printed. */
void credit(GameState *g, int p, int amt);
bool charge(GameState *g, int p, int amt, int toPlayer);

/* finance.c -- insurance (S1.2, LK 8-9, App E). premium reads square_value,
   so a quote tracks inflation and the market without a line of its own.
   tick_insurance runs once per round, warns at exactly INS_WARN_ROUNDS and
   lapses the policy at zero. */
int  premium(const GameState *g, int sq, InsuranceType tier);
void buy_policy(GameState *g, int p, int sq, InsuranceType tier);
void tick_insurance(GameState *g);

/* finance.c -- Rule 11's debt recovery and Rule 14's bankruptcy (D11).
   raise_funds is called by charge and by nothing else; creditor is -1 when
   the Bank is owed. */
bool raise_funds(GameState *g, int p, int needed);
void declare_bankrupt(GameState *g, int p, int creditor);

/* finance.c -- D32's voluntary sale. Section 3 requires it of two
   personalities in so many words and forbids it of a third; the price is the
   current square_value, buildings coming down first at D11's 50%. */
void sell_property(GameState *g, int p, int sq);

/* finance.c -- the two tax squares. Different bases (D2' cash, D16 property
   assets), so deliberately two functions rather than one parameterised. */
int  total_assets(const GameState *g, int p);

/* finance.c -- D33. Income tax is ASSESSED every round into Player.taxesDue
   and COLLECTED when the player lands on square 4. The rate is
   econ.incomeTaxPct, which drifts with inflation (D2', D21) and is scaled at
   assessment time by EFF_TAX_MUL. */
void accrue_income_tax(GameState *g);

/* finance.c -- auctions (LK 19-23, D23). anchorPlayer is whoever's turn
   triggered it; bidding starts with the player after them. The opening price
   is internal: run_auction hands it to the first bidder as minBid, so no
   strategy needs to compute it. */
void run_auction(GameState *g, int sq, int anchorPlayer);
void pay_income_tax(GameState *g, int p);
void pay_community_fund(GameState *g, int p);

/* finance.c -- loans (LK 1-7, D4, D5, D22). loan_capacity is the 75% LTV of
   collateral still free to pledge; max_loan is the same figure gated by
   LK 5's one-loan-at-a-time rule, which is why the increase action reads the
   former. accrue_interest and check_loan_default run once per round in that
   order (D13), so a loan can default on the interest it has just accrued. */
int  current_loan_rate(const GameState *g, int p);
bool eligible_collateral(const GameState *g, int p, int sq);
int  loan_capacity(const GameState *g, int p);
int  max_loan(const GameState *g, int p);
void grant_loan(GameState *g, int p, int amount);
void increase_loan(GameState *g, int p, int extra);
void extend_loan(GameState *g, int p);
void repay_loan(GameState *g, int p, int amount);
void redeem_mortgage(GameState *g, int p, int sq);
void accrue_interest(GameState *g);
void check_loan_default(GameState *g);

/* board.c -- randomness. Seeded once in main; every random draw in the
   program goes through rng_range so the bias fix applies everywhere. */
int rng_range(int lo, int hi);
int roll_die(void);
int roll_dice(int *d1, int *d2);

/* board.c -- the board table and movement.
   board_init reads Rent.csv (D27); csvPath is an explicit override or NULL
   to search the candidate list. Both return false, having explained the
   failure on stderr, if the file cannot be loaded. */
bool board_init(GameState *g, const char *csvPath);
void move_player(GameState *g, int p, int steps);

/* board.c -- ownership queries. development_level puts houses and hotels on
   one scale (0-4, then MAX_HOUSES + 1) so Rule 9's evenness can be judged;
   a hotel stores houses == 0 and would otherwise read as an empty lot. */
bool is_purchasable(const GameState *g, int sq);
int  count_owned(const GameState *g, int p, SquareType type);
bool group_monopoly(const GameState *g, int p, PropertyGroup grp);
int  development_level(const GameState *g, int sq);
int  count_undeveloped(const GameState *g, int p);
const char *group_name(PropertyGroup grp);

/* board.c -- the choke points. Every timed modifier in the game is read in
   one of these four and nowhere else. square_value is built on the
   individual Rent.csv price; mortgage_value and building_cost on the
   Appendix B group figures (D18), which is why they are separate functions. */
int square_value(const GameState *g, int sq);
int purchase_price(const GameState *g, int sq);
int mortgage_value(const GameState *g, int sq);
int square_rent(const GameState *g, int sq, int diceTotal);
int building_cost(const GameState *g, int sq, bool hotel);

/* board.c -- building condition (LK 25-27). condition_tick runs once at the
   end of every round; maintenance_cost prices a full restoration. Table 3's
   rent bands are applied inside square_rent and read nowhere else. */
void condition_tick(GameState *g);
int  maintenance_cost(const GameState *g, int sq);
int  avg_condition(const Square *s);
int  repair_cost(const GameState *g, int sq);

/* board.c -- ageing (LK 16-17). depreciation_tick runs on the five-round
   cadence; the percentage it accumulates is read inside square_value. */
void depreciation_tick(GameState *g);
int  structural_renovation_cost(const GameState *g, int sq);

/* events.c -- the effect registry (D12). effect_modifier is the read side,
   consulted by all four choke points and by the two economy-wide rates;
   square is -1 for those, since they belong to no square. tick_effects and
   tick_cooldowns run once per round, ahead of the cadenced systems so that
   nothing created this round is aged by this round's tick. */
void effect_push(GameState *g, EffectKind kind, EffectScope scopeKind, int scope,
                 int magnitudePct, int owner, int rounds);
int  effect_modifier(const GameState *g, EffectKind kind, int square, int player);
bool effect_active(const GameState *g, EffectKind kind, int square, int player);
void tick_effects(GameState *g);
void tick_cooldowns(GameState *g);

/* events.c -- the cadenced systems (D13). draw_inflation is the only one that
   mutates stored values instead of pushing a record, because LK 14 makes it
   permanent (D12). */
void draw_inflation(GameState *g);
void market_review(GameState *g);
void national_event(GameState *g);
void regional_card(GameState *g);
void government_regulation(GameState *g);

/* events.c -- LK 10-11. fire_disaster strikes one developed property every
   ten rounds; auto_repairs runs every round, so damage pauses a building's
   income rather than ending it. A payout consumes the policy (D20). */
void fire_disaster(GameState *g);
void auto_repairs(GameState *g);

/* events.c -- Appendix A's deck. Shuffled once in game_init and walked as a
   circular queue, which is what "returned to the bottom" means for an array
   plus an index: nothing moves and every card appears before any repeats. */
void deck_init(GameState *g);
void draw_event_card(GameState *g, int p);

/* events.c -- LK 36 block queries. game.c owns every formatted block, so
   these answer rather than print. Both read the live registry, so there is no
   second copy of what is active to drift from it. GRP_NONE = nothing. */
int boom_group(const GameState *g, int *magnitudePct, int *roundsLeft);
int decline_group(const GameState *g, int *magnitudePct, int *roundsLeft);
const char *active_card(const GameState *g, int *magnitudePct, int *roundsLeft);
EconomicCondition prevailing_condition(const GameState *g);

/* players.c -- the decision engines. Placeholder bodies until milestone 6;
   the signatures are final, so that milestone touches players.c and no other
   file. decide_bid returns the amount to bid or 0 to withdraw permanently;
   minBid is the smallest legal bid right now. */
bool decide_buy(GameState *g, int p, int sq);
int  decide_bid(GameState *g, int p, int sq, int minBid);
int  decide_build(GameState *g, int p);
int  decide_maintenance(GameState *g, int p);
int  decide_insurance(GameState *g, int p, InsuranceType *tier);
bool decide_renovate(GameState *g, int p, int sq);
int  decide_liquidate(GameState *g, int p);
bool decide_bail(GameState *g, int p);

/* decide_bank returns the one Bank action to take on this landing (R1.8),
   writing the sum involved to *amount for the three that need one and the
   target to *square for BANK_REDEEM (D31). *square is -1 otherwise. */
BankAction decide_bank(GameState *g, int p, int *amount, int *square);

/* game.c -- the simulation engine. */
bool game_init(GameState *g, const char *csvPath);
void determine_order(GameState *g);
void play_turn(GameState *g, int p);
void play_round(GameState *g);
bool game_over(const GameState *g);
void game_run(GameState *g);
void land_on(GameState *g, int p, int sq, int diceTotal);
void end_block(void);
void round_summary(const GameState *g);
void market_conditions(const GameState *g);
void final_report(const GameState *g);

#endif /* TYPES_H */
