---
title: "Program Design and Data Structures"
subtitle: "How Straight to Jail is put together, and why"
author: "Straight to Jail — SCS 1301"
date: "2026-07-28"
lang: en
toc: true
numbersections: true
---

# What this document is for

The MONOPOLY-LK specification describes about ninety rules across three sections and five
appendices. Six of those rules are timed economic systems that all modify the same numbers on
different schedules, and their effects are required to stack.

Written naively, that is a program where changing one rule breaks three others, and where a wrong
number in the round summary could originate in any of a dozen places. This document explains the
handful of structural decisions that stop that from happening.

The recurring theme: **when many systems modify the same value, funnel them through one place.**

---

# Modelling forty heterogeneous squares

The board holds eleven kinds of square. Properties have a price, a rent, an owner, buildings, and
an insurance policy. Railways have a price and an owner but can never be developed. GO has none of
these. Jail has none of these and also a special arrival rule.

There are three standard ways to model this.

## Option A — a fat struct

One `Square` type with every field any square kind might need, plus a `SquareType` tag saying
which kind it is.

```c
typedef struct {
    SquareType    type;
    const char   *name;
    PropertyGroup group;
    int  price, baseRent, houseCost, hotelCost, mortgageValue;
    int  owner, houses;
    bool hotel, mortgaged, loanLocked, damaged, structDamaged;
    int  age, depreciationPct, conditionPct, unmaintainedRounds, closedRounds;
    InsuranceType policy;
    int  policyRounds;
} Square;

Square board[40];
```

GO carries eighteen fields it will never use. The waste is about 80 bytes per non-property square,
or roughly 1.4 KB across the whole board.

## Option B — a tagged union

```c
typedef struct {
    SquareType  type;
    const char *name;
    union {
        PropertyData property;
        RailwayData  railway;
        UtilityData  utility;
    } as;
} Square;
```

Saves the memory. Costs a discriminated access at every use:

```c
if (sq->type != SQ_PROPERTY) return 0;
return sq->as.property.baseRent;
```

Read the wrong union member and you get garbage with no warning, because the compiler has no way
to check the tag matches the member.

## Option C — parallel arrays

A `PropertyData properties[22]` alongside the board, with an index on each square. Saves the
memory and keeps type safety, at the cost of an indirection and a second index to keep in sync.

## What this project uses, and why

**Option A.** The reasoning:

- 1.4 KB is nothing. The entire `GameState` is about 6 KB either way.
- Direct indexing (`g->board[sq].price`) is the simplest expression that can work, and this is a
  program graded partly on clarity.
- The union's failure mode — reading the wrong member — is silent. A wasted field is not a bug;
  a misread union is.
- Several fields genuinely span kinds. Railways and utilities are mortgageable and can be
  loan-locked, so `mortgaged`, `owner`, and `loanLocked` are not property-only anyway.

The tag still does real work. It drives `land_on`'s dispatch and gates every property-only
operation:

```c
bool is_purchasable(const GameState *g, int sq)
{
    SquareType t = g->board[sq].type;
    return t == SQ_PROPERTY || t == SQ_RAILWAY || t == SQ_UTILITY;
}
```

The lesson generalises: **the cheapest representation is not always the best one.** Optimise for
the failure mode that hurts, which here is a silent wrong number, not a kilobyte.

---

# Permanent versus temporary effects

This is the central design decision of the program.

## The problem

Six systems apply percentage modifiers to the same values:

| System | Cadence | Scope | Duration |
|--------|---------|-------|----------|
| Inflation | every 10 rounds | everything | **permanent** |
| Market boom / decline | every 10 rounds | one colour group | 10 rounds |
| National economic event | every 15 rounds | all players | 15 rounds |
| Regional development card | every 15 rounds | named squares | 15 rounds |
| Government regulation | every 20 rounds | global | 20 rounds |
| National Event Card | on landing | **the drawing player** | 15 rounds, plus inner durations |

Rule-LK 34 requires that when several hit the same group at once, **all percentage changes are
cumulative**. Rule-LK 35 requires that when one expires, values return to "their normal
market-adjusted values unless another active event is still influencing them".

Those two sentences are the whole difficulty. Read them together: an effect must be removable
*without* disturbing the others, and what remains after removal must be whatever the surviving
effects say — not the original constant.

## Why the obvious approach fails

The obvious approach is to apply the modifier to the stored value when the event fires, and undo
it when the event expires:

```c
sq->price = pct(sq->price, +20);      /* boom starts */
...
sq->price = pct(sq->price, -20);      /* boom ends — WRONG */
```

This breaks three ways.

**It does not round-trip.** $1000 \times 1.20 = 1200$, and $1200 \times 0.80 = 960$. The price is
now permanently 4% lower than it started. Over a 500-round game with fifty market reviews, the
drift is enormous.

**It cannot express "unless another active event is still influencing them".** If a boom and a
regional card both raised the same square and the boom expires, what should the value be? With
mutated storage there is no way to know what portion belonged to which effect.

**It cannot represent per-player effects at all.** An Appendix A card affects only the player who
drew it. A stored value is shared by everyone.

The second and third failures are fatal — no amount of careful arithmetic fixes them.

## The split

Divide effects by whether the spec says they are permanent:

| | Mechanism | Examples |
|---|---|---|
| **Permanent** | mutate the stored field | inflation (LK 14 says values are *recalculated*) |
| **Temporary** | record it; read it at access time | booms, declines, cards, events, regulations |

Inflation is genuinely permanent — Rule-LK 14 states the new value *is* the value. Mutating
storage is correct there and there alone.

Everything else is temporary, and temporary effects are never written into storage. They live in a
registry and are consulted when a value is read.

## The registry

```c
typedef struct {
    EffectKind kind;          /* what it modifies: value, rent, build cost, ... */
    int  scopeKind;           /* GLOBAL | GROUP | REGION | SQUARE | PLAYER      */
    int  scope;               /* group index, region bitmask, square, or player */
    int  magnitudePct;        /* signed: +25, -15                               */
    int  owner;               /* the player it belongs to; -1 = everyone        */
    int  roundsLeft;
} Effect;

typedef struct {
    /* ... */
    Effect effects[MAX_EFFECTS];
    int    effectCount;
} Economy;
```

A fixed array. No allocation, no linked list, no ordering requirement.

Three operations:

```c
void effect_push(GameState *g, EffectKind k, int scopeKind, int scope,
                 int magnitudePct, int owner, int rounds);
int  effect_modifier(const GameState *g, EffectKind k, int square, int player);
void tick_effects(GameState *g);
```

`effect_push` appends. `effect_modifier` sums the magnitudes of every matching record.
`tick_effects` decrements every countdown and removes the expired.

## What falls out for free

**Cumulative stacking (LK 34).** Summing the matching magnitudes *is* cumulative composition.
There is no separate code path.

**Reversion on expiry (LK 35).** Drop the record and its contribution disappears. Whatever
survives is exactly "the current market-adjusted value under the remaining active events". This
requires no code at all — it is a consequence of never having written the effect into storage.

**Per-player effects (Appendix A).** `owner` is a field, so four players can hold overlapping card
effects simultaneously and each sees only their own.

**Inner durations.** "Hotels earn double rent for 5 rounds" pushes with `rounds = 5` while a
15-round card effect pushes with `rounds = 15`. The registry does not care that they differ.

## Scope resolution

```c
static bool effect_in_scope(const GameState *g, const Effect *e, int square)
{
    switch (e->scopeKind) {
    case SCOPE_GLOBAL: return true;
    case SCOPE_SQUARE: return square == e->scope;
    case SCOPE_GROUP:  return g->board[square].group == (PropertyGroup)e->scope;
    case SCOPE_REGION: return (g->board[square].regions & (unsigned)e->scope) != 0;
    case SCOPE_PLAYER: return true;    /* the owner field does the filtering */
    }
    return false;
}
```

Five cases, each one line. Adding a new scope kind means adding one case and getting a compiler
warning at every `switch` that needs updating.

## Sizing the array

`MAX_EFFECTS` must cover the worst case: four players each holding several card effects, plus a
boom, a decline, a national event with two sub-effects, a regional card, and a regulation. Sixty-
four is comfortable. Under `#ifdef DEBUG`, assert rather than silently dropping:

```c
#ifdef DEBUG
    if (g->econ.effectCount >= MAX_EFFECTS) {
        fprintf(stderr, "effect registry overflow at round %d\n", g->round);
        abort();
    }
#endif
```

Silently dropping an effect produces a game that runs and is wrong. Aborting produces a game that
tells you what happened.

---

# The choke-point pattern

## The rule

Three functions are the **only** places a modifier is ever read:

```c
int square_value  (const GameState *g, int sq);
int square_rent   (const GameState *g, int sq, int diceTotal);
int building_cost (const GameState *g, int sq, bool hotel);
```

Everything else asks them.

## Why

Count the call sites. A property's *value* is needed by: net worth, insurance premiums, auction
opening bids, the depreciation message, renovation cost, mortgage capacity, the Opportunistic
Trader's appreciation estimate, the Aggressive Investor's bid cap, and the bankruptcy ladder.
That is nine places, and a property's *rent* is needed by roughly twenty.

Now count the modifiers that affect value: inflation, boom, decline, regional card, national
event, event card, depreciation, structural damage. Eight.

Inline the modifier arithmetic and you have up to 72 places to keep correct. Miss one and the
program still runs — it just reports a slightly wrong net worth, or lets a player buy insurance at
a stale premium. Nothing crashes. Nothing warns. You find it, if at all, by hand-checking
arithmetic in a 40,000-line transcript.

Funnel them and there are eight modifiers in one function.

## What each one composes

```c
int square_value(const GameState *g, int sq)
{
    const Square *s = &g->board[sq];
    int v = s->price;                                    /* inflation already applied */

    v = pct(v, -s->depreciationPct);                     /* LK 16 */
    if (s->structDamaged) v = pct(v, -15);               /* LK 28 */
    v = pct(v, effect_modifier(g, EFF_VALUE_MUL, sq, -1));

    return v;
}
```

Four lines, in a defined order, in one place. When Stage 25 adds market booms, it adds nothing
here — `EFF_VALUE_MUL` already covers it, because booms push into the registry rather than
inventing a new mechanism.

`square_rent` is longer because it also routes railway and utility rent and applies the condition
bands, but the shape is identical: start from the stored base, apply each modifier once, return.

## The test of a choke point

If adding a new economic rule requires editing more than one function, the choke point has a hole
in it. Stages 24 through 28 add five new economic systems, and none of them touch `square_value` —
that is the design working.

---

# The round scheduler

## Two nested loops

```
for round in 1..500:
    for player in order[]:
        if not bankrupt: play_turn(player)
    end_of_round(round)
```

`play_turn` is the eight steps of Rule 3. `end_of_round` is where the economy lives.

## Cadences as modular arithmetic

Five distinct schedules, expressed directly:

```c
if (g->round %  5 == 0) depreciation_tick(g);
if (g->round % 10 == 0) { draw_inflation(g); market_review(g); fire_disaster(g); }
if (g->round % 15 == 0) { national_event(g); regional_card(g); }
if (g->round % 20 == 0) government_regulation(g);
```

Round 60 fires the 5-, 10-, 15- and 20-round systems all at once. That is correct and intended —
and it is why the ordering below matters.

## The order is load-bearing

Decision D13 fixes it:

```
1  loan interest accrues        (at each loan's ISSUED rate)
2  loan default check
3  property age +1, building condition -2%
4  insurance countdown, expiry warnings
5  automatic repairs
6  cadenced systems  (5 / 10 / 15 / 20)
7  tick_effects      (decrement durations, drop expired)
8  print Round N Summary
9  print Current Market Conditions
```

Three of these placements are deliberate and would produce different games if moved.

**Interest before the default check (1 before 2).** A loan should be able to default on the
interest that just compounded. Reversed, every loan gets one extra round of grace and defaults
become rarer.

**Effects tick after the systems fire (6 before 7).** An effect created this round must live its
full stated duration. Tick first and a 10-round boom lasts 9 rounds, because it loses one to the
round of its own creation. The symptom — every duration one short — is easy to miss and annoying
to trace.

**Both print blocks last (8, 9).** The round summary must report the state *after* everything that
happened this round, and Rule-LK 36 requires the market block at the end of every round. Printing
either earlier reports stale numbers.

## Why a scheduler rather than scattered checks

The alternative is each subsystem checking the round number itself. That works, but it distributes
the ordering decision across six files, where it becomes invisible and impossible to audit. One
function, nine lines, in the order D13 specifies, is a decision you can point at.

---

# State machines

Three parts of this program are state machines. Recognising them makes each one small.

## Jail

```
                    land on square 30
     FREE  ------------------------------->  JAILED (turns = 0)
       ^                                          |
       |   roll doubles / pay bail /              |  turn passes
       |   auto-release after the 3rd turn        v
       +--------------------------------------  JAILED (turns = 1, 2, 3)
```

Encoded as `bool jailed` plus `int jailTurns`, resolved in one function:

```c
bool resolve_jail(GameState *g, int p)      /* true = the player may move this turn */
{
    Player *pl = &g->players[p];
    if (!pl->jailed) return true;

    int d1, d2;
    roll_dice(&d1, &d2);
    if (d1 == d2)              { pl->jailed = false; move_player(g, p, d1 + d2); return false; }
    if (pl->cash >= JAIL_BAIL) { charge(g, p, JAIL_BAIL, -1); pl->jailed = false; return true; }
    if (++pl->jailTurns >= JAIL_MAX_TURNS) {
        charge(g, p, JAIL_BAIL, -1);          /* D10: auto-pay after the third turn */
        pl->jailed = false;
        return true;
    }
    return false;
}
```

Every transition in Rule 13 is one line. The return value carries the one thing the caller needs:
whether the turn continues.

## Loans

```
   NONE  --grant-->  ACTIVE  --repay in full-->  NONE
                       |
                       +--roundsLeft hits 0 with a balance-->  DEFAULTED
                                                                   |
                                                             foreclosure
                                                                   |
                                                                   v
                                                                 NONE
```

`Loan.active` plus `roundsLeft` holds it. `DEFAULTED` is transient — foreclosure resolves it
within a single call, so it needs no stored representation.

The important field is `ratePct`. Storing the rate **on the loan** rather than reading the global
rate at accrual time is what makes Rule-LK 13 correct by construction: an existing loan keeps its
issued rate no matter what inflation does afterwards. Get this wrong and every loan silently
reprices on every inflation draw.

## Buildings

```
   development:   0 houses --> 1 --> 2 --> 3 --> 4 --> HOTEL
                  (the hotel step clears houses to 0 and sets hotel = true)

   from any of those states, independently:

     unmaintained for 20 rounds  -->  STRUCTURALLY DAMAGED  --renovate-->  restored
     disaster strikes            -->  DAMAGED               --repair---->  restored
```

`int houses` plus `bool hotel`, with the invariant `!(houses > 0 && hotel)` — Rule 10 says a
property can never hold both. The hotel upgrade is not an increment; it sets `houses = 0` and
`hotel = true` in one step.

`damaged` (from a disaster) and `structDamaged` (from neglect) are separate flags with separate
causes, separate consequences, and separate repair paths. Merging them looks tempting and loses
Rule-LK 29, which prices damaged-building renovation differently from ordinary repair.

---

# Fixed-size collections

## Everything is an array

| Collection | Size | Fixed by |
|------------|------|----------|
| Board | 40 | Table 1 |
| Players | 4 | Rule 1 |
| Colour groups | 8 | Appendix B |
| Event cards | 20 | Appendix A |
| Effects | 64 | worst-case analysis |

None of these change at runtime, so none of them need allocation. That eliminates leaks, null
checks, and lifetime bugs as categories rather than managing them.

## The circular queue

Appendix A: *"the top card shall be drawn. After execution, the card shall be returned to the
bottom of the deck."*

The literal reading is a queue: remove from the front, append to the back. The naive
implementation shifts nineteen elements on every draw:

```c
int card = deck[0];
for (int i = 0; i < 19; i++) deck[i] = deck[i + 1];   /* shift everything down */
deck[19] = card;                                       /* put it at the back    */
```

Correct, and doing about 500 times more work than necessary over a full game.

The circular queue does the same thing by moving an index instead of the data:

```c
typedef struct { int cards[DECK_SIZE]; int head; } EventDeck;

int draw(EventDeck *d)
{
    int card = d->cards[d->head];
    d->head = (d->head + 1) % DECK_SIZE;
    return card;
}
```

Nothing moves. `head` advances and wraps.

Why this is the *same* deck: after twenty draws `head` has returned to 0 and the cards come round
again in the same order — which is exactly what "returned to the bottom" produces. The array is a
ring; "front" and "back" are just positions relative to `head`.

```
   initial, head = 0
             [ C0  C1  C2  C3  ...  C19 ]
                ^
                head -- the top of the deck

   after three draws, head = 3
             [ C0  C1  C2  C3  ...  C19 ]
                             ^
                             head

             C0, C1 and C2 were drawn. Nothing moved: they now sit
             *behind* head, which is exactly "the bottom of the deck".
```

The modulo is what makes it circular, and it is the same operation that wraps the board at square
40. Once you see it in both places the pattern is hard to unsee: **a fixed array plus a modular
index is a ring.**

The deck is shuffled once at startup with Fisher-Yates, then never again — the spec describes a
deterministic cycle, not a reshuffle.

## Initialising state

```c
GameState g = {0};      /* everything zero */
game_init(&g);          /* then the non-zero defaults */
```

`game_init` must set every field whose correct initial value is not zero. In this program that is
a short but critical list:

| Field | Value | Why not zero |
|-------|-------|--------------|
| `board[i].owner` | `-1` | zero means "owned by Player 0" |
| `board[i].conditionPct` | `100` | buildings start in perfect condition (LK 25) |
| `board[i].policy` | `INS_NONE` | happens to be 0, but say it anyway |
| `players[i].cash` | `30000` | Rule 1 |
| `econ.interestRatePct` | `8` | Table 9, Stable Economy |
| `round` | `1` | rounds are 1-based; `round % 10` depends on it |

The `owner` one is the trap. Forget it and every property starts owned by the Aggressive
Investor, which produces a game that runs, prints plausible output, and is completely wrong.

---

# Lookup tables over branching

Several spec tables map a small integer to a value. Each can be written as a chain of conditionals
or as an array. The array is better, and the reasons are worth stating.

## Table 6 — rent multipliers

```c
static const int RENT_MULT[MAX_HOUSES + 1] = { 1, 2, 3, 5, 7 };
#define HOTEL_RENT_MULT 10
```

versus:

```c
if      (houses == 0) mult = 1;
else if (houses == 1) mult = 2;
else if (houses == 2) mult = 3;
else if (houses == 3) mult = 5;
else if (houses == 4) mult = 7;
```

The table is five values on one line, it visually matches the spec's table so you can check it by
eye, and it cannot contain an unreachable branch or a missing `else`.

## Table 7 — railway rent

```c
static const int RAILWAY_RENT[4] = { 250, 500, 1000, 2000 };
/* indexed by (stations owned - 1) */
```

The index offset needs a comment. Zero stations owned means you do not own the square, so no rent
is due and the array is never consulted at index `-1` — but write the comment anyway, because the
next reader will wonder.

## Appendix B — group values

```c
typedef struct { int price, baseRent, house, hotel, mortgage; } GroupValues;

static const GroupValues GROUP_VALUES[GRP_COUNT] = {
    /* BROWN     */ {  1500,  150,  500,  2000,  750 },
    /* LIGHTBLUE */ {  2500,  250,  750,  3000, 1250 },
    ...
};
```

Eight rows matching the spec's eight rows. Verifying the implementation against the spec is
reading two tables side by side.

## Table 3 — condition bands

The one case where conditionals win:

```c
int condition_rent_pct(int c)
{
    if (c >= 90) return 100;
    if (c >= 75) return  90;
    if (c >= 50) return  75;
    if (c >= 25) return  50;
    return 0;
}
```

The input is a *range* 0–100, not a small index. A 101-element table would work and would be
absurd. The ordered `if` chain reads exactly like the spec's band table.

The principle: **table when the input is a small dense index; branch when it is a range.**

## `static const`

All of these are `static const`:

- `static` — private to the file, no collisions, no accidental external use.
- `const` — the compiler places them in read-only memory and rejects writes.

Neither is a global variable in the sense the spec prohibits. The concern with globals is *mutable
shared state*; an immutable table transcribed from the spec is closer to a constant than a
variable.

---

# Isolating decisions behind an interface

## The problem

Four personalities each make seven kinds of decision. Written directly, that is 28 behaviours
scattered across whichever function needed them, and `land_on` grows a four-way branch every time
a new decision point appears.

## The interface

Seven functions, fixed signatures, one per decision:

```c
bool decide_buy        (GameState *g, int p, int sq);
int  decide_bid        (GameState *g, int p, int sq, int currentBid);
void decide_bank       (GameState *g, int p);
void decide_insurance  (GameState *g, int p);
void decide_build      (GameState *g, int p);
void decide_maintenance(GameState *g, int p);
void decide_renovate   (GameState *g, int p, int sq);
```

Each switches internally on `g->players[p].strat`. The rest of the program calls them without ever
knowing which personality is acting.

```c
/* game.c — no idea who is playing */
if (decide_buy(g, p, sq)) { purchase(g, p, sq); }
else                      { run_auction(g, sq); }
```

## Why this shape

**One file changes.** Stages 30 through 33 implement four personalities and modify `players.c`
only. Nothing else in the program is touched, so nothing else can break.

**Placeholders work.** Stages 8 through 29 need *some* decision to be made so the game runs and
can be observed. A placeholder body behind a final signature gives you that without a rewrite
later — the signature is a contract that lets the two halves be built in either order.

**The strategies are comparable.** All four implementations of `decide_buy` sit next to each
other, so "always buys" versus "buys only if 50% of cash remains" is a visible contrast rather
than a difference buried in two distant files.

**Decisions cannot cheat.** `decide_buy` returns a `bool`. It cannot move money; only `finance.c`
does that. A strategy that wanted to quietly adjust its own cash would have to change its
signature, which would be conspicuous.

## Where the boundary sits

`decide_*` functions **read** freely and **decide**. They do not mutate money, ownership, or
buildings. The caller executes.

The exceptions are the `void` ones — `decide_bank`, `decide_build`, `decide_insurance`,
`decide_maintenance` — where the decision is *which of several actions*, and returning that
choice would need a small command struct for little benefit. They call into `finance.c` to
execute. The discipline still holds: they call `charge` and `credit`; they never touch `cash`.

---

# Invariants and verification without a test framework

## The constraint

`gcc *.c -o monopoly` globs every `.c` file. A test file with its own `main` produces:

```
multiple definition of `main'
```

So there is no unit-test binary. Five techniques replace it.

## 1. Seeded determinism

```bash
./monopoly 42 > a.txt
./monopoly 42 > b.txt
diff a.txt b.txt        # must be empty
```

This is the foundation. With it, any behavioural change is a diff against a known-good capture.
Without it, nothing else in this list works. It is why `srand` is called exactly once and why
every random draw goes through `rng_range`.

## 2. Dump and verify

Temporarily print a table from `main`, check it against the spec by eye, delete it:

```c
#if 0   /* temporary — board verification, Stage 3 */
for (int i = 0; i < NUM_SQUARES; i++)
    printf("%2d %d %-24s %2d %6d\n", i, g.board[i].type, g.board[i].name,
           g.board[i].group, g.board[i].price);
#endif
```

Crude, and the fastest way to confirm 40 hand-entered squares are right. A transcription error in
the board table is nearly impossible to find later — it surfaces as one group never forming a
monopoly, forty stages downstream.

## 3. Invariant assertions under `#ifdef DEBUG`

An invariant is something that must be true at all times. Check the ones a bug would violate:

```c
#ifdef DEBUG
static void check_invariants(const GameState *g)
{
    for (int grp = 0; grp < GRP_COUNT; grp++) {
        int lo = 99, hi = -1;
        for (int i = 0; i < NUM_SQUARES; i++) {
            if (g->board[i].group != grp || g->board[i].owner < 0) continue;
            if (g->board[i].houses < lo) lo = g->board[i].houses;
            if (g->board[i].houses > hi) hi = g->board[i].houses;
        }
        if (hi >= 0 && hi - lo > 1) {
            fprintf(stderr, "R%d: uneven building in group %d\n", g->round, grp);
            abort();
        }
    }
    for (int i = 0; i < NUM_SQUARES; i++)
        if (g->board[i].houses > 0 && g->board[i].hotel) {
            fprintf(stderr, "R%d: square %d has both houses and a hotel\n", g->round, i);
            abort();
        }
}
#endif
```

The invariants worth checking in this program:

- Even building within a colour group (Rule 9)
- Never houses and a hotel together (Rule 10)
- `houses <= 4` (Rule 9)
- Cash never negative outside the debt-recovery window
- All 40 squares owned by a valid player or `-1`
- `effectCount <= MAX_EFFECTS`
- `deck.head` in 0–19
- No square `loanLocked` while its owner has no active loan

`make debug` compiles with `-DDEBUG`; the release build compiles them out entirely, so the graded
binary carries no cost.

## 4. Output shape checks

The output is highly structured, which makes `grep` a real verification tool:

```bash
./monopoly 42 | grep -c "Summary"                  # must be 500
./monopoly 42 | grep -c "Current Market Conditions" # must be 500
./monopoly 42 | grep -c "purchased"                 # must be <= 28
./monopoly 42 | grep -P 'LKR \d{4,}'                # must be EMPTY (missed separators)
./monopoly 42 | grep -oP 'Square \d+' | sort -u     # must be 0..39 only
```

The last two are worth internalising. The separator grep is a complete audit for one whole class
of §5 conformance bug. The square-range grep catches modular-arithmetic errors immediately.

## 5. Arithmetic spot-checks

Pick one number from the transcript and verify it by hand. An undeveloped Brown property's rent
must be exactly 150 (D7: 10% of 1,500). A Dark Blue hotel must be exactly 10,000 (10× base). A
building at 60% condition collects exactly 75% of its rent.

One verified number per stage catches an entire class of error, because these values flow through
the same choke point every other value does.

## What this does not give you

Regression coverage. Changing `square_rent` in Stage 25 can silently break the Stage 9 behaviour,
and nothing will tell you. The mitigation is the captured transcripts: keep `run-42.txt` from each
stage and diff against it after the next. Unexplained differences are the closest thing to a
failing test this build model allows.

---

# Summary

| Decision | Why |
|----------|-----|
| Fat `Square` struct, not a union | 1.4 KB is cheaper than a silent wrong-member read |
| `GameState` threaded by pointer | signatures document reach; no ambient mutable state |
| Fixed arrays, no `malloc` | every size is known; removes a whole class of bug |
| Effect registry for temporary modifiers | LK 34 stacking and LK 35 reversion become free |
| Permanent effects mutate storage | only inflation; LK 14 says values are recalculated |
| Three choke points | eight modifiers in one place instead of seventy-two |
| Fixed-order scheduler | ordering is a decision you can point at |
| `ratePct` stored on the loan | LK 13 correct by construction |
| Circular queue for the deck | O(1) draw, and it is what "bottom of the deck" means |
| Seven `decide_*` functions | four personalities in one file, comparable side by side |
| Seeded determinism | the only verification tool a glob build permits |
