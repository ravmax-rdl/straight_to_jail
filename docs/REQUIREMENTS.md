# MONOPOLY-LK — Requirements

Source of truth: [`assets/Assignment_1_unlocked.pdf`](../assets/Assignment_1_unlocked.pdf) (SCS 1301 take-home, due 2026-08-16).
Rule numbers below (`Rule N`, `Rule-LK N`, `§N`, `Table N`, `App X`) cite that document. Each requirement has an ID
that the implementation plan traces back to. Checkboxes track acceptance.

---

## R0 — Build & environment

- [ ] **R0.1** Pure C (C99), standard library only. No external dependencies.
- [ ] **R0.2** `gcc *.c -o monopoly` compiles with **zero errors and zero warnings** (§4). A `Makefile` may exist for convenience (`make` → `straight_to_jail`) but must never be required.
- [ ] **R0.3** At minimum these source files, split exactly by this responsibility (Table 5): `types.h`, `board.c`, `players.c`, `finance.c`, `events.c`, `game.c`, `main.c`.
- [ ] **R0.4** No global variables. A single `GameState` struct on `main`'s stack, passed explicitly. No `malloc` (nothing needs it — all sizes are fixed).
- [ ] **R0.5** All money is `int`. Percentage math per decision **D6**.
- [ ] **R0.6** All randomness via `srand`/`rand`. Seed = `argv[1]` if given, else `time(NULL)` (reproducible runs during development).
- [ ] **R0.7** Zero user interaction after launch (§4).

## R1 — Board & entities

- [ ] **R1.1** 40 squares, indexed 0–39 clockwise (Table 1, reconciled below — the PDF's table extracts misleadingly; this layout is verified against raw cell order):

| # | Square | # | Square | # | Square | # | Square |
|---|--------|---|--------|---|--------|---|--------|
| 0 | GO | 10 | Jail / Just Visiting | 20 | Free Parking | 30 | Go To Jail |
| 1 | Pettah (Brown) | 11 | Nugegoda (Pink) | 21 | Kandy City (Red) | 31 | Jaffna Town (Green) |
| 2 | Event — Community Dev. Fund | 12 | Utility — Ceylon Electricity Board | 22 | Event — National Event Card | 32 | Nallur (Green) |
| 3 | Maradana (Brown) | 13 | Maharagama (Pink) | 23 | Peradeniya (Red) | 33 | Insurance — Ceylinco |
| 4 | Tax — Income Tax | 14 | Kottawa (Pink) | 24 | Katugastota (Red) | 34 | Trincomalee (Green) |
| 5 | Railway — Colombo Fort | 15 | Railway — Kandy | 25 | Railway — Galle | 35 | Railway — Jaffna |
| 6 | Bambalapitiya (L.Blue) | 16 | Negombo (Orange) | 26 | Galle Fort (Yellow) | 36 | Event — National Event Card |
| 7 | Event — National Event Card | 17 | Insurance — Sri Lanka Insurance | 27 | Unawatuna (Yellow) | 37 | Nuwara Eliya (D.Blue) |
| 8 | Wellawatte (L.Blue) | 18 | Katunayake (Orange) | 28 | Utility — NWSDB | 38 | Bank — Bank of Ceylon |
| 9 | Mount Lavinia (L.Blue) | 19 | Ja-Ela (Orange) | 29 | Hikkaduwa (Yellow) | 39 | Galle Face (D.Blue) |

- [ ] **R1.2** 22 properties, 8 colour groups, per-group values (App B) + base rent (decision **D7**):

| Group | Price | House | Hotel | Mortgage | Base rent (D7) |
|-------|-------|-------|-------|----------|----------------|
| Brown | 1,500 | 500 | 2,000 | 750 | 150 |
| Light Blue | 2,500 | 750 | 3,000 | 1,250 | 250 |
| Pink | 3,500 | 1,000 | 4,000 | 1,750 | 350 |
| Orange | 4,500 | 1,250 | 5,000 | 2,250 | 450 |
| Red | 5,500 | 1,500 | 6,000 | 2,750 | 550 |
| Yellow | 6,500 | 2,000 | 8,000 | 3,250 | 650 |
| Green | 8,000 | 2,500 | 10,000 | 4,000 | 800 |
| Dark Blue | 10,000 | 3,000 | 12,000 | 5,000 | 1,000 |

- [ ] **R1.3** Each property tracks: price, mortgage value, base rent, house/hotel cost, owner, mortgage status, insurance status, building count (§1.1.1) — plus extension state: age, building condition, depreciation, market-adjusted value.
- [ ] **R1.4** 4 railways: rent 250/500/1,000/2,000 by count owned by one player (Table 2/7); mortgageable; never developable or insurable.
- [ ] **R1.5** 2 utilities: rent = 4× dice (one owned) or 10× dice (both) (Table 8); mortgageable; never developable.
- [ ] **R1.6** Bank of Ceylon square: exactly **one** loan action per landing — obtain / repay part / repay full / extend / increase (§1.1.4, Rule-LK 5).
- [ ] **R1.7** 2 insurance squares; landing allows purchase/renewal of one policy tier for one property (§1.2).

## R2 — Traditional rules (Rules 1–15)

- [ ] **R2.1** 4 players; each starts with LKR 30,000, nothing else (Rule 1).
- [ ] **R2.2** Turn order by dice roll-off, highest first, tied players reroll (Rule 2).
- [ ] **R2.3** 8-step turn sequence (Rule 3): penalties → roll → move → landing action → purchase → construction → financial transactions → end. (Maintenance happens only in step 1 — Rule-LK 27.)
- [ ] **R2.4** Pass or land on GO → +LKR 2,000 (Rule 4).
- [ ] **R2.5** Unowned purchasable square: buy at list price or it goes **immediately** to auction (Rule 5).
- [ ] **R2.6** Rent owed on owned, unmortgaged property; mortgaged collects nothing (Rule 7).
- [ ] **R2.7** Full colour group = monopoly = only then may build (Rule 8).
- [ ] **R2.8** Even building across group; ≤4 houses; hotel replaces exactly 4 houses; never houses + hotel together (Rules 9–10).
- [ ] **R2.9** Rent multipliers on base rent (Table 6): 1×/2×/3×/5×/7× for 0–4 houses; 10× hotel.
- [ ] **R2.10** Income Tax payable immediately; shortfall → debt recovery (Rule 11, decision **D11**).
- [ ] **R2.11** Go To Jail → straight to square 10, no GO money (Rule 12).
- [ ] **R2.12** Jail exit: pay LKR 300 bail, roll doubles, or wait 3 turns (Rule 13; after the 3rd turn see **D10**).
- [ ] **R2.13** Bankruptcy when liabilities exceed assets: buildings removed, policies expire, loans due, assets transferred (Rule 14, **D11**).
- [ ] **R2.14** Game ends when one solvent player remains or after 500 rounds; then highest net worth wins (Rule 15).
- [ ] **R2.15** Net worth = cash + property + buildings + railway + utility + claims receivable − loans − accrued interest − taxes due (Rule 15; receivable is always 0 per **D15**).

## R3 — MONOPOLY-LK extensions

**Loans (Rule-LK 1–7)**
- [ ] **R3.1** Collateral = properties, railways, utilities only — never buildings (LK 1).
- [ ] **R3.2** Max loan = 75% of total mortgage value of eligible (unmortgaged, not loan-locked) collateral (LK 2, decision **D5**).
- [ ] **R3.3** Loan credits cash instantly; pledged assets become *loan-locked*: no sale/trade/auction/re-mortgage, but still earn rent and may be developed (LK 3).
- [ ] **R3.4** Duration 20 rounds; every round `principal += principal × rate / 100` at the loan's **issued** rate (LK 4, decision **D4**).
- [ ] **R3.5** Default → foreclosure: pledged assets to Bank, buildings demolished, their policies cancelled, debt cleared; player continues, or is bankrupt if nothing remains (LK 6–7).

**Insurance & disasters (Rule-LK 8–11, §1.2, App E)**
- [ ] **R3.6** Three tiers, premiums 5% / 10% / 15% of property value; coverage per decision **D3**. One policy per property; valid 20 rounds; reminder 3 rounds before expiry (LK 8–9).
- [ ] **R3.7** Every 10 rounds a random disaster (Fire, Flood, Riot, Building Collapse, Electrical Failure) may strike one random **developed** property; insured → compensation credited immediately, else owner pays repair cost (LK 10, repair cost per **D1**).
- [ ] **R3.8** Damaged buildings collect no rent until repaired; repairs happen automatically once the owner can afford them (LK 11).

**Inflation (Rule-LK 12–14)**
- [ ] **R3.9** Every 10 rounds draw inflation from {−3, 0, 2, 5, 8, 12}%; apply `New = Old × (1 + rate)` to property prices, building/hotel costs, rents, premiums, repair costs, and the interest rate for **new** loans. Existing loans keep their issued rate.

**Depreciation & renovation (Rule-LK 15–17)**
- [ ] **R3.10** Property age +1 per round; unrenovated properties older than 50 rounds lose 1% per 5 rounds, capped at 30%.
- [ ] **R3.11** Landing on one's own property allows renovation: cost 10% of current market value; restores depreciation, resets age.

**National economic events (Rule-LK 18)**
- [ ] **R3.12** Every 15 rounds, one of the 8 events (Tourism Boom, Fuel Crisis, Heavy Monsoon, Economic Recession, Stock Market Boom, Government Housing Programme, Foreign Investment, Political Unrest) fires, affecting **all** players, with the exact listed effects.

**Auctions (Rule-LK 19–23)**
- [ ] **R3.13** Triggered by declined purchase, bankruptcy liquidation, or foreclosure returns. All solvent players; open at 50% of market value; min increment LKR 250; declining to bid withdraws that player permanently; bids capped by cash on hand; no loans mid-auction; no bids → Bank keeps it.

**Government regulations (Rule-LK 24)**
- [ ] **R3.14** Every 20 rounds one of the 8 regulations (Increase Property Tax +50%, Reduce Loan Interest −2%, Housing Subsidy −30% house cost, Luxury Property Tax 25% hotel maintenance tax, Railway Modernization +25%, Electricity Tariff Revision +20%, Insurance Regulation −15% premiums, Anti-Speculation Act ≤3 undeveloped properties) is selected with the listed effects.

**Building condition & maintenance (Rule-LK 25–29)**
- [ ] **R3.15** Every building starts at 100% condition, −2% per round; rent factor by condition band (Table 3): 90–100→100%, 75–89→90%, 50–74→75%, 25–49→50%, <25→closed (no rent).
- [ ] **R3.16** Maintenance only at the start of the owner's turn: house 5% / hotel 8% of construction cost, restores 100%, any number of buildings if affordable.
- [ ] **R3.17** Maintenance ignored >20 consecutive rounds → structural damage: property value −15%, max rent −25%, future maintenance +50% (LK 28). Damaged building renovation costs 25% of replacement value and restores value/rent/condition (LK 29).

**Dynamic property market (Rule-LK 30–34)**
- [ ] **R3.18** Every 10 rounds: one random group booms (+15% price, +15% mortgage, +25% rent, +10% construction, +20% value), another declines (−15% value, −20% rent, −10% mortgage, −25% auction opening), each lasting 10 rounds; no group repeats consecutively; 30-round cooldown per group; concurrent effects stack cumulatively (LK 34, mechanics per **D12**).

**Regional development cards (Rule-LK 35–36, Table 4)**
- [ ] **R3.19** Every 15 rounds one of the **12** regional cards is drawn, active 15 rounds, affecting only its named squares (region tags per **D14**); on expiry, values revert to the current market-adjusted baseline (LK 35). (Table 4 lists twelve cards: Southern Tourism Boom, Port City Expansion, IT Industry Growth, Northern Development Programme, Tea Export Boom, Airport Expansion, University City Growth, Beach Pollution, Flood Damage, Transport Strike, Electricity Tariff Increase, Water Shortage.)
- [ ] **R3.20** The active market-conditions block prints at the end of **every** round (LK 36, format in R5).

**National Event Card deck (App A)**
- [ ] **R3.21** A 20-card deck; drawn **only** when a player lands on an Event square; top card executes, then returns to the bottom (circular queue); effects apply to the drawing player for 15 rounds, on top of the per-card durations (e.g. "double rent for 5 rounds"). This deck is a *separate system* from R3.12.

## R4 — Player strategies (§3)

Each turn a player evaluates every legal action and executes the one best fitting its strategy. Full behaviour lists in §3.1–3.4; proxy formulas for the judgment calls in **D9**.

- [ ] **R4.1 Aggressive Investor** — always buys (if a future rent remains payable); always bids, up to 120% of estimated value; completes groups first; max houses immediately, hotels ASAP; borrows whenever it lifts projected rent; repays only when cash > 2× loan; Basic insurance on houses, Comprehensive on hotels; never sells voluntarily; covets Galle Face & Nuwara Eliya.
- [ ] **R4.2 Conservative Banker** — buys only if ≥50% cash remains; bids only below market value; loans only to stave off bankruptcy, repaid at every Bank visit; Comprehensive on every developed property; no hotels while indebted; prefers railways/utilities; no investments during recessions; renovates at >10% depreciation; biggest cash reserve.
- [ ] **R4.3 Risk Taker** — buys everything possible; always borrows the max, refinances often; bids until cash is gone; hotels as early as possible; insures only after a loss; ignores depreciation until forced; sells cheap assets to fund premium ones; invests through downturns.
- [ ] **R4.4 Opportunistic Trader** — buys only when projected appreciation exceeds construction cost; prefers discounted auctions; borrows only when return beats cost; Comprehensive only on high-value developments; delays construction during inflation, accelerates under housing subsidies; renovates at >15% depreciation; sells ahead of declines; balanced portfolio.

## R5 — Output (§5)

- [ ] **R5.1** Every §5 message reproduced with exact wording, punctuation, and line breaks: pre-game header, roll-off, dice roll, movement, passing GO, purchase, rent, house construction, hotel upgrade, loan obtained/repaid/defaulted, insurance purchase, disaster + claim, full auction sequence, economic event, government regulation, depreciation, insurance-expiry warning, bankruptcy.
- [ ] **R5.2** All amounts formatted with thousands separators: `LKR 12,300`.
- [ ] **R5.3** End of every round: the `Round N Summary` block (per player: Cash, Net Worth, Properties, Hotels, Outstanding Loan — `None` when zero) with `=`/`-` rule lines exactly as §5.
- [ ] **R5.4** End of every round: the Rule-LK 36 `Current Market Conditions` block (Market Boom, Market Decline, Regional Development, Inflation, Current Loan Interest — each with rounds remaining).
- [ ] **R5.5** End of game: `GAME OVER` block with winner, total cash, total property value, outstanding loans, net worth.

## D — Decisions (spec gaps: choose once, document in `types.h`, apply everywhere)

The spec is silent or self-contradictory on each point below. Proposed defaults are pre-loaded so building can start immediately; each carries a comment citing its rule when implemented.

| ID | Gap | Proposed default |
|----|-----|------------------|
| **D1** | Repair cost never quantified (compensation is defined as % of it) | Repair cost = **50% of current construction cost of the buildings on the property** (houses × house cost + hotel cost if hotel) — damage is to buildings, scales with development, tracks inflation |
| **D2** | Income Tax amount never stated | Base **LKR 1,000**, inflation-adjusted; ×1.5 while *Increase Property Tax* regulation active |
| **D3** | Peril coverage inconsistent; two disasters covered by no tier | Literal: Basic {Fire, Flood} @80%; Comprehensive {Fire, Flood, Riot, Vandalism} @100% (App E's "Earthquake" ignored — never occurs); Business Interruption (hotel properties only) covers **all** perils @100% + 5 rounds of hotel rent. Building Collapse & Electrical Failure are uncovered by Basic/Comprehensive — owner pays |
| **D4** | Table 9 says "annual" rate but Rule-LK 4 compounds per round | Follow LK 4 literally: `principal × rate% ` added **every round**; the "annual" label is ignored as inconsistent. Issued rate frozen for the loan's life (LK 13); rate-moving events/regulations shift only the current rate for **new** loans |
| **D5** | Max-loan base stated two ways (§1.1.4 vs LK 2) | Rule-LK 2 wins: 75% of mortgage values of all eligible collateral (properties + railways + utilities), unmortgaged and not already loan-locked |
| **D6** | Rounding convention (mandated to be documented) | All percentage math: `v * (100 + p) / 100` in `int`, truncation toward zero; percentages stored as integer `pct` |
| **D7** | Base rent never quantified (Table 6 multiplies it; App B has no rent column) | Base rent = **10% of group purchase price** (see R1.2 table) |
| **D8** | First-player tie handling detail | Only tied players reroll, repeatedly, until strict order exists (Rule 2 literal) |
| **D9** | Strategy judgment calls need formulas | "Estimated market value" = current market-adjusted value; Aggressive bid cap = 120% of it; Conservative bids strictly below it; Opportunistic "projected appreciation" = value × (sum of active positive group/region modifiers − active negative), buys when that exceeds the group's house cost |
| **D10** | Jail after 3 turns; doubles outside jail | After the 3rd failed turn, bail is auto-paid and the player moves normally. Doubles have no extra effect outside jail (spec is silent — no extra turns, no triple-doubles rule) |
| **D11** | "Normal debt recovery" and bankruptcy asset transfer never defined | On any unpayable charge: 1) sell buildings back at 50% of construction cost, 2) mortgage non-locked assets at mortgage value, 3) still short → bankrupt: creditor receives remaining cash; assets are auctioned (LK 19 trigger list); unsold assets stay with the Bank |
| **D12** | How permanent and temporary effects compose (LK 14 vs LK 34–35) | **Permanent** effects (inflation, permanent event-card value changes) mutate stored values via LK 14. **Temporary** timed effects (booms/declines, regional cards, national events, event cards, regulations) are read-time multipliers keyed off active-effect state, applied multiplicatively in a fixed order — expiry removes the multiplier, which automatically yields LK 35's "revert to market-adjusted baseline" |
| **D13** | Order of end-of-round processing (numbers drift if inconsistent) | After the last turn: loan interest → default check → age +1 / condition −2 → insurance tick → auto-repairs → cadence events (5: depreciation; 10: inflation, market review, disaster; 15: national event, regional card; 20: regulation) → effect-duration tick → round summary → market conditions block |
| **D14** | Events name regions the spec never maps to squares | Tag table: *southern coastal* = {26, 27, 29}; *coastal* = southern + {6, 8, 9, 16, 34}; *low-lying coastal* = coastal; *commercial* = {1, 3, 39}; *NWSDB-surrounding* = {26, 27, 29} |
| **D15** | "Insurance claims receivable" in net worth | Always 0 — LK 10 credits compensation immediately, and Business-Interruption lost rent is paid as an immediate lump sum |

## Definition of done

- [ ] `gcc *.c -o monopoly` → zero errors, zero warnings; `gcc -Wall -Wextra *.c -o monopoly` also clean.
- [ ] `./monopoly` and `./monopoly <seed>` run to completion with no interaction.
- [ ] A full 500-round game completes; a game with early bankruptcies ends correctly with the winner block.
- [ ] Console output audited line-by-line against every §5 template.
- [ ] All R-items above checked; every D-decision implemented exactly once and cited in a comment.
