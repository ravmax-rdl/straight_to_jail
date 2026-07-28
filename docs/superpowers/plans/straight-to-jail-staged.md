# Straight to Jail — Staged Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A fully autonomous four-player economic simulation in pure C — *Straight to Jail*, implementing the MONOPOLY-LK specification — that compiles warning-free with `gcc *.c -o monopoly` and reproduces the spec's §5 output exactly.

**Architecture:** One stack-allocated `GameState` threaded through every function; no globals, no `malloc`, integer money only. Two nested loops in `game.c` (rounds ≤ 500, turns per solvent player) with an end-of-round scheduler firing the 1/5/10/15/20-round economic cadences in a fixed order. All value, rent, and cost reads funnel through three choke points so timed modifiers compose in one place. The four personalities live behind seven `decide_*` functions with fixed signatures.

**Tech Stack:** C99, libc only (`stdio.h`, `stdlib.h`, `string.h`, `time.h`, `stdbool.h`, `limits.h`). No test framework — a glob build cannot tolerate a second `main`.

**Reference documents:**
- Requirements and decisions D1–D15: [`docs/REQUIREMENTS.md`](../../REQUIREMENTS.md)
- Architecture rationale: [`docs/superpowers/specs/2026-07-28-straight-to-jail-architecture-design.md`](../specs/2026-07-28-straight-to-jail-architecture-design.md)
- Concept explanations: [`docs/learning/`](../../learning/) — cited per stage as **Concept:**

This plan supersedes [`archive/2026-07-23-monopoly-lk.md`](archive/2026-07-23-monopoly-lk.md).

---

## Global Constraints

Every stage's requirements implicitly include all of these.

- **`gcc *.c -o monopoly` — zero errors, zero warnings, after every stage.** Development builds additionally run `gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly` and must also be silent.
- **The binary is `monopoly`.** Spec §4 mandates that exact build line. `make straight_to_jail` may exist as an alias; it must never be required.
- **The first output line is `MONOPOLY-LK Simulation`** — spec §5, graded verbatim. *Straight to Jail* is the project name, not the program's banner.
- **Module split per spec Table 5:** `types.h`, `board.c`, `players.c`, `finance.c`, `events.c`, `game.c`, `main.c`. All shared types, constants, and **all** public prototypes live in `types.h`.
- **No global variables.** A single `GameState` on `main`'s stack, passed by pointer. (`rand`'s internal state is libc's, not ours, and is permitted by §4.)
- **No `malloc`.** Every collection has a fixed compile-time size.
- **Money is `int`.** Percentage math is `v * (100 + p) / 100`, truncating toward zero (decision D6). No floating point in the money path, ever.
- **Every monetary figure printed goes through `fmt_lkr`.** No `printf("%d")` on money.
- **Seeding:** `srand(argc > 1 ? (unsigned)atoi(argv[1]) : (unsigned)time(NULL))`, once, in `main`.
- **Zero user interaction** after launch. No `scanf`, no `getchar`.
- **Commit after every stage.** The stage is not done until the build is clean and the commit is made.

### The stage cycle

There is no unit-test binary, so every stage follows this cycle instead:

1. Implement the stage's steps.
2. `gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly` → **silent**.
3. `./monopoly 42` → run to completion.
4. Check the stage's **Verify** block — a specific, named thing to look for in the output.
5. Commit.

---

## File Structure

| File | Owns | First appears |
|------|------|---------------|
| `types.h` | Enums, constants, structs, **every** public prototype, the D-decision header comment block | Stage 1 |
| `main.c` | Seed handling, `GameState` on the stack, pre-game banner, `game_init` + `game_run`. Nothing else. | Stage 1 |
| `Makefile` | `monopoly` (canonical glob), `straight_to_jail` alias, `debug`, `clean` | Stage 1 |
| `board.c` | 40-square table, dice/RNG, movement, ownership and monopoly queries, the three choke points | Stage 2 |
| `game.c` | `game_run`, roll-off, round/turn loops, `land_on` dispatch, end-of-round scheduler, all block-format output | Stage 4 |
| `finance.c` | `fmt_lkr`, `charge`/`credit`, tax, auctions, loans, insurance, net worth, bankruptcy, ageing ticks | Stage 7 |
| `players.c` | The seven `decide_*` strategy functions | Stage 8 |
| `events.c` | Effect registry, disasters, inflation, market review, national events, regional cards, regulations, the card deck | Stage 20 |

---

# Phase 0 — Foundations

### Stage 1: Build skeleton and the pre-game banner

**Goal:** A program that compiles with the canonical build line and prints the §5 pre-game block.

**Spec:** R0.1–R0.3, R0.6, R0.7 · §4 · §5 "Before the Game Begins"
**Concept:** [01 §2 Multi-file compilation](../../learning/01-c-language.md), [01 §9 srand and rand](../../learning/01-c-language.md)

**Files:**
- Create: `types.h`, `main.c`, `Makefile`

**Interfaces:**
- Produces: the `types.h` header guard and the D-decision comment block that every later stage appends to.

- [ ] **Step 1:** Write `types.h` with a header guard and a top comment block recording every decision D1–D15 in one line each, citing the rule it resolves. Nothing else yet.

```c
#ifndef TYPES_H
#define TYPES_H

/* ------------------------------------------------------------------
 * Straight to Jail — implementation of the MONOPOLY-LK specification
 * (SCS 1301 take-home). Spec: assets/Assignment_1_unlocked.pdf
 *
 * SPEC-GAP DECISIONS (see docs/REQUIREMENTS.md §D for full rationale)
 * D1  Repair cost      = 50% of current construction cost on the property   [LK 10]
 * D2  Income Tax       = LKR 1,000 base, inflation-adjusted, x1.5 under the
 *                        Increase Property Tax regulation                   [Rule 11]
 * D3  Coverage         Basic {Fire,Flood} @80%; Comprehensive
 *                        {Fire,Flood,Riot,Vandalism} @100%; Business
 *                        Interruption all perils @100% + 5 rounds hotel rent.
 *                        Building Collapse and Electrical Failure are
 *                        uncovered by Basic/Comprehensive                    [LK 10, App E]
 * D4  Interest         Table 9 rate applied EVERY ROUND; "annual" label
 *                        ignored as inconsistent with LK 4. Issued rate is
 *                        frozen for the loan's life                          [LK 4, LK 13]
 * D5  Max loan         75% of mortgage value of properties + railways +
 *                        utilities (LK 2 beats the narrower §1.1.4)          [LK 2]
 * D6  Rounding         v * (100 + p) / 100 in int, truncating toward zero
 * D7  Base rent        10% of the group's purchase price                     [Table 6]
 * D8  Tie-break        Only tied players reroll, repeatedly                  [Rule 2]
 * D9  Valuation proxy  "estimated market value" = square_value()             [§3]
 * D10 Jail             After the 3rd failed turn bail is auto-paid; doubles
 *                        have no effect outside jail                         [Rule 13]
 * D11 Debt recovery    sell buildings @50% -> mortgage free assets ->
 *                        bankrupt; assets auctioned                          [Rule 11, 14]
 * D12 Effects          Permanent effects mutate stored values; temporary
 *                        effects live in the registry and are read at
 *                        access time                                         [LK 14, 34, 35]
 * D13 Round order      interest -> default -> age/condition -> insurance ->
 *                        repairs -> cadences -> tick -> summary -> market
 * D14 Region tags      see REGION_* below                                    [LK 18, Table 4]
 * D15 Claims receivable  always 0 — compensation is credited immediately     [Rule 15]
 * ------------------------------------------------------------------ */

#endif /* TYPES_H */
```

- [ ] **Step 2:** Write `main.c` printing the §5 pre-game block **exactly**. Note the spaces around each colon — they are in the spec and are graded.

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "types.h"

int main(int argc, char **argv)
{
    srand(argc > 1 ? (unsigned)atoi(argv[1]) : (unsigned)time(NULL));

    printf("MONOPOLY-LK Simulation\n\n");
    printf("Player 1 : Aggressive Investor\n");
    printf("Player 2 : Conservative Banker\n");
    printf("Player 3 : Risk Taker\n");
    printf("Player 4 : Opportunistic Trader\n\n");
    printf("Each player begins with LKR 30,000.\n\n");
    return 0;
}
```

- [ ] **Step 3:** Write `Makefile`. Tabs, not spaces, for recipe lines.

```make
CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic
SRC     = $(wildcard *.c)

monopoly: $(SRC)
	$(CC) *.c -o monopoly

straight_to_jail: $(SRC)
	$(CC) $(CFLAGS) *.c -o straight_to_jail

debug: $(SRC)
	$(CC) $(CFLAGS) -g -DDEBUG *.c -o monopoly

clean:
	rm -f monopoly straight_to_jail

.PHONY: debug clean
```

- [ ] **Step 4:** Build and run.

**Verify:**
```
gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly     # silent
./monopoly 42
```
Expected: exactly the eight lines above, `MONOPOLY-LK Simulation` first, blank line before and after the player list.

**Commit:** `feat: build skeleton and pre-game banner`

---

### Stage 2: Primitives — types, constants, money formatting, dice

**Goal:** Every struct and enum the program will ever use, plus the two utilities every later stage calls.

**Spec:** R0.4, R0.5, R0.6, R1.3 · §4
**Concept:** [01 §3 Enums](../../learning/01-c-language.md), [01 §4 Structs](../../learning/01-c-language.md), [01 §8 Strings and snprintf](../../learning/01-c-language.md), [01 §9 Modulo bias](../../learning/01-c-language.md)

**Files:**
- Modify: `types.h`
- Create: `board.c`, `finance.c`

**Interfaces:**
- Produces:
```c
/* finance.c */
const char *fmt_lkr(char *buf, int amount);   /* buf must be >= 20 bytes; returns buf */
/* board.c */
int rng_range(int lo, int hi);                /* uniform in [lo,hi], no modulo bias   */
int roll_die(void);                           /* 1..6                                 */
int roll_dice(int *d1, int *d2);              /* fills both, returns the total 2..12  */
```

- [ ] **Step 1:** Add all enums to `types.h`. `GRP_COUNT` is the array-sizing sentinel; `GRP_NONE` is `-1` so colour groups index arrays directly from zero.

```c
typedef enum {
    SQ_GO, SQ_PROPERTY, SQ_RAILWAY, SQ_UTILITY, SQ_BANK,
    SQ_INSURANCE, SQ_TAX, SQ_EVENT, SQ_JAIL, SQ_PARKING, SQ_GOTOJAIL
} SquareType;

typedef enum {
    GRP_NONE = -1,
    GRP_BROWN = 0, GRP_LIGHTBLUE, GRP_PINK, GRP_ORANGE,
    GRP_RED, GRP_YELLOW, GRP_GREEN, GRP_DARKBLUE,
    GRP_COUNT
} PropertyGroup;

typedef enum { INS_NONE, INS_BASIC, INS_COMPREHENSIVE, INS_BUSINESS } InsuranceType;
typedef enum { STRAT_AGGRESSIVE, STRAT_CONSERVATIVE, STRAT_RISKTAKER, STRAT_OPPORTUNIST } Strategy;
typedef enum { DIS_FIRE, DIS_FLOOD, DIS_RIOT, DIS_COLLAPSE, DIS_ELECTRICAL, DIS_COUNT } Disaster;

typedef enum {
    EFF_VALUE_MUL, EFF_RENT_MUL, EFF_HOTEL_RENT_MUL, EFF_RAILWAY_RENT_MUL,
    EFF_UTILITY_RENT_MUL, EFF_BUILD_COST_MUL, EFF_PREMIUM_MUL, EFF_MORTGAGE_MUL,
    EFF_AUCTION_OPEN_MUL, EFF_INTEREST_ADD, EFF_TAX_MUL,
    EFF_CLOSED, EFF_CONSTRUCTION_SUSPENDED, EFF_MAX_PROPERTIES,
    EFF_KIND_COUNT
} EffectKind;

typedef enum { SCOPE_GLOBAL, SCOPE_GROUP, SCOPE_REGION, SCOPE_SQUARE, SCOPE_PLAYER } EffectScope;

/* D14 region tags — bitmask, one bit per region */
#define REGION_WESTERN          0x01u
#define REGION_CENTRAL          0x02u
#define REGION_SOUTHERN_COASTAL 0x04u
#define REGION_NORTHERN         0x08u
#define REGION_EASTERN          0x10u
#define REGION_COMMERCIAL       0x20u
#define REGION_NWSDB_ADJACENT   0x40u
#define REGION_COASTAL          0x80u
```

- [ ] **Step 2:** Add all constants to `types.h`, each with a rule citation.

```c
#define NUM_SQUARES     40
#define NUM_PLAYERS      4
#define MAX_ROUNDS     500     /* Rule 15   */
#define START_CASH   30000     /* Rule 1    */
#define GO_SALARY     2000     /* Rule 4    */
#define JAIL_BAIL      300     /* Rule 13   */
#define JAIL_MAX_TURNS   3     /* Rule 13   */
#define AUCTION_INC    250     /* LK 20     */
#define AUCTION_OPEN_PCT 50    /* LK 19     */
#define LOAN_LTV_PCT    75     /* LK 2, D5  */
#define LOAN_ROUNDS     20     /* LK 4      */
#define INS_ROUNDS      20     /* LK 9      */
#define INS_WARN_ROUNDS  3     /* LK 9      */
#define MAX_HOUSES       4     /* Rule 9    */
#define BASE_TAX      1000     /* D2        */
#define COND_DECAY_PCT   2     /* LK 25     */
#define DEPREC_START_AGE 50    /* LK 16     */
#define DEPREC_CAP_PCT  30     /* LK 16     */
#define RENOVATE_PCT    10     /* LK 17     */
#define UNMAINTAINED_LIMIT 20  /* LK 28     */
#define MARKET_COOLDOWN 30     /* LK 33     */
#define DECK_SIZE       20     /* App A     */
#define MAX_EFFECTS     64     /* worst case: 4 players x card effects + globals */
```

- [ ] **Step 3:** Add the `Square`, `Loan`, `Player`, `Effect`, `Economy`, `EventDeck`, `GameState` structs exactly as given in [the architecture design §4](../specs/2026-07-28-straight-to-jail-architecture-design.md).

- [ ] **Step 4:** Implement `fmt_lkr` in `finance.c`. It renders the integer with thousands separators and no currency prefix (callers supply `LKR `). Handles negatives and zero.

```c
const char *fmt_lkr(char *buf, int amount)
{
    char digits[16];
    int  n = 0, i, j = 0, neg = (amount < 0);
    unsigned v = (unsigned)(neg ? -(long)amount : (long)amount);

    if (v == 0) digits[n++] = '0';
    while (v > 0) { digits[n++] = (char)('0' + (v % 10)); v /= 10; }

    if (neg) buf[j++] = '-';
    for (i = n - 1; i >= 0; i--) {
        buf[j++] = digits[i];
        if (i > 0 && i % 3 == 0) buf[j++] = ',';
    }
    buf[j] = '\0';
    return buf;
}
```

- [ ] **Step 5:** Implement the RNG helpers in `board.c`. The rejection loop removes modulo bias — see the concept note.

```c
int rng_range(int lo, int hi)
{
    int span = hi - lo + 1;
    int limit = RAND_MAX - (RAND_MAX % span);
    int r;
    do { r = rand(); } while (r >= limit);
    return lo + (r % span);
}

int roll_die(void)             { return rng_range(1, 6); }
int roll_dice(int *d1, int *d2) { *d1 = roll_die(); *d2 = roll_die(); return *d1 + *d2; }
```

- [ ] **Step 6:** Add all six prototypes to `types.h`.

- [ ] **Step 7:** Add a temporary check block to `main` — print `fmt_lkr` for `0, 5, 999, 1000, 30000, 1234567, -2500` and a tally of 6,000 `roll_dice` totals. Confirm, then **delete the block**.

**Verify:**
```
gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly     # silent
./monopoly 42
```
Expected during Step 7: `0`, `5`, `999`, `1,000`, `30,000`, `1,234,567`, `-2,500`. The dice tally must peak at 7 (~1000 occurrences) and taper symmetrically to ~167 each at 2 and 12. If it is flat, `rng_range` is wrong.

**Commit:** `feat: core types, money formatting, seeded dice`

---

### Stage 3: The board table

**Goal:** All 40 squares populated with names, types, groups, region tags, and values.

**Spec:** R1.1, R1.2, R1.4, R1.5 · Table 1 · Appendix B · D7 · D14
**Concept:** [02 §2 Modelling heterogeneous squares](../../learning/02-program-design.md), [01 §5 Arrays of structs](../../learning/01-c-language.md)

**Files:**
- Modify: `board.c`, `types.h`

**Interfaces:**
- Produces: `void board_init(GameState *g);`

- [ ] **Step 1:** Add a file-local group value table to `board.c`. Base rent is 10% of purchase price (D7).

```c
typedef struct { int price, baseRent, house, hotel, mortgage; } GroupValues;

static const GroupValues GROUP_VALUES[GRP_COUNT] = {
    /* BROWN     */ {  1500,  150,  500,  2000,  750 },
    /* LIGHTBLUE */ {  2500,  250,  750,  3000, 1250 },
    /* PINK      */ {  3500,  350, 1000,  4000, 1750 },
    /* ORANGE    */ {  4500,  450, 1250,  5000, 2250 },
    /* RED       */ {  5500,  550, 1500,  6000, 2750 },
    /* YELLOW    */ {  6500,  650, 2000,  8000, 3250 },
    /* GREEN     */ {  8000,  800, 2500, 10000, 4000 },
    /* DARKBLUE  */ { 10000, 1000, 3000, 12000, 5000 }
};
```

- [ ] **Step 2:** Write `board_init` filling all 40 squares from the R1.1 table. Properties take their values from `GROUP_VALUES[group]`. Every square gets `owner = -1`, `conditionPct = 100`, `policy = INS_NONE`, everything else zero.

Region tags (D14), applied as a bitmask:

| Squares | Tags |
|---------|------|
| 1 Pettah, 3 Maradana, 39 Galle Face | `WESTERN \| COMMERCIAL` |
| 6 Bambalapitiya, 8 Wellawatte, 9 Mount Lavinia | `WESTERN \| COASTAL` |
| 11 Nugegoda, 13 Maharagama, 14 Kottawa | `WESTERN` |
| 16 Negombo | `WESTERN \| COASTAL` |
| 18 Katunayake, 19 Ja-Ela | `WESTERN` |
| 21 Kandy City, 23 Peradeniya, 24 Katugastota | `CENTRAL` |
| 26 Galle Fort, 27 Unawatuna, 29 Hikkaduwa | `SOUTHERN_COASTAL \| COASTAL \| NWSDB_ADJACENT` |
| 31 Jaffna Town, 32 Nallur | `NORTHERN` |
| 34 Trincomalee | `NORTHERN \| EASTERN \| COASTAL` |
| 37 Nuwara Eliya | `CENTRAL` |
| 5, 15, 25, 35 railways | `COMMERCIAL` |

- [ ] **Step 3:** Add a temporary dump block to `main` printing `index | type | name | group | price | baseRent | regions` for all 40 squares.

- [ ] **Step 4:** Check the dump line-by-line against the R1.1 table. Confirm the totals: 22 properties, 4 railways, 2 utilities, 1 bank, 2 insurance, 1 tax, 4 event, 4 special = 40. Confirm group counts: Brown 2, Light Blue 3, Pink 3, Orange 3, Red 3, Yellow 3, Green 3, Dark Blue 2 = 22.

- [ ] **Step 5:** Delete the dump block.

**Verify:** the counts above are exact. A group with the wrong member count silently breaks monopoly detection in Stage 13 and is very hard to find later — check it now.

**Commit:** `feat: 40-square board table`

---

# Phase 1 — A board that plays

### Stage 4: Turn-order roll-off

**Goal:** `game_init` and the §5 roll-off block that establishes `order[]`.

**Spec:** R2.1, R2.2 · Rule 1, Rule 2 · D8 · §5 "Determining the First Player"
**Concept:** [02 §7 State initialisation](../../learning/02-program-design.md)

**Files:**
- Create: `game.c`
- Modify: `main.c`, `types.h`

**Interfaces:**
- Consumes: `board_init`, `roll_dice`
- Produces:
```c
void game_init(GameState *g);
void determine_order(GameState *g);
```

- [ ] **Step 1:** `game_init` — zero the whole `GameState`, call `board_init`, set the four players' names and strategies in Player 1–4 order (Aggressive Investor, Conservative Banker, Risk Taker, Opportunistic Trader), each with `cash = START_CASH`, `pos = 0`, `owner`-style fields cleared, `econ.interestRatePct = 8` (Table 9, Stable Economy).

- [ ] **Step 2:** `determine_order` per Rule 2 and D8 — every player rolls two dice; sort descending by roll; **only tied players reroll**, repeatedly, until the order is strict. Print the §5 block:

```
Aggressive Investor rolls 9.
Conservative Banker rolls 6.
Risk Taker rolls 11.
Opportunistic Trader rolls 5.

Risk Taker will begin the game.

Turn order:
Risk Taker
Opportunistic Trader
Aggressive Investor
Conservative Banker
```

Reroll rounds print the same `X rolls N.` line, so a tie is visible in the transcript.

- [ ] **Step 3:** Call `game_init` and `determine_order` from `main` after the banner.

**Verify:** `./monopoly 42` prints four roll lines, a blank line, `<name> will begin the game.`, a blank line, `Turn order:` and four names. Run seeds 1, 7, 42, 99 — the winner must differ across seeds. Run seed 42 twice — output must be byte-identical.

**Commit:** `feat: turn order roll-off`

---

### Stage 5: Round and turn loops, movement, GO

**Goal:** The game physically plays — 500 rounds of four players moving around the board.

**Spec:** R2.3 (steps 2–3), R2.4 · Rule 3, Rule 4 · §5 "Dice Roll", "Player Movement", "Passing GO"
**Concept:** [02 §5 The round scheduler](../../learning/02-program-design.md), [01 §11 Off-by-one and modular wrapping](../../learning/01-c-language.md)

**Files:**
- Modify: `game.c`, `board.c`, `types.h`

**Interfaces:**
- Produces:
```c
void move_player(GameState *g, int p, int steps);   /* board.c: wraps, credits GO, prints */
void play_turn(GameState *g, int p);                /* game.c */
void play_round(GameState *g);                      /* game.c */
void game_run(GameState *g);                        /* game.c */
bool game_over(const GameState *g);                 /* game.c */
```

- [ ] **Step 1:** `move_player` — record `from = players[p].pos`, compute `to = (from + steps) % NUM_SQUARES`, print the movement line, then credit GO if the move passed or landed on it (`to < from` after wrapping, or `to == 0`).

```c
void move_player(GameState *g, int p, int steps)
{
    char b[20];
    Player *pl = &g->players[p];
    int from = pl->pos;
    int to   = (from + steps) % NUM_SQUARES;

    printf("%s moves from Square %d to Square %d.\n", pl->name, from, to);
    pl->pos = to;

    if (to < from || to == 0) {
        pl->cash += GO_SALARY;
        printf("%s passed GO.\n", pl->name);
        printf("Collected LKR %s.\n", fmt_lkr(b, GO_SALARY));
        printf("Current Balance : LKR %s.\n", fmt_lkr(b, pl->cash));
    }
}
```

- [ ] **Step 2:** `play_turn` — steps 2 and 3 of Rule 3 only for now. Roll, print `%s rolled %d.`, move.

- [ ] **Step 3:** `play_round` — increment `g->round`, then loop `g->order[]` calling `play_turn` for each non-bankrupt player.

- [ ] **Step 4:** `game_over` — true when fewer than two players are solvent. `game_run` — loop `play_round` while `g->round < MAX_ROUNDS && !game_over(g)`.

- [ ] **Step 5:** Call `game_run` from `main`.

**Verify:** `./monopoly 42 | head -40` shows the roll-off then repeating `rolled` / `moves from Square A to Square B` pairs. `./monopoly 42 | grep -c "passed GO"` should be roughly 500 × 4 × (7/40) ≈ 350 — a wildly different number means the GO detection is wrong. Confirm no square index outside 0–39 ever appears: `./monopoly 42 | grep -oP 'Square \d+' | sort -u`.

**Commit:** `feat: round and turn loops with movement`

---

### Stage 6: Round summary, termination, GAME OVER

**Goal:** Both block-format outputs exist in skeleton form, and the game ends properly.

**Spec:** R2.14, R5.3, R5.5 · Rule 15 · §5 "End of Every Round", "End of Game"
**Concept:** [03 §8 Net worth as a balance sheet](../../learning/03-economic-math.md)

**Files:**
- Modify: `game.c`, `finance.c`, `types.h`

**Interfaces:**
- Produces:
```c
int  net_worth(const GameState *g, int p);      /* finance.c — v1: cash only */
void round_summary(const GameState *g);         /* game.c */
void final_report(const GameState *g);          /* game.c */
```

- [ ] **Step 1:** `net_worth` v1 returns `players[p].cash`. Stages 9, 18 and 29 extend it; the signature never changes.

- [ ] **Step 2:** `round_summary` — the §5 block, exactly. 45 `=` characters, 45 `-` characters between players, no separator after the last player before the closing `=` line.

```
=============================================
Round 37 Summary
=============================================
Aggressive Investor
Cash : LKR 12,300
Net Worth : LKR 82,500
Properties : 12
Hotels : 3
Outstanding Loan : LKR 6,500
---------------------------------------------
...
=============================================
```

Players print in `order[]` sequence. `Properties` and `Hotels` are 0 for now. `Outstanding Loan` prints `None` when there is no loan — **not** `LKR 0`.

- [ ] **Step 3:** Call `round_summary` at the end of `play_round`.

- [ ] **Step 4:** `final_report` — the §5 GAME OVER block. Winner is the last solvent player, or the highest `net_worth` if 500 rounds elapsed. Note this block puts its labels and values on **separate lines**, unlike the round summary:

```
=============================================
GAME OVER
Winner
Aggressive Investor
Total Cash
LKR 65,400
Total Property Value
LKR 210,700

Outstanding Loans

None

Net Worth

LKR 276,100

=============================================
```

- [ ] **Step 5:** Call `final_report` from `game_run` after the loop.

**Verify:** `./monopoly 42 | tail -30` shows the final round summary then the GAME OVER block. `./monopoly 42 | grep -c "Summary"` = 500. Count the rule characters: `./monopoly 42 | grep -m1 '^=' | wc -c` → 46 (45 plus the newline).

**Commit:** `feat: round summary, termination, final report`

---

# Phase 2 — Property transactions

### Stage 7: The money path and income tax

**Goal:** One function moves money. Nothing else ever touches `cash` directly.

**Spec:** R0.5, R2.10 · Rule 11 · D2, D6, D11 · §5 (amounts)
**Concept:** [03 §1 Integer money](../../learning/03-economic-math.md), [03 §2 The truncation convention](../../learning/03-economic-math.md)

**Files:**
- Modify: `finance.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
void credit(GameState *g, int p, int amt);
bool charge(GameState *g, int p, int amt, int toPlayer);  /* toPlayer -1 = Bank.
                                                             false = could not pay  */
int  pct(int value, int percent);                         /* v * (100+percent)/100, D6 */
int  pct_of(int value, int percent);                      /* v * percent/100, D6       */
void pay_tax(GameState *g, int p);
```

- [ ] **Step 1:** Implement the two percentage helpers in `finance.c`. Every percentage in the program goes through one of these — that is what makes D6 a single decision rather than forty.

```c
int pct(int value, int percent)    { return (int)((long)value * (100 + percent) / 100); }
int pct_of(int value, int percent) { return (int)((long)value * percent / 100); }
```

The `long` cast matters: `10000 * 130` overflows nothing, but a stacked value late in a 500-round game can. See the overflow analysis in the economic-math document.

- [ ] **Step 2:** `credit` — add to cash. `charge` — if `cash >= amt`, deduct, pay `toPlayer` if not `-1`, return true. Otherwise return false. The full D11 debt-recovery ladder lands in Stage 29; until then a `false` return is simply reported and the charge is skipped.

- [ ] **Step 3:** `pay_tax` — charge `BASE_TAX` (D2) to the Bank. The inflation adjustment and the ×1.5 regulation multiplier arrive in Stages 24 and 27.

- [ ] **Step 4:** Add `land_on` to `game.c` as a `switch` on `g->board[sq].type` with every case present and only `SQ_TAX` implemented. Wire it into `play_turn` as Rule 3 step 4.

```c
void land_on(GameState *g, int p, int sq, int diceTotal)
{
    switch (g->board[sq].type) {
    case SQ_TAX:       pay_tax(g, p); break;
    case SQ_GO:        case SQ_JAIL:    case SQ_PARKING:
    case SQ_PROPERTY:  case SQ_RAILWAY: case SQ_UTILITY:
    case SQ_BANK:      case SQ_INSURANCE: case SQ_EVENT:
    case SQ_GOTOJAIL:  break;   /* implemented in later stages */
    }
}
```

Listing every enum case explicitly rather than using `default:` means adding a new `SquareType` later produces a compiler warning instead of silent nothing.

- [ ] **Step 5:** Retrofit `round_summary` to use `net_worth`, and confirm every money print in the program already routes through `fmt_lkr`.

**Verify:** `./monopoly 42 | grep -A2 "Square 4"` shows a tax payment. Cash in the round summaries must fall over time for players landing on square 4 and never go negative.

**Commit:** `feat: money path and income tax`

---

### Stage 8: Property purchase and ownership

**Goal:** Players buy unowned squares at list price.

**Spec:** R2.5 · Rule 5 · §5 "Purchasing Property"
**Concept:** [02 §9 Isolating decisions behind an interface](../../learning/02-program-design.md)

**Files:**
- Create: `players.c`
- Modify: `game.c`, `board.c`, `types.h`

**Interfaces:**
- Produces:
```c
bool decide_buy(GameState *g, int p, int sq);   /* players.c — PLACEHOLDER until Stage 30 */
bool is_purchasable(const GameState *g, int sq);
int  count_owned(const GameState *g, int p, SquareType t);
```

- [ ] **Step 1:** `is_purchasable` — true for `SQ_PROPERTY`, `SQ_RAILWAY`, `SQ_UTILITY`. `count_owned` — count squares of type `t` owned by `p`.

- [ ] **Step 2:** Placeholder `decide_buy` — return `cash >= price` for every strategy. Mark it clearly:

```c
/* PLACEHOLDER — replaced with the four real strategies in Stages 30-33.
   Signature is final; only the body changes. */
bool decide_buy(GameState *g, int p, int sq)
{
    return g->players[p].cash >= g->board[sq].price;
}
```

- [ ] **Step 3:** In `land_on`, for the three purchasable types with `owner == -1`: call `decide_buy`; on true, charge the price and set `owner = p`, printing the §5 block. On false, do nothing yet — the auction lands in Stage 11.

```
Aggressive Investor purchased Galle Fort for LKR 4,500.
Remaining Balance : LKR 18,000.
```

- [ ] **Step 4:** Extend `round_summary`'s `Properties` field to `count_owned(g, p, SQ_PROPERTY)`.

**Verify:** `./monopoly 42 | grep -c purchased` should be at most 28 (22 properties + 4 railways + 2 utilities). A number above 28 means a square is being bought twice — the `owner == -1` guard is wrong. Confirm the four `Properties :` counts in the last round summary sum to at most 22.

**Commit:** `feat: property purchase and ownership`

---

### Stage 9: Value and rent choke points

**Goal:** The two functions that every later stage will extend rather than duplicate.

**Spec:** R2.6, R2.9 · Rule 7, Table 6 · D12
**Concept:** [02 §4 The choke-point pattern](../../learning/02-program-design.md), [02 §8 Lookup tables over branching](../../learning/02-program-design.md)

**Files:**
- Modify: `board.c`, `game.c`, `finance.c`, `types.h`

**Interfaces:**
- Produces:
```c
int square_value(const GameState *g, int sq);
int square_rent (const GameState *g, int sq, int diceTotal);
```

- [ ] **Step 1:** `square_value` v1 — return the stored `price`. Stages 21, 22, 24 and 25 add depreciation, structural damage, inflation and effect multipliers **inside this function only**.

- [ ] **Step 2:** `square_rent` v1 for `SQ_PROPERTY` — base rent × the Table 6 development multiplier, as a lookup table rather than an `if` chain. Return 0 if unowned or mortgaged.

```c
/* Table 6 — index by house count; hotel handled separately */
static const int RENT_MULT[MAX_HOUSES + 1] = { 1, 2, 3, 5, 7 };
#define HOTEL_RENT_MULT 10
```

- [ ] **Step 3:** In `land_on`, when a purchasable square is owned by someone else, charge rent and print the §5 block. Note the order of these three lines and the trailing full stops:

```
Risk Taker landed on Galle Fort.
Rent Paid : LKR 750.
Owner : Aggressive Investor.
```

Landing on your own property charges nothing.

- [ ] **Step 4:** Extend `net_worth` to add `square_value` for every square owned by `p`.

**Verify:** `./monopoly 42 | grep -A2 "landed on"` — every rent block has all three lines. `Rent Paid` for an undeveloped property must equal exactly 10% of its group's purchase price (D7): Brown 150, Dark Blue 1,000. Net worth in the round summary must now exceed cash for any player owning property.

**Commit:** `feat: value and rent choke points`

---

### Stage 10: Railway and utility rent

**Goal:** The two non-residential rent rules.

**Spec:** R1.4, R1.5 · Table 7, Table 8
**Concept:** [02 §8 Lookup tables](../../learning/02-program-design.md), [03 §9 Dice distribution](../../learning/03-economic-math.md)

**Files:**
- Modify: `board.c`, `types.h`

**Interfaces:**
- Consumes: `count_owned`, `square_rent`

- [ ] **Step 1:** Add the railway table to `board.c` and branch `square_rent` on `SQ_RAILWAY`. Rent depends on how many railways the **owner** holds, not the visitor.

```c
/* Table 7 — index by [stations owned - 1] */
static const int RAILWAY_RENT[4] = { 250, 500, 1000, 2000 };
```

- [ ] **Step 2:** Branch `square_rent` on `SQ_UTILITY` — `4 * diceTotal` if the owner holds one utility, `10 * diceTotal` if both (Table 8). This is why `square_rent` takes `diceTotal`; every other square type ignores it.

- [ ] **Step 3:** Confirm `land_on` passes the actual dice total through from `play_turn`, not a stale or zero value.

**Verify:** `./monopoly 42 | grep -B1 -A2 "Square 12\|Square 28"` — utility rent must always be a multiple of 4 (or 10) and between 8 and 120. Railway rent must only ever be one of 250, 500, 1000, 2000; check with `./monopoly 42 | grep -A1 "landed on Colombo Fort" | grep "Rent Paid" | sort -u`.

**Commit:** `feat: railway and utility rent`

---

### Stage 11: Auctions

**Goal:** A declined purchase goes immediately to auction among all solvent players.

**Spec:** R2.5, R3.13 · Rule 5, Rule 6, LK 19–23 · §5 "Auction"
**Concept:** [03 §10 English ascending auctions](../../learning/03-economic-math.md)

**Files:**
- Modify: `finance.c`, `players.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
void run_auction(GameState *g, int sq);
int  decide_bid(GameState *g, int p, int sq, int currentBid);  /* 0 = withdraw.
                                                                  PLACEHOLDER until Stage 30 */
```

- [ ] **Step 1:** Placeholder `decide_bid` — bid `currentBid + AUCTION_INC` while that is affordable and at most 60% of `square_value`; otherwise return 0.

- [ ] **Step 2:** `run_auction` per LK 19–23:
  - Opening bid = 50% of `square_value` (LK 19).
  - All solvent, non-bankrupt players participate, in `order[]` sequence.
  - Increments of at least `AUCTION_INC` (LK 20).
  - **Withdrawal is permanent** — a player who declines is out for the rest of this auction (LK 21). Track with a local `bool active[NUM_PLAYERS]`.
  - A bid may never exceed the bidder's cash, and no loan may be taken mid-auction (LK 22).
  - If nobody bids at all, the square stays with the Bank (LK 23).
  - Loop until one active bidder remains.

The §5 block:
```
Auction Started.
Property :
Nuwara Eliya
Opening Bid :
LKR 3,500.
Risk Taker bids LKR 3,750.
Aggressive Investor bids LKR 4,000.
Conservative Banker withdraws.
Opportunistic Trader withdraws.
Aggressive Investor wins the auction.
```

- [ ] **Step 3:** Call `run_auction` from `land_on` when `decide_buy` returns false.

- [ ] **Step 4:** Guard against the infinite loop: if every player withdraws on the opening round, exit immediately with the property unsold. Assert under `#ifdef DEBUG` that the loop body runs at most 200 times.

**Verify:** Run seeds until an auction fires (`./monopoly 42 | grep -c "Auction Started"`). If none do, temporarily force `decide_buy` to return false, confirm the block, then revert. Each auction must end with exactly one `wins the auction` line or no winner at all; the winner's cash must drop by exactly the final bid.

**Commit:** `feat: auctions`

---

# Phase 3 — Jail and development

### Stage 12: The jail state machine

**Goal:** Go To Jail works, and jailed players use the three documented exits.

**Spec:** R2.11, R2.12 · Rule 12, Rule 13 · D10
**Concept:** [02 §6 State machines](../../learning/02-program-design.md)

**Files:**
- Modify: `game.c`, `types.h`

**Interfaces:**
- Produces: `bool resolve_jail(GameState *g, int p);`  /* true = player may move this turn */

- [ ] **Step 1:** In `land_on`, `SQ_GOTOJAIL` sets `pos = 10`, `jailed = true`, `jailTurns = 0` and **does not** pay GO — the player is moved directly, not walked (Rule 12).

- [ ] **Step 2:** `resolve_jail` runs as Rule 3 step 1. The state machine:

```
not jailed              -> return true
jailed, rolls doubles   -> release, move by that roll, return false (turn consumed)
jailed, can pay bail    -> charge JAIL_BAIL, release, return true
jailed, jailTurns < 3   -> jailTurns++, return false
jailed, jailTurns == 3  -> auto-pay bail (D10), release, return true
```

Landing on square 10 without being sent there is Just Visiting — no state change.

- [ ] **Step 3:** Print each transition. The spec gives no jail template in §5, so use the same voice as its neighbours: `%s is in Jail.`, `%s rolled doubles and left Jail.`, `%s paid LKR 300 bail.`

- [ ] **Step 4:** Wire `resolve_jail` into `play_turn` before the roll; skip steps 2–7 when it returns false.

**Verify:** `./monopoly 42 | grep -c "Square 30 to Square 30"` must be 0 — a player sent to jail must appear at square 10, not 30. `./monopoly 42 | grep -A3 "moves from Square .* to Square 30"` shows the jail transition each time. No player may stay jailed more than four consecutive turns.

**Commit:** `feat: jail state machine`

---

### Stage 13: Monopolies and even house construction

**Goal:** Owning a full colour group unlocks building, and houses go up evenly.

**Spec:** R2.7, R2.8 · Rule 8, Rule 9 · §5 "Building Construction"
**Concept:** [02 §10 Invariants](../../learning/02-program-design.md)

**Files:**
- Modify: `board.c`, `players.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
bool group_monopoly(const GameState *g, int p, PropertyGroup grp);
int  building_cost(const GameState *g, int sq, bool hotel);   /* third choke point */
void decide_build(GameState *g, int p);   /* PLACEHOLDER until Stage 30 */
```

- [ ] **Step 1:** `group_monopoly` — true when every square of that group is owned by `p`. Return false for `GRP_NONE`.

- [ ] **Step 2:** `building_cost` v1 — return the stored `houseCost` or `hotelCost`. Stages 24–28 add subsidy, boom and inflation modifiers **inside this function only**.

- [ ] **Step 3:** Placeholder `decide_build` — for each group the player monopolises, build one house on the property with the fewest houses, while affordable. That single rule enforces Rule 9's even-building requirement automatically.

- [ ] **Step 4:** Building rules: no building on a mortgaged property; maximum `MAX_HOUSES`; new buildings start at `conditionPct = 100`. Print the §5 block:

```
Aggressive Investor constructed one house on Galle Fort.
Construction Cost : LKR 1,500.
```

- [ ] **Step 5:** Wire `decide_build` into `play_turn` as Rule 3 step 6.

- [ ] **Step 6:** Add a `#ifdef DEBUG` invariant check after every build: within any group, `max(houses) - min(houses) <= 1`.

**Verify:** `./monopoly 42 | grep -c "constructed one house"` > 0. Compile with `make debug` and run — the even-building assertion must never fire. Confirm no property ever exceeds 4 houses.

**Commit:** `feat: monopolies and even house construction`

---

### Stage 14: Hotels and the rent multiplier table

**Goal:** Four houses convert to a hotel; rent follows Table 6 all the way up.

**Spec:** R2.8, R2.9 · Rule 10, Table 6 · §5 "Hotel Construction"
**Concept:** [02 §6 State machines](../../learning/02-program-design.md)

**Files:**
- Modify: `players.c`, `board.c`, `game.c`, `types.h`

- [ ] **Step 1:** Extend `decide_build` — once every property in a monopolised group has 4 houses, upgrade to hotels. A hotel **replaces** the four houses: set `houses = 0`, `hotel = true`. The two are never simultaneously present (Rule 10).

- [ ] **Step 2:** Print the §5 hotel line — note it has no cost line, unlike house construction:

```
Aggressive Investor upgraded Galle Fort to a Hotel.
```

- [ ] **Step 3:** Extend `square_rent` — `hotel` uses `HOTEL_RENT_MULT` (10×), otherwise `RENT_MULT[houses]`.

- [ ] **Step 4:** Extend `round_summary`'s `Hotels` field to count `hotel` squares owned by each player.

- [ ] **Step 5:** Add a `#ifdef DEBUG` invariant: `!(houses > 0 && hotel)` on every square.

**Verify:** `./monopoly 42 | grep -c "upgraded .* to a Hotel"` > 0 by round 500. The `Hotels :` counts in the round summaries must be non-zero late in the game. Confirm a hotel's rent is exactly 10× base rent (Dark Blue hotel = 10,000).

**Commit:** `feat: hotels and rent multipliers`

---

### Stage 15: Building condition, rent bands, and maintenance

**Goal:** Buildings decay every round and must be maintained at the start of a turn.

**Spec:** R3.15, R3.16 · LK 25, LK 26, LK 27 · Table 3
**Concept:** [03 §6 Linear decay](../../learning/03-economic-math.md), [02 §8 Lookup tables](../../learning/02-program-design.md)

**Files:**
- Modify: `finance.c`, `players.c`, `board.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
void age_and_condition_tick(GameState *g);   /* every round */
int  condition_rent_pct(int conditionPct);   /* Table 3 band */
void decide_maintenance(GameState *g, int p);/* PLACEHOLDER until Stage 30 */
```

- [ ] **Step 1:** `age_and_condition_tick` — at the end of every round, `age++` on every property and `conditionPct -= COND_DECAY_PCT` on every square carrying buildings, floored at 0. Also increment `unmaintainedRounds` on those squares (used in Stage 22).

- [ ] **Step 2:** `condition_rent_pct` — Table 3, as a band lookup:

```c
int condition_rent_pct(int c)
{
    if (c >= 90) return 100;
    if (c >= 75) return  90;
    if (c >= 50) return  75;
    if (c >= 25) return  50;
    return 0;                  /* below 25% the building is Closed — LK 26 */
}
```

- [ ] **Step 3:** Apply it inside `square_rent`, but **only when the square carries buildings**. An undeveloped property has no condition to decay and collects full base rent.

- [ ] **Step 4:** Placeholder `decide_maintenance` — maintain every building below 75% condition while affordable. Cost is 5% of construction cost per house, 8% per hotel (LK 27); restores `conditionPct` to 100 and resets `unmaintainedRounds` to 0.

- [ ] **Step 5:** Wire `decide_maintenance` into `play_turn` as **Rule 3 step 1 only**. LK 27 permits maintenance nowhere else — not at the end of the turn, not when landing on the property.

- [ ] **Step 6:** Call `age_and_condition_tick` at the end of `play_round`, before `round_summary`.

**Verify:** Add a temporary print of one developed square's `conditionPct` each round; confirm it falls exactly 2 per round and snaps to 100 on maintenance. Then remove it. Confirm rent on a building at 60% condition is exactly 75% of its full rent, truncating.

**Commit:** `feat: building condition, rent bands, maintenance`

---

# Phase 4 — Banking

### Stage 16: Collateral, borrowing capacity, and loan origination

**Goal:** Players can take a secured loan at the Bank square.

**Spec:** R1.6, R3.1, R3.2, R3.3 · LK 1, LK 2, LK 3, LK 5 · D5 · §5 "Obtaining a Loan"
**Concept:** [03 §7 Loan-to-value](../../learning/03-economic-math.md)

**Files:**
- Modify: `finance.c`, `players.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
bool eligible_collateral(const GameState *g, int p, int sq);
int  max_loan(const GameState *g, int p);
void grant_loan(GameState *g, int p, int amount);
void decide_bank(GameState *g, int p);   /* PLACEHOLDER until Stage 30 */
```

- [ ] **Step 1:** `eligible_collateral` — owned by `p`, type is property/railway/utility, and **not** already mortgaged or loan-locked. Buildings are never collateral (LK 1).

- [ ] **Step 2:** `max_loan` — 75% of the summed `mortgageValue` of all eligible collateral (LK 2, D5). Returns 0 if the player already has an active loan — LK 5 permits one loan at a time.

- [ ] **Step 3:** `grant_loan` — credit the cash, set `loan.active`, `loan.principal = amount`, `loan.ratePct = econ.interestRatePct` (**frozen for the loan's life**, LK 13), `loan.roundsLeft = LOAN_ROUNDS`. Mark every pledged square `loanLocked = true`.

Loan-locked assets still earn rent and may still be developed — they simply cannot be sold, traded, auctioned or re-mortgaged (LK 3).

Print the §5 block, listing each collateral square on its own line:
```
Aggressive Investor obtained a secured loan.
Loan Amount : LKR 15,000.
Collateral :
Galle Fort
Unawatuna
Interest Rate : 8%
Duration : 20 Rounds
```

- [ ] **Step 4:** Placeholder `decide_bank` — borrow `max_loan` if there is no active loan, otherwise repay what is affordable.

- [ ] **Step 5:** Wire `SQ_BANK` in `land_on` to `decide_bank`. LK 5 allows **exactly one** loan action per landing — enforce it structurally by having `decide_bank` return after its first action.

**Verify:** `./monopoly 42 | grep -A6 "obtained a secured loan"` shows the full block with at least one collateral name. Loan amounts must never exceed 75% of the listed collateral's mortgage values — check one by hand. `Interest Rate : 8%` at first, since no rate-moving system exists yet.

**Commit:** `feat: collateral, borrowing capacity, loan origination`

---

### Stage 17: Interest accrual and repayment

**Goal:** Debt compounds every round and can be paid down.

**Spec:** R3.4 · LK 4, LK 5, LK 13 · D4 · §5 "Loan Repayment"
**Concept:** [03 §5 Compound interest](../../learning/03-economic-math.md), [03 §4 Overflow headroom](../../learning/03-economic-math.md)

**Files:**
- Modify: `finance.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
void accrue_interest(GameState *g);
void repay_loan(GameState *g, int p, int amount);
```

- [ ] **Step 1:** `accrue_interest` — at the end of every round, for each active loan: `principal += pct_of(principal, loan.ratePct)` at the **issued** rate, then `roundsLeft--`.

D4 takes LK 4 literally: the Table 9 figure applies per round despite being labelled annual. At 8% that is ×4.66 over a 20-round loan; at 15% it is ×16.4. That is intended and is what makes default a live threat.

- [ ] **Step 2:** `repay_loan` — charge the amount, reduce the principal. On full settlement clear `loan.active` and unlock every `loanLocked` square owned by that player. Print the §5 block, noting the value sits on its own line:

```
Aggressive Investor repaid LKR 5,000.
Outstanding Balance :
LKR 11,200.
```

- [ ] **Step 3:** Extend `decide_bank` to cover the LK 5 action set: obtain, repay part, repay in full, extend duration, increase amount. Exactly one per landing.

- [ ] **Step 4:** Extend `net_worth` to subtract `loan.principal`.

- [ ] **Step 5:** Extend `round_summary`'s `Outstanding Loan` to print the principal, or `None` when there is no active loan.

- [ ] **Step 6:** Add a `#ifdef DEBUG` guard that principal never exceeds `INT_MAX / 2` — if it ever trips, the overflow analysis in the economic-math document explains the fix.

**Verify:** Trace one loan across rounds with `./monopoly 42 | grep "Outstanding\|obtained a secured"`. Between repayments the balance must grow by exactly `principal * rate / 100` each round. `Outstanding Loan : None` must appear for players with no loan — never `LKR 0`.

**Commit:** `feat: interest accrual and loan repayment`

---

### Stage 18: Default and foreclosure

**Goal:** An unpaid loan at term forecloses its collateral.

**Spec:** R3.5 · LK 6, LK 7 · §5 "Loan Default"
**Concept:** [02 §6 State machines](../../learning/02-program-design.md)

**Files:**
- Modify: `finance.c`, `game.c`, `types.h`

**Interfaces:**
- Produces: `void check_loan_default(GameState *g);`

- [ ] **Step 1:** `check_loan_default` — runs immediately after `accrue_interest`, so a loan can default on the interest that just compounded. When `roundsLeft` reaches 0 with a principal still outstanding:
  - Transfer every `loanLocked` square owned by that player to the Bank (`owner = -1`).
  - Demolish their buildings (`houses = 0`, `hotel = false`).
  - Cancel those squares' insurance policies (`policy = INS_NONE`, `policyRounds = 0`).
  - Clear the debt entirely and deactivate the loan (LK 6).
  - If the player has no assets left at all, they are bankrupt (LK 7) — flag it; Stage 29 implements the consequences.

- [ ] **Step 2:** Print the §5 block:
```
Aggressive Investor has defaulted.
Collateral has been foreclosed.
Outstanding debt cleared.
```

- [ ] **Step 3:** Foreclosed assets return to the Bank, which is one of the three LK 19 auction triggers. Queue them for auction — reuse `run_auction`.

- [ ] **Step 4:** Call `check_loan_default` from the end-of-round scheduler, immediately after `accrue_interest`.

**Verify:** Run seeds until a default fires. After it, that player's `Outstanding Loan` must read `None`, their `Properties` count must have dropped, and their `Hotels` count must be 0 for the foreclosed squares. No square may remain `loanLocked` with no active loan — check under `make debug`.

**Commit:** `feat: loan default and foreclosure`

---

# Phase 5 — Insurance and hazard

### Stage 19: Insurance policies

**Goal:** Three policy tiers, purchasable at the two insurance squares, valid 20 rounds.

**Spec:** R1.7, R3.6 · §1.2, LK 8, LK 9, Appendix E · D3 · §5 "Purchasing Insurance", "Insurance Expiry"
**Concept:** [03 §11 Premiums versus expected loss](../../learning/03-economic-math.md)

**Files:**
- Modify: `finance.c`, `players.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
int  premium(const GameState *g, int sq, InsuranceType t);   /* 5/10/15% of square_value */
void buy_policy(GameState *g, int p, int sq, InsuranceType t);
void tick_insurance(GameState *g);
void decide_insurance(GameState *g, int p);   /* PLACEHOLDER until Stage 30 */
```

- [ ] **Step 1:** `premium` — 5%, 10% or 15% of `square_value` for Basic, Comprehensive and Business Interruption respectively (Appendix E). Because it reads `square_value`, premiums track inflation and market swings automatically.

- [ ] **Step 2:** `buy_policy` — charge the premium, set `policy` and `policyRounds = INS_ROUNDS`. One policy per property; buying again replaces and resets. Print the §5 block:

```
Comprehensive Insurance purchased.
Property : Galle Fort
Premium : LKR 450.
```

- [ ] **Step 3:** `tick_insurance` — decrement `policyRounds` on every insured square each round. At exactly `INS_WARN_ROUNDS` print the §5 warning; at 0 clear the policy.

```
Insurance policy on Galle Fort expires in 3 rounds.
```

- [ ] **Step 4:** Placeholder `decide_insurance` — insure any developed, uninsured property the player can afford, choosing Basic.

- [ ] **Step 5:** Wire `SQ_INSURANCE` in `land_on` to `decide_insurance`. Call `tick_insurance` from the scheduler.

**Verify:** `./monopoly 42 | grep -c "Insurance purchased"` > 0. Every purchase must be followed roughly 17 rounds later by exactly one expiry warning: `./monopoly 42 | grep -c "expires in 3 rounds"` should be close to the purchase count. Confirm the premium equals exactly 5/10/15% of that property's current value.

**Commit:** `feat: insurance policies`

---

### Stage 20: Disasters, claims, and repairs

**Goal:** Every 10 rounds a disaster may strike a developed property; insurance pays out.

**Spec:** R3.7, R3.8 · LK 10, LK 11 · D1, D3 · §5 "Disaster"
**Concept:** [03 §11 Premiums versus expected loss](../../learning/03-economic-math.md)

**Files:**
- Create: `events.c`
- Modify: `finance.c`, `board.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
int  repair_cost(const GameState *g, int sq);   /* D1: 50% of construction cost on it */
bool covers(InsuranceType t, Disaster d);       /* D3 coverage matrix */
void fire_disaster(GameState *g);               /* every 10 rounds */
void auto_repairs(GameState *g);                /* every round */
```

- [ ] **Step 1:** `repair_cost` per D1 — 50% of the current construction cost of the buildings on the square: `houses × building_cost(house)` or `building_cost(hotel)` for a hotel, halved. Because it reads `building_cost`, it tracks inflation.

- [ ] **Step 2:** `covers` per D3 — a plain matrix, with the gap documented:

```c
/* D3. Note: Building Collapse and Electrical Failure are covered by NO tier
   below Business Interruption. The spec's peril list (LK 10) and its coverage
   table (App E) disagree; this follows both literally. */
bool covers(InsuranceType t, Disaster d)
{
    switch (t) {
    case INS_BASIC:         return d == DIS_FIRE || d == DIS_FLOOD;
    case INS_COMPREHENSIVE: return d == DIS_FIRE || d == DIS_FLOOD || d == DIS_RIOT;
    case INS_BUSINESS:      return true;
    case INS_NONE:          return false;
    }
    return false;
}
```

- [ ] **Step 3:** `fire_disaster` — pick a random developed property; pick a random `Disaster`. Set `damaged = true`. If the policy covers the peril, credit compensation (80% of repair cost for Basic, 100% for Comprehensive and Business Interruption) and print the claim block. Otherwise the owner pays the repair cost out of pocket. Either way set `sufferedLoss = true` on the owner — the Risk Taker's insurance trigger depends on it.

```
Flood occurred.
Affected Property :
Unawatuna.
Insurance Claim Approved.
Compensation Paid :
LKR 2,500.
```

- [ ] **Step 4:** Gate rent on `damaged` inside `square_rent` — a damaged building collects nothing until repaired (LK 11).

- [ ] **Step 5:** `auto_repairs` — each round, any owner who can afford the repair cost on a damaged square pays it and clears `damaged` (LK 11).

- [ ] **Step 6:** Call `fire_disaster` on the 10-round cadence and `auto_repairs` every round, from the scheduler.

**Verify:** `./monopoly 42 | grep -c "occurred"` should be close to 50 (500 rounds ÷ 10), fewer if no developed property exists early. Confirm both paths appear: `grep -A4 occurred` should show some blocks with `Insurance Claim Approved` and some without. A damaged property must collect zero rent — confirm by checking a `landed on` for a damaged square shows `Rent Paid : LKR 0.` or no rent block at all.

**Commit:** `feat: disasters, claims, repairs`

---

# Phase 6 — Ageing

### Stage 21: Property age, depreciation, renovation

**Goal:** Old unrenovated properties lose value; landing on your own lets you renovate.

**Spec:** R3.10, R3.11 · LK 15, LK 16, LK 17 · §5 "Property Depreciation"
**Concept:** [03 §6 Linear versus compounding decay](../../learning/03-economic-math.md)

**Files:**
- Modify: `finance.c`, `players.c`, `board.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
void depreciation_tick(GameState *g);   /* every 5 rounds */
void decide_renovate(GameState *g, int p, int sq);   /* PLACEHOLDER until Stage 30 */
```

- [ ] **Step 1:** `depreciation_tick` — every 5 rounds, any property with `age > DEPREC_START_AGE` gains 1 percentage point of `depreciationPct`, capped at `DEPREC_CAP_PCT` (30%). Print the §5 block on each change:

```
Property
Maharagama
has depreciated by 5%.
Current Value
LKR 4,750.
```

- [ ] **Step 2:** Apply `depreciationPct` inside `square_value` — this is why `Current Value` in the message above is simply `square_value(g, sq)`.

- [ ] **Step 3:** Placeholder `decide_renovate` — renovate when `depreciationPct > 10` and affordable. Cost is `RENOVATE_PCT` (10%) of current market value; it clears `depreciationPct` and resets `age` to 0 (LK 17).

- [ ] **Step 4:** Wire `decide_renovate` into `land_on` for the case where the landing player already owns the square. LK 17 permits renovation only there.

- [ ] **Step 5:** Call `depreciation_tick` on the 5-round cadence.

**Verify:** No depreciation message may appear before round 51 — `./monopoly 42 | grep -n "has depreciated" | head -1` and cross-check the round. No property may exceed 30% depreciation. After a renovation, that property's `square_value` must return to its undepreciated level.

**Commit:** `feat: property age, depreciation, renovation`

---

### Stage 22: Structural damage

**Goal:** Twenty rounds of neglected maintenance permanently harms a property.

**Spec:** R3.17 · LK 28, LK 29
**Concept:** [02 §6 State machines](../../learning/02-program-design.md)

**Files:**
- Modify: `finance.c`, `board.c`, `types.h`

- [ ] **Step 1:** In `age_and_condition_tick`, when `unmaintainedRounds > UNMAINTAINED_LIMIT` on a square with buildings, set `structDamaged = true` **once** (not repeatedly).

- [ ] **Step 2:** Apply the three LK 28 consequences at their choke points:
  - Property value −15% → inside `square_value`.
  - Maximum rent −25% → inside `square_rent`.
  - Future maintenance +50% → inside the maintenance cost calculation in `decide_maintenance`.

- [ ] **Step 3:** Extend `decide_renovate` for LK 29 — renovating a structurally damaged building costs 25% of replacement value and clears `structDamaged`, restoring value, rent and condition together.

- [ ] **Step 4:** Print `Structural damage has occurred at %s.` — §5 gives no template, so match the voice of the depreciation block.

**Verify:** Under `make debug`, confirm `structDamaged` is set at most once per square by asserting it never transitions true→true. Confirm a structurally damaged property's value is exactly 15% below its otherwise-computed value.

**Commit:** `feat: structural damage and damaged-building renovation`

---

# Phase 7 — The living economy

### Stage 23: The effect registry and the market conditions block

**Goal:** The machinery every remaining economic system plugs into.

**Spec:** R3.20 · LK 34, LK 35, LK 36 · D12 · §5 "Rule-LK 36 output messages"
**Concept:** [02 §3 Permanent versus temporary effects](../../learning/02-program-design.md), [03 §3 Composing percentages](../../learning/03-economic-math.md)

**Files:**
- Modify: `events.c`, `board.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
void effect_push(GameState *g, EffectKind k, int scopeKind, int scope,
                 int magnitudePct, int owner, int rounds);
int  effect_modifier(const GameState *g, EffectKind k, int square, int player);
void tick_effects(GameState *g);
void market_conditions(const GameState *g);
```

- [ ] **Step 1:** `effect_push` — append to `econ.effects[]` if `effectCount < MAX_EFFECTS`. Under `#ifdef DEBUG`, assert on overflow rather than silently dropping.

- [ ] **Step 2:** `effect_modifier` — walk the array; for each record whose `kind` matches, whose `owner` is `-1` or the given player, and whose scope matches the square (global / that group / that region bit / that exact square / that player), compose the magnitudes. **Compose additively into a single percentage**, then apply once:

```c
int effect_modifier(const GameState *g, EffectKind k, int square, int player)
{
    int total = 0, i;
    for (i = 0; i < g->econ.effectCount; i++) {
        const Effect *e = &g->econ.effects[i];
        if (e->kind != k) continue;
        if (e->owner != -1 && e->owner != player) continue;
        if (!effect_in_scope(g, e, square)) continue;
        total += e->magnitudePct;          /* LK 34: cumulative */
    }
    return total;   /* caller does pct(value, total) */
}
```

LK 34 says "all percentage changes are cumulative", which reads naturally as additive composition. The economic-math document explains why additive and multiplicative composition differ and why additive is the defensible reading here — record the choice in a comment.

- [ ] **Step 3:** `tick_effects` — decrement `roundsLeft` on every record, then compact the array by removing expired ones (swap-with-last or shift; either is fine at this size).

- [ ] **Step 4:** Apply `effect_modifier` inside all three choke points: `square_value` (`EFF_VALUE_MUL`), `square_rent` (`EFF_RENT_MUL`, plus `EFF_HOTEL_RENT_MUL`, `EFF_RAILWAY_RENT_MUL`, `EFF_UTILITY_RENT_MUL` on the right square types), `building_cost` (`EFF_BUILD_COST_MUL`).

- [ ] **Step 5:** `market_conditions` — the §5 Rule-LK 36 block. 41 `=` characters. Each section prints its active effect and rounds remaining, or nothing if inactive.

```
=========================================
Current Market Conditions
=========================================

Market Boom
-------------
Southern Province (+20%)
Rounds Remaining : 7

Market Decline
----------------
Western Residential (-15%)
Rounds Remaining : 4

Regional Development
-----------------------
Northern Development Programme
(+30%)
Rounds Remaining : 12

Inflation
------------
+5%

Current Loan Interest
-----------------------
9%

=========================================
```

- [ ] **Step 6:** Call `market_conditions` at the end of `play_round`, **after** `round_summary`. Call `tick_effects` before both.

**Verify:** `./monopoly 42 | grep -c "Current Market Conditions"` = 500. With no effect systems wired yet, the Inflation and Current Loan Interest sections show `+0%` and `8%` and the others are empty. Under `make debug`, `effectCount` must never exceed `MAX_EFFECTS`.

**Commit:** `feat: effect registry and market conditions block`

---

### Stage 24: Inflation

**Goal:** Every 10 rounds, prices permanently reprice.

**Spec:** R3.9 · LK 12, LK 13, LK 14 · D6
**Concept:** [03 §3 Composing percentages](../../learning/03-economic-math.md), [03 §4 Overflow headroom](../../learning/03-economic-math.md)

**Files:**
- Modify: `events.c`, `game.c`, `types.h`

**Interfaces:**
- Produces: `void draw_inflation(GameState *g);`

- [ ] **Step 1:** `draw_inflation` — pick uniformly from `{-3, 0, 2, 5, 8, 12}` (LK 12) and store it in `econ.inflationPct` for the LK 36 block.

```c
static const int INFLATION_RATES[6] = { -3, 0, 2, 5, 8, 12 };
```

- [ ] **Step 2:** Apply it **permanently** (D12, LK 14) by mutating the stored fields of every square: `price`, `baseRent`, `houseCost`, `hotelCost`, `mortgageValue`, each through `pct(v, rate)`.

Insurance premiums and repair costs need no separate handling — they are derived from `square_value` and `building_cost`, so they inflate automatically. That is the choke-point pattern paying for itself.

- [ ] **Step 3:** Move `econ.interestRatePct` by the same rate, for **new loans only**. Existing loans keep `loan.ratePct` from issue (LK 13). This is the single most commonly mis-implemented rule in the spec — the `Loan` struct having its own `ratePct` field is what makes it correct by construction.

- [ ] **Step 4:** Print the inflation announcement. §5 gives no template; match the economic-event voice: `Inflation Rate : +5%` followed by `All property values, costs and rents have been recalculated.`

- [ ] **Step 5:** Call `draw_inflation` on the 10-round cadence.

**Verify:** `./monopoly 42 | grep -c "Inflation Rate"` = 50. Confirm that after an inflation draw, a property's purchase price has changed by exactly that percentage (truncating). Confirm an existing loan's `Interest Rate` in its original block does **not** change — grep the loan block and the LK 36 block and compare; they should diverge after the first non-zero draw.

**Commit:** `feat: inflation`

---

### Stage 25: Market booms and declines

**Goal:** Every 10 rounds one group booms and another declines, for 10 rounds each.

**Spec:** R3.18 · LK 30, LK 31, LK 32, LK 33, LK 34
**Concept:** [02 §3 The effect registry](../../learning/02-program-design.md)

**Files:**
- Modify: `events.c`, `game.c`, `types.h`

**Interfaces:**
- Produces: `void market_review(GameState *g);`

- [ ] **Step 1:** `market_review` — pick two distinct groups, one to boom and one to decline. Respect LK 33: a group cannot be selected again until 30 rounds have elapsed (`econ.groupCooldown[]`), and per LK 30 cannot repeat the same event in consecutive reviews (`econ.lastBoomGroup`, `econ.lastDeclineGroup`).

- [ ] **Step 2:** Push the LK 31 boom effects, all scoped `SCOPE_GROUP` for 10 rounds:

| Effect | Magnitude |
|--------|-----------|
| `EFF_VALUE_MUL` | +20 |
| `EFF_RENT_MUL` | +25 |
| `EFF_MORTGAGE_MUL` | +15 |
| `EFF_BUILD_COST_MUL` | +10 |

LK 31 also raises purchase prices 15%; push that as a `EFF_VALUE_MUL` companion or a dedicated kind — purchase price and market value are the same number in this model, so `+20` on value already governs both. Document the choice.

- [ ] **Step 3:** Push the LK 32 decline effects, scoped `SCOPE_GROUP` for 10 rounds:

| Effect | Magnitude |
|--------|-----------|
| `EFF_VALUE_MUL` | −15 |
| `EFF_RENT_MUL` | −20 |
| `EFF_MORTGAGE_MUL` | −10 |
| `EFF_AUCTION_OPEN_MUL` | −25 |

- [ ] **Step 4:** Apply `EFF_AUCTION_OPEN_MUL` inside `run_auction`'s opening bid calculation, and `EFF_MORTGAGE_MUL` inside `max_loan`'s collateral sum.

- [ ] **Step 5:** Populate the Market Boom and Market Decline sections of `market_conditions` from the live registry, showing the group name and rounds remaining.

- [ ] **Step 6:** Decrement `groupCooldown[]` each round; call `market_review` on the 10-round cadence.

**Verify:** `./monopoly 42 | grep -A3 "Market Boom"` shows a group and a countdown that decreases by one each round and disappears after 10. Under `make debug`, assert no group is selected while its cooldown is above zero. Confirm a boomed group's rent is exactly 25% above its unboomed value.

**Commit:** `feat: market booms and declines`

---

### Stage 26: National economic events and regional development cards

**Goal:** Two systems on the 15-round cadence — one global, one regional.

**Spec:** R3.12, R3.19 · LK 18, LK 35, Table 4 · D14 · §5 "Economic Event"
**Concept:** [02 §3 Effect scopes](../../learning/02-program-design.md)

**Files:**
- Modify: `events.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
void national_event(GameState *g);
void regional_card(GameState *g);
```

- [ ] **Step 1:** `national_event` — pick one of the eight LK 18 events at random, push its effects with `owner = -1` and `rounds = 15`:

| Event | Effects |
|-------|---------|
| Tourism Boom | `HOTEL_RENT_MUL +100` global; `VALUE_MUL +15` on `REGION_SOUTHERN_COASTAL` |
| Fuel Crisis | `RAILWAY_RENT_MUL +100` global; `BUILD_COST_MUL +20` global |
| Heavy Monsoon | `PREMIUM_MUL +20` global; `VALUE_MUL −10` on `REGION_COASTAL`; flood weight doubled |
| Economic Recession | `VALUE_MUL −15`, `RENT_MUL −10` global; `INTEREST_ADD +15` |
| Stock Market Boom | `VALUE_MUL +10` global; `INTEREST_ADD −10` |
| Government Housing Programme | `BUILD_COST_MUL −25` global |
| Foreign Investment | `VALUE_MUL +20` on `REGION_COMMERCIAL` |
| Political Unrest | `HOTEL_RENT_MUL −50` global; riot weight doubled |

The two "risk" effects (flood weight, riot weight) are read by `fire_disaster` when picking a peril — implement as a weight lookup that consults the registry rather than a separate flag.

Print the §5 block. Its third line is the event's own effect description:
```
Economic Event
Tourism Boom
Southern Province properties increase in value by 15%.
```

- [ ] **Step 2:** `regional_card` — pick one of the **twelve** Table 4 cards, push its effects for 15 rounds. (Table 4 lists 12, not 13 — the earlier requirements draft miscounted.)

| Card | Effect |
|------|--------|
| Southern Tourism Boom | `RENT_MUL +40` on squares 26, 27, 29 |
| Port City Expansion | `VALUE_MUL +25` on squares 1, 3, 5 |
| IT Industry Growth | `VALUE_MUL +20` on squares 13, 11, 14 |
| Northern Development Programme | `VALUE_MUL +30` on squares 31, 32, 34 |
| Tea Export Boom | `VALUE_MUL +35` on square 37 |
| Airport Expansion | `RENT_MUL +30` on squares 16, 18, 19 |
| University City Growth | `VALUE_MUL +20` on squares 23, 21 |
| Beach Pollution | `RENT_MUL −30` on `REGION_SOUTHERN_COASTAL` |
| Flood Damage | `VALUE_MUL −20` on `REGION_COASTAL` |
| Transport Strike | `RAILWAY_RENT_MUL −40` global |
| Electricity Tariff Increase | `UTILITY_RENT_MUL +25` global |
| Water Shortage | `UTILITY_RENT_MUL +20` on square 28; `VALUE_MUL −10` on `REGION_NWSDB_ADJACENT` |

Multi-square cards push one effect per square with `SCOPE_SQUARE`, or one effect with `SCOPE_REGION` where a region tag already captures the set exactly.

- [ ] **Step 3:** Populate the Regional Development section of `market_conditions` with the card name, its magnitude, and rounds remaining.

- [ ] **Step 4:** Call both on the 15-round cadence, national event first.

**Verify:** `./monopoly 42 | grep -c "^Economic Event"` ≈ 33 (500 ÷ 15). Every event must be followed by two more lines. Confirm expiry is automatic: a regional card's section must vanish from the LK 36 block exactly 15 rounds after it appears, and the affected squares' values must return to their market-adjusted baseline — that is LK 35, and it should require no code of its own.

**Commit:** `feat: national economic events and regional development cards`

---

### Stage 27: Government regulations

**Goal:** Every 20 rounds one regulation takes effect.

**Spec:** R3.14 · LK 24 · D2 · §5 "Government Regulation"
**Concept:** [02 §3 Effect registry](../../learning/02-program-design.md)

**Files:**
- Modify: `events.c`, `finance.c`, `players.c`, `game.c`, `types.h`

**Interfaces:**
- Produces: `void government_regulation(GameState *g);`

- [ ] **Step 1:** Pick one of the eight LK 24 regulations. A regulation stays active until the next one replaces it, so push with a duration of 20 rounds.

| Regulation | Implementation |
|------------|----------------|
| Increase Property Tax | `EFF_TAX_MUL +50` global — read in `pay_tax` (D2) |
| Reduce Loan Interest | `EFF_INTEREST_ADD −2` — read when setting a new loan's rate |
| Housing Subsidy | `EFF_BUILD_COST_MUL −30` global |
| Luxury Property Tax | charge 25% of developed value on hotel owners each round |
| Railway Modernization | `EFF_RAILWAY_RENT_MUL +25` global |
| Electricity Tariff Revision | `EFF_UTILITY_RENT_MUL +20` global |
| Insurance Regulation | `EFF_PREMIUM_MUL −15` global; coverage unchanged |
| Anti-Speculation Act | `EFF_MAX_PROPERTIES 3` — gate in `decide_buy` |

- [ ] **Step 2:** Wire the three non-multiplier regulations to their read sites: `pay_tax` consults `EFF_TAX_MUL`; `grant_loan` consults `EFF_INTEREST_ADD`; `decide_buy` refuses a purchase that would take the player above three **undeveloped** properties while `EFF_MAX_PROPERTIES` is active.

- [ ] **Step 3:** Luxury Property Tax is a recurring charge, not a multiplier — apply it in the scheduler while the regulation is active.

- [ ] **Step 4:** Print the §5 block — three lines, the second naming the regulation and the third describing it:

```
Government Regulation
Housing Subsidy Introduced.
Construction costs reduced by 30%.
```

- [ ] **Step 5:** Call on the 20-round cadence.

**Verify:** `./monopoly 42 | grep -c "^Government Regulation"` = 25. When Housing Subsidy is active, the `Construction Cost :` in build messages must be 30% below the group's list cost. When Increase Property Tax is active, tax charges must be 1,500 rather than 1,000 (before inflation).

**Commit:** `feat: government regulations`

---

### Stage 28: The National Event Card deck

**Goal:** A 20-card circular deck drawn only on Event squares.

**Spec:** R3.21 · Appendix A
**Concept:** [02 §7 Circular queues](../../learning/02-program-design.md)

**Files:**
- Modify: `events.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
void deck_init(GameState *g);
void draw_event_card(GameState *g, int p);
```

This is a **separate system** from Stage 26's national economic events. Different trigger (landing, not cadence), different scope (the drawing player, not everyone), different card pool. Do not merge them.

- [ ] **Step 1:** `deck_init` — fill `deck.cards[0..19]` with the 20 card ids and shuffle with Fisher-Yates. `head = 0`.

- [ ] **Step 2:** `draw_event_card` — read `cards[head]`, execute it, then `head = (head + 1) % DECK_SIZE`. That single line is what "returned to the bottom of the deck" means for a circular queue: nothing moves, only the index advances. Drawing is O(1) and no element is ever shifted.

- [ ] **Step 3:** Execute each card, scoped `SCOPE_PLAYER` to the drawer with `owner = p` and `rounds = 15` unless the card names a shorter duration (Appendix A: "in addition to other modifiers"):

| Card | Implementation |
|------|----------------|
| Tourism Hype | `HOTEL_RENT_MUL +100`, 5 rounds |
| Fuel Shortage | `RAILWAY_RENT_MUL +100`, 5 rounds |
| Heavy Floods | damage a random `REGION_COASTAL` property immediately |
| Political Rally | `EFF_CLOSED` on a random owned square, 2 rounds |
| Stock Market Rise | `VALUE_MUL +10`, 15 rounds |
| Economic Downturn | `VALUE_MUL −15`, 15 rounds |
| Housing Subsidy | `BUILD_COST_MUL −30`, 15 rounds |
| Interest Rate Cut | `INTEREST_ADD −2`, 15 rounds |
| Interest Rate Increase | `INTEREST_ADD +2`, 15 rounds |
| Tax Amnesty | credit LKR 2,000 to **every** player immediately |
| Power Failure | `UTILITY_RENT_MUL −50`, 3 rounds |
| Foreign Funding | `VALUE_MUL +15` on `REGION_COMMERCIAL`, 15 rounds |
| Port Expansion | `VALUE_MUL +20` on railways, 15 rounds |
| Festival Season | `HOTEL_RENT_MUL +50`, 15 rounds |
| Labour Strike | `CONSTRUCTION_SUSPENDED`, 2 rounds |
| Insurance Discount | `PREMIUM_MUL −20`, 15 rounds |
| Property Revaluation | `VALUE_MUL +15` on a random group, 15 rounds |
| Currency Depreciation | `BUILD_COST_MUL +10`, 15 rounds |
| Government Grant | credit LKR 5,000 to a random player immediately |
| National Disaster | damage a random developed property immediately |

- [ ] **Step 4:** Honour `EFF_CLOSED` in `square_rent` (no rent while closed) and `EFF_CONSTRUCTION_SUSPENDED` in `decide_build` (no building while suspended).

- [ ] **Step 5:** Print the card name and effect on draw, in the same two-line voice as the economic event block.

- [ ] **Step 6:** Wire `SQ_EVENT` in `land_on` to `draw_event_card`. Call `deck_init` from `game_init`.

**Verify:** `./monopoly 42 | grep -c "Event Card"` > 0. Over a 500-round game the draws must cycle: extract the card names in order and confirm no card appears a 2nd time before all 20 have appeared once. Under `make debug`, assert `head` stays within 0–19.

**Commit:** `feat: national event card deck`

---

# Phase 8 — Failure and personalities

### Stage 29: Debt recovery, bankruptcy, and full net worth

**Goal:** Players who cannot pay liquidate in a defined order, then go bankrupt.

**Spec:** R2.13, R2.15 · Rule 11, Rule 14, Rule 15 · D11, D15 · §5 "Bankruptcy"
**Concept:** [03 §8 Net worth as a balance sheet](../../learning/03-economic-math.md)

**Files:**
- Modify: `finance.c`, `game.c`, `types.h`

**Interfaces:**
- Produces:
```c
bool raise_funds(GameState *g, int p, int needed);   /* D11 ladder */
void declare_bankrupt(GameState *g, int p, int creditor);
```

- [ ] **Step 1:** `raise_funds` — the D11 ladder, in this exact order:
  1. Sell buildings back to the Bank at 50% of construction cost.
  2. Mortgage unmortgaged, non-loan-locked assets at their mortgage value.
  3. If still short, return false.

- [ ] **Step 2:** Rewire `charge` — when `cash < amt`, call `raise_funds` first. Only if that fails does the player go bankrupt. This makes `charge` the single place insolvency is ever detected.

- [ ] **Step 3:** `declare_bankrupt` per Rule 14 — remove all buildings, cancel all policies, make loans immediately due, transfer remaining cash to the creditor (or the Bank), set `bankrupt = true`, and send every owned square to auction (LK 19 trigger). Print the §5 block:

```
Risk Taker has been declared bankrupt.
Remaining assets transferred to the Bank.
```

- [ ] **Step 4:** Complete `net_worth` per Rule 15 — `cash + property + buildings + railway + utility + claims receivable − loans − accrued interest − taxes due`. Building value is book value at construction cost. Claims receivable is always 0 (D15), because LK 10 credits compensation immediately.

- [ ] **Step 5:** Confirm `game_over` and `final_report` handle the last-solvent-player ending, and that `play_round` skips bankrupt players.

**Verify:** Force bankruptcies by temporarily setting `START_CASH` to 3000; confirm the ladder fires in order (buildings sold before mortgages), the bankruptcy block prints, the game ends early with a winner, and no bankrupt player takes a further turn. **Restore `START_CASH` to 30000 before committing.**

**Commit:** `feat: debt recovery, bankruptcy, net worth`

---

### Stage 30: Aggressive Investor

**Goal:** The first real personality. Replace placeholder bodies only.

**Spec:** R4.1 · §3.1 · D9
**Concept:** [02 §9 Isolating decisions behind an interface](../../learning/02-program-design.md)

**Files:**
- Modify: `players.c` **only**

Every `decide_*` becomes a `switch (g->players[p].strat)` with the `STRAT_AGGRESSIVE` arm implemented and the other three falling through to the existing placeholder body. Stages 31–33 fill the remaining arms. **No signature changes, no other file touched.**

- [ ] **Step 1:** `decide_buy` — always buy if the player can still afford one future rent afterwards. Prefer squares that complete a group. Prioritise Galle Face (39) and Nuwara Eliya (37).
- [ ] **Step 2:** `decide_bid` — always participate; bid up to 120% of `square_value` (D9).
- [ ] **Step 3:** `decide_build` — maximum houses immediately on any monopoly; convert to hotels as soon as legal.
- [ ] **Step 4:** `decide_bank` — borrow whenever the funds would raise projected rental income; repay only when cash exceeds twice the outstanding loan.
- [ ] **Step 5:** `decide_insurance` — Basic on houses, Comprehensive on hotels.
- [ ] **Step 6:** `decide_maintenance` and `decide_renovate` — maintain to protect rent; never sell voluntarily.
- [ ] **Step 7:** Keep a comment checklist mapping each §3.1 bullet to the line implementing it.

**Verify:** `./monopoly 42` — the Aggressive Investor's `Properties` and `Hotels` counts should lead the field by round 100, and its `Cash` should be among the lowest. If it is hoarding cash, `decide_buy` is too conservative.

**Commit:** `feat: aggressive investor strategy`

---

### Stage 31: Conservative Banker

**Goal:** The capital-preservation personality.

**Spec:** R4.2 · §3.2 · D9

**Files:**
- Modify: `players.c` **only**

- [ ] **Step 1:** `decide_buy` — buy only if at least 50% of current cash remains afterwards. Prefer railways and utilities. Refuse all purchases while `EFF_VALUE_MUL` is globally negative (an economic recession).
- [ ] **Step 2:** `decide_bid` — participate only while the bid stays strictly below `square_value`.
- [ ] **Step 3:** `decide_bank` — borrow only when bankruptcy is otherwise unavoidable; repay in full at every Bank visit when affordable.
- [ ] **Step 4:** `decide_insurance` — Comprehensive on every developed property.
- [ ] **Step 5:** `decide_build` — houses yes, but **no hotels while any loan is outstanding**.
- [ ] **Step 6:** `decide_renovate` — renovate once `depreciationPct > 10`.
- [ ] **Step 7:** Comment checklist against §3.2.

**Verify:** Across three seeds, the Conservative Banker must hold the largest `Cash` figure in the majority of round summaries — that is §3.2's stated outcome, and it is the cheapest signal that the strategy is wired correctly.

**Commit:** `feat: conservative banker strategy`

---

### Stage 32: Risk Taker

**Goal:** The maximally leveraged personality.

**Spec:** R4.3 · §3.3 · D9

**Files:**
- Modify: `players.c` **only**

- [ ] **Step 1:** `decide_buy` — buy every available property whenever legally possible. Prefer expensive groups.
- [ ] **Step 2:** `decide_bid` — bid until cash is exhausted.
- [ ] **Step 3:** `decide_bank` — always borrow the maximum; refinance at every opportunity.
- [ ] **Step 4:** `decide_build` — hotels as early as possible.
- [ ] **Step 5:** `decide_insurance` — buy **only** after `sufferedLoss` is set. This is the field Stage 20 exists to populate.
- [ ] **Step 6:** `decide_maintenance` and `decide_renovate` — ignore depreciation until repair is unavoidable. Sell lower-value properties to fund premium developments.
- [ ] **Step 7:** Comment checklist against §3.3.

**Verify:** The Risk Taker must reach the loan-default and bankruptcy paths across several seeds — it is the player that exercises Stages 18 and 29. If it never defaults in five seeds, `decide_bank` is not borrowing the maximum.

**Commit:** `feat: risk taker strategy`

---

### Stage 33: Opportunistic Trader

**Goal:** The market-adaptive personality — the only one that reads the economy.

**Spec:** R4.4 · §3.4 · D9

**Files:**
- Modify: `players.c` **only**

- [ ] **Step 1:** `decide_buy` — buy only when projected appreciation exceeds construction cost. Per D9, projected appreciation is `square_value × (sum of active positive modifiers − active negative modifiers) / 100`, read straight from `effect_modifier`.
- [ ] **Step 2:** `decide_bid` — prefer auctions to direct purchase; bid more freely below market value than the Conservative Banker does.
- [ ] **Step 3:** `decide_bank` — borrow only when projected return exceeds `interestRatePct`.
- [ ] **Step 4:** `decide_build` — delay while inflation is positive; accelerate while `EFF_BUILD_COST_MUL` is negative (a housing subsidy).
- [ ] **Step 5:** `decide_insurance` — Comprehensive only on high-value developments.
- [ ] **Step 6:** `decide_renovate` — renovate once `depreciationPct > 15`. Sell squares carrying a negative `EFF_VALUE_MUL`.
- [ ] **Step 7:** Maintain a balanced portfolio — properties, railways and utilities. Comment checklist against §3.4.

**Verify:** Confirm the four personalities visibly diverge: run three seeds and compare the final round summary. Aggressive leads on Hotels, Conservative on Cash, Risk Taker on Properties (or is bankrupt), Opportunistic somewhere in between. Four identical-looking players means the `switch` arms are not being reached.

**Commit:** `feat: opportunistic trader strategy`

---

# Phase 9 — Conformance

### Stage 34: Output audit

**Goal:** Every §5 template matches emitted output character-for-character.

**Spec:** R5.1–R5.5 · §5

**Files:**
- Modify: whichever files fail the audit

- [ ] **Step 1:** Extract the §5 templates to a reference file:

```bash
pdftotext -layout assets/Assignment_1_unlocked.pdf - | sed -n '/^5 *Required Output Messages/,/^A *National Event Cards/p' > /tmp/spec-output.txt
```

- [ ] **Step 2:** Capture a full run: `./monopoly 42 > /tmp/run.txt`.

- [ ] **Step 3:** Walk the 22 templates one at a time — pre-game, roll-off, dice roll, movement, passing GO, purchase, rent, house construction, hotel construction, loan obtained, loan repaid, loan default, insurance purchase, disaster, auction, economic event, government regulation, depreciation, insurance expiry, bankruptcy, round summary, GAME OVER, market conditions. For each, `grep` the live output and compare against the spec text.

- [ ] **Step 4:** Check the details that are easy to get wrong and are graded: spaces around colons (`Cash : LKR 12,300`, not `Cash: LKR 12,300`); trailing full stops present on some lines and absent on others; which values sit on their own line; the exact `=` and `-` rule-line lengths (45 for the round summary, 41 for market conditions); blank lines within blocks.

- [ ] **Step 5:** Verify no money is ever printed without thousands separators: `grep -nP 'LKR \d{4,}' /tmp/run.txt` must return nothing.

- [ ] **Step 6:** Fix every mismatch found.

**Verify:** all 22 templates ticked off, and the separator grep returns empty.

**Commit:** `fix: output conformance with spec section 5`

---

### Stage 35: Final validation

**Goal:** Sign-off against the definition of done.

**Spec:** R0.2, R0.7 · the Definition of Done in `REQUIREMENTS.md`

**Files:**
- Modify: `README.md`, `docs/REQUIREMENTS.md`

- [ ] **Step 1:** Both builds silent:
```bash
gcc *.c -o monopoly
gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly
```

- [ ] **Step 2:** Run five seeds to completion: `for s in 1 7 42 99 12345; do ./monopoly $s > /tmp/run-$s.txt || echo "FAILED $s"; done`. None may crash, hang, or await input.

- [ ] **Step 3:** Confirm both endings occur across the seed set — at least one 500-round game and at least one early win by bankruptcy. Force the second with a low `START_CASH` if no seed produces it naturally, then restore.

- [ ] **Step 4:** Confirm determinism: `./monopoly 42 > /tmp/a.txt && ./monopoly 42 > /tmp/b.txt && diff /tmp/a.txt /tmp/b.txt` — no output.

- [ ] **Step 5:** Confirm no interaction: `./monopoly 42 < /dev/null` completes normally.

- [ ] **Step 6:** Run `make debug` and confirm no invariant assertion fires across all five seeds.

- [ ] **Step 7:** Tick every checkbox in `docs/REQUIREMENTS.md` that now passes.

- [ ] **Step 8:** Update `README.md` — replace the "Planning phase, no C source yet" status with build and run instructions.

**Verify:** every box in the Definition of Done checked.

**Commit:** `chore: final validation and documentation`

---

## Self-review notes

**Spec coverage.** R0 → Stages 1, 2, 35 · R1 → 3, 8, 10, 16, 19 · R2 → 4–14, 29 · R3.1–3.5 → 16–18 · R3.6–3.8 → 19–20 · R3.9 → 24 · R3.10–3.11 → 21 · R3.12 → 26 · R3.13 → 11 · R3.14 → 27 · R3.15–3.17 → 15, 22 · R3.18 → 25 · R3.19–3.20 → 23, 26 · R3.21 → 28 · R4 → 30–33 · R5 → every stage plus 34. No orphan requirements.

**Known intentional gaps.** The `decide_*` placeholder bodies in Stages 8–29 are throwaway by design; Stages 30–33 replace bodies only, never signatures, so no other file is touched. `net_worth` is built in four passes (Stages 6, 9, 17, 29) — each extends it without changing its signature. `square_value`, `square_rent` and `building_cost` are likewise extended in place across Stages 9, 13, 15, 20–25 rather than duplicated.

**Type consistency.** Every cross-stage name comes from an Interfaces block, and `types.h` is the single prototype home — so any drift between what one stage produces and another consumes fails the build rather than silently diverging.

**Corrections to earlier documents.** Table 4 lists **12** regional development cards, not 13. `REQUIREMENTS.md` R3.19 has been corrected and now names all twelve.

---

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-28-straight-to-jail-staged.md`. Two execution options:

1. **Subagent-Driven (recommended)** — a fresh subagent per stage, reviewed between stages, fast iteration.
2. **Inline Execution** — stages executed in this session with checkpoints for review.
