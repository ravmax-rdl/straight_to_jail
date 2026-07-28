---
title: "C for Straight to Jail"
subtitle: "The language features this simulation actually uses"
author: "Straight to Jail — SCS 1301"
date: "2026-07-28"
lang: en
toc: true
numbersections: true
---

# Scope

This document covers the C that *Straight to Jail* uses, and nothing else. Every example is
either code from the project or a mistake the project invites. It assumes you have written some
C before but have not built a seven-file program with it.

The spec's constraints are unusually specific, and three of them shape almost every decision:

- `gcc *.c -o monopoly` must compile with **no warnings**.
- **No global variables.**
- **No floating point in the money path.**

Together these push you toward a particular style: one big state struct passed by pointer, fixed
arrays instead of allocation, and integer arithmetic with an explicit rounding rule.

---

# Multi-file compilation

## What `gcc *.c -o monopoly` actually does

The shell expands `*.c` to `board.c events.c finance.c game.c main.c players.c`. GCC then
performs four steps on each:

```
board.c --[preprocess]--> board.i --[compile]--> board.s --[assemble]--> board.o
                                                                          |
main.c, game.c, ... --> ... --> main.o, game.o, ...                       |
                                                                          v
                                                      [link] -----> monopoly
```

Each `.c` file becomes a **translation unit**, compiled in complete isolation. `board.c` has no
idea `finance.c` exists. It gets compiled first, or last, or in parallel — the order is not
defined and must not matter.

This is the single most important fact about C's build model, and almost every confusing
multi-file error follows from it.

## Declaration versus definition

A **declaration** says *this thing exists somewhere, here is its shape*. A **definition** says
*here is the thing*.

```c
int square_value(const GameState *g, int sq);        /* declaration — no body */

int square_value(const GameState *g, int sq)         /* definition — has a body */
{
    return g->board[sq].price;
}
```

When `game.c` calls `square_value`, the compiler needs the declaration to know the argument types
and return type. It does **not** need the definition — that is the linker's job, later.

If you call a function the compiler has never seen declared, C99 makes it an error. (C89 would
silently assume it returned `int`, which is exactly the sort of thing C99 removed.) So:

> Every function used outside its own `.c` file needs a declaration visible to the caller.

That is what `types.h` is for.

## Header files and the one-definition rule

`types.h` holds declarations only — struct definitions, enums, constants, and function
prototypes. It holds **no function bodies and no variable definitions**.

The reason is the linker. If `types.h` contained

```c
int helper(void) { return 42; }        /* WRONG — a definition in a header */
```

then every `.c` file that includes it gets its own copy of `helper`, and the linker sees six
definitions of the same symbol:

```
multiple definition of `helper'
```

The rule: **headers declare, `.c` files define.**

## Header guards

`types.h` gets included by all seven files, and some of them may include each other indirectly.
Without protection the preprocessor would paste the struct definitions in twice and you would get
`redefinition of 'struct Square'`.

```c
#ifndef TYPES_H
#define TYPES_H
/* ... everything ... */
#endif /* TYPES_H */
```

The first time the preprocessor sees this file, `TYPES_H` is undefined, so it defines it and
processes the body. Every subsequent time, `TYPES_H` is already defined and the whole file is
skipped. This idiom is not optional in a multi-file project.

## `static` for file-local helpers

A function or variable at file scope is visible to the whole program by default. `static` at file
scope means *this name is private to this translation unit*.

```c
/* board.c */
static const GroupValues GROUP_VALUES[GRP_COUNT] = { ... };   /* board.c only */

int square_value(const GameState *g, int sq) { ... }          /* public */
```

Two reasons to use it in this project:

1. **It prevents name collisions.** `finance.c` could have its own `static` helper called
   `apply` and so could `events.c`, with no conflict.
2. **It documents intent.** A reader of `board.c` knows `GROUP_VALUES` is nobody else's business.

Rule of thumb: everything in a `.c` file is `static` unless its prototype is in `types.h`.

## Why the module split is not arbitrary

The spec's Table 5 mandates a split by responsibility, not by convenience:

| File | Responsibility |
|------|----------------|
| `types.h` | The shared vocabulary |
| `board.c` | Where things are and what they are worth |
| `players.c` | What the four personalities decide |
| `finance.c` | Where money goes |
| `events.c` | What the world does to you |
| `game.c` | What happens when |
| `main.c` | Starting up |

The test of a good split is: *when a rule changes, how many files do you touch?* If the rent
formula changes, only `board.c` should move. If the Risk Taker gets greedier, only `players.c`.
That property is worth protecting, and it is why `players.c` never touches `cash` directly — it
returns a decision and lets `finance.c` move the money.

---

# Enums

An enum defines a set of named integer constants and a type to hold them.

```c
typedef enum {
    SQ_GO, SQ_PROPERTY, SQ_RAILWAY, SQ_UTILITY, SQ_BANK,
    SQ_INSURANCE, SQ_TAX, SQ_EVENT, SQ_JAIL, SQ_PARKING, SQ_GOTOJAIL
} SquareType;
```

`SQ_GO` is 0, `SQ_PROPERTY` is 1, and so on. You could use raw integers, and the program would
run identically. You should not, for three reasons.

**Readability.** `if (sq.type == SQ_RAILWAY)` says what it means; `if (sq.type == 2)` does not.

**Warnings.** With `-Wall`, a `switch` on an enum that omits a case produces:

```
warning: enumeration value 'SQ_GOTOJAIL' not handled in switch
```

That warning is a feature. When you add a new square type in a later stage, the compiler tells
you every `switch` that needs updating. This is why `land_on` lists every case explicitly rather
than using `default:` — a `default` label suppresses the warning and hides the work.

**Type documentation.** A function taking `SquareType` cannot be accidentally passed a player
index.

## The count sentinel

A common trick, used here for property groups:

```c
typedef enum {
    GRP_NONE = -1,
    GRP_BROWN = 0, GRP_LIGHTBLUE, GRP_PINK, GRP_ORANGE,
    GRP_RED, GRP_YELLOW, GRP_GREEN, GRP_DARKBLUE,
    GRP_COUNT
} PropertyGroup;
```

`GRP_COUNT` is 8 — one past the last real group. It is not a group; it is the *number* of groups,
and it stays correct automatically if a group is ever added.

```c
static const GroupValues GROUP_VALUES[GRP_COUNT] = { ... };   /* exactly 8 entries */
int cooldown[GRP_COUNT];                                       /* one slot per group */

for (int g = 0; g < GRP_COUNT; g++) { ... }                    /* iterate all groups */
```

Note why `GRP_NONE` is `-1` rather than `0`. If `GRP_NONE` were 0, every real group would shift up
by one and `GROUP_VALUES[square.group]` would need a `- 1` correction at every use — an off-by-one
waiting to happen. Making the invalid value negative keeps the valid ones as clean array indices.

## Enums as bit flags

Region tags need a different shape, because a square can belong to several regions at once.
Trincomalee is northern, eastern, *and* coastal. An enum cannot hold three values; a bitmask can.

```c
#define REGION_WESTERN          0x01u   /* 0000 0001 */
#define REGION_CENTRAL          0x02u   /* 0000 0010 */
#define REGION_SOUTHERN_COASTAL 0x04u   /* 0000 0100 */
#define REGION_NORTHERN         0x08u   /* 0000 1000 */
#define REGION_EASTERN          0x10u   /* 0001 0000 */
#define REGION_COMMERCIAL       0x20u   /* 0010 0000 */
```

Each value occupies one bit. Combine with `|`, test with `&`:

```c
sq->regions = REGION_NORTHERN | REGION_EASTERN | REGION_COASTAL;

if (sq->regions & REGION_COASTAL) { /* affected by a coastal event */ }
```

The `u` suffix marks the constants as unsigned, matching the `unsigned regions` field and keeping
`-Wsign-conversion` quiet.

---

# Structs

A struct groups related values into one type.

```c
typedef struct { bool active; int principal, ratePct, roundsLeft; } Loan;
```

`typedef struct { ... } Loan;` lets you write `Loan x;` rather than `struct Loan x;`. Both forms
are correct C; the typedef form is used throughout this project for brevity.

## Access

Dot for a value, arrow for a pointer:

```c
Player  p  = g->players[0];
Player *pp = &g->players[0];

p.cash  = 100;      /* value:   dot   */
pp->cash = 100;     /* pointer: arrow */
```

`pp->cash` is exactly `(*pp).cash`. The arrow exists because that expression is unbearable to
type.

## Copy semantics — the trap

Structs in C are **value types**. Assignment copies the whole thing, field by field.

```c
Player p = g->players[0];   /* a COPY */
p.cash += 5000;             /* modifies the copy */
                            /* g->players[0].cash is unchanged */
```

This bites in loops. Compare:

```c
for (int i = 0; i < NUM_PLAYERS; i++) {
    Player pl = g->players[i];       /* WRONG — a copy */
    pl.cash += GO_SALARY;            /* discarded when the loop iterates */
}

for (int i = 0; i < NUM_PLAYERS; i++) {
    Player *pl = &g->players[i];     /* right — a pointer to the real thing */
    pl->cash += GO_SALARY;
}
```

The first loop compiles without a single warning and does nothing. If a value in this project
mysteriously fails to change, this is the first thing to check.

## Designated initialisers

C99 lets you initialise by field name, which survives someone reordering the struct:

```c
g->board[26] = (Square){
    .type = SQ_PROPERTY,
    .name = "Galle Fort",
    .group = GRP_YELLOW,
    .regions = REGION_SOUTHERN_COASTAL | REGION_COASTAL,
    .owner = -1,
    .conditionPct = 100
};
```

Every field you do not name is zero-initialised. That is a real guarantee, not a convention, and
it is why the struct above does not need `.houses = 0, .hotel = false, .mortgaged = false, ...`.

Note `.owner = -1` and `.conditionPct = 100` **are** named, precisely because their correct
initial values are not zero. Getting `owner` wrong here would make the Bank appear to be Player 0
and every property would start owned.

---

# Arrays of structs

```c
Square board[NUM_SQUARES];      /* 40 squares */
Player players[NUM_PLAYERS];    /*  4 players */
```

Contiguous memory, indexed from 0, no allocation.

## Sizing

`NUM_SQUARES` is a `#define`, not a variable, so the array size is fixed at compile time. C99 does
permit variable-length arrays, but this project never needs one — every collection has a size the
spec fixes: 40 squares, 4 players, 8 groups, 20 cards.

If you ever need the length of an array whose size you did not write down:

```c
size_t n = sizeof arr / sizeof arr[0];
```

This works **only** where `arr` is a real array. Pass it to a function and it decays to a pointer,
`sizeof arr` becomes the pointer size, and the expression silently yields 1 or 2. Never compute a
length inside a function that received the array as a parameter — pass the length separately.

## Why no `malloc`

The spec says dynamic allocation "should be used only where justified". Nothing here is justified.
Every collection has a known fixed size, so:

```c
GameState g;        /* on main's stack, ~6 KB */
game_init(&g);
game_run(&g);
```

What you get for free by not allocating:

- No leaks, because nothing is allocated.
- No null checks, because nothing can fail to allocate.
- No use-after-free, no double-free, no dangling pointers.
- Automatic cleanup when `main` returns.

`malloc` earns its keep when a size is genuinely unknown until runtime. Reaching for it when the
size is written in the spec adds failure modes and removes nothing.

---

# Pointers and passing state

## Why `GameState *` and not `GameState`

```c
void play_turn(GameState g, int p);      /* WRONG on both counts */
void play_turn(GameState *g, int p);     /* right */
```

The first version fails for two independent reasons:

**Mutations are lost.** `g` is a copy. Anything `play_turn` changes vanishes on return.

**It is enormous.** `GameState` is roughly 6 KB. Copying it on every call, for a function called
2,000 times a game, is thousands of pointless memory copies. The pointer is 8 bytes.

## The pattern

Every function that touches game state takes `GameState *g` as its first parameter, and it is the
first parameter every time. This is what replaces globals: instead of the state being ambiently
available, it is passed explicitly, so a function's signature tells you exactly what it can reach.

```c
void  credit      (GameState *g, int p, int amt);
bool  charge      (GameState *g, int p, int amt, int toPlayer);
int   square_value(const GameState *g, int sq);
```

## Why globals are banned

The spec says to avoid them, and it is right. With a global `GameState`:

- Any function can modify any state, so tracking down where a value changed means reading the
  whole program.
- Function signatures stop telling you anything.
- Two games cannot run in one process.

The cost of passing the pointer is one extra parameter. The benefit is that
`int square_value(const GameState *g, int sq)` is a complete and honest description of what that
function can see and do.

---

# `const`-correctness

`const GameState *g` means *this function will not modify the state*. The compiler enforces it.

```c
int square_value(const GameState *g, int sq)
{
    g->board[sq].price = 500;    /* error: assignment of member in read-only object */
    return g->board[sq].price;
}
```

Use it on every query function — `square_value`, `square_rent`, `net_worth`, `count_owned`,
`group_monopoly`, `round_summary`, `market_conditions`. Omit it on anything that mutates.

The payoff is that a reader can tell, from the signature alone, whether calling a function can
change the game. In a program where a dozen systems all modify the same shared state, that is
worth having.

Note the placement:

```c
const GameState *g;    /* pointer to const state — the state cannot change  */
GameState *const g;    /* const pointer — the pointer cannot be reassigned  */
```

You want the first. The second is almost never useful.

---

# Strings and `snprintf`

## Names are `const char *`

```c
const char *name;      /* in Square and Player */
```

Names come from string literals in `board_init` (`"Galle Fort"`) and `game_init`
(`"Risk Taker"`). String literals live in read-only memory for the whole program, so storing a
pointer is safe, needs no copying, and needs no buffer. The `const` is not decorative — writing
through a pointer to a literal is undefined behaviour, and the `const` makes the compiler stop you.

## `fmt_lkr` and buffer discipline

Every monetary figure prints with thousands separators — `LKR 12,300`. C has no built-in way to do
this portably, so the project writes one:

```c
const char *fmt_lkr(char *buf, int amount);
```

The caller supplies the buffer. That is a deliberate choice: the alternative — a `static char`
buffer inside the function — would break the moment you printed two amounts in one statement,
because the second call would overwrite the first before `printf` ran.

```c
char b1[20], b2[20];
printf("Rent %s of %s\n", fmt_lkr(b1, rent), fmt_lkr(b2, value));   /* two buffers */
```

`20` bytes is comfortable: the widest possible `int` is `-2147483648`, which with separators is
`-2,147,483,648` — 14 characters plus a terminator.

## `snprintf`, never `sprintf`

Where you do build strings, use the bounded form:

```c
char buf[64];
snprintf(buf, sizeof buf, "%s owns %d properties", name, count);
```

`sprintf` has no idea how big `buf` is and will write past the end. `snprintf` takes the size and
truncates instead. There is no situation in this project where `sprintf` is the right call.

## The `printf` specifiers this project needs

| Specifier | Use |
|-----------|-----|
| `%d` | round numbers, dice, square indices, counts, percentages |
| `%s` | names, and every `fmt_lkr` result |
| `%%` | a literal `%` — needed for `Interest Rate : 8%` |

That is the whole list. Money never uses `%d` directly, which makes `grep -P 'LKR \d{4,}'`
a complete audit for missed separators.

---

# Randomness

## Seeding

```c
srand(argc > 1 ? (unsigned)atoi(argv[1]) : (unsigned)time(NULL));
```

`srand` is called **once**, in `main`. Calling it again mid-run resets the sequence and destroys
reproducibility — a surprisingly common bug when someone adds a "reshuffle" function.

Taking the seed from `argv[1]` is what makes `./monopoly 42` reproducible, which is the backbone
of this project's entire verification approach. Without a test framework, "run it twice and diff"
is the main tool available, and it only works if the run is deterministic.

## Modulo bias

The obvious way to get a die roll is wrong:

```c
int roll_die(void) { return rand() % 6 + 1; }    /* biased */
```

`rand()` returns a value in `[0, RAND_MAX]`. If `RAND_MAX` is 32767, then `32768 % 6 == 2`, so the
values 0 and 1 each occur 5462 times across the range while 2 through 5 occur 5461 times. Faces 1
and 2 come up about 0.02% more often than the rest.

For a board game that difference is invisible. But the fix is four lines, and doing it correctly
once means you never have to reason about whether it matters:

```c
int rng_range(int lo, int hi)
{
    int span  = hi - lo + 1;
    int limit = RAND_MAX - (RAND_MAX % span);   /* largest exact multiple of span */
    int r;
    do { r = rand(); } while (r >= limit);      /* reject the ragged tail */
    return lo + (r % span);
}
```

The loop discards the few values in the incomplete final block. It terminates with probability 1
and, for a six-sided die, almost always on the first iteration.

## Where randomness enters

Six places, all through `rng_range`:

1. Dice, every turn.
2. The turn-order roll-off, once.
3. Disaster selection, every 10 rounds.
4. Inflation draw, every 10 rounds.
5. Market group selection, every 10 rounds.
6. Event, regional card, and regulation selection.

Plus the Fisher-Yates shuffle of the event deck at startup. Routing all of them through one
function means the bias fix applies everywhere and the seed governs everything.

---

# Compiler warnings

The build must be silent under:

```bash
gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly
```

These are the warnings this project will actually trip.

## `unused parameter`

```c
bool decide_buy(GameState *g, int p, int sq)
{
    return true;                    /* warning: unused parameter 'g' */
}
```

Common with the placeholder strategy functions. Two honest fixes — use the parameter, or mark it
deliberately unused:

```c
(void)g;    /* explicitly ignored */
```

Do not delete the parameter. The signature is fixed by the plan and the real implementation
arrives later.

## `comparison of integer expressions of different signedness`

```c
for (int i = 0; i < sizeof arr / sizeof arr[0]; i++)   /* int vs size_t */
```

`sizeof` yields `size_t`, which is unsigned. Compare with a `size_t` loop variable, or cast the
bound to `int`.

## `missing initializer for field`

Fires when you partially initialise a struct positionally:

```c
Square s = { SQ_PROPERTY, "Pettah" };   /* warning under -Wextra */
```

Designated initialisers avoid it entirely, which is one more reason to use them.

## `enumeration value not handled in switch`

Covered above. This one is a gift — never silence it with `default:`.

## `-pedantic`

Rejects GCC extensions, so the code stays portable C99. Worth keeping on: the grader's compiler
may not be yours.

---

# The bugs this project invites

Five failure modes that this specific program makes easy.

## 1. Integer overflow from compounding

The single most dangerous arithmetic in the program.

`int` on a typical platform holds up to 2,147,483,647. Decision D4 compounds loan interest **every
round** at the tabled rate. At 15%:

$$P_{20} = P_0 \times 1.15^{20} \approx 16.4\,P_0$$

An LKR 50,000 loan left unpaid for 20 rounds becomes LKR 820,000. That is fine. But the naive
percentage helper is not:

```c
int pct(int value, int percent) { return value * (100 + percent) / 100; }
```

`value * 115` overflows `int` once `value` exceeds about 18.6 million. Overflow of a *signed*
integer is undefined behaviour in C — not "wraps around", but genuinely undefined, and GCC's
optimiser is entitled to assume it never happens.

The fix is to widen before multiplying:

```c
int pct(int value, int percent) { return (int)((long)value * (100 + percent) / 100); }
```

`long` is at least 32 bits by the standard and 64 on most modern platforms; the cast happens
*before* the multiply, so the product has room. The economic-math document works through the
headroom numbers in detail.

## 2. Off-by-one in modular wrapping

The board wraps at 40. Getting this wrong is easy and the symptom is subtle:

```c
pl->pos = (pl->pos + steps) % NUM_SQUARES;
```

The classic mistake is `% (NUM_SQUARES - 1)`, giving a 39-square board where square 39 is never
landed on. Nothing crashes. The output looks entirely plausible. You find it three stages later
when Galle Face is somehow never bought.

Detecting the GO crossing has its own subtlety:

```c
if (to < from || to == 0)
```

`to < from` catches the wrap. `to == 0` catches landing exactly on GO, which the first test misses
because 0 is not less than 0 when `from` is also 0 — and a player starting on GO who rolls 40
would be missed. Both conditions are needed.

## 3. Uninitialised struct fields

```c
GameState g;              /* every field is garbage */
game_run(&g);             /* undefined behaviour */
```

Local variables in C are not zeroed. `GameState g;` on `main`'s stack contains whatever was there
before. `game_init` must set every field that matters — and specifically must set `owner = -1` on
all 40 squares, because zero means "owned by Player 0".

The safe idiom:

```c
GameState g = {0};        /* all fields zeroed */
game_init(&g);            /* then set the non-zero defaults */
```

## 4. Truncation accumulating in the wrong direction

Integer division truncates toward zero, so `pct_of(999, 50)` is 499, not 500. One rupee vanishes.
That is fine and intended — but only if the convention is applied *consistently*. If one call site
rounds and another truncates, two paths that should agree will drift apart by a few rupees and
then by a few hundred, and reconciling the round summary becomes impossible.

Route every percentage through `pct` and `pct_of`. Never write `x * 115 / 100` inline.

## 5. Copying a struct when you meant to reference one

Covered under struct copy semantics above. It is listed again here because it is the bug most
likely to cost you an hour: the code is correct-looking, warning-free, and does nothing.

---

# Checklist

Before committing any stage:

- [ ] `gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly` prints nothing
- [ ] No function body or variable definition in `types.h`
- [ ] Every new cross-file function has a prototype in `types.h`
- [ ] Every file-local helper is `static`
- [ ] Every query function takes `const GameState *`
- [ ] Every struct loop uses a pointer, not a copy
- [ ] Every percentage goes through `pct` or `pct_of`
- [ ] Every money print goes through `fmt_lkr`
- [ ] `srand` is called exactly once
- [ ] `./monopoly 42` twice produces identical output
