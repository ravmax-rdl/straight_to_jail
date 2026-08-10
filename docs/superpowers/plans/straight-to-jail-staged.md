# Straight to Jail — Staged Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use `superpowers:subagent-driven-development` (recommended) or
> `superpowers:executing-plans` to implement this plan milestone-by-milestone. Steps use checkbox (`- [ ]`) syntax.

**Goal:** A fully autonomous four-player economic simulation in pure C — *Straight to Jail*, implementing the
MONOPOLY-LK specification — that compiles warning-free with `gcc *.c -o monopoly` and reproduces the spec's §5 output
exactly.

**Architecture:** One stack-allocated `GameState` threaded through every function; no globals, no `malloc`, no linked
lists. Money stored as `int`, ratio arithmetic in `double` rounded at one boundary (**D6′**). Two nested loops in
`game.c` (rounds ≤ 500, turns per solvent player) with an end-of-round scheduler firing the 5/10/15/20-round cadences in
a fixed order. All value, rent, cost and mortgage reads funnel through four choke points. The four personalities live
behind seven `decide_*` functions with fixed signatures.

**Tech stack:** C99, libc only — `stdio.h`, `stdlib.h`, `string.h`, `time.h`, `stdbool.h`, `limits.h`. **No `math.h`**
(**R0.9**). No test framework: a glob build cannot tolerate a second `main`.

**Reference documents:**
- Requirements and decisions D1–D26: [`docs/REQUIREMENTS.md`](../../REQUIREMENTS.md)
- Architecture rationale: [`docs/superpowers/specs/straight-to-jail-architecture-design.md`](../specs/straight-to-jail-architecture-design.md)
- Concept explanations: [`docs/reference/`](../../reference/) — cited per milestone as **Concept:**

**Revised 2026-08-10.** Replaces the previous 35-stage plan. The clarification set (per-property prices, 15% income tax,
Community Development Fund, float interest, single-claim insurance, age-from-purchase, auction rotation) is folded in,
and the structure is re-phased around the 2026-08-16 23:55 deadline.

---

## Schedule shape

Six milestones, one per remaining day. **Every milestone ends with a program that compiles clean, runs to completion,
and could be submitted as-is.** Losing a day costs depth, never a working build.

| Day | Milestone | Ends with |
|-----|-----------|-----------|
| 1 | Foundations | 500 rounds of four players moving, with both round-end blocks and GAME OVER |
| 2 | Transactions & jail | Property changes hands, rent flows, taxes bite, auctions run, jail works |
| 3 | Development, banking & failure | Houses, hotels, maintenance, loans, default, bankruptcy, true net worth |
| 4 | The living economy | Inflation, booms, events, regional cards, regulations, the card deck |
| 5 | Insurance & ageing | Policies, disasters, claims, repairs, depreciation, renovation, structural damage |
| 6 | Personalities & conformance | The four strategies, §5 audit, multi-seed validation |

**If a day slips**, drop in this order: milestone 5's structural damage (LK 28–29) → milestone 4's regional cards →
milestone 4's card deck. Never drop milestone 6 — §3 behaviours and §5 wording carry more marks than any single LK rule.

---

## Global constraints

Every milestone's requirements implicitly include all of these.

- **`gcc *.c -o monopoly` — zero errors, zero warnings, at the end of every milestone.** Development builds also run
  `gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly` and must be silent.
- **The binary is `monopoly`.** Spec §4 mandates that exact build line.
- **The first line of output is `MONOPOLY-LK Simulation`** (**R5.7**). The ASCII art currently in `main.c` must go.
- **Module split per Table 5.** All shared types, constants and **all** public prototypes live in `types.h`.
- **No global variables. No `malloc`. No linked lists. No `math.h`.**
- **Money is stored `int`.** Percentages go through `apply_pct` / `pct_of`, which are the only functions containing a
  `double` (**D6′**).
- **Every monetary figure printed goes through `fmt_lkr`.**
- **Seeding:** `srand(argc > 1 ? (unsigned)atoi(argv[1]) : (unsigned)time(NULL))`, once, in `main`.
- **Zero user interaction.**
- **§5 wording is verified inside the milestone that writes it**, not deferred.
- **Commit at the end of every milestone**, and ideally after each numbered step group.

### The verification cycle

1. Implement the step.
2. `gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly` → **silent**.
3. `./monopoly 42` → runs to completion.
4. Check the milestone's **Verify** block.
5. Commit.

---

## File structure

| File | Owns | First appears |
|------|------|---------------|
| `types.h` | Enums, constants, structs, **every** public prototype, the D-decision header block | M1 |
| `main.c` | Seed handling, `GameState` on the stack, `game_init` + `game_run`. Nothing else | M1 |
| `Makefile` | `monopoly` (canonical glob), `straight_to_jail` alias, `debug`, `clean` | M1 |
| `board.c` | 40-square table, dice/RNG, movement, ownership queries, the four choke points | M1 |
| `game.c` | `game_run`, roll-off, round/turn loops, `land_on` dispatch, the scheduler, all block output | M1 |
| `finance.c` | `fmt_lkr`, rounding helpers, `charge`/`credit`, tax, auctions, loans, insurance, net worth, bankruptcy | M1 |
| `players.c` | The seven `decide_*` strategy functions | M2 |
| `events.c` | Effect registry, disasters, inflation, market review, national events, regional cards, regulations, card deck | M2 (stub), M4 (real) |

---

# Milestone 1 — Foundations

**Goal:** A program that plays 500 rounds of four players moving around a correctly-valued board, printing both
round-end blocks and a final report.

**Spec:** R0.\*, R1.1–R1.7, R2.1–R2.5, R2.16, R5.4–R5.7 · §4, §5 · Rules 1–4, 15 · D6′, D7′, D8′, D14, D18, D26
**Concept:** [01 §2 Multi-file compilation](../../reference/01-c-language.md), [01 §4 Structs](../../reference/01-c-language.md), [02 §2 Modelling heterogeneous squares](../../reference/02-program-design.md)

**Files:** create `types.h`, `main.c`, `Makefile`, `board.c`, `finance.c`, `game.c`

### 1.1 Skeleton and the decision block

- [x] **Step 1:** Write `types.h` with a header guard and a top comment block recording decisions D1–D26 in one line
      each, citing the rule it resolves. Mark **D2′, D6′, D7′** explicitly as *supersedes the PDF*.
- [x] **Step 2:** Write `main.c`: seed, `GameState g` on the stack, `game_init`, `game_run`. Print the §5 pre-game block
      **exactly** — note the spaces around each colon, and that `MONOPOLY-LK Simulation` is line 1.

```
MONOPOLY-LK Simulation

Player 1 : Aggressive Investor
Player 2 : Conservative Banker
Player 3 : Risk Taker
Player 4 : Opportunistic Trader

Each player begins with LKR 30,000.
```

- [x] **Step 3:** Delete the ASCII-art banner. If it is worth keeping, guard it with `#ifdef SPLASH` — the graded build
      never defines it.
- [x] **Step 4:** Write `Makefile` (tabs, not spaces): `monopoly` uses the canonical glob line; `straight_to_jail` and
      `debug` (`-g -DDEBUG`) are conveniences; `clean` removes both binaries. Add `.gitignore` for `monopoly`,
      `straight_to_jail`, `*.exe`, `*.o`.

### 1.2 Types, constants, money and dice

- [x] **Step 5:** Add the enums to `types.h`. Note `SQ_COMMUNITY` is distinct from `SQ_EVENT` (**D17**).

```c
typedef enum {
    SQ_GO, SQ_PROPERTY, SQ_RAILWAY, SQ_UTILITY, SQ_BANK, SQ_INSURANCE,
    SQ_TAX, SQ_COMMUNITY, SQ_EVENT, SQ_JAIL, SQ_PARKING, SQ_GOTOJAIL
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
    EFF_AUCTION_OPEN_MUL, EFF_INTEREST_MUL, EFF_INTEREST_ADD, EFF_TAX_MUL,
    EFF_CLOSED, EFF_CONSTRUCTION_SUSPENDED, EFF_MAX_PROPERTIES,
    EFF_FLOOD_RISK, EFF_RIOT_RISK,
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

Both interest kinds exist deliberately (**D21**): large event shifts are relative (`MUL`), while the explicit ±2%
adjustments from LK 24 and App A are percentage points (`ADD`), because a relative 2% rounds to a no-op. Additive
shifts apply first, then multiplicative. `EFF_MAX_PROPERTIES` reuses `magnitudePct` as a plain count — note that in a
comment.

- [x] **Step 6:** Add the constants, each with a rule citation.

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
#define LOAN_ROUNDS     20     /* LK 4, D19 */
#define BASE_INTEREST_PCT 8    /* Table 9 Stable Economy, D21 */
#define INS_ROUNDS      20     /* LK 9      */
#define INS_WARN_ROUNDS  3     /* LK 9      */
#define MAX_HOUSES       4     /* Rule 9    */
#define INCOME_TAX_PCT  15     /* D2', D16  */
#define COMMUNITY_PCT   10     /* D16       */
#define COND_DECAY_PCT   2     /* LK 25     */
#define DEPREC_START_AGE 50    /* LK 16     */
#define DEPREC_CAP_PCT  30     /* LK 16     */
#define RENOVATE_PCT    10     /* LK 17     */
#define UNMAINTAINED_LIMIT 20  /* LK 28     */
#define MARKET_COOLDOWN 30     /* LK 33     */
#define DECK_SIZE       20     /* App A     */
#define MAX_EFFECTS    128     /* 4 players x card effects + globals + square-scoped regionals */
#define STATION_PRICE 1500     /* clarification */
#define STATION_MORTGAGE 750   /* clarification */
```

- [x] **Step 7:** Add the `Square`, `Loan`, `Player`, `Effect`, `Economy`, `EventDeck`, `GameState` structs exactly as in
      [architecture §4](../specs/straight-to-jail-architecture-design.md).
- [x] **Step 8:** Implement the money boundary in `finance.c` (**D6′**). These three are the *only* functions in the
      program containing a `double`.

```c
int money_round(double v) { return (int)(v + (v >= 0.0 ? 0.5 : -0.5)); }   /* no math.h, R0.9 */
int apply_pct (int value, int p) { return money_round(value * (1.0 + p / 100.0)); }
int pct_of    (int value, int p) { return money_round(value * (p / 100.0));        }
```

- [x] **Step 9:** Implement `fmt_lkr(char *buf, int amount)` in `finance.c` — thousands separators, no currency prefix
      (callers supply `LKR `), handles zero and negatives. `buf` must be ≥ 20 bytes.
- [x] **Step 10:** Implement `rng_range(lo, hi)` in `board.c` with a rejection loop (no modulo bias), plus `roll_die`
      and `roll_dice(int *d1, int *d2)`.
- [x] **Step 11:** Temporary check block in `main`: print `fmt_lkr` for `0, 5, 999, 1000, 30000, 1234567, -2500`, and a
      tally of 6,000 `roll_dice` totals. Confirm, then **delete the block**.

### 1.3 The board table

- [x] **Step 12:** Add two file-local tables to `board.c`. The group table supplies **only** construction costs and
      mortgage value (**D18**); its base price column is retained for reference and loan documentation.

```c
typedef struct { int basePrice, house, hotel, mortgage; } GroupValues;

static const GroupValues GROUP_VALUES[GRP_COUNT] = {  /* App B — D18 */
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

static const PropertyValues PROPERTY_VALUES[22] = {   /* Rent.csv — D7' */
    {  1,  1500,  100 }, {  3,  1800,  120 },
    {  6,  2500,  180 }, {  8,  2700,  200 }, {  9,  3000,  220 },
    { 11,  3500,  260 }, { 13,  3800,  280 }, { 14,  4000,  300 },
    { 16,  4500,  350 }, { 18,  4700,  370 }, { 19,  5000,  400 },
    { 21,  5500,  450 }, { 23,  5800,  480 }, { 24,  6000,  500 },
    { 26,  6500,  600 }, { 27,  6800,  620 }, { 29,  7000,  650 },
    { 31,  8000,  750 }, { 32,  8300,  780 }, { 34,  8500,  800 },
    { 37, 10000, 1000 }, { 39, 12000, 1200 }
};
```

- [x] **Step 13:** Write `board_init` filling all 40 squares from the R1.1 table. Properties take `price` and `baseRent`
      from `PROPERTY_VALUES` and `mortgageValue`/`houseCost`/`hotelCost` from `GROUP_VALUES[group]`. Railways and
      utilities take `price = STATION_PRICE`, `mortgageValue = STATION_MORTGAGE`. Every square gets `owner = -1`,
      `purchasedRound = -1`, `conditionPct = 100`, `policy = INS_NONE`, everything else zero.
- [x] **Step 14:** Apply the **D14** region bitmasks:

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

- [x] **Step 15:** Temporary dump block in `main` printing `index | type | name | group | price | baseRent | mortgage |
      regions` for all 40 squares. Check line-by-line against R1.1 and R1.3.
- [x] **Step 16:** Confirm the totals: 22 properties, 4 railways, 2 utilities, 1 bank, 2 insurance, 1 tax, 1 community,
      3 event, 4 special = 40. Group counts: Brown 2, Light Blue 3, Pink 3, Orange 3, Red 3, Yellow 3, Green 3,
      Dark Blue 2 = 22. Then delete the dump.

> A group with the wrong member count silently breaks monopoly detection in milestone 3 and is very hard to find
> later. Check it now.

### 1.4 Roll-off, loops, movement

- [x] **Step 17:** `game_init` — zero the `GameState`, call `board_init`, set the four players' names and strategies in
      Player 1–4 order, `cash = START_CASH`, `pos = 0`, `econ.interestRatePct = BASE_INTEREST_PCT`,
      `econ.incomeTaxPct = INCOME_TAX_PCT` (**D2′**), `econ.activeRegulation = -1`, `round = 0`.
- [x] **Step 18:** `determine_order` per Rule 2 and **D8′**. Every player rolls two dice; rank descending. **Only tied
      players reroll, and the reroll permutes only their own positions** — untied ranks never move. Repeat while ties
      remain. Print the §5 block; reroll rounds print the same `X rolls N.` line so ties are visible.

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

- [x] **Step 19:** `move_player` — record `from`, compute `to = (from + steps) % NUM_SQUARES`, print the movement line,
      then credit GO if `to < from || to == 0` (Rule 4).

```
Aggressive Investor moves from Square 12 to Square 20.
Aggressive Investor passed GO.
Collected LKR 2,000.
Current Balance : LKR 24,500.
```

- [x] **Step 20:** `play_turn` — Rule 3 steps 2–3 only for now: roll, print `%s rolled %d.`, move.
- [x] **Step 21:** `play_round` — increment `g->round`, then loop `g->order[]` calling `play_turn` for each non-bankrupt
      player. `game_over` — true when fewer than two players are solvent. `game_run` — loop while
      `g->round < MAX_ROUNDS && !game_over(g)`.

### 1.5 Round-end blocks and the final report

- [x] **Step 22:** `net_worth` v1 returns `players[p].cash`. Milestones 2 and 3 extend it; the signature never changes.
- [x] **Step 23:** `round_summary` — the §5 block, **compact** (**D26**). 45 `=` characters, 45 `-` between players,
      no separator after the last player before the closing `=` line. Players print in `order[]` sequence.
      `Outstanding Loan` prints `None` when there is no loan — **not** `LKR 0`.

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

- [x] **Step 24:** `market_conditions` skeleton — the LK 36 block with 41-character rules. Inflation shows `+0%`,
      Current Loan Interest shows `8%`, the other three sections are empty until milestone 4.
- [x] **Step 25:** Call `round_summary` then `market_conditions` at the end of `play_round`.
- [x] **Step 26:** `final_report` — the §5 GAME OVER block. Winner is the last solvent player, or the highest
      `net_worth` after 500 rounds. Note this block puts labels and values on **separate** lines, unlike the summary.
      Call it from `game_run` after the loop.

**Verify:**
```
gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly    # silent
./monopoly 42 | head -20                                # banner, roll-off, first turns
./monopoly 42 | grep -c "Summary"                       # 500
./monopoly 42 | grep -m1 '^=' | wc -c                   # 46 (45 rules + newline)
./monopoly 42 | grep -c "passed GO"                     # ~350  (500 x 4 x 7/40)
./monopoly 42 | grep -oE 'Square [0-9]+' | sort -u      # nothing outside 0-39
./monopoly 42 > a.txt && ./monopoly 42 > b.txt && diff a.txt b.txt   # empty
```
Run seeds 1, 7, 42, 99 — the first player must differ across seeds.

**Commit:** `feat: foundations — board, loops, movement, round blocks`

---

# Milestone 2 — Transactions & jail

**Goal:** Property changes hands, rent flows, both tax squares bite, declined purchases go to auction, and jail works.

**Spec:** R2.6–R2.7, R2.10–R2.14, R3.14–R3.16 · Rules 5–7, 11–13 · LK 19–23 · D1, D6′, D10, D11, D16, D17, D23
**Concept:** [03 §1 Integer money](../../reference/03-economic-math.md), [02 §4 The choke-point pattern](../../reference/02-program-design.md), [03 §10 English ascending auctions](../../reference/03-economic-math.md)

**Files:** create `players.c`, `events.c` (stub); modify `finance.c`, `board.c`, `game.c`, `types.h`

### 2.1 The money path and the two tax squares

- [ ] **Step 1:** `credit(g, p, amt)` — add to cash. `charge(g, p, amt, toPlayer)` — if `cash >= amt`, deduct, pay
      `toPlayer` unless `-1` (Bank), return true; otherwise return false. The full **D11** ladder lands in milestone 3;
      until then a `false` return is reported and the charge skipped. Nothing else in the program touches `cash`.
- [ ] **Step 2:** `effect_modifier` stub in `events.c` returning `0`, with the real signature. This lets milestone 2
      write the choke points in their **final** shape.
- [ ] **Step 3:** `total_assets(g, p)` (**D16**) — sum of `square_value` over the **22 coloured properties** owned by
      `p`. Buildings, railways and utilities excluded. This is the Community Development Fund's base and nothing else's.
- [ ] **Step 4:** The two tax squares take **different bases**, so they are two functions, not one parameterised helper:
  - `pay_income_tax` (**D2′**) — charges `pct_of(players[p].cash, rate)` where
    `rate = apply_pct(econ.incomeTaxPct, effect_modifier(EFF_TAX_MUL, ...))`.
  - `pay_community_fund` (**D16**) — charges `pct_of(total_assets(g, p), COMMUNITY_PCT)`.

> The Fund reads `square_value`, so it tracks the market automatically — exactly what the clarification means by "10%
> will also be affected by the market fluctuations". Income Tax instead tracks the market through its *rate*:
> `econ.incomeTaxPct` seeds at 15 and is moved by each inflation draw in milestone 4, the same way the loan rate is.
> A cash-based tax also gives the two squares genuinely different characters — the Fund punishes landholding, Income
> Tax punishes hoarding, which is the Conservative Banker's whole strategy.

- [ ] **Step 5:** Add `land_on` to `game.c` as a `switch` on `g->board[sq].type` with **every enum case listed
      explicitly** and no `default:`, so adding a `SquareType` later produces a warning rather than silence. Implement
      `SQ_TAX` and `SQ_COMMUNITY`; leave the rest as empty cases. Wire it into `play_turn` as Rule 3 step 4.

### 2.2 Purchase and the choke points

- [ ] **Step 6:** `is_purchasable` — true for `SQ_PROPERTY`, `SQ_RAILWAY`, `SQ_UTILITY`. `count_owned(g, p, type)`.
- [ ] **Step 7:** Placeholder `decide_buy` in `players.c` — `return cash >= square_value(g, sq);` for every strategy.
      Mark it clearly: *replaced in milestone 6; signature is final, only the body changes.*
- [ ] **Step 8:** `square_value` v1 — stored **individual** `price`, then `apply_pct` with
      `effect_modifier(EFF_VALUE_MUL, ...)`. Depreciation and structural damage slot in at milestone 5, **inside this
      function only**.
- [ ] **Step 9:** `mortgage_value` v1 — stored **group** `mortgageValue`, then `EFF_MORTGAGE_MUL`. Separate from
      `square_value` because **D18** gives them different bases.
- [ ] **Step 10:** `square_rent` v1 for `SQ_PROPERTY` — individual `baseRent` × the Table 6 development multiplier, as a
      lookup table rather than an `if` chain, then `EFF_RENT_MUL`. Returns 0 if unowned or mortgaged.

```c
static const int RENT_MULT[MAX_HOUSES + 1] = { 1, 2, 3, 5, 7 };   /* Table 6 */
#define HOTEL_RENT_MULT 10
```

- [ ] **Step 11:** Railway rent — by count owned **by the owner**, not the visitor, then `EFF_RAILWAY_RENT_MUL`.
      Utility rent — `4 × diceTotal` for one utility, `10 × diceTotal` for both, then `EFF_UTILITY_RENT_MUL`. Confirm
      `land_on` receives the live dice total from `play_turn`.

```c
static const int RAILWAY_RENT[4] = { 250, 500, 1000, 2000 };      /* Table 7 */
```

- [ ] **Step 12:** In `land_on`, for the three purchasable types: if `owner == -1`, call `decide_buy`; on true charge
      the price, set `owner = p` and `purchasedRound = g->round` (**D19**), and print the §5 block. On false, run the
      auction. If owned by another player, charge rent and print the §5 rent block. Landing on your own charges nothing.

```
Aggressive Investor purchased Galle Fort for LKR 6,500.
Remaining Balance : LKR 18,000.
```
```
Risk Taker landed on Galle Fort.
Rent Paid : LKR 600.
Owner : Aggressive Investor.
```

- [ ] **Step 13:** Extend `net_worth` to add `square_value` for every square owned by `p`. Extend `round_summary`'s
      `Properties` field to `count_owned(g, p, SQ_PROPERTY)`.

### 2.3 Auctions

- [ ] **Step 14:** Placeholder `decide_bid` — bid `currentBid + AUCTION_INC` while affordable and at most 60% of
      `square_value`; otherwise return 0 (withdraw).
- [ ] **Step 15:** `run_auction(g, sq, anchorPlayer)` per LK 19–23 and **D23**:
  - Opening bid = `pct_of(square_value(g, sq), AUCTION_OPEN_PCT)`, then `EFF_AUCTION_OPEN_MUL` (LK 32's −25%).
  - Participants: all solvent, non-bankrupt players — **including the one who declined the purchase**.
  - Bidding order starts with the player **immediately after `anchorPlayer`** in `order[]`, then clockwise.
  - Minimum increment `AUCTION_INC`; a bid may never exceed the bidder's cash; no loan may be taken mid-auction.
  - Withdrawal is permanent for that auction — track with a local `bool active[NUM_PLAYERS]`.
  - Loop until one active bidder remains → they pay and take ownership, `purchasedRound = g->round`.
  - **If every player withdraws at the opening price, ownership stays with the Bank** (LK 23) and nothing is charged.
- [ ] **Step 16:** Guard the loop: `#ifdef DEBUG` assert the body runs at most 200 times.

```
Auction Started.
Property :
Nuwara Eliya
Opening Bid :
LKR 5,000.
Risk Taker bids LKR 5,250.
Aggressive Investor bids LKR 5,500.
Conservative Banker withdraws.
Opportunistic Trader withdraws.
Aggressive Investor wins the auction.
```

### 2.4 Jail

- [ ] **Step 17:** In `land_on`, `SQ_GOTOJAIL` sets `pos = 10`, `jailed = true`, `jailTurns = 0`, and **does not** pay
      GO — the player is moved directly, not walked (Rule 12).
- [ ] **Step 18:** `resolve_jail(g, p)` runs as Rule 3 step 1, returning whether the player may move this turn:

```
not jailed              -> true
jailed, rolls doubles   -> release, move by that roll, return false (turn consumed)
jailed, can pay bail    -> charge JAIL_BAIL, release, return true
jailed, jailTurns < 3   -> jailTurns++, return false
jailed, jailTurns == 3  -> auto-pay bail (D10), release, return true
```

Landing on square 10 without being sent there is Just Visiting — no state change.

- [ ] **Step 19:** Print each transition. §5 gives no jail template, so match the voice of its neighbours:
      `%s is in Jail.`, `%s rolled doubles and left Jail.`, `%s paid LKR 300 bail.`
- [ ] **Step 20:** Wire `resolve_jail` into `play_turn` before the roll; skip steps 2–7 when it returns false.

**Verify:**
```
./monopoly 42 | grep -c purchased                       # <= 28 (22 props + 4 rail + 2 util)
./monopoly 42 | grep -A2 "landed on" | head -30         # every rent block has all three lines
./monopoly 42 | grep -A1 "landed on Colombo Fort" | grep "Rent Paid" | sort -u   # only 250/500/1000/2000
./monopoly 42 | grep -c "Square 30 to Square 30"        # 0 — jailed players appear at 10, not 30
./monopoly 42 | grep -c "Auction Started"               # > 0
```
Undeveloped rent must equal the individual base rent exactly: Pettah 100, Galle Face 1,200. Utility rent must be a
multiple of 4 or 10 and lie between 8 and 120. Each auction ends with exactly one `wins the auction` line or none at
all, and the winner's cash drops by exactly the final bid. If no auction fires, force `decide_buy` to return false
temporarily, confirm the block, then revert.

**Commit:** `feat: transactions, taxes, auctions, jail`

---

# Milestone 3 — Development, banking & failure

**Goal:** The traditional game is complete end to end — build, borrow, default, go bankrupt, and a true net worth.

**Spec:** R2.8–R2.9, R2.15, R2.17, R3.1–R3.5, R3.18–R3.19 · Rules 8–10, 14–15 · LK 1–7, 25–27 · D4, D5, D11, D19, D21, D22
**Concept:** [02 §10 Invariants](../../reference/02-program-design.md), [03 §5 Compound interest](../../reference/03-economic-math.md), [03 §7 Loan-to-value](../../reference/03-economic-math.md), [03 §8 Net worth as a balance sheet](../../reference/03-economic-math.md)

**Files:** modify `board.c`, `players.c`, `finance.c`, `game.c`, `types.h`

### 3.1 Monopolies, houses, hotels

- [ ] **Step 1:** `group_monopoly(g, p, grp)` — true when every square of that group is owned by `p`. False for
      `GRP_NONE`.
- [ ] **Step 2:** `building_cost(g, sq, hotel)` — stored group cost, then `EFF_BUILD_COST_MUL`. The third choke point.
- [ ] **Step 3:** Placeholder `decide_build` — for each monopolised group, build one house on the property with the
      fewest houses, while affordable. That single rule enforces Rule 9's even-building requirement automatically.
      Refuse on mortgaged squares, cap at `MAX_HOUSES`, new buildings start at `conditionPct = 100`.
- [ ] **Step 4:** Extend `decide_build` — once every property in a group has 4 houses, upgrade to hotels. A hotel
      **replaces** the four houses: `houses = 0`, `hotel = true`. Never both (Rule 10).
- [ ] **Step 5:** Extend `square_rent` — `hotel` uses `HOTEL_RENT_MULT` and `EFF_HOTEL_RENT_MUL`.
- [ ] **Step 6:** Extend `round_summary`'s `Hotels` count. Wire `decide_build` into `play_turn` as Rule 3 step 6.
- [ ] **Step 7:** `#ifdef DEBUG` invariants: within any group `max(houses) - min(houses) <= 1`, and
      `!(houses > 0 && hotel)` on every square.

```
Aggressive Investor constructed one house on Galle Fort.
Construction Cost : LKR 2,000.
```
```
Aggressive Investor upgraded Galle Fort to a Hotel.
```

> The hotel line has no cost line, unlike house construction. That asymmetry is in §5 and is graded.

### 3.2 Condition and maintenance

- [ ] **Step 8:** `condition_tick(g)` — at the end of every round, `conditionPct -= COND_DECAY_PCT` on every square
      carrying buildings, floored at 0, and `unmaintainedRounds++` on those squares.
- [ ] **Step 9:** `condition_rent_pct(c)` — Table 3 as a band lookup: ≥90 → 100, ≥75 → 90, ≥50 → 75, ≥25 → 50,
      else 0 (Closed, LK 26).
- [ ] **Step 10:** Apply it inside `square_rent`, **only when the square carries buildings**. An undeveloped property
      has no condition to decay and collects full base rent.
- [ ] **Step 11:** Placeholder `decide_maintenance` — maintain every building below 75% while affordable. Cost is 5% of
      construction cost per house, 8% per hotel (LK 27); restores `conditionPct = 100` and `unmaintainedRounds = 0`.
- [ ] **Step 12:** Wire `decide_maintenance` into `play_turn` as **Rule 3 step 1 only**. LK 27 permits maintenance
      nowhere else.

### 3.3 Loans

- [ ] **Step 13:** `eligible_collateral(g, p, sq)` — owned by `p`, type property/railway/utility, and **not** already
      mortgaged or loan-locked. Buildings are never collateral (LK 1).
- [ ] **Step 14:** `max_loan(g, p)` — 75% of the summed `mortgage_value` of all eligible collateral (LK 2, **D5**).
      Returns 0 if the player already has an active loan (LK 5 permits one at a time).
- [ ] **Step 15:** `grant_loan(g, p, amount)` — credit the cash; set `loan.active`, `loan.principal = amount`,
      `loan.ratePct = econ.interestRatePct` (**frozen for life**, LK 13), `loan.issuedRound = g->round`,
      `loan.termRounds = LOAN_ROUNDS`. Pledge the **minimum** set of assets, highest `mortgage_value` first, whose 75%
      LTV covers the amount (**D22**); mark those `loanLocked = true`. Loan-locked assets still earn rent and may still
      be developed. Print the §5 block, listing each collateral square on its own line.

```
Aggressive Investor obtained a secured loan.
Loan Amount : LKR 15,000.
Collateral :
Galle Fort
Unawatuna
Interest Rate : 8%
Duration : 20 Rounds
```

- [ ] **Step 16:** `accrue_interest(g)` — at the end of every round, for each active loan
      `principal = apply_pct(principal, loan.ratePct)` at the **issued** rate (**D4**, **D6′**).

> **D4** takes LK 4 literally: the Table 9 figure applies per round despite being labelled annual. At 8% that is ×4.66
> over a 20-round loan; at 15% it is ×16.4. That is intended, and it is what makes default a live threat.

- [ ] **Step 17:** `repay_loan(g, p, amount)` — charge, reduce the principal; on full settlement clear `loan.active` and
      unlock every `loanLocked` square owned by that player.

```
Aggressive Investor repaid LKR 5,000.
Outstanding Balance :
LKR 11,200.
```

- [ ] **Step 18:** `decide_bank` (placeholder) covering the full LK 5 action set — obtain, repay part, repay in full,
      extend the period (`termRounds += LOAN_ROUNDS`), increase the amount (top up to `max_loan`, re-freezing the rate
      on the combined balance). **Exactly one action per landing**: return after the first. Wire `SQ_BANK` to it.

> **R1.8**: this is the only route to repayment. A player who never lands on square 38 within their term defaults.
> That is the clarified rule, not a bug.

- [ ] **Step 19:** `check_loan_default(g)` — immediately after `accrue_interest`, so a loan can default on the interest
      that just compounded. When `g->round >= loan.issuedRound + loan.termRounds` with principal outstanding: transfer
      every `loanLocked` square owned by that player to the Bank, demolish their buildings, cancel those squares'
      policies, clear the debt, deactivate the loan (LK 6). Send the foreclosed squares to `run_auction` (LK 19). If the
      player has no assets left at all, flag bankrupt (LK 7).

```
Aggressive Investor has defaulted.
Collateral has been foreclosed.
Outstanding debt cleared.
```

- [ ] **Step 20:** Extend `net_worth` to subtract `loan.principal`, and `round_summary`'s `Outstanding Loan` to print
      the principal or `None`.
- [ ] **Step 21:** `#ifdef DEBUG` guards: principal never exceeds `INT_MAX / 2`; no square is `loanLocked` while its
      owner has no active loan.

### 3.4 Debt recovery and bankruptcy

- [ ] **Step 22:** `raise_funds(g, p, needed)` — the **D11** ladder, in this exact order:
  1. Sell buildings back to the Bank at 50% of construction cost.
  2. Mortgage unmortgaged, non-loan-locked assets at `mortgage_value`.
  3. Still short → return false.

  **Repaying or refinancing a loan is not a rung** — LK 5 and the clarification confine that to the Bank square.

- [ ] **Step 23:** Rewire `charge` — when `cash < amt`, call `raise_funds` first; only if that fails does the player go
      bankrupt. This makes `charge` the single place insolvency is ever detected.
- [ ] **Step 24:** `declare_bankrupt(g, p, creditor)` per Rule 14 — remove all buildings, cancel all policies, make the
      loan immediately due, transfer remaining cash to the creditor (or the Bank), set `bankrupt = true`, and send every
      owned square to auction (LK 19).

```
Risk Taker has been declared bankrupt.
Remaining assets transferred to the Bank.
```

- [ ] **Step 25:** Complete `net_worth` per Rule 15 — cash + property + buildings + railway + utility + claims
      receivable − loans − accrued interest − taxes due. Building value is book value at construction cost. Claims
      receivable is always 0 (**D15**).
- [ ] **Step 26:** Confirm `game_over` and `final_report` handle the last-solvent-player ending and that `play_round`
      skips bankrupt players.

**Verify:**
```
./monopoly 42 | grep -c "constructed one house"         # > 0
./monopoly 42 | grep -c "upgraded .* to a Hotel"        # > 0 by round 500
./monopoly 42 | grep -A6 "obtained a secured loan"      # full block, >= 1 collateral name
./monopoly 42 | grep "Outstanding\|obtained a secured"  # trace one loan across rounds
make debug && ./monopoly 42                             # no assertion fires
```
Between repayments a balance must grow by exactly `principal × rate / 100`, rounded. A loan amount must never exceed
75% of its listed collateral's mortgage values — check one by hand. `Outstanding Loan : None` must appear for
loan-free players, never `LKR 0`. Confirm a hotel's rent is exactly 10× its individual base rent (Galle Face hotel =
12,000). Force bankruptcies by temporarily setting `START_CASH` to 3000: the ladder must fire in order (buildings sold
before mortgages), the block must print, the game must end early with a winner, and no bankrupt player may take a
further turn. **Restore `START_CASH` to 30000 before committing.**

**Commit:** `feat: development, banking, default, bankruptcy`

---

# Milestone 4 — The living economy

**Goal:** Every timed economic system, plugged into one registry, with the LK 36 block populated.

**Spec:** R3.10, R3.13, R3.17, R3.21–R3.24, R5.5 · LK 12–14, 18, 24, 30–36, App A · D12, D13, D14, D21, D24, D25
**Concept:** [02 §3 Permanent versus temporary effects](../../reference/02-program-design.md), [03 §3 Composing percentages](../../reference/03-economic-math.md), [02 §7 Circular queues](../../reference/02-program-design.md)

**Files:** modify `events.c`, `board.c`, `finance.c`, `players.c`, `game.c`, `types.h`

### 4.1 The registry

- [ ] **Step 1:** `effect_push` — append to `econ.effects[]` if `effectCount < MAX_EFFECTS`; `#ifdef DEBUG` assert on
      overflow rather than silently dropping.
- [ ] **Step 2:** Replace the `effect_modifier` stub with the real walk: for each record whose `kind` matches, whose
      `owner` is `-1` or the given player, and whose scope matches the square (global / that group / that region bit /
      that exact square / that player), **sum** the magnitudes (LK 34 reads naturally as additive) and return the total.
      The caller applies it once via `apply_pct`.
- [ ] **Step 3:** `tick_effects` — decrement `roundsLeft` on every record, then compact out the expired.
- [ ] **Step 4:** Confirm all four choke points already consult the registry (they were written that way in milestone 2)
      and that no arithmetic needed to move. If any did, the choke-point rule was violated somewhere — fix it there.
- [ ] **Step 5:** Wire the end-of-round scheduler in **D13** order, and call `tick_effects` **after** the cadenced
      systems fire, so an effect created this round lives its full stated duration.

### 4.2 Inflation

- [ ] **Step 6:** `draw_inflation(g)` — pick uniformly from `{-3, 0, 2, 5, 8, 12}` (LK 12), store in
      `econ.inflationPct`.
- [ ] **Step 7:** Apply **permanently** (**D12**, LK 14) by mutating every square's stored `price`, `baseRent`,
      `houseCost`, `hotelCost`, `mortgageValue` through `apply_pct`.

> Premiums, repair costs and both tax bases need no separate handling — they derive from `square_value` and
> `building_cost`, so they inflate automatically. That is the choke-point pattern paying for itself.

- [ ] **Step 8:** Move `econ.interestRatePct` **and `econ.incomeTaxPct`** by the same factor — the loan rate applies to
      **new loans only** (**D21**), and the tax rate is what makes Income Tax "adjusted by inflation" (**D2′**). Existing loans keep
      `loan.ratePct` from issue (LK 13). This is the most commonly mis-implemented rule in the spec; the `Loan` struct
      owning its own `ratePct` is what makes it correct by construction.
- [ ] **Step 9:** Print the announcement. §5 gives no template — match the economic-event voice:
      `Inflation Rate : +5%` then `All property values, costs and rents have been recalculated.`

### 4.3 Booms and declines

- [ ] **Step 10:** `market_review(g)` — pick two distinct groups, one to boom and one to decline. Respect LK 33 (a group
      cannot be re-selected until 30 rounds have elapsed — `econ.groupCooldown[]`) and LK 30 (no group repeats the same
      event in consecutive reviews — `lastBoomGroup`, `lastDeclineGroup`). Decrement the cooldowns each round.
- [ ] **Step 11:** Push the LK 31 boom effects, `SCOPE_GROUP`, 10 rounds: `VALUE_MUL +20`, `RENT_MUL +25`,
      `MORTGAGE_MUL +15`, `BUILD_COST_MUL +10`. LK 31's separate "+15% purchase prices" is subsumed by `VALUE_MUL`,
      because purchase price and market value are the same number in this model — document that in a comment.
- [ ] **Step 12:** Push the LK 32 decline effects, `SCOPE_GROUP`, 10 rounds: `VALUE_MUL −15`, `RENT_MUL −20`,
      `MORTGAGE_MUL −10`, `AUCTION_OPEN_MUL −25`.
- [ ] **Step 13:** Populate the Market Boom and Market Decline sections of `market_conditions` from the live registry.

### 4.4 National events and regional cards

- [ ] **Step 14:** `national_event(g)` — one of the eight LK 18 events at random, pushed with `owner = -1`,
      `rounds = 15`:

| Event | Effects |
|-------|---------|
| Tourism Boom | `HOTEL_RENT_MUL +100` global; `VALUE_MUL +15` on `SOUTHERN_COASTAL` |
| Fuel Crisis | `RAILWAY_RENT_MUL +100` global; `BUILD_COST_MUL +20` global |
| Heavy Monsoon | `PREMIUM_MUL +20` global; `VALUE_MUL −10` on `COASTAL`; `FLOOD_RISK +100` |
| Economic Recession | `VALUE_MUL −15`, `RENT_MUL −10` global; `INTEREST_MUL +15` |
| Stock Market Boom | `VALUE_MUL +10` global; `INTEREST_MUL −10` |
| Government Housing Programme | `BUILD_COST_MUL −25` global |
| Foreign Investment | `VALUE_MUL +20` on `COMMERCIAL` |
| Political Unrest | `HOTEL_RENT_MUL −50` global; `RIOT_RISK +100` |

`INTEREST_MUL` is **relative** (**D21**): Recession takes 8% to 9%, matching §5's sample. `FLOOD_RISK` and `RIOT_RISK`
are read by milestone 5's disaster roll as peril weights, not as separate flags.

```
Economic Event
Tourism Boom
Southern Province properties increase in value by 15%.
```

- [ ] **Step 15:** `regional_card(g)` — one of the **twelve** Table 4 cards, 15 rounds. Multi-square cards push one
      `SCOPE_SQUARE` effect per square, or one `SCOPE_REGION` effect where a tag captures the set exactly.

| Card | Effect |
|------|--------|
| Southern Tourism Boom | `RENT_MUL +40` on 26, 27, 29 |
| Port City Expansion | `VALUE_MUL +25` on 1, 3, 5 |
| IT Industry Growth | `VALUE_MUL +20` on 13, 11, 14 |
| Northern Development Programme | `VALUE_MUL +30` on 31, 32, 34 |
| Tea Export Boom | `VALUE_MUL +35` on 37 |
| Airport Expansion | `RENT_MUL +30` on 16, 18, 19 |
| University City Growth | `VALUE_MUL +20` on 23, 21 |
| Beach Pollution | `RENT_MUL −30` on `SOUTHERN_COASTAL` |
| Flood Damage | `VALUE_MUL −20` on `COASTAL` |
| Transport Strike | `RAILWAY_RENT_MUL −40` global |
| Electricity Tariff Increase | `UTILITY_RENT_MUL +25` global |
| Water Shortage | `UTILITY_RENT_MUL +20` on 28; `VALUE_MUL −10` on `NWSDB_ADJACENT` |

- [ ] **Step 16:** Populate the Regional Development section of `market_conditions` with card name, magnitude and rounds
      remaining. Call both systems on the 15-round cadence, national event first.

### 4.5 Government regulations

- [ ] **Step 17:** `government_regulation(g)` — one of the eight LK 24 regulations, pushed for 20 rounds, replacing the
      previous one. Store the choice in `econ.activeRegulation`.

| Regulation | Implementation |
|------------|----------------|
| Increase Property Tax | `TAX_MUL +50` global — read in the tax helper (**D2′**) |
| Reduce Loan Interest | `INTEREST_ADD −2` — percentage points (**D21**), read when setting a new loan's rate |
| Housing Subsidy | `BUILD_COST_MUL −30` global |
| Luxury Property Tax | charged **once on activation** (**D24**) — 25% of each hotel property's value including buildings |
| Railway Modernization | `RAILWAY_RENT_MUL +25` global |
| Electricity Tariff Revision | `UTILITY_RENT_MUL +20` global |
| Insurance Regulation | `PREMIUM_MUL −15` global; coverage unchanged |
| Anti-Speculation Act | `MAX_PROPERTIES 3` — gate in `decide_buy` (**D25**) |

- [ ] **Step 18:** Wire the three non-multiplier regulations to their read sites: the tax helper consults `TAX_MUL`;
      `grant_loan` consults `INTEREST_MUL`; `decide_buy` refuses a purchase that would take the player above three
      **undeveloped** properties while `MAX_PROPERTIES` is active. Comment that **D25** makes LK 24's five-round
      development clause unreachable.

```
Government Regulation
Housing Subsidy Introduced.
Construction costs reduced by 30%.
```

### 4.6 The National Event Card deck

- [ ] **Step 19:** `deck_init` — fill `deck.cards[0..19]` with the 20 card ids, shuffle with Fisher–Yates, `head = 0`.
      Call it from `game_init`.
- [ ] **Step 20:** `draw_event_card(g, p)` — read `cards[head]`, execute, then `head = (head + 1) % DECK_SIZE`. That one
      line is what "returned to the bottom of the deck" means for an array-plus-index queue: nothing moves, only the
      index advances. O(1), no shifting, no linked list (**R0.5**).
- [ ] **Step 21:** Execute each card scoped `SCOPE_PLAYER` to the drawer with `owner = p` and `rounds = 15` unless the
      card names a shorter duration (App A: "in addition to other modifiers").

| Card | Implementation |
|------|----------------|
| Tourism Hype | `HOTEL_RENT_MUL +100`, 5 rounds |
| Fuel Shortage | `RAILWAY_RENT_MUL +100`, 5 rounds |
| Heavy Floods | damage a random `COASTAL` property immediately |
| Political Rally | `EFF_CLOSED` on a random owned square, 2 rounds |
| Stock Market Rise | `VALUE_MUL +10`, 15 rounds |
| Economic Downturn | `VALUE_MUL −15`, 15 rounds |
| Housing Subsidy | `BUILD_COST_MUL −30`, 15 rounds |
| Interest Rate Cut | `INTEREST_ADD −2`, 15 rounds |
| Interest Rate Increase | `INTEREST_ADD +2`, 15 rounds |
| Tax Amnesty | credit LKR 2,000 to **every** player immediately |
| Power Failure | `UTILITY_RENT_MUL −50`, 3 rounds |
| Foreign Funding | `VALUE_MUL +15` on `COMMERCIAL`, 15 rounds |
| Port Expansion | `VALUE_MUL +20` on railways, 15 rounds |
| Festival Season | `HOTEL_RENT_MUL +50`, 15 rounds |
| Labour Strike | `CONSTRUCTION_SUSPENDED`, 2 rounds |
| Insurance Discount | `PREMIUM_MUL −20`, 15 rounds |
| Property Revaluation | `VALUE_MUL +15` on a random group, 15 rounds |
| Currency Depreciation | `BUILD_COST_MUL +10`, 15 rounds |
| Government Grant | credit LKR 5,000 to a random player immediately |
| National Disaster | damage a random developed property immediately |

- [ ] **Step 22:** Honour `EFF_CLOSED` in `square_rent` and `EFF_CONSTRUCTION_SUSPENDED` in `decide_build`. Wire
      `SQ_EVENT` (squares 7, 22, 36 **only** — square 2 is `SQ_COMMUNITY`, **D17**) to `draw_event_card`. Print the card
      name and effect in the two-line economic-event voice.

**Verify:**
```
./monopoly 42 | grep -c "Current Market Conditions"     # 500
./monopoly 42 | grep -c "Inflation Rate"                # 50
./monopoly 42 | grep -c "^Economic Event"               # ~33  (500 / 15)
./monopoly 42 | grep -c "^Government Regulation"        # 25   (500 / 20)
make debug && ./monopoly 42                             # effectCount never exceeds MAX_EFFECTS
```
After an inflation draw a property's price must have changed by exactly that percentage, rounded. An existing loan's
`Interest Rate` must **not** change while the LK 36 block's rate does — grep both and confirm they diverge after the
first non-zero draw. A boomed group's rent must sit exactly 25% above its unboomed value. A regional card's section must
vanish from the LK 36 block exactly 15 rounds after it appears, with the affected squares returning to their
market-adjusted baseline — that is LK 35, and it should require no code of its own. Extract the drawn card names in
order and confirm no card repeats before all 20 have appeared.

**Commit:** `feat: effect registry, inflation, market, events, regulations, card deck`

---

# Milestone 5 — Insurance & ageing

**Goal:** Policies, disasters, claims, repairs, and the two decay systems.

**Spec:** R1.9, R3.6–R3.9, R3.11–R3.12, R3.20 · §1.2, LK 8–11, 15–17, 28–29, App E · D1, D3, D19, D20
**Concept:** [03 §11 Premiums versus expected loss](../../reference/03-economic-math.md), [03 §6 Linear versus compounding decay](../../reference/03-economic-math.md)

**Files:** modify `finance.c`, `events.c`, `players.c`, `board.c`, `game.c`, `types.h`

### 5.1 Policies

- [ ] **Step 1:** `premium(g, sq, tier)` — 5% / 10% / 15% of `square_value` (App E), then `EFF_PREMIUM_MUL`. Because it
      reads `square_value`, premiums track inflation and market swings automatically.
- [ ] **Step 2:** `buy_policy(g, p, sq, tier)` — charge the premium, set `policy` and `policyRounds = INS_ROUNDS`. One
      policy per property; buying again replaces and resets.

```
Comprehensive Insurance purchased.
Property : Galle Fort
Premium : LKR 650.
```

- [ ] **Step 3:** `tick_insurance(g)` — decrement `policyRounds` on every insured square each round. At exactly
      `INS_WARN_ROUNDS` print the §5 warning; at 0 clear the policy.

```
Insurance policy on Galle Fort expires in 3 rounds.
```

- [ ] **Step 4:** Placeholder `decide_insurance` — insure any developed, uninsured property the player can afford,
      choosing Basic. Wire `SQ_INSURANCE` in `land_on` to it.

### 5.2 Disasters, claims, repairs

- [ ] **Step 5:** `repair_cost(g, sq)` (**D1**) — 50% of the current construction cost of the buildings on the square:
      `houses × building_cost(house)`, or `building_cost(hotel)` for a hotel, halved. Reading `building_cost` makes it
      track inflation.
- [ ] **Step 6:** `covers(tier, disaster)` (**D3**) — a plain matrix, with the spec's gap documented in a comment:

```c
/* D3. Building Collapse and Electrical Failure are covered by NO tier below
   Business Interruption. The peril list (LK 10) and the coverage table (App E)
   disagree; this follows both literally. App E's "Earthquake" never occurs. */
```

Basic → {Fire, Flood} at 80%. Comprehensive → {Fire, Flood, Riot} at 100% (Vandalism is listed but never rolled).
Business Interruption → all perils at 100%, plus five rounds of hotel rent as an immediate lump sum, hotel properties
only.

- [ ] **Step 7:** `fire_disaster(g)` — every 10 rounds pick a random **developed** property and a random `Disaster`,
      weighting Flood and Riot by `EFF_FLOOD_RISK` / `EFF_RIOT_RISK`. Set `damaged = true`. If the policy covers the
      peril, credit the compensation and print the claim block; otherwise the owner pays the repair cost. Either way set
      `sufferedLoss = true` on the owner — the Risk Taker's insurance trigger depends on it.
- [ ] **Step 8:** **A payout consumes the policy** (**D20**) — set `policy = INS_NONE`, `policyRounds = 0`, regardless of
      rounds remaining. Not doing this is the single easiest way to fail the clarification.

```
Flood occurred.
Affected Property :
Unawatuna.
Insurance Claim Approved.
Compensation Paid :
LKR 2,500.
```

- [ ] **Step 9:** Gate rent on `damaged` inside `square_rent` — a damaged building collects nothing until repaired
      (LK 11).
- [ ] **Step 10:** `auto_repairs(g)` — each round, any owner who can afford the repair cost on a damaged square pays it
      and clears `damaged` (LK 11).

### 5.3 Depreciation and renovation

- [ ] **Step 11:** `depreciation_tick(g)` — every 5 rounds, any property whose age (`round - purchasedRound`, **D19**)
      exceeds `DEPREC_START_AGE` gains one percentage point of `depreciationPct`, capped at `DEPREC_CAP_PCT`. Unowned
      property never ages, so `purchasedRound == -1` is skipped.

```
Property
Maharagama
has depreciated by 5%.
Current Value
LKR 4,750.
```

- [ ] **Step 12:** Apply `depreciationPct` inside `square_value` — which is why `Current Value` above is simply
      `square_value(g, sq)`.
- [ ] **Step 13:** Placeholder `decide_renovate` — renovate when `depreciationPct > 10` and affordable. Cost is
      `RENOVATE_PCT` of current market value; clears `depreciationPct` and resets age by setting
      `purchasedRound = g->round` (LK 17). Wire it into `land_on` for the case where the landing player already owns the
      square — LK 17 permits renovation only there.

### 5.4 Structural damage

- [ ] **Step 14:** In `condition_tick`, when `unmaintainedRounds > UNMAINTAINED_LIMIT` on a square with buildings, set
      `structDamaged = true` **once**.
- [ ] **Step 15:** Apply LK 28's three consequences at their choke points: value −15% inside `square_value`; maximum
      rent −25% inside `square_rent`; maintenance +50% inside the maintenance cost calculation.
- [ ] **Step 16:** Extend `decide_renovate` for LK 29 — renovating a structurally damaged building costs 25% of
      replacement value and clears `structDamaged`, restoring value, rent and condition together.
- [ ] **Step 17:** Print `Structural damage has occurred at %s.` — §5 gives no template, so match the depreciation
      block's voice.

**Verify:**
```
./monopoly 42 | grep -c "Insurance purchased"           # > 0
./monopoly 42 | grep -c "expires in 3 rounds"           # close to the purchase count
./monopoly 42 | grep -c "occurred"                      # close to 50, fewer if nothing is developed early
./monopoly 42 | grep -A4 occurred                       # both paths appear: some with a claim, some without
./monopoly 42 | grep -n "has depreciated" | head -1     # never before round 51
```
A premium must equal exactly 5/10/15% of the property's current value. No property may exceed 30% depreciation. After a
renovation, `square_value` must return to its undepreciated level. A structurally damaged property's value must be
exactly 15% below its otherwise-computed value. **After a claim is paid, that property must show no policy** — confirm
by grepping for a second claim on the same property with no intervening purchase.

**Commit:** `feat: insurance, disasters, depreciation, structural damage`

---

# Milestone 6 — Personalities & conformance

**Goal:** The four §3 behaviours, then sign-off against the definition of done.

**Spec:** R4.1–R4.4, R5.1–R5.7, R0.\* · §3, §5 · D9
**Concept:** [02 §9 Isolating decisions behind an interface](../../reference/02-program-design.md)

### 6.1 The four strategies

Each `decide_*` becomes a `switch (g->players[p].strat)` with one arm per personality. **`players.c` only — no signature
changes, no other file touched.** Keep a comment checklist mapping each §3 bullet to the line implementing it; that
mapping is what a viva question will ask for.

- [ ] **Step 1 — Aggressive Investor (§3.1):** always buys if one future rent remains payable; prefers squares that
      complete a group; prioritises Galle Face (39) and Nuwara Eliya (37); always bids, up to 120% of `square_value`
      (**D9**); max houses immediately, hotels as soon as legal; borrows whenever the funds raise projected rent;
      repays only when cash exceeds twice the loan; Basic on houses, Comprehensive on hotels; never sells voluntarily.
- [ ] **Step 2 — Conservative Banker (§3.2):** buys only if ≥50% of cash remains afterwards; prefers railways and
      utilities; refuses purchases while a global `VALUE_MUL` is negative (a recession); bids strictly below
      `square_value`; borrows only when bankruptcy is otherwise unavoidable, repays in full at every Bank visit;
      Comprehensive on every developed property; **no hotels while any loan is outstanding**; renovates above 10%
      depreciation.
- [ ] **Step 3 — Risk Taker (§3.3):** buys every available property; prefers expensive groups; always borrows the
      maximum and refinances at every opportunity; bids until cash is exhausted; hotels as early as possible; insures
      **only** after `sufferedLoss` is set; ignores depreciation until repair is unavoidable; sells lower-value
      properties to fund premium developments.
- [ ] **Step 4 — Opportunistic Trader (§3.4):** buys only when projected appreciation exceeds construction cost, where
      projected appreciation is `square_value × (sum of active positive modifiers − active negative) / 100` read
      straight from `effect_modifier` (**D9**); prefers auctions to direct purchase; borrows only when projected return
      beats `interestRatePct`; delays construction while inflation is positive, accelerates while `BUILD_COST_MUL` is
      negative; Comprehensive only on high-value developments; renovates above 15% depreciation; sells squares carrying
      a negative `VALUE_MUL`; maintains a balanced portfolio.

**Verify:** run three seeds and compare the final round summary. Aggressive should lead on Hotels, Conservative on Cash,
Risk Taker on Properties or bankrupt, Opportunistic in between. **Four similar-looking players means the `switch` arms
are not being reached.** The Risk Taker must reach default and bankruptcy across several seeds — it is the player that
exercises milestone 3's failure paths; if it never defaults in five seeds, `decide_bank` is not borrowing the maximum.

**Commit:** `feat: the four player strategies`

### 6.2 Output audit

- [ ] **Step 5:** Extract the §5 templates for side-by-side comparison:
      `pdftotext -layout assets/Assignment_1_unlocked.pdf - > spec.txt`, then read from
      `5 Required Output Messages` to `A National Event Cards`.
- [ ] **Step 6:** Capture a full run: `./monopoly 42 > run.txt`.
- [ ] **Step 7:** Walk all 23 templates: pre-game, roll-off, dice roll, movement, passing GO, purchase, rent, house
      construction, hotel construction, loan obtained, loan repaid, loan default, insurance purchase, insurance expiry,
      disaster, auction, economic event, government regulation, depreciation, bankruptcy, round summary, GAME OVER,
      market conditions.
- [ ] **Step 8:** Check the details that are easy to get wrong and are graded: spaces around colons
      (`Cash : LKR 12,300`, never `Cash: LKR 12,300`); trailing full stops present on some lines and absent on others;
      which values sit on their own line; rule-line lengths (45 for the round summary, 41 for market conditions);
      blank lines only where **D26** says.
- [ ] **Step 9:** Confirm no money prints without separators: `grep -nE 'LKR [0-9]{4,}' run.txt` must return nothing.
- [ ] **Step 10:** Fix every mismatch found.

### 6.3 Final validation

- [ ] **Step 11:** Both builds silent: `gcc *.c -o monopoly` and `gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly`.
- [ ] **Step 12:** Five seeds to completion — `for s in 1 7 42 99 12345; do ./monopoly $s > run-$s.txt || echo "FAILED
      $s"; done`. None may crash, hang, or await input.
- [ ] **Step 13:** Confirm both endings occur across the seed set — at least one 500-round game and at least one early
      win by bankruptcy. Force the second with a low `START_CASH` if no seed produces it, then restore.
- [ ] **Step 14:** Determinism: `./monopoly 42 > a.txt && ./monopoly 42 > b.txt && diff a.txt b.txt` — no output.
- [ ] **Step 15:** No interaction: `./monopoly 42 < /dev/null` completes normally.
- [ ] **Step 16:** `make debug` across all five seeds — no invariant assertion fires.
- [ ] **Step 17:** Constraint sweep — `grep -rn "malloc\|calloc\|realloc\|math\.h" *.c *.h` returns nothing;
      `grep -n "double" *.c` returns only the three helpers in `finance.c`; no file-scope variable outside `const`
      tables.
- [ ] **Step 18:** Tick every checkbox in `docs/REQUIREMENTS.md` that now passes. Update `README.md` with build and run
      instructions.

**Commit:** `chore: output conformance and final validation`

---

## Self-review notes

**Spec coverage.** R0 → M1, M6 · R1 → M1, M2, M3, M5 · R2 → M1–M3 · R3.1–R3.5 → M3 · R3.6–R3.9 → M5 · R3.10 → M4 ·
R3.11–R3.12 → M5 · R3.13 → M4 · R3.14–R3.16 → M2 · R3.17 → M4 · R3.18–R3.19 → M3 · R3.20 → M5 · R3.21–R3.24 → M4 ·
R4 → M6 · R5 → every milestone, audited in M6. No orphan requirements.

**Deliberate incrementalism.** The `decide_*` placeholders in M2–M5 are throwaway by design; M6 replaces bodies only,
never signatures, so no other file is touched. `net_worth` is built in three passes (M1, M2, M3) without changing its
signature. `square_value`, `square_rent`, `building_cost` and `mortgage_value` are extended in place across M2–M5 rather
than duplicated. `effect_modifier` ships as a stub in M2 so the choke points are written once, in final shape.

**Type consistency.** Every cross-milestone name comes from a step's stated interface, and `types.h` is the single
prototype home — so drift between what one milestone produces and another consumes fails the build rather than
silently diverging.

**Corrections to earlier drafts.** Table 4 lists **12** regional development cards, not 13. Square 2 is the Community
Development Fund, not a card square, so there are **3** card squares, not 4. Base rent is per-property from `Rent.csv`,
not 10% of a group price. Income Tax is a percentage of assets, not a flat LKR 1,000. Interest and percentage
arithmetic uses `double` rounded to `int`, not integer truncation.

---

## Execution handoff

Two execution options:

1. **Subagent-Driven (recommended)** — a fresh subagent per milestone section, reviewed between sections.
2. **Inline execution** — milestones executed in this session with checkpoints for review.
