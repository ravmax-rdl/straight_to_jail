# Straight to Jail — Architecture Design

**Project:** Straight to Jail — a pure-C implementation of the MONOPOLY-LK specification
**Spec:** [`assets/Assignment_1_unlocked.pdf`](../../../assets/Assignment_1_unlocked.pdf) (SCS 1301, due 2026-08-16)
**Requirements:** [`docs/REQUIREMENTS.md`](../../REQUIREMENTS.md) — R-items and decisions D1–D15
**Date:** 2026-07-28
**Status:** approved

---

## Naming

*Straight to Jail* is the name of this project and repository. **MONOPOLY-LK** is the name the
assignment gives to the *ruleset* being implemented. The distinction matters because two things
are graded verbatim and must not be renamed:

| Thing | Value | Why fixed |
|-------|-------|-----------|
| Build command | `gcc *.c -o monopoly` | Mandated by spec §4 |
| Binary name | `monopoly` | Same |
| First output line | `MONOPOLY-LK Simulation` | Spec §5 output template, graded character-for-character |

`make straight_to_jail` may exist as a convenience alias. Everywhere else — documents, plans,
comments, README, commit messages — the project is *Straight to Jail*.

---

## 1. What this document is

`REQUIREMENTS.md` answers **what** must be true. This document answers **how** the program is
shaped so that all of it can be true at once. It does not repeat the board table, the value
tables, or the D-decisions; those live in `REQUIREMENTS.md` and are cited by ID.

The companion documents are:

- [`docs/superpowers/plans/2026-07-28-straight-to-jail-staged.md`](../plans/2026-07-28-straight-to-jail-staged.md) — the 35-stage build order
- [`docs/learning/`](../../learning/) — three reference documents explaining the C, the data
  structures, and the arithmetic this design depends on

---

## 2. Hard constraints

These come from spec §4 and shape every decision below.

1. **Pure C, standard library only.** No external dependencies of any kind.
2. **`gcc *.c -o monopoly` must succeed with zero errors and zero warnings.** Development builds
   additionally pass `-Wall -Wextra` clean. There is no test framework, because a glob build
   cannot tolerate a second `main`.
3. **At minimum the Table 5 module split:** `types.h`, `board.c`, `players.c`, `finance.c`,
   `events.c`, `game.c`, `main.c`.
4. **Global variables avoided.** All state lives in one `GameState` on `main`'s stack, passed by
   pointer.
5. **Dynamic allocation only where justified.** Nothing here is justified — every collection has a
   fixed, known size (40 squares, 4 players, 20 cards, 8 groups). No `malloc`.
6. **All monetary values are `int`.** No floating point anywhere in the money path.
7. **Zero user interaction** after launch.

---

## 3. The shape of the program

```
main.c        seed, GameState on the stack, pre-game banner, hand off
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
choke points     net worth                               card deck
```

Two nested loops drive everything: rounds (≤ 500) and, inside each, one turn per solvent player
in the fixed order established by the opening roll-off. Between the last turn of a round and the
first turn of the next sits the **scheduler**, which fires the economic cadences.

### 3.1 Dependency direction

`board.c` and `finance.c` know nothing about strategies. `players.c` reads game state and returns
decisions but never mutates money directly — it calls into `finance.c`. `events.c` pushes effects
and mutates stored values but never prints a player's turn. `game.c` is the only module that
orchestrates; `main.c` is the only module with an entry point.

This means a change to a player personality touches exactly one file, and a change to how rent is
computed touches exactly one function.

---

## 4. Core data structures

All of it fits in one struct, on the stack, roughly 6 KB.

```c
typedef struct {                     /* one board square; property fields idle otherwise */
    SquareType    type;              /* SQ_GO, SQ_PROPERTY, SQ_RAILWAY, SQ_UTILITY, ... */
    const char   *name;
    PropertyGroup group;             /* GRP_NONE for non-properties                     */
    unsigned      regions;           /* REGION_* bitmask, D14                           */

    int  price, baseRent, houseCost, hotelCost, mortgageValue;  /* permanent-adjusted, D12 */

    int  owner;                      /* -1 = Bank                                       */
    int  houses;                     /* 0..4; mutually exclusive with hotel             */
    bool hotel;
    bool mortgaged, loanLocked, damaged, structDamaged;

    int  age, depreciationPct, conditionPct, unmaintainedRounds, closedRounds;
    InsuranceType policy;
    int  policyRounds;
} Square;

typedef struct { bool active; int principal, ratePct, roundsLeft; } Loan;

typedef struct {
    const char *name;
    Strategy    strat;
    int  cash, pos, jailTurns, taxesDue;
    bool bankrupt, jailed, sufferedLoss;   /* sufferedLoss gates Risk Taker's insurance */
    Loan loan;
} Player;

typedef struct {                     /* a single timed modifier — see §5              */
    EffectKind kind;
    int  scopeKind;                  /* SCOPE_GLOBAL | GROUP | REGION | SQUARE | PLAYER */
    int  scope;                      /* group index, region bitmask, square, or player  */
    int  magnitudePct;               /* signed: +25, -15                                */
    int  owner;                      /* player the effect belongs to; -1 = everyone     */
    int  roundsLeft;
} Effect;

typedef struct {
    int  inflationPct;               /* most recent draw, for the LK 36 block           */
    int  interestRatePct;            /* current rate for NEW loans only (LK 13)         */
    int  groupCooldown[GRP_COUNT];   /* LK 33: 30-round bar on re-selection             */
    int  lastBoomGroup, lastDeclineGroup;
    Effect effects[MAX_EFFECTS];
    int    effectCount;
} Economy;

typedef struct { int cards[DECK_SIZE]; int head; } EventDeck;   /* circular queue, App A */

typedef struct {
    Square    board[40];
    Player    players[4];
    int       order[4];
    Economy   econ;
    EventDeck deck;
    int       round;                 /* 1-based                                        */
} GameState;
```

Why a fat `Square` rather than a union: 22 of the 40 squares use the property fields, and the
non-property squares waste a few dozen bytes each. A union would save under 1 KB and cost a
discriminated-access dance on every read. Direct indexing into a flat array is the simplest thing
that works, and simplicity is the graded quality here.

---

## 5. The effect registry

This is the one place the design departs from an obvious reading of the spec, and it is the load-
bearing decision of the whole program.

### The problem

Six separate systems apply timed percentage modifiers:

| System | Cadence | Scope | Duration |
|--------|---------|-------|----------|
| Market boom / decline (LK 30–34) | every 10 rounds | one colour group | 10 rounds |
| National economic event (LK 18) | every 15 rounds | all players, often a region | 15 rounds |
| Regional development card (Table 4) | every 15 rounds | named squares | 15 rounds |
| Government regulation (LK 24) | every 20 rounds | global | until replaced |
| National Event Card (Appendix A) | on landing | **the drawing player** | 15 rounds, *plus* per-card inner durations |
| Inflation (LK 12–14) | every 10 rounds | everything | permanent |

Appendix A is what breaks the naive approach. A card affects only the player who drew it, for 15
rounds, and may carry its own shorter duration — "Hotels earn double rent for 5 rounds". Four
players can each be holding several overlapping card effects simultaneously. A flat set of
`Economy` fields (`boomGroup`, `econEvent`, `regionalCard`, …) has exactly one slot per system and
silently overwrites.

Rule-LK 34 then requires that when several effects hit the same group at once, they **stack
cumulatively** — so the representation has to keep them all, not collapse them.

### The solution

A fixed-size array of `Effect` records. Every timed system reduces to *push one Effect*.

```c
void  effect_push(GameState *g, EffectKind k, int scopeKind, int scope,
                  int magnitudePct, int owner, int rounds);
int   effect_modifier(const GameState *g, EffectKind k, int square, int player);
void  tick_effects(GameState *g);      /* decrement; compact out the expired */
```

`effect_modifier` walks the list, selects the records whose kind, scope and owner match, and
composes their magnitudes. `tick_effects` runs once at the end of every round.

Three properties fall out for free:

1. **Cumulative stacking (LK 34)** is just "keep walking the list".
2. **Reversion on expiry (LK 35)** — "values revert to the current market-adjusted baseline, not
   the original constant" — happens automatically, because the effect was never written into the
   stored value in the first place. Drop the record and the multiplier disappears.
3. **Per-player card effects (Appendix A)** are representable, because `owner` is a field rather
   than an assumption.

### Permanent versus temporary (decision D12)

| | Mechanism | Example |
|---|---|---|
| **Permanent** | mutates the stored field in `Square` | inflation (LK 14), "property values increase by 10%" |
| **Temporary** | lives in the effect registry, read at access time | booms, declines, regional cards, regulations, event cards |

Both are then read through the same choke points, so no caller needs to know which kind it is
dealing with.

---

## 6. Choke points

Three functions are the **only** places a modifier is ever consulted. Every other module asks
them and takes the answer.

```c
int square_value  (const GameState *g, int sq);
int square_rent   (const GameState *g, int sq, int diceTotal);
int building_cost (const GameState *g, int sq, bool hotel);
```

`square_value` composes: stored price → depreciation (LK 16) → structural damage (LK 28) →
active VALUE_MUL effects. `square_rent` composes: base rent → development multiplier (Table 6) →
building condition band (Table 3) → damaged/closed gates → active RENT_MUL effects; it also
routes railway rent (by count owned) and utility rent (by dice) to their own tables.
`building_cost` composes stored cost with subsidy, boom, and Currency-Depreciation effects.

The reason this is a rule and not a suggestion: there are roughly a dozen call sites that need a
property's value and roughly twenty that need its rent. If the modifier arithmetic is inlined at
each, the numbers drift the first time a new effect is added, and the drift is invisible — the
program still runs, just wrong. Funnelling through three functions makes a missed modifier a
compile-time-shaped problem rather than a silent accounting error.

---

## 7. The scheduler

Rule 3's eight steps run per turn; the economic systems run per round, on staggered cadences.
Order is fixed by decision D13 and is load-bearing — the same events in a different order produce
different balances.

**Per turn (Rule 3):**

1. Resolve outstanding penalties — jail, taxes due. **The only point at which maintenance may be
   performed** (LK 27).
2. Roll two dice.
3. Move clockwise, wrapping mod 40; credit GO on pass or land.
4. Resolve the landing action for the square type.
5. Purchase if eligible — decline sends it immediately to auction.
6. Construct if eligible — monopoly only, built evenly.
7. Complete financial transactions — loan actions only on the Bank square.
8. End turn.

**End of round, in this exact order:**

```
loan interest accrues (every round, at each loan's ISSUED rate)
loan default check
property age +1, building condition -2%
insurance countdown + 3-round expiry warnings
automatic repairs
  round % 5  == 0  ->  depreciation accrual
  round % 10 == 0  ->  inflation draw, market review, disaster roll
  round % 15 == 0  ->  national economic event, regional development card
  round % 20 == 0  ->  government regulation
tick_effects (decrement durations, compact expired)
print Round N Summary
print Current Market Conditions
```

Interest accrues **before** the default check so a loan can default on the interest that just
compounded. Effects tick **after** the cadenced systems fire, so an effect created this round
lives its full stated duration rather than losing a round to its own creation.

---

## 8. Strategies

The four personalities live behind seven functions with identical signatures, each switching on
`Player.strat`:

```c
bool decide_buy       (GameState *g, int p, int sq);
int  decide_bid       (GameState *g, int p, int sq, int currentBid);   /* 0 = withdraw */
void decide_bank      (GameState *g, int p);
void decide_insurance (GameState *g, int p, int sq);
void decide_build     (GameState *g, int p);
void decide_maintenance(GameState *g, int p);
void decide_renovate  (GameState *g, int p, int sq);
```

They are implemented as throwaway placeholders early (buy if affordable, bid to 60% of value) so
that every stage before Phase 8 has a running game to observe, then replaced body-for-body in
stages 30–33. Signatures never change, so no other file is touched when the personalities land.

Spec §3 states each behaviour in prose that sometimes needs a formula — "bids until the property
reaches 120% of its estimated market value" needs *estimated market value* defined. Those proxies
are decision D9.

---

## 9. Output

Spec §5 prescribes exact console messages for every significant event, and they are graded
character-for-character. Two rules follow:

- Messages are emitted **at the event site**, not collected and printed later, so that the
  interleaving matches a turn's actual causal order.
- Every monetary figure goes through `fmt_lkr`, which renders thousands separators
  (`LKR 12,300`). No `printf("%d")` on money, anywhere.

The two block-format outputs — `Round N Summary` and `Current Market Conditions` — print at the
end of **every** round, in that order, with the `=` and `-` rule lines exactly as §5 shows.

---

## 10. Verification without a test framework

The glob build forbids a second `main`, so there is no unit-test binary. What replaces it:

1. **Seeded determinism.** `./monopoly 42` is byte-for-byte reproducible, so any behavioural
   change is a diff.
2. **Dump-and-verify.** A temporary dump block in `main` prints a table (the 40 squares, the
   effect registry, a loan schedule), checked against the spec by eye, then deleted before commit.
3. **Invariant checks under `#ifdef DEBUG`.** Even-building across a group, cash never negative
   outside the debt-recovery window, all 22 properties accounted for, effect count within bounds.
4. **Per-stage output verification.** Every stage in the plan names a command to run and a
   specific thing to look for in its output.
5. **Multi-seed runs at the end.** At least five seeds, including one full 500-round game and one
   that ends early by bankruptcy.

---

## 11. Risks

| Risk | Mitigation |
|------|-----------|
| `int` overflow — compounding interest and stacked inflation over 500 rounds | Overflow headroom analysis in the economic-math document; cap stored values; interest computed as `p / 100 * rate + p % 100 * rate / 100` where headroom is tight |
| §5 wording drift | Stage 34 is a dedicated line-by-line audit against every template extracted from the PDF |
| Effect registry overflow | `MAX_EFFECTS` sized for worst case (4 players × several card effects + 4 global systems), with a `#ifdef DEBUG` assert |
| Strategy stalemate — nobody ever goes bankrupt, all 500 rounds run every time | Acceptable; Rule 15 defines the net-worth tiebreak for exactly this |
| Spec ambiguity resolved differently in two modules | Every D-decision is implemented exactly once, at a choke point, with a comment citing its ID |

---

## 12. Definition of done

- `gcc *.c -o monopoly` and `gcc -Wall -Wextra *.c -o monopoly` both silent.
- `./monopoly` and `./monopoly <seed>` run to completion with no interaction.
- A full 500-round game completes; a game with early bankruptcies ends with the winner block.
- Every §5 template audited line-by-line against emitted output.
- Every R-item in `REQUIREMENTS.md` checked; every D-decision implemented once and cited.
