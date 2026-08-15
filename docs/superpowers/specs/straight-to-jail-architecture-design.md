# Straight to Jail — Architecture Design

**Project:** Straight to Jail — a pure-C implementation of the MONOPOLY-LK specification
**Spec:** [`assets/Assignment_1_unlocked.pdf`](../../../assets/Assignment_1_unlocked.pdf) (SCS 1301, due 2026-08-16 23:55)
**Supplemental data:** [`assets/Rent.csv`](../../../assets/Rent.csv) + the lecturer's clarification set
**Requirements:** [`docs/REQUIREMENTS.md`](../../REQUIREMENTS.md) — R-items and decisions D1–D26
**Revised:** 2026-08-10 — clarification set folded in
**Status:** approved

---

## Naming

*Straight to Jail* is the name of this project and repository. **MONOPOLY-LK** is the name the
assignment gives to the *ruleset* being implemented. The distinction matters because three things
are graded verbatim and must not be renamed:

| Thing | Value | Why fixed |
|-------|-------|-----------|
| Build command | `gcc *.c -o monopoly` | Mandated by spec §4 |
| Binary name | `monopoly` | Same |
| First output line | `MONOPOLY-LK Simulation` | Spec §5, graded character-for-character |

`make straight_to_jail` may exist as a convenience alias. Everywhere else — documents, plans,
comments, README, commit messages — the project is *Straight to Jail*.

**Consequence:** the ASCII-art banner currently in `main.c` cannot survive. §5 requires
`MONOPOLY-LK Simulation` as the first line of output. Either delete the art or move it behind a
`#ifdef SPLASH` that the graded build never defines.

---

## 1. What this document is

`REQUIREMENTS.md` answers **what** must be true. This document answers **how** the program is
shaped so that all of it can be true at once. It does not repeat the board table, the value tables,
or the D-decisions; those live in `REQUIREMENTS.md` and are cited by ID.

Companion documents:

- [`docs/superpowers/plans/straight-to-jail-staged.md`](../plans/straight-to-jail-staged.md) — the six-milestone build order
- [`docs/reference/`](../../reference/) — three notes explaining the C, the data structures, and the arithmetic this design depends on

---

## 2. Hard constraints

From spec §4 and the clarification set. These shape every decision below.

1. **Pure C, standard library only.** No external dependencies.
2. **`gcc *.c -o monopoly` must succeed with zero errors and zero warnings.** Development builds
   additionally pass `-std=c99 -Wall -Wextra -pedantic` clean. There is no test framework, because a
   glob build cannot tolerate a second `main`.
3. **The Table 5 module split:** `types.h`, `board.c`, `players.c`, `finance.c`, `events.c`,
   `game.c`, `main.c`.
4. **No global variables.** All state lives in one `GameState` on `main`'s stack, passed by pointer.
5. **No dynamic memory and no linked lists — anywhere, including the CSV reader.** Every collection
   is a fixed-size array indexed by `int`. Nothing here needs otherwise: 40 squares, 4 players,
   22 properties, 20 cards, 8 groups are all compile-time constants. Where the spec says "queue"
   (App A's deck), an array plus a head index is the whole implementation. Reading `Rent.csv` is the
   one place a reader might expect `malloc`; it uses a fixed stack buffer and writes straight into
   `g->board`, and POSIX `getline` is excluded for allocating (and for not being C99).
6. **Per-property values are read from `assets/Rent.csv` at runtime** with `fopen`/`fgets`/`fclose`
   (**R0.10**, **D27**). No transcribed copy of the file exists in any source file.
7. **Money is stored as `int`; ratio arithmetic is `double`, rounded at the boundary.** This is
   decision **D6′**, and it reverses the earlier integer-truncation rule. Interest, percentages,
   premiums and tax all compute in `double` and pass through one rounding helper on the way back to
   `int`. No `double` is ever *stored* as a balance.
8. **No `math.h`.** `round()` may require `-lm`, and the mandated build line does not supply it.
   Rounding is arithmetic: `(int)(v + (v >= 0 ? 0.5 : -0.5))`.
9. **Zero user interaction** after launch. Reading a data file is not interaction — nothing is read
   from stdin, and the run is still fully autonomous.

---

## 3. The shape of the program

```
main.c        seed, GameState on the stack, hand off
   |
   v
game.c        game_init -> determine_order -> [round loop] -> final_report
   |                                             |
   |                                    [turn loop, per solvent player]
   |                                             |
   |                                    play_turn: the 8 steps of Rule 3
   |                                             |
   |                                     land_on: dispatch on SquareType
   |                                             |
   +-----------------+---------------+-----------+-----------+
   |                 |               |                       |
board.c          finance.c       players.c               events.c
movement,        money, loans,   the four                effects,
ownership,       insurance,      decide_* engines        cadenced
value/rent       auctions,                               systems,
choke points     tax, net worth                          card deck
```

Two nested loops drive everything: rounds (≤ 500) and, inside each, one turn per solvent player in
the fixed order established by the opening roll-off. Between the last turn of a round and the first
turn of the next sits the **scheduler**, which fires the economic cadences.

### 3.1 Dependency direction

`board.c` and `finance.c` know nothing about strategies. `players.c` reads game state and returns
decisions but never mutates money directly — it calls into `finance.c`. `events.c` pushes effects and
mutates stored values but never prints a player's turn. `game.c` is the only module that
orchestrates; `main.c` is the only module with an entry point.

A change to a player personality touches exactly one file. A change to how rent is computed touches
exactly one function.

---

## 4. Core data structures

All of it fits in one struct, on the stack, roughly 7 KB.

```c
typedef struct {                     /* one board square; property fields idle otherwise */
    SquareType    type;              /* SQ_GO, SQ_PROPERTY, SQ_RAILWAY, SQ_COMMUNITY, ... */
    const char   *name;
    PropertyGroup group;             /* GRP_NONE for non-properties                      */
    unsigned      regions;           /* REGION_* bitmask, D14                            */

    /* Permanent-adjusted stored values (mutated only by inflation, D12).
       price and baseRent are INDIVIDUAL (Rent.csv, D7'/D18).
       mortgageValue, houseCost and hotelCost come from the group table (App B, D18). */
    int  price, baseRent, mortgageValue, houseCost, hotelCost;

    int  owner;                      /* -1 = Bank                                        */
    int  purchasedRound;             /* D19: age is round - purchasedRound; -1 = unowned */
    int  houses;                     /* 0..4; mutually exclusive with hotel              */
    bool hotel;
    bool mortgaged, loanLocked, damaged, structDamaged;

    int  depreciationPct, conditionPct, unmaintainedRounds;
    InsuranceType policy;
    int  policyRounds;
} Square;

typedef struct {
    bool active;
    int  principal;                  /* grows every round at ratePct                    */
    int  ratePct;                    /* frozen at issue, LK 13                          */
    int  issuedRound;                /* D19: matures at issuedRound + LOAN_ROUNDS       */
    int  termRounds;                 /* 20, extended by the LK 5 "extend" action        */
} Loan;

typedef struct {
    const char *name;
    Strategy    strat;
    int  cash, pos, jailTurns, taxesDue;
    bool bankrupt, jailed, sufferedLoss;   /* sufferedLoss gates Risk Taker's insurance */
    Loan loan;
} Player;

typedef struct {                     /* a single timed modifier — see §5               */
    EffectKind kind;
    int  scopeKind;                  /* SCOPE_GLOBAL | GROUP | REGION | SQUARE | PLAYER */
    int  scope;                      /* group index, region bitmask, square, or player  */
    int  magnitudePct;               /* signed: +25, -15                                */
    int  owner;                      /* player the effect belongs to; -1 = everyone     */
    int  roundsLeft;
} Effect;

typedef struct {
    int  inflationPct;               /* most recent draw, for the LK 36 block           */
    int  interestRatePct;            /* current rate for NEW loans only (D21)           */
    int  incomeTaxPct;               /* seeded at 15, inflation-adjusted (D2')          */
    int  groupCooldown[GRP_COUNT];   /* LK 33: 30-round bar on re-selection             */
    int  lastBoomGroup, lastDeclineGroup;
    int  activeRegulation;           /* -1 = none                                       */
    Effect effects[MAX_EFFECTS];
    int    effectCount;
} Economy;

typedef struct { int cards[DECK_SIZE]; int head; } EventDeck;   /* array + index, App A */

typedef struct {
    Square    board[NUM_SQUARES];
    Player    players[NUM_PLAYERS];
    int       order[NUM_PLAYERS];
    Economy   econ;
    EventDeck deck;
    int       round;                 /* 1-based                                         */
} GameState;
```

Why a fat `Square` rather than a union: 22 of the 40 squares use the property fields, and the
non-property squares waste a few dozen bytes each. A union would save under 1 KB and cost a
discriminated-access dance on every read. Direct indexing into a flat array is the simplest thing
that works, and simplicity is the graded quality here.

Why `purchasedRound` rather than an `age` counter: **D19**. Age exists only after purchase, so it is
a derived value, and deriving it removes the possibility of the counter and the ownership flag
disagreeing.

### 4.1 Loading `Rent.csv`

Two sources populate the 22 property squares, and **D18** keeps them apart because they answer
different questions:

| Source | Supplies | Form |
|--------|----------|------|
| Appendix B group table | house cost, hotel cost, mortgage value | compiled in, `static const` |
| `assets/Rent.csv` | **individual** purchase price and base rent | **read at runtime** |

The individual values are deliberately *not* compiled in (**R0.10**, **D7′**). The lecturer's file is
the single source of truth; a transcribed copy would be a second one, free to drift out of step the
first time either is edited. Editing a price in the CSV changes the next run without recompiling —
which is the practical test of whether this is really file handling or just a table with extra steps.

`board_init` therefore runs in two passes. The compiled-in layout and group-derived values go down
first, then the CSV overlays `price` and `baseRent` onto the property squares:

```c
bool board_init(GameState *g, const char *csvPath);   /* false = could not load */
```

**The join is on the property name, not on row order.** The CSV is grouped by colour while the board
is in board order, so pairing them positionally would be a silent hazard the first time either is
reordered. Matching `LAYOUT[i].name` against the CSV's `Property` column means a name that does not
match is an error the loader can name and report.

**Everything is fixed-size.** One `char line[256]` on the stack — the longest real line is about 45
bytes — and the destination array already exists. There is nothing to allocate, which is why R0.5
survives contact with file I/O. `getline` would have allocated and is not C99 besides.

**Validation is total**, because a partially-loaded board is worse than no board: the first player to
land on a gap would buy a property for nothing. Every row must have exactly four fields, a property
name on the board, a group agreeing with the board's, and a strictly positive integer price and base
rent; no property may appear twice; and all 22 must be present when the file closes. `strtol` rather
than `atoi` throughout, because `atoi` cannot distinguish `"0"` from `"not a number"`.

**Failure is fatal and quiet on stdout** (**D27**). `main` calls `game_init` *before* printing the
pre-game banner, so a failed load emits its diagnostic on stderr and returns 1 with zero bytes
written to stdout — no partial §5 block, no pollution of the graded stream (**R5.7**). There is no
fallback table, by design.

**Both line endings are accepted.** The supplied file is CRLF while `.gitattributes` normalises to LF
on checkout, so the reader strips whichever it finds rather than assuming.

---

## 5. The effect registry

This is the one place the design departs from an obvious reading of the spec, and it is the
load-bearing decision of the whole program.

### The problem

Six separate systems apply timed percentage modifiers:

| System | Cadence | Scope | Duration |
|--------|---------|-------|----------|
| Market boom / decline (LK 30–34) | every 10 rounds | one colour group | 10 rounds |
| National economic event (LK 18) | every 15 rounds | all players, often a region | 15 rounds |
| Regional development card (Table 4) | every 15 rounds | named squares | 15 rounds |
| Government regulation (LK 24) | every 20 rounds | global | until replaced |
| National Event Card (App A) | on landing | **the drawing player** | 15 rounds, *plus* per-card inner durations |
| Inflation (LK 12–14) | every 10 rounds | everything | permanent |

Appendix A is what breaks the naive approach. A card affects only the player who drew it, for 15
rounds, and may carry its own shorter duration — "Hotels earn double rent for 5 rounds". Four players
can each hold several overlapping card effects simultaneously. A flat set of `Economy` fields has
exactly one slot per system and silently overwrites.

Rule-LK 34 then requires that when several effects hit the same group at once they **stack
cumulatively** — so the representation has to keep them all, not collapse them.

### The solution

A fixed-size array of `Effect` records. Every timed system reduces to *push one Effect*.

```c
void  effect_push(GameState *g, EffectKind k, int scopeKind, int scope,
                  int magnitudePct, int owner, int rounds);
int   effect_modifier(const GameState *g, EffectKind k, int square, int player);
void  tick_effects(GameState *g);      /* decrement; compact out the expired */
```

`effect_modifier` walks the array, selects the records whose kind, scope and owner match, and sums
their magnitudes (LK 34 reads naturally as additive). `tick_effects` runs once at the end of every
round.

Three properties fall out for free:

1. **Cumulative stacking (LK 34)** is just "keep walking the array".
2. **Reversion on expiry (LK 35)** — "values revert to the current market-adjusted baseline, not the
   original constant" — happens automatically, because the effect was never written into the stored
   value. Drop the record and the multiplier disappears.
3. **Per-player card effects (App A)** are representable, because `owner` is a field rather than an
   assumption.

`effect_modifier` ships from milestone 2 as a stub returning `0`, so the choke points are written
once, in their final shape, and gain teeth in milestone 4 without a single edit outside `events.c`.

### Permanent versus temporary (decision D12)

| | Mechanism | Example |
|---|---|---|
| **Permanent** | mutates the stored field in `Square` | inflation (LK 14), a card's "property values increase by 10%" |
| **Temporary** | lives in the registry, read at access time | booms, declines, regional cards, regulations, event cards |

Both are read through the same choke points, so no caller needs to know which kind it faces.

---

## 6. Choke points

Four functions are the **only** places a modifier is ever consulted. Every other module asks them and
takes the answer.

```c
int square_value   (const GameState *g, int sq);
int square_rent    (const GameState *g, int sq, int diceTotal);
int building_cost  (const GameState *g, int sq, bool hotel);
int mortgage_value (const GameState *g, int sq);
```

- `square_value` composes: stored **individual** price → depreciation (LK 16) → structural damage
  (LK 28) → active `VALUE_MUL` effects.
- `square_rent` composes: stored **individual** base rent → development multiplier (Table 6) →
  building-condition band (Table 3) → damaged / closed gates → active `RENT_MUL` effects. It also
  routes railway rent (by count owned) and utility rent (by dice) to their own tables.
- `building_cost` composes the stored group cost with subsidy, boom and currency-depreciation effects.
- `mortgage_value` composes the stored **group** mortgage figure with active `MORTGAGE_MUL` effects.
  It is separate from `square_value` precisely because **D18** gives the two different bases.

Two derived helpers sit directly on top and are likewise the single home for their rule:

```c
int total_assets (const GameState *g, int p);   /* D16: sum of square_value over owned properties */
int repair_cost  (const GameState *g, int sq);  /* D1:  50% of construction cost on the square    */
```

The reason this is a rule and not a suggestion: roughly a dozen call sites need a property's value
and roughly twenty need its rent. If the modifier arithmetic is inlined at each, the numbers drift
the first time a new effect is added, and the drift is invisible — the program still runs, just
wrong. Funnelling through four functions makes a missed modifier a structural problem rather than a
silent accounting error.

### 6.1 The money boundary

Decision **D6′** puts exactly one function between `double` arithmetic and stored balances:

```c
int money_round(double v);   /* (int)(v + (v >= 0 ? 0.5 : -0.5)) — no math.h, R0.9 */

int apply_pct(int value, int pct)   { return money_round(value * (1.0 + pct / 100.0)); }
int pct_of   (int value, int pct)   { return money_round(value * (pct  / 100.0));      }
```

Every percentage in the program goes through one of these. A `double` never leaves them.

---

## 7. The scheduler

Rule 3's eight steps run per turn; the economic systems run per round on staggered cadences. Order is
fixed by decision **D13** and is load-bearing — the same events in a different order produce different
balances.

**Per turn (Rule 3):**

1. Resolve outstanding penalties — jail, taxes due. **The only point at which maintenance may be
   performed** (LK 27).
2. Roll two dice.
3. Move clockwise, wrapping mod 40; credit GO on pass or land.
4. Resolve the landing action for the square type.
5. Purchase if eligible — declining sends it immediately to auction.
6. Construct if eligible — monopoly only, built evenly.
7. Complete financial transactions — loan actions only on the Bank square.
8. End turn.

**End of round, in this exact order:**

```
loan interest accrues (every round, at each loan's ISSUED rate)
loan default check     (round >= issuedRound + termRounds with principal outstanding)
building condition -2%; unmaintained counters advance
insurance countdown + 3-round expiry warnings
automatic repairs
  round % 5  == 0  ->  depreciation accrual   (properties owned > 50 rounds)
  round % 10 == 0  ->  inflation draw, market review, disaster roll
  round % 15 == 0  ->  national economic event, regional development card
  round % 20 == 0  ->  government regulation
tick_effects (decrement durations, compact expired)
print Round N Summary
print Current Market Conditions
```

Interest accrues **before** the default check so a loan can default on the interest that just
compounded. Effects tick **after** the cadenced systems fire, so an effect created this round lives
its full stated duration rather than losing a round to its own creation. Property age needs no tick —
it is `round - purchasedRound` (**D19**).

---

## 8. Strategies

The four personalities live behind seven functions with identical signatures, each switching on
`Player.strat`:

```c
bool decide_buy        (GameState *g, int p, int sq);
int  decide_bid        (GameState *g, int p, int sq, int currentBid);   /* 0 = withdraw */
void decide_bank       (GameState *g, int p);
void decide_insurance  (GameState *g, int p);
void decide_build      (GameState *g, int p);
void decide_maintenance(GameState *g, int p);
void decide_renovate   (GameState *g, int p, int sq);
```

They ship as throwaway placeholders from milestone 2 (buy if affordable, bid to 60% of value) so that
every milestone has a running game to observe, then are replaced body-for-body in milestone 6.
Signatures never change, so no other file is touched when the personalities land.

Spec §3 states each behaviour in prose that sometimes needs a formula — "bids until the property
reaches 120% of its estimated market value" needs *estimated market value* defined. Those proxies are
decision **D9**.

---

## 9. Output

Spec §5 prescribes exact console messages, graded character-for-character. Three rules follow:

- Messages are emitted **at the event site**, not collected and printed later, so the interleaving
  matches a turn's actual causal order.
- Every monetary figure goes through `fmt_lkr`, which renders thousands separators (`LKR 12,300`).
  No `printf("%d")` on money, anywhere.
- Blank lines are content (**D26**, revised twice). Within a block a labelled category opens a new
  group; between blocks, every message type is terminated by `end_block()` in `game.c`, so two
  different kinds of output never run together. The `Round N Summary` and `Current Market
  Conditions` tables are the exceptions and stay internally compact.

The two block-format outputs — `Round N Summary` (45-character rules) and `Current Market Conditions`
(41-character rules) — print at the end of **every** round, in that order.

Because §5 wording is graded and cheap to get right at write-time, each milestone verifies its own
templates as they are written. There is no single late audit stage holding all the risk.

---

## 10. Verification without a test framework

The glob build forbids a second `main`, so there is no unit-test binary. What replaces it:

1. **Seeded determinism.** `./monopoly 42` is byte-for-byte reproducible, so any behavioural change
   is a diff.
2. **Dump-and-verify.** A temporary dump block in `main` prints a table (the 40 squares, the effect
   registry, a loan schedule), checked against the spec by eye, then deleted before commit.
3. **Invariant checks under `#ifdef DEBUG`.** Even building across a group, cash never negative
   outside the debt-recovery window, all 22 properties accounted for, effect count within bounds,
   no square loan-locked without an active loan.
4. **Per-milestone output verification.** Every milestone names commands to run and specific things
   to look for.
5. **Multi-seed runs at the end.** At least five seeds, including one full 500-round game and one
   that ends early by bankruptcy.

---

## 11. Risks

| Risk | Mitigation |
|------|-----------|
| Six days to deadline with no source committed | Six milestones, each ending in a submittable program. Losing a day costs depth, never a working build |
| §5 wording drift | Templates verified inside the milestone that writes them, not in one late audit |
| `int` overflow from compounding interest and stacked inflation | Headroom analysis: 50 inflation draws averaging +4% is ≈7×; a 20-round loan at 15% is ≈16×; worst realistic principal stays under 10 M, well inside `int`. A `#ifdef DEBUG` guard asserts principal < `INT_MAX / 2` |
| `double` leaking into stored balances | One rounding helper; `apply_pct` and `pct_of` are the only callers; grep for `double` outside `finance.c` in the final audit |
| Effect registry overflow | `MAX_EFFECTS` sized for the worst case (4 players × several card effects + 4 global systems + 3 square-scoped regional effects), with a `#ifdef DEBUG` assert |
| Loans rarely repayable, since repayment requires landing on square 38 | Intended by the clarification. Defaults are a live and frequent outcome, which is what exercises LK 6–7 |
| Strategy stalemate — nobody goes bankrupt, all 500 rounds run every time | Acceptable; Rule 15 defines the net-worth tiebreak for exactly this |
| Spec ambiguity resolved differently in two modules | Every D-decision is implemented exactly once, at a choke point, with a comment citing its ID |

---

## 12. Definition of done

- `gcc *.c -o monopoly` and `gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly` both silent.
- `./monopoly` and `./monopoly <seed>` run to completion with no interaction.
- A full 500-round game completes; a game with early bankruptcies ends with the winner block.
- `./monopoly 42` twice is byte-identical.
- Every §5 template audited line-by-line against emitted output.
- No `malloc`, `calloc`, `realloc`, `getline`, linked lists, globals, or `math.h` in the tree.
- Every price and base rent traces to a row of `Rent.csv`; editing the file changes the next run
  without recompiling; a missing or malformed file exits 1 on stderr with nothing on stdout.
- Every R-item in `REQUIREMENTS.md` checked; every D-decision implemented once and cited.
