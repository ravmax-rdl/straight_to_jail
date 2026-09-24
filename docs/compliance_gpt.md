Overall compliance verdict

Partial compliance — substantial implementation, but not fully conformant to the assignment.

The program builds cleanly, runs autonomously, implements nearly all major systems, and survives broad seeded execution.
However, 11 confirmed deviations remain, including three high-severity gameplay errors:

1.  Incorrect initial turn order.
2.  Incomplete Anti-Speculation Act.
3.  Existing loan rates changed by loan increases.

The repository’s checked-off status claims are therefore overstated, particularly R2.2, R2.3, R3.13, R3.17, R3.19, and
parts of R4 in docs/REQUIREMENTS.md.

────────────────────────────────────────────────────────────────────────────────

Requirements checklist

┌────────────────────────────────────────┬────────┬────────────────────────────────────────────────────────────────────┐
│ Requirement │ Status │ Evidence/qualification │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §1.1: 40-square indexed board and │ Pass │ Board contains indices 0–39 and the required property, railway, │
│ specified layout │ │ utility, bank, insurance, tax, jail, parking, and event locations. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §1.1.1: Eight colour groups, ownership │ Pass │ All 22 colour properties and eight groups represented. Ownership, │
│ and development state │ │ mortgage, insurance, buildings, condition, age basis, and value │
│ │ │ fields exist. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §1.1.2: Four railways and count-based │ Pass │ Four stations; 250/500/1,000/2,000 rent schedule; mortgageable, │
│ rent │ │ not developable or insurable. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §1.1.3: Two utilities and dice-based │ Pass │ One utility uses 4× dice; both use 10× dice. │
│ rent │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §1.1.4: Bank and one financial action │ Pass │ Loan creation, repayment, extension, increase, and mortgage │
│ per landing │ │ redemption implemented. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §1.2: Three insurance policies, │ Pass │ Policy state, premiums, compensation, expiry, reminders, and │
│ premiums, coverage, 20-round expiry, │ │ per-property coverage implemented. Strategy-specific tier │
│ reminder │ │ selection has a separate defect below. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rule 1: Four players, LKR 30,000, no │ Pass │ Confirmed in initialization and seeded output. │
│ initial assets/liabilities │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rule 2: Highest roller starts; play │ Fail │ All four players are sorted by roll rather than selecting the │
│ proceeds clockwise │ │ starter and rotating clockwise. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rule 3: Required eight-step turn │ Partia │ Core movement sequence exists, but voluntary liquidation—financial │
│ sequence │ l │ step 7—runs before construction step 6. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rule 4: Passing/landing GO pays LKR │ Pass │ Implemented. │
│ 2,000 │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rule 5: Purchase or immediate auction │ Pass │ Implemented. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rule 6: Auction participation and LKR │ Pass │ Implemented with solvent-player filtering and permanent │
│ 250 increments │ │ withdrawal. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rule 7: Rent and mortgage suppression │ Pass │ Owned unmortgaged squares collect rent; mortgaged squares do not. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rules 8–10: Monopoly, even │ Pass │ Implemented; DEBUG invariants completed successfully. │
│ construction, four houses/hotel │ │ │
│ replacement │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rule 11: Immediate Income Tax and debt │ Pass │ Implemented using the documented 15%-of-cash clarification. │
│ recovery │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rules 12–13: Go To Jail and three │ Pass │ Transfer without GO payment; bail, doubles, and three-turn release │
│ release methods │ │ paths exist. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rule 14: Bankruptcy and asset │ Pass │ Buildings, policies, debt, foreclosure, auctions, and remaining │
│ disposition │ │ assets handled. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Rule 15: Termination and net-worth │ Pass │ MAX_ROUNDS is 500 and the controller checks it. Natural 500-round │
│ winner │ static │ termination was not reached in sampled runs. │
│ │ ally │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 1–3: Loan collateral, 75% capacity, │ Pass │ Eligible collateral and loan-lock restrictions implemented. │
│ locking │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 4–7: Interest, Bank actions, │ Pass, │ Accrual, duration, repayment, extension, default, and │
│ default and foreclosure │ except │ continuation/bankruptcy implemented. │
│ │ LK 13 │ │
│ │ intera │ │
│ │ ction │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 8–11: Insurance, disasters, │ Pass │ Implemented. │
│ repairs, rent suspension │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 12 and LK 14: Inflation schedule │ Pass │ Implemented using integer stored values and rounded percentage │
│ and compounded value update │ │ helpers. Extreme-value arithmetic has a robustness defect below. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 13: Inflation effects; existing │ Partia │ Ordinary inflation does not directly rewrite existing rates, but │
│ loan rates unchanged │ l │ increasing an existing loan rewrites its entire rate. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 15–17: Property age, depreciation, │ Pass │ Implemented with age derived from purchase round. │
│ renovation │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 18: Eight national economic events │ Partia │ All eight exist, but Foreign Investment incorrectly affects │
│ │ l │ railways. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 19–23: Auction opening, bidding, │ Pass │ Implemented. │
│ withdrawal, affordability, no-bid │ │ │
│ result │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 24: Eight government regulations │ Partia │ Most effects work; Anti-Speculation lacks the five-round │
│ │ l │ development rule, and Luxury Property Tax uses an invented landing │
│ │ │ trigger. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 25–26: Per-building condition and │ Pass │ Per-building state and condition-based rent implemented using │
│ rent bands │ │ documented averaging. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 27: Maintain any affordable number │ Partia │ Maintenance is all-or-nothing per property, preventing affordable │
│ of buildings │ l │ partial maintenance of houses. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 28–29: Structural damage and │ Pass │ Implemented. │
│ renovation │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 30–34: Dynamic market cycles, │ Pass │ Implemented using timed effect records and group cooldowns. │
│ cooldown, cumulative effects │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ LK 35–36: Regional expiry and │ Pass │ Regional modifiers expire back to the adjusted baseline; market │
│ end-of-round condition output │ │ conditions print every round. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §3.1: Aggressive Investor │ Partia │ Most behavior is implemented, but Basic insurance on a house is │
│ │ l │ not upgraded when it becomes a hotel. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §3.2: Conservative Banker │ Partia │ Cash reserve, recession, debt, development, insurance, and │
│ │ l │ renovation rules exist; railway/utility preference has no decision │
│ │ │ effect. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §3.3: Risk Taker │ Pass │ Buying, borrowing, aggressive auctions, early hotels, post-loss │
│ │ │ insurance, liquidation, and downturn behavior implemented. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §3.4: Opportunistic Trader │ Partia │ Market-sensitive purchase/construction/loan behavior exists; │
│ │ l │ balanced residential/railway/utility portfolio requirement is not │
│ │ │ implemented. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §4: C implementation and required │ Pass │ Required files exist and match their primary responsibilities. │
│ source files │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §4: Autonomous execution/no user input │ Pass │ No runtime interaction required. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §4: Programmatic decisions and PRNG │ Pass │ Strategies and seeded rand-based behavior implemented. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §4: Integer stored monetary │ Pass │ Stored money uses integers; ratio calculations round at │
│ calculations │ │ boundaries. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §4: Player/property financial records │ Pass │ Required state represented. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §4: gcc \*.c -o monopoly compilation │ Pass │ Canonical and strict C99 builds completed silently. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §5: Significant-event messages │ Partia │ Required message families and formatting are present, but the │
│ │ l │ printed turn order reflects the Rule 2 defect. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ §5: Round summaries, market │ Pass │ Present in completed runs. │
│ conditions, and game-over report │ │ │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Appendix A: 20-card circular deck and │ Partia │ Deck mechanics exist; Political Rally targets only a drawer-owned │
│ effects │ l │ property instead of an unrestricted random property. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Appendices B–E: Values, rent tables, │ Pass │ Implemented, subject to documented CSV and peril-coverage │
│ interest table, insurance policies │ │ clarifications. │
├────────────────────────────────────────┼────────┼────────────────────────────────────────────────────────────────────┤
│ Runtime CSV property prices/rents │ Pass │ Modified CSV values affected execution without recompilation; │
│ │ │ malformed/missing files fail cleanly. │
└────────────────────────────────────────┴────────┴────────────────────────────────────────────────────────────────────┘

────────────────────────────────────────────────────────────────────────────────

Findings

F1 — High: Initial turn order is not clockwise from the highest roller

Violated requirement: Rule 2 and the §5 worked example. The highest roller starts, then play proceeds clockwise—not in
descending roll order. See PDF lines 139 and 427–433.

Location: game.c:69-89, game.c:133-164

Evidence: determine_order() sorts all four players by score:

```c
sort_slice(g->order, score, 0, NUM_PLAYERS);
```

Seed 42 produced:

- Aggressive: 7
- Conservative: 6
- Risk Taker: 11
- Opportunistic: 4
- Actual order: Risk Taker → Aggressive → Conservative → Opportunistic
- Required clockwise order: Risk Taker → Opportunistic → Aggressive → Conservative

Expected: Determine only the starting player; rotate the original player order from that player. Rerolls should resolve
a tie for the starting position without globally sorting unrelated players.

Recommended fix: Replace the global score sort with:

1.  Find the highest total.
2.  Reroll only tied highest players until one starter remains.
3.  Set order[i] = (starter + i) % NUM_PLAYERS.

────────────────────────────────────────────────────────────────────────────────

F2 — High: Anti-Speculation Act omits the five-round development allowance/deadline

Violated requirement: LK 24: additional purchases require development within five rounds. PDF lines 269–270.

Location: players.c:116-145

Evidence: The implementation explicitly acknowledges the omission:

```c
/* What is not implemented is a deadline ... */
```

purchase_permitted() instead rejects another colour-property acquisition whenever the player already owns three
undeveloped properties.

Expected: An additional property may be acquired, but the player must immediately initiate/complete the required
development within five rounds, with tracked deadline state and a defined consequence.

Actual: The acquisition is rejected outright, including cases where the player could legally develop the new
acquisition.

Recommended fix: Track each qualifying acquisition and its owner-lap/game-round deadline. Permit the acquisition when
immediate development is legally possible, prioritize required development, and implement/document a consequence for
expiry. Because the PDF does not define that consequence, lecturer clarification is required before choosing one.

────────────────────────────────────────────────────────────────────────────────

F3 — High: Increasing a loan rewrites the existing balance’s frozen interest rate

Violated requirement: LK 13: “Existing loan rates remain unchanged.” PDF line 223.

Location: finance.c:544-593, especially line 591

Evidence:

```c
rate = current_loan_rate(g);
...
pl->loan.ratePct = rate;
```

The implementation adds extra to the existing principal, then assigns the current market rate to the entire combined
loan.

Expected: Existing debt retains its issued rate. A top-up must not retrospectively reprice the prior balance.

Actual: A favorable or unfavorable current rate reprices all accumulated principal.

Recommended fix: Either:

- track principal tranches with separate frozen rates; or
- compute and store a weighted effective rate when adding the new tranche, with explicit rounding.

A single-rate weighted balance is the smaller change but loses exact per-tranche compounding.

────────────────────────────────────────────────────────────────────────────────

F4 — Medium: Financial liquidation executes before construction

Violated requirement: Rule 3 steps 6–7. PDF lines 141–148.

Location: game.c:929-943

Evidence:

```c
liquidate_step(g, p); /* 7, ahead of 6 */
build_step(g, p);     /* 6 */
```

The comment confirms the reversal is deliberate.

Expected: Construct first; complete financial transactions afterward.

Actual: Property liquidation may fund construction in the same turn.

Recommended fix: Restore the specified order. If Risk Taker strategy needs funds for future premium development, perform
liquidation as step 7 and let construction use those funds on a subsequent turn.

────────────────────────────────────────────────────────────────────────────────

F5 — Medium: Maintenance cannot select an affordable subset of houses

Violated requirement: LK 27 permits maintenance of “any number of buildings” when sufficient funds are available. PDF
lines 289–293.

Location: board.c:794-820, game.c:882-901

Evidence: maintenance_cost() totals every house on a property:

```c
cost = pct_of(building_cost(...) * s->houses, MAINT_HOUSE_PCT);
```

After payment, restore_condition(s) restores all buildings. If the aggregate is unaffordable, maintenance stops.

Expected: A player able to afford one or two of four houses can maintain that subset.

Actual: Maintenance is all-or-nothing per property.

Recommended fix: Select individual buildings or an affordable count, charge per building, and restore only those
selected condition entries.

────────────────────────────────────────────────────────────────────────────────

F6 — Medium: Two required portfolio preferences have no decision effect

Violated requirements:

- §3.2 Conservative Banker prefers railways and utilities.
- §3.4 Opportunistic Trader maintains a balanced residential/railway/utility portfolio.

PDF lines 367–369 and 379–383.

Location: players.c:387-402, players.c:431-480, players.c:489-508

Evidence: The Conservative purchase function explicitly states the preference “has no expression.” Opportunistic
purchase and bidding consider projected appreciation/cost but not current asset-class counts or portfolio balance.

Expected: Legal alternatives should be ranked so those preferences affect purchases or auctions.

Actual: A railway, utility, and colour property meeting the same financial threshold receive no strategy-specific
portfolio weighting.

Recommended fix: Add tie-breaking/scoring based on asset class:

- Conservative: increase railway/utility acquisition priority without weakening the 50% reserve rule.
- Opportunistic: penalize overrepresented classes and prefer underrepresented residential/railway/utility classes when
  expected-return tests pass.

────────────────────────────────────────────────────────────────────────────────

F7 — Medium: Aggressive Investor can retain Basic insurance after upgrading to a hotel

Violated requirement: §3.1 requires Basic insurance for houses and Comprehensive insurance for hotels. PDF line 363.

Location: players.c:815-835; hotel upgrade at game.c:823-825

Evidence: decide_insurance() skips every property with an existing policy:

```c
if (s->owner != p || s->policy != INS_NONE)
   continue;
```

Hotel construction does not upgrade or cancel the previous house policy.

Expected: On a later insurance landing, a hotel with Basic coverage should be eligible for a Comprehensive upgrade.

Actual: The Basic policy remains until expiry or claim.

Recommended fix: Treat an existing lower-tier policy as upgradeable when the strategy’s desired tier changes; charge a
clearly documented premium or premium difference.

────────────────────────────────────────────────────────────────────────────────

F8 — Medium: Foreign Investment also increases railway values

Violated requirement: LK 18 applies Foreign Investment to “Commercial properties.” Railways are specified as a separate
board category. PDF lines 239 and 241–245.

Location: board.c:67-101, events.c:493-495

Evidence: All four railways carry REGION_COMMERCIAL; Foreign Investment applies EFF_VALUE_MUL to that region.

Expected: Only designated commercial properties receive the 20% value increase.

Actual: All railways receive it as well.

Recommended fix: Separate the concepts:

- REGION_COMMERCIAL_PROPERTY
- a distinct station/Port Expansion scope

Use only the former for Foreign Investment.

────────────────────────────────────────────────────────────────────────────────

F9 — Medium: Luxury Property Tax is triggered by landing on Income Tax

Violated requirement: LK 24 calls this an “annual maintenance tax” on hotel properties. PDF line 261.

Location: events.c:740-789; calls from finance.c:187 and finance.c:202

Evidence: The code comments and implementation deliberately levy it only when the hotel owner lands on the tax square
while the regulation is active.

Expected: A recurring maintenance levy, though the PDF does not define what “annual” means in simulation rounds.

Actual: A probabilistic board-landing levy. A player can owe nothing for the full regulation duration by not landing on
square 4.

Recommended fix: Obtain clarification for annual cadence. The least inventive interpretation is a periodic sweep at a
documented interval, independent of landing position.

────────────────────────────────────────────────────────────────────────────────

F10 — Low: Political Rally is restricted to property owned by the card drawer

Violated requirement: Appendix A says “One random property closed for 2 rounds.” PDF line 683.

Location: events.c:1237-1242

Evidence:

```c
sq = random_owned_square(g, p);
```

Expected: Selection from the assignment’s unqualified property population.

Actual: Only a property owned by the drawing player can be selected; nothing occurs if that player owns none.

Recommended fix: Select a random eligible property across the board unless lecturer clarification establishes
drawer-owned scope.

────────────────────────────────────────────────────────────────────────────────

F11 — Medium: Reachable signed-overflow expressions bypass saturation

Violated constraint: Reliable integer monetary calculation under the 500-round simulation.

Location: board.c:698, board.c:810, board.c:842, board.c:1002, board.c:1005

Evidence: Stored values can saturate at INT_MAX, but several raw int multiplications occur before the saturating
percentage helper, for example:

```c
building_cost(...) * s->houses
s->baseRent * HOTEL_RENT_MULT
s->baseRent * RENT_MULT[s->houses]
```

Signed overflow is undefined behavior in C.

Expected: Intermediate arithmetic remains defined even when inflation has driven a stored base near INT_MAX.

Actual: Multiplication may overflow before clamping. This was not triggered by the sampled natural runs, but the failing
arithmetic is reachable from valid saturated state.

Recommended fix: Add saturating multiply/add helpers or promote operands to double/int64_t, then clamp before converting
to int.

────────────────────────────────────────────────────────────────────────────────

Testing performed

Compilation

- gcc \*.c -o monopoly_review.exe — pass; no output
- gcc -std=c99 -Wall -Wextra -pedantic \*.c -o monopoly_strict.exe — pass; no warnings
- Strict DEBUG build with -DDEBUG — pass

Runtime

- Seed 42 — exit 0, 426,019 stdout bytes, zero stderr.
- Seed 1 — exit 0, 1,097,069 stdout bytes, zero stderr.
- DEBUG seed 42 — exit 0; no invariant failure.
- Seeds 1–30 — all exit 0 with zero stderr.
- Observed completion range: round 10 through round 229.
- Seed 42 repeated twice — byte-identical SHA-256 output.

Input/error paths

- Missing explicit CSV — exit 1, diagnostic on stderr, zero stdout.
- Malformed CSV — exit 1, line-specific diagnostic on stderr, zero stdout.
- Modified temporary CSV — Pettah purchase changed to LKR 1,600 and rent to LKR 111 without recompilation.

Uncovered or incompletely exercised edges

- No natural sampled game reached the 500-round limit.
- No automated unit/integration test files exist.
- Thirty seeded games do not guarantee execution of every event-card/regulation combination.
- Loan-rate top-up behavior, partial maintenance, insurance tier upgrades, and extreme saturated-value arithmetic need
  targeted state-level tests after correction.

────────────────────────────────────────────────────────────────────────────────

Assumptions and ambiguities

1.  Meaning of “round”: The PDF does not define it. The repository uses a game round ending when every solvent player
    completes a board lap (D30), while player-owned durations use owner laps (D34). This materially changes
    10/15/20-round cadences and should be lecturer-confirmed.
2.  Luxury Property Tax cadence: “Annual” is undefined, but a tax-square trigger is not stated in the PDF.
3.  Anti-Speculation penalty: The five-round obligation is explicit; the consequence for missing it is not.
4.  Multiple-building condition: The repository uses average condition (D37). The PDF gives per-building condition but
    one property-level rent table.
5.  Repair cost: Not quantified; repository assumes 50% of current replacement cost (D1).
6.  CSV clarifications: Individual purchase prices/base rents come from assets/Rent.csv; Appendix B supplies group
    mortgage and construction values. This is documented and verified at runtime.
7.  Other documented resolutions: Mortgage redemption, asset-sale price, event durations, regional mappings, tax bases,
    rounding, claim consumption, and coverage gaps are repository-defined decisions where the PDF is incomplete or
    contradictory.
8.  Net worth: The implementation follows Rule 15’s explicit formula rather than the introductory prose’s inconsistent
    mortgage-liability wording.

Review confidence: high for static conformance and exercised paths; moderate for rare event combinations and the
unobserved 500-round boundary.
