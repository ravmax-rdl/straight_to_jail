---
title: "The Economic Mathematics"
subtitle: "Integer arithmetic, compounding, decay, and expected value in Straight to Jail"
author: "Straight to Jail — SCS 1301"
date: "2026-07-28"
lang: en
toc: true
numbersections: true
---

# Why this matters

The specification forbids floating point in the money path and requires percentage effects to
stack. Those two requirements interact badly if you are careless: integer arithmetic truncates,
truncation errors accumulate, and a rounding convention chosen differently in two places produces
two answers to the same question.

This document works out the arithmetic once, so the rest of the program can just apply it.

---

# Why integers

Spec §4: *"All monetary calculations shall be performed using integer values."*

The requirement is not arbitrary. Money is a **discrete** quantity — there is no such thing as
LKR 4,500.7 — and floating point represents discrete decimal quantities badly.

The standard demonstration:

```c
printf("%.20f\n", 0.1 + 0.2);   /* 0.30000000000000004441 */
```

`0.1` has no exact representation in binary floating point, any more than $1/3$ has an exact
decimal representation. Every arithmetic operation compounds the error slightly. Over a 500-round
simulation with tens of thousands of monetary operations, two players who should have identical
balances can end up differing by fractions of a rupee — and then the equality test that decides
who wins the net-worth tiebreak gives the wrong answer.

With `int`, LKR 4,500 is exactly 4,500, forever. Addition and subtraction are exact. The only
inexactness is division, and that is under your control.

## What you give up

Division truncates. `pct_of(999, 50)` is 499, not 499.5 and not 500. One rupee disappears.

This is acceptable — the spec chose it — but it means **the rounding convention must be a
decision, applied identically everywhere.** Which brings us to the next section.

---

# The truncation convention

## The rule (decision D6)

Every percentage calculation goes through one of two helpers:

```c
int pct   (int value, int percent) { return (int)((long)value * (100 + percent) / 100); }
int pct_of(int value, int percent) { return (int)((long)value * percent       / 100); }
```

`pct(v, p)` applies a percentage *change*: `pct(1000, +20)` is 1200, `pct(1000, -15)` is 850.
`pct_of(v, p)` takes a percentage *of* a value: `pct_of(1000, 20)` is 200.

C's integer division truncates toward zero, so both round **down** in magnitude.

## Multiply before dividing

The order matters and is not negotiable:

```c
value * percent / 100      /* right — one truncation, at the end   */
value / 100 * percent      /* WRONG — truncates first, then scales */
```

With `value = 4500, percent = 5`:

- Correct: $4500 \times 5 = 22500$, then $22500 / 100 = 225$.
- Wrong: $4500 / 100 = 45$, then $45 \times 5 = 225$. Same answer, this time.

With `value = 4550, percent = 5`:

- Correct: $4550 \times 5 = 22750$, then $/100 = 227$.
- Wrong: $4550 / 100 = 45$ (losing 50), then $\times 5 = 225$.

The second form threw away the remainder *before* scaling it, so the error was multiplied by 5.
The general rule: **in integer arithmetic, divide last.**

## Where the error goes

Each truncation loses less than 1 rupee, and always in the same direction: values drift very
slightly **downward** relative to exact arithmetic.

Bounding it: over a 500-round game, a given property's value passes through perhaps 50 inflation
recalculations. Worst case, each loses 1 rupee — a cumulative drift under 50 rupees on a value in
the thousands, well under 1%.

That is fine. What is *not* fine is inconsistency. If `square_value` truncates and one caller
rounds, then two code paths that should produce the same number produce different ones, and the
round summary stops reconciling with the transactions that produced it. The absolute error does
not matter here; the *disagreement* does.

Hence: never write `x * 115 / 100` inline. Always `pct(x, 15)`.

---

# Composing percentages

Rule-LK 34: *"If multiple economic events affect the same property group simultaneously, all
percentage changes are cumulative."*

A boom (+25% rent) and a regional card (+40% rent) hit the same square. What is the rent?

## Two readings

**Multiplicative** — apply each in turn:

$$R = R_0 \times 1.25 \times 1.40 = 1.75\,R_0$$

**Additive** — sum the percentages, apply once:

$$R = R_0 \times (1 + 0.25 + 0.40) = 1.65\,R_0$$

They differ, because $(1+a)(1+b) = 1 + a + b + ab$, and that $ab$ cross-term is 10% here.

## Why this project uses additive

Three reasons, in increasing order of importance.

**It matches the wording.** "All percentage changes are cumulative" reads more naturally as
"add up the percentages" than as "compound them".

**It is order-independent.** Addition is commutative, so it does not matter which effect was
pushed onto the registry first. Multiplicative composition is *also* commutative in exact
arithmetic — but not in **truncating integer** arithmetic:

$$\text{trunc}(\text{trunc}(1000 \times 1.25) \times 1.40) = \text{trunc}(1250 \times 1.40) = 1750$$
$$\text{trunc}(\text{trunc}(1000 \times 1.40) \times 1.25) = \text{trunc}(1400 \times 1.25) = 1750$$

These agree, but only by luck of the numbers. With $R_0 = 999$ they do not. Applying $N$ effects
multiplicatively means $N$ truncations whose combined error depends on the order the effects
happened to be pushed. Applying them additively means **one** truncation, always.

**Expiry round-trips exactly.** This is the decisive argument. Consider a +20% effect that starts
and later expires.

Multiplicative, mutating storage:

$$1000 \xrightarrow{\times 1.20} 1200 \xrightarrow{\times 0.80} 960$$

The value is permanently 4% below where it started, because $1.20 \times 0.80 = 0.96 \ne 1$.
Fifty market reviews later, prices have collapsed for no reason any rule describes.

Additive, in the registry:

$$\text{value} = \text{stored} \times (1 + \Sigma m_i)$$

Removing effect $i$ removes $m_i$ from the sum. The result is *exactly* what the remaining
effects say — which is precisely Rule-LK 35's "return to their normal market-adjusted values
unless another active event is still influencing them", with no correction arithmetic at all.

That is not a coincidence. The registry design and additive composition are the same decision
viewed from two angles: **the value is always recomputed from scratch, never adjusted
incrementally.** Incremental adjustment is what does not round-trip.

## In code

```c
int total = effect_modifier(g, EFF_RENT_MUL, sq, -1);   /* sums the magnitudes */
rent = pct(rent, total);                                 /* applies once        */
```

Not:

```c
for each effect: rent = pct(rent, e->magnitudePct);      /* WRONG — compounds   */
```

## A caution

Additive composition can drive a value negative. Three simultaneous −40% effects sum to −120%,
and `pct(1000, -120)` is $-200$. Clamp at the choke point:

```c
if (v < 0) v = 0;
```

Rent and value are never negative in this game. A negative rent would pay the visitor, which no
rule permits.

---

# Overflow headroom

## The limit

`int` is guaranteed at least 16 bits by the standard, and is 32 bits on every platform this will
run on:

$$\texttt{INT\_MAX} = 2{,}147{,}483{,}647 \approx 2.1 \times 10^9$$

Signed integer overflow in C is **undefined behaviour** — not wraparound. The optimiser is
entitled to assume it cannot happen, and programs that overflow can behave in ways that make no
sense at all.

## Where this program gets close

The danger is not the stored values; it is the *intermediate products* inside `pct`.

```c
value * (100 + percent)
```

overflows when

$$\text{value} > \frac{\texttt{INT\_MAX}}{100 + p}$$

| Modifier $p$ | Multiplier | Safe up to |
|---|---|---|
| 0% | 100 | 21,474,836 |
| +15% | 115 | 18,673,770 |
| +100% | 200 | 10,737,418 |
| +200% (stacked) | 300 | 7,158,278 |

So: **can any value in this game exceed 7 million?**

## Working it out

**Property values.** Dark Blue starts at 10,000. Inflation fires 50 times in a 500-round game,
drawn from $\{-3, 0, 2, 5, 8, 12\}$ with mean $+4\%$. Expected:

$$10{,}000 \times 1.04^{50} \approx 10{,}000 \times 7.11 = 71{,}000$$

Comfortable. But the worst case — every draw the maximum 12% — is:

$$10{,}000 \times 1.12^{50} \approx 10{,}000 \times 289 = 2{,}890{,}000$$

Vanishingly unlikely ($6^{-50}$), but with a +200% stacked modifier the product is
$2{,}890{,}000 \times 300 = 867$ million. Still inside `int` — but the margin is now a factor of
2.5, not a factor of 300.

**Loan principals.** This is where it actually breaks. Total mortgage value across all 22
properties is LKR 57,250; adding railways and utilities brings the collateral base to roughly
70,000, so `max_loan` is about 52,500 at game start. Under sustained inflation that scales with
everything else — say 15,000,000 in the extreme case above. Then decision D4 compounds it at 15%
for 20 rounds:

$$15{,}000{,}000 \times 1.15^{20} \approx 15{,}000{,}000 \times 16.37 = 245{,}000{,}000$$

And now `pct(245000000, 15)` computes $245{,}000{,}000 \times 115 = 28.2$ **billion**, which
overflows a 32-bit `int` by more than an order of magnitude.

## The fix

Widen before multiplying:

```c
int pct(int value, int percent) { return (int)((long)value * (100 + percent) / 100); }
```

The cast to `long` happens **before** the multiplication, so the product is computed in `long`
(64-bit on every modern platform, and at least 32-bit by the standard). The division brings it
back into range and the cast is safe.

Note where the cast goes:

```c
(long)value * (100 + percent)      /* right — promotes both operands  */
(long)(value * (100 + percent))    /* WRONG — overflows, THEN widens  */
```

The second casts a value that has already overflowed. It looks almost identical and does nothing.

## A guard worth having

```c
#ifdef DEBUG
    if (loan->principal > INT_MAX / 200) {
        fprintf(stderr, "R%d: loan principal near overflow: %d\n", g->round, loan->principal);
        abort();
    }
#endif
```

If it ever fires you have learned something real about the parameters, and you can cap principals
or widen the field. Silent overflow teaches you nothing.

---

# Compound interest

## The model

Rule-LK 4: *"At the end of every complete round"* interest is added to the loan. Decision D4 takes
this literally — the Table 9 rate applies **per round**, despite the table calling it annual.

$$P_{n+1} = P_n + P_n \times r = P_n(1 + r)$$

and therefore

$$P_n = P_0 (1 + r)^n$$

In code:

```c
loan->principal += pct_of(loan->principal, loan->ratePct);
```

Note `loan->ratePct`, not a fresh Table 9 reading. Rule-LK 13 freezes a loan's rate at issue, so
the table governs *new* loans only. Storing the rate on the loan makes this correct by
construction; looking the condition up again at accrual time would silently reprice every
outstanding loan the moment the economy moved, which is the single most commonly mis-implemented
rule in this specification.

The one exception is deliberate and is **D21**'s: Economic Recession and Stock Market Boom leave
the issued rate alone but scale what a live loan compounds at while they last, so the accrual
reads `apply_pct(loan->ratePct, EFF_INTEREST_MUL)` rather than `ratePct` bare. Every row of the
table above is now a rate a loan can actually be written at — before **D21** was revised, only
the 8% seed was, and the other four rows were unused.

## What the rates actually do

Over a full 20-round loan term (Table 9 rates, D4 per-round application):

| Economic condition | Rate | $(1+r)^{20}$ | LKR 10,000 becomes |
|---|---|---|---|
| Economic Boom | 5% | 2.65 | 26,533 |
| Stable Economy | 8% | 4.66 | 46,610 |
| Moderate Inflation | 10% | 6.73 | 67,275 |
| High Inflation | 12% | 9.65 | 96,463 |
| Economic Recession | 15% | 16.37 | 163,665 |

These are severe, and they are meant to be. A loan taken in a recession and left unpaid multiplies
sixteenfold before it falls due — which is what makes Rule-LK 6 foreclosure a live threat rather
than a formality, and what gives the Risk Taker's "always borrow the maximum" a real cost.

## Doubling time

How many rounds until a debt doubles? Solve $(1+r)^n = 2$:

$$n = \frac{\ln 2}{\ln(1 + r)}$$

| Rate | Exact | Rule of 72 |
|---|---|---|
| 5% | 14.2 rounds | 14.4 |
| 8% | 9.0 rounds | 9.0 |
| 10% | 7.3 rounds | 7.2 |
| 12% | 6.1 rounds | 6.0 |
| 15% | 5.0 rounds | 4.8 |

The **rule of 72** — divide 72 by the percentage rate — is the mental shortcut, and it is accurate
to within a few percent across this whole range. Useful when reading a transcript: a loan at 8%
should visibly double about every nine rounds. If it is doubling every four, something is applying
the rate twice.

## The ambiguity, stated plainly

Table 9 is headed "Annual Interest Rate". Rule-LK 4 compounds every round. There is no statement
anywhere in the spec of how many rounds make a year.

Decision D4 follows Rule-LK 4 literally, because it is the operative rule — the table supplies the
figure, the rule supplies the schedule. The alternative reading (divide by some rounds-per-year
constant) requires inventing a constant the spec never gives.

Document whichever you choose. The numbers differ by more than an order of magnitude, so a marker
reading your output needs to know which convention produced it.

---

# Decay: linear versus compounding

Two systems reduce values over time, and they use *different* mathematics. Confusing them is easy.

## Building condition — linear

Rule-LK 25: condition decreases by 2% at the end of every round, from a starting 100%.

This is **subtraction of percentage points**, not compounding:

$$C_n = 100 - 2n$$

Not $100 \times 0.98^n$. The difference is real — after 50 rounds, linear decay gives 0 while
compounding gives $100 \times 0.98^{50} = 36.4$.

Linear is correct here: the spec says the condition *rating* falls by 2, and a rating is a number
of points, not a multiplier.

```c
sq->conditionPct -= COND_DECAY_PCT;
if (sq->conditionPct < 0) sq->conditionPct = 0;
```

The floor at zero matters. Without it the value goes negative and the Table 3 band lookup still
returns 0, so nothing visibly breaks — but a condition of $-40$ printed in a debug dump is
confusing, and any later code that treats it as a percentage will misbehave.

### What the bands mean in rounds

Combining $C_n = 100 - 2n$ with Table 3:

| Rounds unmaintained | Condition | Rent collected |
|---|---|---|
| 0–5 | 100–90% | 100% |
| 6–12 | 88–76% | 90% |
| 13–25 | 74–50% | 75% |
| 26–37 | 48–26% | 50% |
| 38+ | ≤24% | **closed — no rent** |

So a building left entirely alone stops earning after 38 rounds. Structural damage (Rule-LK 28)
arrives earlier, at 20 rounds unmaintained, while the building is still in the 75% band.

That timing is worth noticing when writing strategies: a player who maintains every 12 rounds
never leaves the 90% band and never risks structural damage, and that interval is a reasonable
default for the placeholder `decide_maintenance`.

## Property depreciation — accumulating percentage points, capped

Rule-LK 16: properties older than 50 rounds lose value at 1% per 5 rounds, capped at 30%.

Also linear, also in percentage points, but with a threshold and a ceiling:

$$D_n = \min\left(30,\ \left\lfloor \frac{n - 50}{5} \right\rfloor\right) \quad \text{for } n > 50$$

The cap is reached at $n = 50 + 150 = 200$ rounds. A property bought early and never renovated
sits at exactly 30% depreciation for the last 300 rounds of the game.

Applied at the choke point:

```c
v = pct(v, -s->depreciationPct);
```

Renovation (Rule-LK 17) costs 10% of current market value, clears `depreciationPct`, and resets
`age` to 0 — so the 50-round clock starts again.

### Is renovation worth it?

Renovation costs 10% of value to recover up to 30% of value. Purely on value it pays once
depreciation exceeds 10%, which is exactly the Conservative Banker's stated threshold (§3.2) — a
nice confirmation that the spec's strategy descriptions are internally consistent.

The Opportunistic Trader waits until 15% (§3.4), trading a little value for liquidity. The Risk
Taker ignores it entirely (§3.3) and pays for that in the endgame net-worth comparison.

## Why they cannot share code

| | Condition | Depreciation |
|---|---|---|
| Applies to | buildings | the property itself |
| Rate | 2 points per round | 1 point per 5 rounds |
| Starts | immediately | after 50 rounds |
| Floor / ceiling | 0% | 30% |
| Restored by | maintenance (LK 27) | renovation (LK 17) |
| Affects | rent, via Table 3 bands | value, directly |

Six differences. They are separate fields, separate ticks, and separate repair paths.

---

# Loan-to-value

## The rule

Rule-LK 2, with decision D5: the maximum loan is **75% of the total mortgage value** of all
eligible collateral — unmortgaged, not already loan-locked properties, railways, and utilities.
Buildings are never collateral (Rule-LK 1).

$$L_{\max} = \left\lfloor 0.75 \times \sum_{s \in \text{eligible}} M_s \right\rfloor$$

## Why lenders discount

The 75% figure is a **haircut**, and it exists in real lending for reasons that apply here too:

- **The collateral may fall in value.** A market decline cuts mortgage values by 10% (Rule-LK 32).
  Lend 100% and the bank is instantly under-secured.
- **Liquidation is imperfect.** Foreclosed assets go to auction (Rule-LK 19), opening at 50% of
  market value. The bank may not recover the full mortgage value.
- **Interest accrues on top.** The debt grows every round while the collateral does not.

The last point is the sharpest one in this game. At 8% per round, a loan at exactly 75% LTV
exceeds the value of its own collateral after just four rounds:

$$0.75 \times 1.08^4 = 1.02$$

At 15%, it takes two. **Every loan in this game is under-secured almost immediately**, which is
why Rule-LK 6 clears the debt entirely on foreclosure rather than pursuing the shortfall — the
bank writes it off because there is no realistic way to recover it.

## Note the base

Buildings are excluded. A Dark Blue property with a hotel might be worth 22,000 all-in, but only
its 5,000 mortgage value counts toward collateral. That is Rule-LK 1 and it substantially limits
borrowing for a heavily developed player — a genuine strategic constraint that the Aggressive
Investor runs into and the Conservative Banker never tests.

---

# Net worth as a balance sheet

## The identity

Rule 15:

$$\text{Net Worth} = \underbrace{C + P + B + R + U + I}_{\text{assets}} - \underbrace{(L + A + T)}_{\text{liabilities}}$$

| Term | Meaning | Source |
|---|---|---|
| $C$ | Cash | `players[p].cash` |
| $P$ | Property value | $\sum$ `square_value` over owned properties |
| $B$ | Building value | book value at construction cost |
| $R$ | Railway value | $\sum$ `square_value` over owned railways |
| $U$ | Utility value | $\sum$ `square_value` over owned utilities |
| $I$ | Insurance claims receivable | **always 0** (D15) |
| $L$ | Outstanding loans | `loan.principal` |
| $A$ | Accrued interest | already folded into $L$ |
| $T$ | Taxes due | always zero -- income tax is charged on landing, so nothing is ever outstanding (**D33**) |

This is the accounting identity $\text{Assets} - \text{Liabilities} = \text{Equity}$, and it is
what decides the winner if all 500 rounds elapse.

## Two subtleties

**Claims receivable is always zero.** Rule-LK 10 credits compensation immediately on a disaster,
so a claim never sits outstanding across a round boundary. Business Interruption's "lost hotel
rental income for 5 rounds" is likewise paid as an immediate lump sum under D3. The term stays in
the formula because Rule 15 lists it, and it is always 0.

**Accrued interest is not a separate term.** Rule-LK 4 adds interest *to the principal* each
round, so by the time you read `loan.principal` the accrued interest is already inside it. Adding
$A$ separately would double-count. The formula lists them separately because the spec describes
them as separate concepts; the implementation must not.

## Why buildings are at book value

`square_value` covers the land. Buildings are valued at what was paid to construct them, not at a
market-adjusted figure, because the spec never defines a market value for a building — only a
construction cost and a maintenance cost.

Consequence worth knowing: a building at 20% condition still counts at full book value in net
worth, even though it collects no rent at all. That is a defensible reading (the structure exists
and could be maintained back to full) and it should be documented, because it noticeably favours
the Risk Taker, who builds heavily and maintains nothing.

---

# The dice distribution

## Two dice are not one die

Rolling $2d6$ gives totals 2 through 12, but not uniformly. There are 36 equally likely
outcomes:

| Total | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Ways | 1 | 2 | 3 | 4 | 5 | 6 | 5 | 4 | 3 | 2 | 1 |
| $P$ | $\frac{1}{36}$ | $\frac{2}{36}$ | $\frac{3}{36}$ | $\frac{4}{36}$ | $\frac{5}{36}$ | $\frac{6}{36}$ | $\frac{5}{36}$ | $\frac{4}{36}$ | $\frac{3}{36}$ | $\frac{2}{36}$ | $\frac{1}{36}$ |

A triangular distribution peaking at 7. The mean:

$$E[2d6] = 2 \times 3.5 = 7$$

Seven is both the mean and the mode, and it is six times more likely than 2 or 12.

This is why `roll_dice` must roll **two separate dice and add them**, never `rng_range(2, 12)`.
The second would be uniform — 2 and 7 equally likely — and would change movement patterns, utility
rent, and landing frequencies throughout the game.

```c
int roll_dice(int *d1, int *d2) { *d1 = roll_die(); *d2 = roll_die(); return *d1 + *d2; }
```

Verifying it is a one-liner: tally 6,000 rolls and confirm the histogram is triangular, peaking
near 1,000 at total 7 and tapering to about 167 at 2 and 12. A flat histogram means somebody
reached for `rng_range(2, 12)`.

Keeping `*d1` and `*d2` separate is not decoration — Rule 13's doubles-to-leave-jail check needs
them individually.

## Consequences for the game

**Utility rent.** Rule Table 8 sets rent at $4 \times$ or $10 \times$ the dice total, so expected
rent is $4 \times 7 = 28$ or $10 \times 7 = 70$. Utilities are worth very little in this variant —
a Dark Blue hotel collects 10,000. That asymmetry justifies the Conservative Banker's stated
preference for "predictable income" (§3.2) being about *variance*, not magnitude.

**GO salary.** A player advances 7 squares per turn on average, so a lap of 40 takes about 5.7
turns. Over 500 turns:

$$\frac{500 \times 7}{40} = 87.5 \text{ laps}$$

At LKR 2,000 per lap, that is **LKR 175,000 per player** over a full game — nearly six times the
starting cash, and the single largest income source in the simulation. Any strategy analysis that
ignores GO is wrong.

It also gives a cheap correctness check. Across four players:

```bash
./monopoly 42 | grep -c "passed GO"     # expect roughly 350
```

Substantially fewer means the wrap detection is missing cases; substantially more means it is
double-counting.

**Landing frequency.** Squares are not hit equally often. Jail is an absorbing detour that pulls
players to square 10, so the squares 6 to 9 past it — 16 through 19, the Orange group — are hit
noticeably more than average. This is well known in ordinary Monopoly and holds here too. It is
not something the program needs to model; it is something to expect when reading a transcript and
wondering why the Orange group generated so much rent.

---

# English ascending auctions

## The mechanism

Rules-LK 19 through 23 describe a standard **English ascending auction**:

- Opening bid: 50% of market value (LK 19).
- Minimum increment: LKR 250 (LK 20).
- Withdrawal is **permanent** (LK 21).
- Bids capped by cash on hand; no borrowing mid-auction (LK 22).
- No bids at all: the Bank keeps it (LK 23).

## Why the opening is a discount

Opening at 50% guarantees the auction path can beat the direct-purchase path. A property declined
at list price can be won at half that if nobody else wants it — which is exactly what the
Opportunistic Trader's "prefers discounted auction purchases rather than direct purchases" (§3.4)
is exploiting.

It also means declining to buy is not always a mistake. The strategic content of Rule 5 is real:
decline, and you might get the same square for less — but you might also lose it to someone else.

## Why permanent withdrawal matters

In a normal ascending auction a bidder can re-enter. Rule-LK 21 forbids it, which changes the
optimal play: withdrawing is irreversible, so a bidder should stay in until the price genuinely
exceeds their valuation, not drop out tactically hoping to return.

It also guarantees termination. Each round of bidding either raises the price by at least 250 or
removes a bidder, and both are bounded — the price by the bidders' cash, the bidders by there
being four of them. The auction cannot loop forever. That is worth an explicit `#ifdef DEBUG`
iteration cap anyway, because a bug in the withdrawal bookkeeping would hang the whole simulation.

## Valuation as strategy

Each personality's bid cap is its valuation, and the spec states them directly:

| Player | Cap | Source |
|---|---|---|
| Aggressive Investor | 120% of market value | §3.1 |
| Conservative Banker | strictly below market value | §3.2 |
| Risk Taker | all available cash | §3.3 |
| Opportunistic Trader | below value, weighted by projected appreciation | §3.4 |

The Aggressive Investor systematically overpays by up to 20% and accepts it in exchange for
completing groups faster — a rational trade, since a completed group unlocks building, and a
hotel multiplies rent tenfold. The Risk Taker's cap is not a valuation at all, which is the point.

## How many rounds an auction runs

From an opening of 50% of value, reaching full value takes

$$\frac{0.5 \times V}{250}$$

increments. For a Dark Blue property at 10,000 that is 20 increments — about five bids each with
four bidders. Auctions in this game are long, and their transcripts are correspondingly verbose.
That is expected, not a bug.

---

# Premiums versus expected loss

Insurance is the one system where the spec's own strategy descriptions imply the numbers are
*not* favourable, and working that out is instructive.

## The parameters

| Policy | Premium | Perils covered (D3) | Payout |
|---|---|---|---|
| Basic | 5% of value | Fire, Flood | 80% of repair cost |
| Comprehensive | 10% of value | Fire, Flood, Riot, Vandalism | 100% |
| Business Interruption | 15% of value | all | 100% + 5 rounds of hotel rent |

Policies last 20 rounds (LK 9). Disasters fire every 10 rounds and hit **one** random developed
property (LK 10). Repair cost is 50% of the construction cost on the property (D1).

## A worked example

A Dark Blue property (value 10,000) with a hotel (construction cost 12,000), and suppose 10
developed properties exist across the board.

**Comprehensive premium:** $0.10 \times 10{,}000 = 1{,}000$ for 20 rounds.

**Expected payout over those 20 rounds:**

- Disasters in the window: $20 / 10 = 2$.
- Probability this property is chosen: $1/10$ each time.
- Expected hits: $2 \times 0.1 = 0.2$.
- Probability the peril is covered: Comprehensive covers 3 of the 5 in Rule-LK 10, so $0.6$.
- Repair cost: $0.5 \times 12{,}000 = 6{,}000$.

$$E[\text{payout}] = 0.2 \times 0.6 \times 6{,}000 = 720$$

**Premium 1,000 against an expected payout of 720.** The policy is a losing proposition in
expectation, by about 28%.

## This is not a bug

Real insurance is priced this way — the premium must exceed expected losses or the insurer fails.
Buying it is rational for reasons expectation alone does not capture:

- **Ruin avoidance.** A 6,000 repair bill on a player holding 2,000 cash forces the D11 debt-
  recovery ladder: sell buildings at half price, mortgage assets. The *realised* cost of an
  uninsured disaster is far above the repair figure.
- **Variance reduction.** The Conservative Banker's objective (§3.2) is stability, not expected
  value. Paying 280 in expectation to eliminate a tail risk is precisely what a risk-averse agent
  should do.

Which is why §3.2 says the Conservative Banker *always* buys Comprehensive while §3.3 says the
Risk Taker buys none until it has already been burned. Both are coherent given their stated
objectives, and the arithmetic above is what makes them different rather than one being wrong.

## The uncovered perils

Decision D3 notes that **Building Collapse and Electrical Failure are covered by no tier below
Business Interruption**. Rule-LK 10 lists five perils; Appendix E's table names four, two of which
(Vandalism, Earthquake) are not in the list at all.

That is a genuine defect in the spec, not a misreading. Following both texts literally leaves a
$2/5$ chance that a disaster is uninsurable at Basic and Comprehensive level — which drags the
expected payout above down further and makes Business Interruption's 15% premium look better than
it otherwise would.

Whatever you decide, document it once and apply it in one function. `covers()` is that function.

---

# Quick reference

| Quantity | Formula | Note |
|---|---|---|
| Percentage change | `pct(v,p) = (long)v*(100+p)/100` | truncates, divides last |
| Percentage of | `pct_of(v,p) = (long)v*p/100` | same |
| Effect composition | $v \times (1 + \Sigma m_i)$ | additive, one truncation, LK 34 |
| Compound interest | $P_n = P_0(1+r)^n$ | per **round**, at the issued rate |
| Doubling time | $n = \ln 2 / \ln(1+r) \approx 72/r\%$ | rule of 72 |
| Building condition | $C_n = 100 - 2n$ | linear, floors at 0 |
| Depreciation | $\min(30, \lfloor (n-50)/5 \rfloor)$ | after round 50, caps at 30% |
| Max loan | $0.75 \sum M_s$ | eligible collateral only, no buildings |
| Net worth | $C+P+B+R+U-L-T$ | $I=0$; $A$ already inside $L$ |
| Dice mean | $E[2d6] = 7$ | triangular, not uniform |
| GO income | $\approx 87.5$ laps $\times$ 2,000 | ≈ 175,000 per player per game |
| Auction opening | $0.5 \times V$ | increments of 250 |
| Overflow ceiling | $\texttt{INT\_MAX}/(100+p)$ | why `pct` casts to `long` |
