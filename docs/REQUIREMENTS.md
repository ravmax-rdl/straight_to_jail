# MONOPOLY-LK — Requirements

Source of truth: [`assets/Assignment_1_unlocked.pdf`](../assets/Assignment_1_unlocked.pdf) (SCS 1301 take-home, due 2026-08-16 23:55).
Supplemental data: [`assets/Rent.csv`](../assets/Rent.csv) — per-property purchase prices and base rents, plus the
lecturer's clarification set (recorded as decisions **D2′**, **D6′**, **D7′**, **D16**–**D22** below).

Rule numbers (`Rule N`, `Rule-LK N`, `§N`, `Table N`, `App X`) cite the PDF. Each requirement has an ID that the
implementation plan traces back to. Checkboxes track acceptance.

**Where the PDF and the clarifications disagree, the clarifications win** — they are later and authoritative. Every
such override is a numbered decision so it can be defended at the viva.

---

## R0 — Build & environment

- [x] **R0.1** Pure C (C99), standard library only. No external dependencies.
- [x] **R0.2** `gcc *.c -o monopoly` compiles with **zero errors and zero warnings** (§4). A `Makefile` may exist for convenience but must never be required.
- [x] **R0.3** At minimum these source files, split exactly by this responsibility (Table 5): `types.h`, `board.c`, `players.c`, `finance.c`, `events.c`, `game.c`, `main.c`.
- [x] **R0.4** No global variables. A single `GameState` struct on `main`'s stack, passed explicitly.
- [x] **R0.5** **No dynamic memory and no linked lists, anywhere, including the CSV reader.** Every collection is a fixed-size array indexed by `int`. No `malloc`, no `calloc`, no `realloc`, no `free`, no self-referential node structs, and no POSIX `getline` (it allocates, and is not C99). Nothing in the spec needs them — all sizes are known at compile time (40 squares, 4 players, 22 properties, 20 cards, 8 groups), and the CSV is read line-by-line into a fixed stack buffer straight into `g->board`.
- [x] **R0.6** All *stored* money is `int`. Ratio and interest arithmetic is computed in `double` and rounded to the nearest `int` at the boundary, through one helper (decision **D6′**).
- [x] **R0.7** All randomness via `srand`/`rand`. Seed = `argv[1]` if given, else `time(NULL)`, so development runs are reproducible.
- [x] **R0.8** Zero user interaction after launch (§4). No `scanf`, no `getchar`.
- [x] **R0.9** No `math.h`. `round()` risks needing `-lm`, which the mandated `gcc *.c -o monopoly` line does not supply. Rounding is arithmetic (see **D6′**).
- [x] **R0.10** **Per-property rent data is read from `assets/Rent.csv` at runtime using file handling** — `fopen`/`fgets`/`fclose` over fixed stack buffers. The values are data, not code: no transcribed copy of the CSV exists in any source file. Location and failure policy are decision **D27**.

## R1 — Board & entities

- [x] **R1.1** 40 squares, indexed 0–39 clockwise (Table 1):

| # | Square | # | Square | # | Square | # | Square |
|---|--------|---|--------|---|--------|---|--------|
| 0 | GO | 10 | Jail / Just Visiting | 20 | Free Parking | 30 | Go To Jail |
| 1 | Pettah (Brown) | 11 | Nugegoda (Pink) | 21 | Kandy City (Red) | 31 | Jaffna Town (Green) |
| 2 | **Community Development Fund** | 12 | Utility — Ceylon Electricity Board | 22 | Event — National Event Card | 32 | Nallur (Green) |
| 3 | Maradana (Brown) | 13 | Maharagama (Pink) | 23 | Peradeniya (Red) | 33 | Insurance — Ceylinco |
| 4 | Tax — Income Tax | 14 | Kottawa (Pink) | 24 | Katugastota (Red) | 34 | Trincomalee (Green) |
| 5 | Railway — Colombo Fort | 15 | Railway — Kandy | 25 | Railway — Galle | 35 | Railway — Jaffna |
| 6 | Bambalapitiya (L.Blue) | 16 | Negombo (Orange) | 26 | Galle Fort (Yellow) | 36 | Event — National Event Card |
| 7 | Event — National Event Card | 17 | Insurance — Sri Lanka Insurance | 27 | Unawatuna (Yellow) | 37 | Nuwara Eliya (D.Blue) |
| 8 | Wellawatte (L.Blue) | 18 | Katunayake (Orange) | 28 | Utility — NWSDB | 38 | Bank — Bank of Ceylon |
| 9 | Mount Lavinia (L.Blue) | 19 | Ja-Ela (Orange) | 29 | Hikkaduwa (Yellow) | 39 | Galle Face (D.Blue) |

- [x] **R1.2** Square 2 is a **Community Development Fund** square, not a National Event Card square (decision **D17**). Table 1 types it "Event", but it draws no card — it levies a tax. There are therefore exactly **three** card squares: 7, 22, 36.

- [x] **R1.3** 22 properties with **individual** purchase prices and base rents, **read at runtime from `assets/Rent.csv`** (**R0.10**, decisions **D7′** and **D27**). The table below records what the file contains as of the current revision; it is documentation, not a specification of compiled-in constants, and the program must reproduce whatever the file says rather than these figures:

| Group | Property | Sq | Price | Base rent |
|-------|----------|----|-------|-----------|
| Brown | Pettah | 1 | 1,500 | 100 |
| Brown | Maradana | 3 | 1,800 | 120 |
| Light Blue | Bambalapitiya | 6 | 2,500 | 180 |
| Light Blue | Wellawatte | 8 | 2,700 | 200 |
| Light Blue | Mount Lavinia | 9 | 3,000 | 220 |
| Pink | Nugegoda | 11 | 3,500 | 260 |
| Pink | Maharagama | 13 | 3,800 | 280 |
| Pink | Kottawa | 14 | 4,000 | 300 |
| Orange | Negombo | 16 | 4,500 | 350 |
| Orange | Katunayake | 18 | 4,700 | 370 |
| Orange | Ja-Ela | 19 | 5,000 | 400 |
| Red | Kandy City | 21 | 5,500 | 450 |
| Red | Peradeniya | 23 | 5,800 | 480 |
| Red | Katugastota | 24 | 6,000 | 500 |
| Yellow | Galle Fort | 26 | 6,500 | 600 |
| Yellow | Unawatuna | 27 | 6,800 | 620 |
| Yellow | Hikkaduwa | 29 | 7,000 | 650 |
| Green | Jaffna Town | 31 | 8,000 | 750 |
| Green | Nallur | 32 | 8,300 | 780 |
| Green | Trincomalee | 34 | 8,500 | 800 |
| Dark Blue | Nuwara Eliya | 37 | 10,000 | 1,000 |
| Dark Blue | Galle Face | 39 | 12,000 | 1,200 |

- [x] **R1.4** **Group** values (App B) supply construction costs and mortgage value only (decision **D18**). The group Purchase Price column is the loan-calculation basis and is never charged to a buyer; note it equals the cheapest member of each group.

| Group | Group base price | House | Hotel | Mortgage value |
|-------|------------------|-------|-------|----------------|
| Brown | 1,500 | 500 | 2,000 | 750 |
| Light Blue | 2,500 | 750 | 3,000 | 1,250 |
| Pink | 3,500 | 1,000 | 4,000 | 1,750 |
| Orange | 4,500 | 1,250 | 5,000 | 2,250 |
| Red | 5,500 | 1,500 | 6,000 | 2,750 |
| Yellow | 6,500 | 2,000 | 8,000 | 3,250 |
| Green | 8,000 | 2,500 | 10,000 | 4,000 |
| Dark Blue | 10,000 | 3,000 | 12,000 | 5,000 |

- [x] **R1.5** Each square tracks: individual price, individual base rent, group mortgage value, house/hotel cost, owner, `purchasedRound`, mortgage status, loan-lock status, insurance policy + remaining rounds, building count, hotel flag, depreciation, condition, unmaintained-round count, damaged and structurally-damaged flags. **Age is derived, not stored** — it is `round − purchasedRound` (**D19**), so it cannot disagree with ownership.
- [x] **R1.6** 4 railways: price **1,500**, mortgage value **750**; rent 250/500/1,000/2,000 by count owned by one player (Table 2/7); mortgageable; never developable or insurable.
- [x] **R1.7** 2 utilities: price **1,500**, mortgage value **750**; rent = 4× dice (one owned) or 10× dice (both) (Table 8); mortgageable; never developable.
- [x] **R1.8** Bank of Ceylon square: exactly **one** action per landing — LK 5's five loan actions (obtain / repay part / repay full / extend / increase) plus **mortgage redemption** (decision **D31**). §1.1.4 and Rule-LK 5 give the loan actions; redemption is the only route by which a mortgage can be lifted, and it shares the square and the one-action rule. **A loan can likewise be repaid only by landing here**; there is no other route (clarification, decision **D19**).
- [x] **R1.9** 2 insurance squares; landing allows purchase or renewal of one policy tier for one property (§1.2).

## R2 — Traditional rules (Rules 1–15)

- [x] **R2.1** 4 players; each starts with LKR 30,000 and nothing else (Rule 1).
- [x] **R2.2** Turn order by dice roll-off, highest first. Only tied players reroll, and the reroll permutes **only their own positions** — untied players keep their ranks (Rule 2, decision **D8′**).
- [x] **R2.3** 8-step turn sequence (Rule 3): penalties → roll → move → landing action → purchase → construction → financial transactions → end. Maintenance happens only in step 1 (Rule-LK 27).
- [x] **R2.4** A **turn** is one dice roll for one player unless another condition prevails. A **round** is one **lap of the board**: it ends once every solvent player has passed GO since it began (decision **D30**). Players continue taking turns in `order[]` sequence throughout, so a round spans several turns each — about six, and 20–32 turns in total while four players remain.
- [x] **R2.5** Pass or land on GO → +LKR 2,000 (Rule 4).
- [x] **R2.6** Unowned purchasable square: buy at its individual list price, or it goes **immediately** to auction (Rule 5).
- [x] **R2.7** Rent owed on owned, unmortgaged property; mortgaged collects nothing (Rule 7). Landing on one's own property charges nothing.
- [x] **R2.8** Full colour group = monopoly = only then may build (Rule 8).
- [x] **R2.9** Even building across group; ≤4 houses; hotel replaces exactly 4 houses; never houses + hotel together (Rules 9–10).
- [x] **R2.10** Rent multipliers on base rent (Table 6): 1×/2×/3×/5×/7× for 0–4 houses; 10× hotel.
- [x] **R2.11** Income Tax (square 4) = **15% of the player's current cash** (decision **D2′**), payable immediately; shortfall → debt recovery (Rule 11, decision **D11**). The rate is inflation-adjusted and scaled by the *Increase Property Tax* regulation.
- [x] **R2.12** Community Development Fund (square 2) = **10% of total assets** — the current market value of owned properties only, buildings excluded (decision **D16**) — payable immediately, same shortfall handling. Because the base is read through `square_value`, the levy tracks market fluctuations automatically.
- [x] **R2.13** Go To Jail → straight to square 10, no GO money (Rule 12).
- [x] **R2.14** Jail exit: pay LKR 300 bail, roll doubles, or wait 3 turns (Rule 13; after the 3rd turn see **D10**).
- [x] **R2.15** Bankruptcy when liabilities exceed assets: buildings removed, policies expire, loans due, assets transferred (Rule 14, **D11**).
- [x] **R2.16** Game ends when one solvent player remains or after 500 rounds; then highest net worth wins (Rule 15).
- [x] **R2.17** Net worth = cash + property + buildings + railway + utility + claims receivable − loans − accrued interest − taxes due (Rule 15; receivable is always 0 per **D15**).

## R3 — MONOPOLY-LK extensions

**Loans (Rule-LK 1–7)**
- [x] **R3.1** Collateral = properties, railways, utilities only — never buildings (LK 1).
- [x] **R3.2** Max loan = 75% of total mortgage value of eligible (unmortgaged, not loan-locked) collateral (LK 2, decision **D5**). Mortgage value is the fixed group figure of R1.4, adjusted only by active `MORTGAGE_MUL` effects.
- [x] **R3.3** Loan credits cash instantly. Only the **minimum** set of assets needed to cover the amount at 75% LTV is pledged (decision **D22**). Pledged assets become *loan-locked*: no sale/trade/auction/re-mortgage, but they still earn rent and may be developed (LK 3).
- [x] **R3.3a** The issued rate is **Appendix D Table 9 read by the prevailing economic condition** — Recession 15%, Stock Market Boom 5%, else High Inflation 12% above a 5% draw, Moderate 10% above 0, Stable 8% at or below 0 — plus any active ±2 percentage-point adjustment from LK 24 or Appendix A (decision **D21**).
- [x] **R3.4** Duration 20 laps of the borrower from issue. Every lap `principal += principal × rate / 100` at the loan's **issued** rate, which LK 13 freezes for life, scaled only by an active Economic Recession or Stock Market Boom (LK 4, decisions **D4**, **D21**, **D34**).
- [x] **R3.5** Default → foreclosure: pledged assets to Bank, buildings demolished, their policies cancelled, debt cleared; player continues, or is bankrupt if nothing remains (LK 6–7). Foreclosed assets go to auction (LK 19).

**Insurance & disasters (Rule-LK 8–11, §1.2, App E)**
- [x] **R3.6** Three tiers, premiums 5% / 10% / 15% of current property value; coverage per decision **D3**. One policy per property; valid 20 rounds; reminder 3 rounds before expiry (LK 8–9).
- [x] **R3.7** A policy covers **exactly one claim**. Paying out consumes it, whatever rounds remain (clarification, decision **D20**).
- [x] **R3.8** Every 10 rounds a random disaster (Fire, Flood, Riot, Building Collapse, Electrical Failure) may strike one random **developed** property; insured → compensation credited immediately, else the owner pays the repair cost (LK 10, repair cost per **D1**).
- [x] **R3.9** Damaged buildings collect no rent until repaired; repairs happen automatically once the owner can afford them (LK 11).

**Inflation (Rule-LK 12–14)**
- [x] **R3.10** Every 10 rounds draw inflation from {−3, 0, 2, 5, 8, 12}%; apply `New = Old × (1 + rate)` permanently to individual property prices, individual base rents, building/hotel costs, and mortgage values. Premiums and repair costs inflate automatically because they derive from those. The current loan interest rate moves by the same factor, for **new** loans only (**D21**).

**Depreciation & renovation (Rule-LK 15–17)**
- [x] **R3.11** Property age counts rounds **since purchase**, not since game start; unowned property never ages (clarification, decision **D19**). Properties older than 50 rounds without renovation lose 1% per 5 rounds, capped at 30%.
- [x] **R3.12** Landing on one's own property allows renovation: cost 10% of current market value; restores depreciation, resets age.

**National economic events (Rule-LK 18)**
- [x] **R3.13** Every 15 rounds, one of the 8 events (Tourism Boom, Fuel Crisis, Heavy Monsoon, Economic Recession, Stock Market Boom, Government Housing Programme, Foreign Investment, Political Unrest) fires, affecting **all** players for 15 rounds, with the exact listed effects. Economic Recession and Stock Market Boom set the Table 9 row for new loans and scale live ones (**D21**).

**Auctions (Rule-LK 19–23)**
- [x] **R3.14** Triggered by declined purchase, bankruptcy liquidation, or foreclosure returns. Opening bid = **50% of the square's current market value** (LK 19), reduced a further 25% while its group is in decline (LK 32). Minimum increment LKR 250. All solvent players participate, **including the player who declined**.
- [x] **R3.15** Bidding starts with the player **immediately after the current player** in `order[]` and proceeds clockwise (clarification, decision **D23**). For bankruptcy and foreclosure auctions the anchor is the player whose turn it is.
- [x] **R3.16** On each opportunity a player either raises the bid or withdraws; withdrawal is permanent for that auction. The last remaining bidder pays their bid and takes ownership. A bid may never exceed the bidder's cash, and no loan may be taken mid-auction (LK 22). If **every** player declines at the opening price, ownership stays with the Bank (LK 23).

**Government regulations (Rule-LK 24)**
- [x] **R3.17** Every 20 rounds one of the 8 regulations is selected: Increase Property Tax (+50% income tax), Reduce Loan Interest (−2%), Housing Subsidy (−30% house cost), Luxury Property Tax (25% of developed hotel-property value, charged **once on activation** per **D24**), Railway Modernization (+25% railway rent), Electricity Tariff Revision (+20% utility rent), Insurance Regulation (−15% premiums, coverage unchanged), Anti-Speculation Act (≤3 undeveloped properties per **D25**).

**Building condition & maintenance (Rule-LK 25–29)**
- [x] **R3.18** Every building starts at 100% condition, −2% at the end of every round; rent factor by condition band (Table 3): 90–100→100%, 75–89→90%, 50–74→75%, 25–49→50%, <25→closed (no rent).
- [x] **R3.19** Maintenance only at the start of the owner's turn: house 5% / hotel 8% of construction cost, restores 100%, any number of buildings if affordable.
- [x] **R3.20** Maintenance ignored >20 consecutive rounds → structural damage: property value −15%, max rent −25%, future maintenance +50% (LK 28). Renovating a structurally damaged building costs 25% of replacement value and restores value, rent and condition (LK 29).

**Dynamic property market (Rule-LK 30–34)**
- [x] **R3.21** Every 10 rounds: one random group booms (+15% purchase price, +15% mortgage, +25% rent, +10% construction, +20% value), another declines (−15% value, −20% rent, −10% mortgage, −25% auction opening), each lasting 10 rounds; no group repeats the same event in consecutive reviews; 30-round cooldown per group; concurrent effects stack cumulatively (LK 34, mechanics per **D12**).

**Regional development cards (Rule-LK 35–36, Table 4)**
- [x] **R3.22** Every 15 rounds one of the **12** regional cards is drawn, active 15 rounds, affecting only its named squares (region tags per **D14**); on expiry values revert to the current market-adjusted baseline (LK 35).
- [x] **R3.23** The active market-conditions block prints at the end of **every** round (LK 36, format in R5).

**National Event Card deck (App A)**
- [x] **R3.24** A 20-card deck; drawn **only** when a player lands on square 7, 22 or 36; the top card executes, then returns to the bottom (circular queue over a fixed array); effects apply to the drawing player for 15 rounds, on top of the per-card durations. A *separate system* from R3.13.

## R4 — Player strategies (§3)

Each turn a player evaluates every legal action and executes the one best fitting its strategy. Full behaviour lists in
§3.1–3.4; proxy formulas for the judgment calls in **D9**.

- [x] **R4.1 Aggressive Investor** — always buys if one future rent remains payable; always bids, up to 120% of estimated value; completes groups first; max houses immediately, hotels ASAP; borrows whenever it lifts projected rent; repays only when cash > 2× loan; Basic insurance on houses, Comprehensive on hotels; never sells voluntarily; covets Galle Face & Nuwara Eliya.
- [x] **R4.2 Conservative Banker** — buys only if ≥50% cash remains; bids only below market value; loans only to stave off bankruptcy, repaid at every Bank visit; Comprehensive on every developed property; no hotels while indebted; prefers railways/utilities; no investments during recessions; renovates at >10% depreciation; largest cash reserve.
- [x] **R4.3 Risk Taker** — buys everything possible; always borrows the max, refinances often; bids until cash is gone; hotels as early as possible; insures only after a loss; ignores depreciation until forced; sells cheap assets to fund premium ones; invests through downturns.
- [x] **R4.4 Opportunistic Trader** — buys only when projected appreciation exceeds construction cost; prefers discounted auctions; borrows only when return beats cost; Comprehensive only on high-value developments; delays construction during inflation, accelerates under housing subsidies; renovates at >15% depreciation; sells ahead of declines; balanced portfolio.

## R5 — Output (§5)

- [x] **R5.1** Every §5 message reproduced with exact wording, punctuation and line breaks: pre-game header, roll-off, dice roll, movement, passing GO, purchase, rent, house construction, hotel upgrade, loan obtained/repaid/defaulted, insurance purchase, disaster + claim, full auction sequence, economic event, government regulation, depreciation, insurance-expiry warning, bankruptcy.
- [x] **R5.2** Blank lines are content, in two forms (decision **D26**). Within a block, a labelled category opens a new group. Between blocks, **every message type ends with a blank line** — `end_block()` — so two different kinds of output never run together. The `Round N Summary` and `Current Market Conditions` tables stay internally compact.
- [x] **R5.3** All amounts formatted with thousands separators: `LKR 12,300`. Every monetary print goes through `fmt_lkr`.
- [x] **R5.4** End of every round: the `Round N Summary` block (per player: Cash, Net Worth, Properties, Hotels, Outstanding Loan — `None` when zero) with 45-character `=`/`-` rule lines.
- [x] **R5.5** End of every round, immediately after the summary: the Rule-LK 36 `Current Market Conditions` block with 41-character rule lines (Market Boom, Market Decline, Regional Development, Inflation, Current Loan Interest — each with rounds remaining).
- [x] **R5.6** End of game: `GAME OVER` block with winner, total cash, total property value, outstanding loans, net worth.
- [x] **R5.7** `MONOPOLY-LK Simulation` is the **first line of output**. No banner, ASCII art or diagnostic may precede it.

---

## D — Decisions

Every point below is either a spec gap or a place where the lecturer's clarifications override the PDF. Each is
implemented exactly once, at a choke point, with a comment citing its ID.

### Superseded by clarification

| ID | Was | Now |
|----|-----|-----|
| ~~**D2**~~ | Income Tax = LKR 1,000 base | **D2′** Income Tax = **15% of the player's current cash**, **assessed every round** and collected on square 4 (**D33**). The rate lives in `econ.incomeTaxPct`, seeded at 15, moved multiplicatively by each inflation draw exactly as the loan rate is (**D21**), and scaled again at charge time by `EFF_TAX_MUL` — ×1.5 under *Increase Property Tax* |
| ~~**D6**~~ | All percentage math in `int`, truncating | **D6′** Money is **stored** as `int`. Ratio, interest and percentage arithmetic is computed in `double` and rounded to nearest at the boundary by one helper, `int money_round(double)`. No `math.h` (**R0.9**): rounding is `(int)(v + (v >= 0 ? 0.5 : -0.5))` |
| ~~**D7**~~ | Base rent = 10% of group purchase price | **D7′** Individual purchase price and individual base rent come from `Rent.csv` (**R1.3**), **read from the file at startup** (**D27**) rather than transcribed into a static table. The group figures are construction costs and mortgage value only |

### Active decisions

| ID | Gap | Resolution |
|----|-----|-----------|
| **D1** | Repair cost never quantified | Repair cost = **50% of current construction cost of the buildings on the property** (houses × house cost, or hotel cost) — damage is to buildings, so it scales with development and tracks inflation through `building_cost`. **The base is only coherent because damage never reaches a bare lot.** LK 11 is the operative sentence — *"damaged **buildings** cannot collect rent"* — and LK 10 says the same from the other side, striking *"one randomly selected **developed** property"*. Appendix A muddied this: its *National Disaster* card says "random **developed** property damaged" but *Heavy Floods* says only "random **coastal** property damaged", and taking that literally damaged vacant lots, where this base is zero — 184 of 338 repairs printed `Repair Cost : LKR 0.`, restoring nothing at no cost. LK 11 precedes, so Heavy Floods now selects a developed coastal property or does nothing. Selling the last building also clears the flag, since the damage was the buildings'; `reset_to_bank` already did so on the foreclosure and bankruptcy paths. A DEBUG invariant asserts no square is ever damaged without buildings, so a fourth emptying path cannot reintroduce it. **Repricing to a spec-stated figure was considered and declined**: LK 17's 10% of current market value is defined for every square, but LK 17 is the *depreciation* mechanic — §2.4, elective on landing, restoring age and wear — while LK 11's repair is automatic and about damage, and adopting its price would have made repair on developed property roughly 4× cheaper (mean LKR 663 against 2,713), weakening disasters and the case for insurance with them |
| **D3** | Peril coverage inconsistent; two disasters covered by no tier | Literal: Basic {Fire, Flood} @80%; Comprehensive {Fire, Flood, Riot, Vandalism} @100% (App E's "Earthquake" ignored — it never occurs); Business Interruption (hotel properties only) covers **all** perils @100% + 5 rounds of hotel rent, paid as an immediate lump sum. Building Collapse and Electrical Failure are uncovered by Basic and Comprehensive — the owner pays |
| **D4** | Table 9 says "annual" but LK 4 compounds per round | Follow LK 4 literally: interest added **every round**; the "annual" label is ignored as inconsistent. Issued rate frozen for the loan's life (LK 13). **A loan is a single-player instrument, so "every round" is every lap of the BORROWER** (**D34**) — which is what makes the instrument coherent: its term is twenty of their laps, so it accrues exactly twenty times over that term however fast or slow they move. Compounding on the game round while the term counted laps charged a quick player fewer periods than a slow one for the same twenty-lap loan. Applied once per lap rather than as a single compound step, so **D6′**'s rounding still lands at each period as LK 4 describes |
| **D5** | Max-loan base stated two ways (§1.1.4 vs LK 2) | LK 2 wins: 75% of mortgage values of all eligible collateral — properties, railways and utilities — unmortgaged and not already loan-locked |
| **D8′** | Roll-off tie handling | Only tied players reroll, and the reroll decides **only their positions among themselves**; untied players keep their ranks. Repeat while ties remain (Rule 2 + clarification) |
| **D9** | Strategy judgment calls need formulas | "Estimated market value" = `square_value()`. Aggressive bid cap = 120% of it; Conservative bids strictly below it; Opportunistic "projected appreciation" = value × (sum of active positive modifiers − active negative), and it buys when that exceeds the group's house cost |
| **D10** | Jail after 3 turns; doubles outside jail | After the 3rd failed turn bail is auto-paid and the player moves normally. Doubles have no effect outside jail — no extra turns, no triple-doubles rule (spec is silent) |
| **D11** | "Normal debt recovery" and bankruptcy transfer never defined | On any unpayable charge: (1) sell buildings back at 50% of construction cost, (2) mortgage non-locked assets at mortgage value, (3) still short → bankrupt: the creditor receives remaining cash, assets are auctioned (LK 19 trigger), unsold assets stay with the Bank. **Repaying a loan is not available as a fund-raising step** — LK 5 and the clarification confine repayment to the Bank square |
| **D12** | How permanent and temporary effects compose (LK 14 vs LK 34–35) | **Permanent** effects (inflation, permanent card value changes) mutate stored values. **Temporary** timed effects (booms, declines, regional cards, national events, event cards, regulations) live in the effect registry and are read at access time, composed additively per LK 34. Expiry removes the record, which automatically yields LK 35's "revert to the market-adjusted baseline" |
| **D13** | Order of end-of-round processing | Loan interest → default check → age/condition tick → insurance tick → auto-repairs → cadenced systems (5: depreciation; 10: inflation, market review, disaster; 15: national event, regional card; 20: regulation) → effect-duration tick → round summary → market conditions block |
| **D14** | Events name regions the spec never maps to squares | `SOUTHERN_COASTAL` = {26, 27, 29}; `COASTAL` = southern + {6, 8, 9, 16, 34}; `LOW_LYING_COASTAL` = `COASTAL`; `COMMERCIAL` = {1, 3, 39} and the four railways where a card names stations; `NWSDB_ADJACENT` = {26, 27, 29} |
| **D15** | "Insurance claims receivable" in net worth | Always 0 — LK 10 credits compensation immediately, and Business-Interruption lost rent is paid as an immediate lump sum |
| **D16** | "Total assets" for the Community Development Fund | Sum of `square_value` over the **22 coloured properties** owned by the player. Buildings, railways and utilities are excluded. Because it reads `square_value`, the base tracks the market automatically, which is what the clarification means by "10% will also be affected by the market fluctuations". This base belongs to the Fund alone — Income Tax charges cash (**D2′**), so the two squares are genuinely different instruments rather than the same tax at two rates |
| **D17** | Square 2 typed "Event" but named Community Development Fund | Its own `SQ_COMMUNITY` type. It draws no card. Card squares are 7, 22 and 36 only |
| **D18** | Which price governs which calculation | Individual price (`Rent.csv`) → purchase, market value, rent basis, renovation cost, tax base, auction opening. Group base price (App B) → mortgage value only, i.e. loan capacity and debt-recovery proceeds. Group columns also supply house and hotel construction cost |
| **D19** | Whether durations count player turns or game rounds | One clock: the game round. A loan stores `issuedRound` and matures at `issuedRound + 20`; property age is `round − purchasedRound`. Player-scoped and global effects therefore need no separate counter, because every solvent player takes exactly one turn per round — the two readings coincide numerically and diverge only on bankruptcy, when the player has left the game |
| **D20** | Whether a policy survives a payout | It does not. A claim consumes the policy regardless of remaining rounds (clarification) |
| **D21** | How a loan's interest rate is set | **REVISED. Appendix D is a lookup, not a seed.** Table 9 introduces itself as "loan interest rates vary according to the prevailing economic conditions"; the clarification supplies the mapping the spec never did, so the condition picks the row and **the row is the rate**. Nothing drifts, and `econ.interestRatePct` no longer exists. **Precedence:** Economic Recession → 15% and Stock Market Boom → 5% both outrank the inflation reading. Table 9's "Economic Boom" is read as the **LK 18 event**, not LK 33's per-group Market Boom — that is a property condition, is in force in most rounds, and keying on it would make 5% the common case rather than the rare one. With neither event running, LK 12's latest draw decides: **above 5% → High Inflation 12%**, **above 0 → Moderate 10%**, and a draw **at or below 0 → Stable Economy 8%**, deflation and zero being no inflation at all. LK 12 draws from a fixed six (−3, 0, 2, 5, 8, 12), so all five rows are reachable — −3 and 0 Stable, 2 and 5 Moderate, 8 and 12 High — and none of the table is decorative. Under the previous reading only the 8% seed was ever a real issued rate. **The ±2 percentage-point instruments still land on top** (`EFF_INTEREST_ADD`): LK 24's *Reduce Loan Interest* and App A's *Interest Rate Cut* / *Increase* each name a direct 2-point move and describe no condition Table 9 has a row for, so they modify the row rather than being expressed by it. The issued rate may therefore sit outside the table's band — across 30 seeds it spans 3% to 17% around the five figures. **`EFF_INTEREST_MUL` is absent from the issued rate and governs live loans instead.** Recession and Stock Market Boom already choose their own rows, so applying their shift at issue would charge the condition twice; instead they leave the frozen rate alone (LK 13) and scale what an existing debt compounds at while they last — a recession makes a live loan bite harder, a boom eases it. Which event is in force is remembered in `econ.activeEvent`, for the same reason `activeCard` is: Table 9 keys on the condition, and a condition is an identity an `Effect` record cannot carry |
| **D22** | Which assets a loan pledges | The minimum set — highest mortgage value first — whose 75% LTV covers the requested amount. LK 3 is silent on scope; locking only what is needed is strictly more playable and never exceeds LK 2's cap |
| **D23** | Auction bidding order | Starts with the player immediately after the current player in `order[]`, then clockwise. For bankruptcy and foreclosure auctions the anchor is the player whose turn it is. The player who declined the purchase may bid |
| **D24** | Luxury Property Tax "annual" cadence | Charged **once when the regulation activates**, at 25% of each hotel-bearing property's value including its buildings. Regulations run about 20 rounds, so once per activation is the closest available reading of "annual". Unlike D4, LK 24 supplies no per-round cadence to override this |
| **D25** | Anti-Speculation Act — what the cap counts and how it is enforced | **The cap is on what a player OWNS, not on how they came to own it.** LK 24 allows at most three undeveloped properties, so the cap gates the auction exactly as it gates a direct purchase — bidding was previously the way round it. It counts **colour properties only**: railways and utilities can never be developed, so counting them would put a player holding four stations in permanent breach of an obligation they have no legal way to discharge, and §1.1.1 calls the twenty-two colour squares "properties" with stations and utilities described separately in §1.1.2 and §1.1.3. **Holding more than three invokes LK 24's second clause**, immediate development, which is implemented as construction becoming compulsory: above the cap every personality builds, and preferences are suspended — including R4.2's refusal to build hotels while indebted, the regulation being the more specific instruction. A player can exceed the cap only transiently, either because the regulation activated while they already held more or because the D11 ladder demolished buildings out from under them; the compulsory build brings them back. **This supersedes the earlier reading**, which enforced the cap alone and dismissed the five-round clause as unreachable |
| **D26** | Whether §5's vertical gaps are content | **They are, and there are two kinds — revised twice.** The first reading emitted everything compact, on the theory that §5's gaps were LaTeX spacing between verbatim chunks. Re-extracting §5 with blank lines made visible disproved that: within a block, a **labelled category opens a new group** — `Property :`, `Opening Bid :`, `Loan Amount :`, `Interest Rate :` each sit under their own heading with a blank above. The second revision adds the rule the clarification made explicit: **every message type ends with a blank line**, so two different kinds of output are never run together. That terminator is `end_block()` in `game.c` — named rather than a bare `putchar`, because the blank is content and a reader tidying away a stray newline would silently break the format. Thirty-two blocks call it. It is emitted only on the paths that actually printed: when a charge fails and the Rule 14 ladder takes over, the ladder's own blocks supply the spacing, so the aborted block must not add a second one. Two blocks stay internally compact because their extraction is unambiguous — the **Round Summary**'s 30 consecutive lines (the one blank the extraction shows falls exactly at the page-22 break, the artifact the original reading correctly identified — it was only wrong to generalise from it) and the **Current Market Conditions** table. The blocks invented here follow whichever §5 block they were shaped after: maintenance takes construction's spacing, the LK 5 extend and increase actions take the loan block's, and **D31**'s redemption and **D32**'s sale take loan repayment's. `GAME OVER` straddles the same kind of page break and remains the one place §5 cannot arbitrate; it is still emitted compact |
| **D27** | Where `Rent.csv` lives and what happens when it does not | **Searched, then fatal.** `board_init` tries `assets/Rent.csv`, then `Rent.csv`, then `../assets/Rent.csv`, so the program runs whether it is launched from the repository root or from the source directory. `argv[2]` overrides the search with an explicit path, and an explicit path is never second-guessed — failing to open the file the caller named is the error worth reporting. If nothing opens, or the data is malformed, `main` prints a diagnostic **to stderr** and returns 1 **before any stdout output**, so a failed run never emits a partial pre-game block and never pollutes the graded stdout stream (**R5.7**). There is deliberately **no fallback table**: a compiled-in copy would be a second source of truth, free to drift silently out of step with the file. Validation is total — every row must have 4 fields, a property name that exists on the board, a group matching the board's, a strictly positive integer price and base rent, and no duplicates; and all 22 property squares must be covered when the file closes. Both LF and CRLF line endings are accepted, since `.gitattributes` normalises on checkout while the supplied file is CRLF |
| **D28** | The spec states net worth twice, and the two disagree | **Rule 15 governs.** The introduction says net worth considers "available cash, owned properties, developed buildings, insurance compensation receivable, outstanding loans, **and mortgage liabilities**", while Rule 15 gives the formula as `Cash + Property Value + Building Value + Railway Value + Utility Value + Insurance Claims Receivable − Outstanding Loans − Accrued Interest − Taxes Due` — no mortgage term, but two liabilities the intro omits. Rule 15 is the numbered rule and the one §5 reports against, so it wins; the intro is prose scene-setting. **`net_worth` therefore carries no mortgage term.** The consequence is accepted rather than overlooked: a mortgaged square is still counted at full market value, so mortgaging *raises* the reported figure by the cash it releases, and a player can show a healthy net worth while holding nothing unencumbered. Accrued interest needs no separate term because LK 4 compounds it into the principal (**D4**), so subtracting the principal subtracts it; taxes due is structurally zero, since both tax squares charge on the spot, but is subtracted rather than assumed. Claims receivable is permanently 0 (**D15**). §5's `GAME OVER` block shares this basis: its *Total Property Value* line reads `square_value` and `building_cost` exactly as `net_worth` does, so `Cash + Property − Net Worth` equals the outstanding loan on every seed. It summed the stored price before, which ignored depreciation, structural damage, every market effect and the buildings, and left the block's two figures on bases that could not be reconciled. Note this is a question of accounting only — a mortgage and a loan are genuinely separate instruments in the rules (LK 3 has loan-locked collateral "continue earning rent" and bars it from being "additionally mortgaged", while Rule 7 stops a *mortgaged* property earning at all) |
| **D29** | What happens when a balance outgrows `int` | **Saturate at `INT_MAX`/`INT_MIN`, in `money_round` and nowhere else.** Casting a `double` outside `int`'s range is undefined behaviour in C rather than merely a wrong number, and milestone 4 puts it within reach of ordinary play: **D21** compounds the loan rate with every inflation draw, **D4** compounds the principal every round at that rate, and LK 5 sets no limit on how often a term may be extended. Seed 1 reaches a balance of 1.2 billion by round 384 with all three rules behaving exactly as written. The spec never contemplates a figure that large, and a debt beyond any possible repayment is unaffected by which number is printed beside it — so the decision is not about the value but about the boundary always yielding a valid `int`, with a saturated result acting as a recognisable sentinel rather than an arbitrary wrapped one. The DEBUG guard in `accrue_interest` changed to match: it asserted a ceiling on the premise that real terms cap out in the low hundreds of thousands, which inflation retires, so it now asserts only that a balance which compounds upward never comes back negative |
| **D30** | What counts as one round | **A lap of the board.** A round ends once every solvent player has passed GO since it began, not after one turn each. The PDF never defines the unit: it says only "every complete round" (LK 4 interest, LK 15 property age, LK 25 building condition) and sets the limit at 500 of them, leaving the boundary open. **This supersedes the earlier reading recorded against R2.4.** Players keep taking turns in `order[]` throughout, including any who have already lapped — stopping them would hand the slowest player the most turns, which no rule asks for and nothing benefits from. Bankrupt players are not waited for; Rule 15 has removed them, and a round that waited on someone who will never move again would never end. Termination rests on Rule 13 and **D10** rather than on luck: Jail is the only state that stops a player moving and D10 releases them after the third failed attempt, so a lap takes about six turns and cannot take unboundedly many — asserted by a DEBUG guard rather than assumed. Every timed system already keys off `g->round` under **D19**, so the cadences, loan terms, policy life, property age and effect durations all became laps without a line of change; they still fire at exactly rounds/10, /15 and /20 |
| **D31** | Nothing in the spec lifts a mortgage | **Repay the current mortgage value at the Bank square, one action per landing.** Mortgaging is fully defined — LK 2 prices it, Rule 7 suppresses the rent, R3.3 bars it on loan-locked assets, and **D11** makes it the recovery ladder's second rung — but no rule says how the state ends, which left it permanent: a property mortgaged once earned nothing again and barred its colour group from development under Rule 8 for the rest of the game. The spec's own introduction calls it a "mortgage **liability**", and a liability is something discharged rather than a sale. The price is the **current** `mortgage_value`, the same choke point that priced the advance, so redemption tracks inflation and LK 32's decline exactly as the advance did and no new figure is invented. **No interest accrues meanwhile** — unlike a loan the spec gives a mortgage neither a rate nor a term to accrue over, and inventing one would be a second new rule propping up the first. The venue is the Bank square because that is where the spec already confines debt settlement (**R1.8**, **D19**); redemption competes with LK 5's five actions for the single action a landing allows, and loan business outranks it, since under **D4** a loan compounds every round while a mortgage sits still. Effect on play is large: across eight seeds hotels rose from 3 to 113 and the Aggressive Investor went from building none to leading on them |
| **D32** | §3 requires selling a property but names no price | **Sold to the Bank at the current `square_value`; buildings come down first at D11's 50%.** §3.3 has the Risk Taker "sell lower-value properties to finance premium developments", §3.4 has the Opportunistic Trader "sell properties expected to decrease in value following economic events", and §3.1 forbids it of the Aggressive Investor — which only means anything if the act exists. The Bank buys back at what it sells for, and that symmetry is the argument; the alternative fails on its own terms, since paying `mortgage_value` would make selling strictly worse than mortgaging (same cash, keeps the property) so no player would ever sell and all three bullets would be dead letters. At market value a sale realises no profit by itself but moves capital out of an asset the market is about to mark down, which is exactly what §3.4 describes. LK 3 bars a loan-locked square; a mortgaged one is refused rather than netted off, since **D31** now gives the owner a way to settle that first. A group being developed is never broken for cash — forfeiting Rule 8 costs more than the sale raises. **This corrects an earlier misreading** that sold one level of *development* instead: buildings return at half what they cost, so selling and rebuilding destroyed value every cycle and the board churned at roughly one demolition per construction |
| **D33** | When income tax is assessed, and what "Taxes Due" describes | **Assessed every round, collected on square 4.** Rule 11 makes landing the moment of *payment* — "landing on Income Tax requires payment of the specified tax immediately" — but never says the liability begins there, and the PDF specifies no amount at all, "the specified tax" being a dangling reference. Rule 15 subtracts **Taxes Due** from net worth, a term that has to describe something: a tax existing only during the instant of landing could never be *due*. So the assessment accrues each round into `Player.taxesDue` at **D2′**'s 15% of current cash, and square 4 is where the balance is settled. The rate is not fixed — `econ.incomeTaxPct` drifts with every inflation draw (**D21**) and is scaled at assessment time by `EFF_TAX_MUL`, ×1.5 under LK 24's *Increase Property Tax* — so the same holding is taxed differently as the economy moves. `EFF_TAX_MUL` is read with square −1, an assessment belonging to the player rather than to wherever they are standing. The accumulator saturates for the reason **D29** gives. A landing with nothing outstanding prints `No tax is outstanding.`, §5 giving no template for a case its own model cannot produce. **Measured impact is large and intended:** across twelve seeds tax is collected 175 times averaging LKR 6,641 (largest 43,550), mean cash held falls from roughly 33,000 to under 8,000, and games end in 438 rounds against 1,023 before |
| **D34** | Which clock a duration counts on | **Two clocks, split by who an effect touches.** Supersedes D19's single clock. A **single-player** effect is measured in that player's own laps — a loan matures 20 GO passes after issue, an insurance policy lapses 20 after purchase, an Appendix A card ages by its drawer. A **global** effect, and the 500-round termination limit, count game rounds: inflation, market reviews, disasters, national events, regional cards, regulations, depreciation and building condition. The reasoning is the clarification's own: a player must not be able to delay a consequence relative to everyone else, so anything the whole board experiences stays on the shared clock, while anything one player brought on themselves travels with them. `Player.laps` counts GO passes; `Player.lapsPrev` snapshots it at the head of each round so the registry can age a player-scoped record by what that player actually did. `effect_push` credits back the laps already completed when a record is created mid-round, the same courtesy the tick-before-cadences ordering gives global records. A bankrupt owner's records are dropped — their laps stop, and a duration measured against a stopped clock would never expire |

---

## Definition of done

- [x] `gcc *.c -o monopoly` → zero errors, zero warnings; `gcc -std=c99 -Wall -Wextra -pedantic *.c -o monopoly` also clean.
- [x] `./monopoly` and `./monopoly <seed>` run to completion with no interaction.
- [x] A full 500-round game completes; a game with early bankruptcies ends correctly with the winner block.
- [x] `./monopoly 42` twice produces byte-identical output.
- [x] Console output audited line-by-line against every §5 template.
- [x] No `malloc`, `calloc`, `realloc`, `getline`, linked lists, globals, or `math.h` anywhere in the tree.
- [x] Every per-property price and base rent in a run traces to a row of `assets/Rent.csv`; no source file contains a transcribed copy.
- [x] Editing a price in `Rent.csv` changes the next run without recompiling.
- [x] Running from a directory where the CSV cannot be found exits 1 with a stderr diagnostic and **zero bytes on stdout**.
- [x] A short, duplicated, misnamed, mis-grouped, or non-numeric CSV row is reported with its line number rather than silently accepted.
- [x] All R-items above checked; every D-decision implemented exactly once and cited in a comment.
