 Partially compliant. The program is a complete autonomous MONOPOLY-LK simulation: it builds with the mandated gcc *.c -o
 monopoly line, uses the required file split, and implements most of Rules 1–15, Rule-LK 1–36, Appendices A–E, and the §5
 message set. Several confirmed defects remain. Other PDF mismatches are intentional, documented clarifications (the
 D-decisions in docs/REQUIREMENTS.md).

 This review treats the PDF as primary and the D-decisions as accepted overrides where they conflict. Findings below are
 things the code gets wrong against the PDF and against its own written decisions, plus the PDF mismatches that a marker
 using only the assignment would still mark.

 ────────────────────────────────────────────────────────────────────────────────

 Requirements checklist

 Legend: Pass / Partial / Fail / N/V (not verifiable from the PDF or not observed in runs).

 ### §4 Program requirements

 ┌─────┬────────────────────────────────────────────────────────────┬───────────────────────────────────────────────────┐
 │ ID  │ Requirement                                                │ Verdict                                           │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P1  │ Four autonomous players, no interaction after launch       │ Pass                                              │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P2  │ Decisions from assigned behaviours                         │ Partial (see §3)                                  │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P3  │ PRNG (srand/rand)                                          │ Pass                                              │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P4  │ Monetary values stored as int                              │ Pass (ratios use double only in money_round /     │
 │     │                                                            │ apply_pct / pct_of; D6′)                          │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P5  │ Board spaces as data structures                            │ Pass                                              │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P6  │ Independent player financial records                       │ Pass                                              │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P7  │ Property tracks owner, development, insurance, mortgage,   │ Pass                                              │
 │     │ depreciation, valuation                                    │                                                   │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P8  │ End: one solvent player or 500 rounds                      │ Pass (code path exists; tested games ended by     │
 │     │                                                            │ bankruptcy)                                       │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P9  │ Winner by net worth / last solvent                         │ Pass                                              │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P10 │ Files: types.h, board.c, players.c, finance.c, events.c,   │ Pass                                              │
 │     │ game.c, main.c                                             │                                                   │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P11 │ gcc *.c -o monopoly with no errors                         │ Pass (also clean under -std=c99 -Wall -Wextra     │
 │     │                                                            │ -pedantic)                                        │
 ├─────┼────────────────────────────────────────────────────────────┼───────────────────────────────────────────────────┤
 │ P12 │ Avoid globals; dynamic allocation only if justified        │ Pass (GameState on main’s stack; no malloc)       │
 └─────┴────────────────────────────────────────────────────────────┴───────────────────────────────────────────────────┘

 ### Board and traditional rules

 ┌─────┬──────────────────────────────────────────────┬─────────────────────────────────────────────────────────────────┐
 │ ID  │ Requirement                                  │ Verdict                                                         │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T1  │ 40 squares, indices 0–39, Table 1            │ Pass                                                            │
 │     │ names/types                                  │                                                                 │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T2  │ Eight colour groups, membership as specified │ Pass                                                            │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T3  │ Appendix B house / hotel / mortgage values   │ Pass                                                            │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T4  │ Individual purchase prices and base rents    │ Pass via assets/Rent.csv (D7′; App B purchase column is not     │
 │     │                                              │ charged)                                                        │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T5  │ Railways: not developable/insurable; rent    │ Pass                                                            │
 │     │ 250/500/1000/2000                            │                                                                 │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T6  │ Utilities: 4× / 10× dice; mortgageable; not  │ Pass                                                            │
 │     │ developable                                  │                                                                 │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T7  │ Rule 1: LKR 30,000, nothing else             │ Pass                                                            │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T8  │ Rule 2: highest starts, then clockwise       │ Fail (Finding 1)                                                │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T9  │ Rule 3 eight-step turn                       │ Partial (purchase folded into landing; construction/liquidation │
 │     │                                              │ swapped)                                                        │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T10 │ Rule 4: GO ± LKR 2,000 on pass or land       │ Pass                                                            │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T11 │ Rule 5: buy or immediate auction             │ Pass                                                            │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T12 │ Rule 6 / LK 19–23 auctions                   │ Pass (opening 50% of market value, +250, withdraw permanent,    │
 │     │                                              │ cash cap, Bank keeps if no bids)                                │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T13 │ Rule 7: rent; mortgaged collects 0           │ Pass                                                            │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T14 │ Rules 8–10: monopoly, even build, hotel      │ Pass                                                            │
 │     │ replaces 4 houses                            │                                                                 │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T15 │ Table 6 multipliers 1/2/3/5/7/10             │ Pass                                                            │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T16 │ Rule 11 Income Tax                           │ Partial (charged on landing; amount is D2′ 15% of cash — PDF    │
 │     │                                              │ never states a figure)                                          │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T17 │ Rule 12 Go To Jail, no GO money              │ Pass                                                            │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T18 │ Rule 13 jail: bail 300 / doubles / 3 turns   │ Pass (which exit is a strategy choice; D10)                     │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T19 │ Rule 14 bankruptcy                           │ Pass (D11 ladder: sell buildings at 50% → mortgage → bankrupt + │
 │     │                                              │ auction)                                                        │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T20 │ Rule 15 net worth formula                    │ Pass vs the numbered formula (D28 drops the intro’s             │
 │     │                                              │ mortgage-liability term)                                        │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T21 │ Square 2 Community Development Fund          │ Partial (D17: levy, not an App A card, despite Table 1 typing   │
 │     │                                              │ it Event)                                                       │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T22 │ Free Parking                                 │ Pass (no effect; PDF is silent)                                 │
 ├─────┼──────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────┤
 │ T23 │ Doubles extra turn outside jail              │ Pass (not in the PDF; correctly omitted)                        │
 └─────┴──────────────────────────────────────────────┴─────────────────────────────────────────────────────────────────┘

 ### MONOPOLY-LK extensions

 ┌─────────┬───────────────────────────────────────┬────────────────────────────────────────────────────────────────────┐
 │ ID      │ Requirement                           │ Verdict                                                            │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L1–L3   │ Loans: collateral, 75% LTV, lock, one │ Pass                                                               │
 │         │ loan                                  │                                                                    │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L4      │ Interest every complete round,        │ Partial (D4/D34: borrower’s laps, not game rounds; Table 9         │
 │         │ 20-round term                         │ “annual” ignored)                                                  │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L5      │ Bank: one of obtain / repay part /    │ Pass (D42: no refinance; §1.1.4’s refinance is treated as          │
 │         │ full / extend / increase              │ increase)                                                          │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L6–L7   │ Default / foreclosure / possible      │ Pass (foreclosed assets then auctioned, LK 19)                     │
 │         │ bankruptcy                            │                                                                    │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L8–L9   │ Three insurance tiers, 20 rounds,     │ Partial (premiums/coverage match; §1.2 “renew” is refused — D36)   │
 │         │ warn at 3                             │                                                                    │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L10–L11 │ Disaster every 10 rounds;             │ Pass (always fires if anything is developed; “may occur” is read   │
 │         │ compensation; no rent until repair    │ as a cadence)                                                      │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L12–L14 │ Inflation set {−3,0,2,5,8,12}%; New = │ Partial (property prices, building costs, property rents, derived  │
 │         │ Old × (1+r)                           │ premiums/repairs yes; railway/utility rents no — Finding 5)        │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L16–L17 │ Age, 1%/5 rounds after 50, cap 30%;   │ Pass on depreciation/age; “increases rental” is unquantified       │
 │         │ renovate 10%                          │                                                                    │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L18     │ Eight national events, listed effects │ Partial (Finding 2)                                                │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L19–L23 │ Auctions                              │ Pass                                                               │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L24     │ Eight regulations                     │ Partial (7 of 8 exact; Anti-Speculation cap yes, 5-round deadline  │
 │         │                                       │ no — Finding 6)                                                    │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L25–L27 │ Condition 100%−2%/round; Table 3      │ Pass                                                               │
 │         │ bands; maintenance costs              │                                                                    │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L28–L29 │ Structural damage after >20 rounds;   │ Partial (Finding 4)                                                │
 │         │ 25% rebuild                           │                                                                    │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L30–L34 │ Market boom/decline percentages,      │ Pass                                                               │
 │         │ consecutive-event bar, 30-round       │                                                                    │
 │         │ cooldown, cumulative stack            │                                                                    │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ L35–L36 │ 12 regional cards, 15 rounds, revert; │ Pass (boom/decline titles use colour groups, not the sample’s      │
 │         │ market-conditions block               │ “Southern Province”)                                               │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ A       │ 20-card deck, draw on Event squares,  │ Pass (squares 7/22/36 only)                                        │
 │         │ bottom of deck                        │                                                                    │
 ├─────────┼───────────────────────────────────────┼────────────────────────────────────────────────────────────────────┤
 │ C/D/E   │ Rent tables, Table 9 rates, insurance │ Pass with D3/D21 readings                                          │
 │         │ premiums                              │                                                                    │
 └─────────┴───────────────────────────────────────┴────────────────────────────────────────────────────────────────────┘

 ### §3 strategies

 ┌──────────────────┬───────────────────────────────────────────────────────────────────────────────────────────────────┐
 │ Player           │ Verdict                                                                                           │
 ├──────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────┤
 │ Aggressive       │ Pass on buy/bid/build/loan/insurance/no-sell; group and Galle Face/Nuwara Eliya priority only in  │
 │ Investor         │ auction reserves, not on the landed square                                                        │
 ├──────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────┤
 │ Conservative     │ Partial — 50% cash, below-market bids, repay at Bank, Comprehensive, no hotels while indebted,    │
 │ Banker           │ recession freeze, renovate >10% all present; “prefers railways and utilities” has no code (D44)   │
 ├──────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────┤
 │ Risk Taker       │ Pass under D42 (increase = “refinance”); hotels, max loan, bid-to-cash, insure-after-loss, sell   │
 │                  │ cheap for premium                                                                                 │
 ├──────────────────┼───────────────────────────────────────────────────────────────────────────────────────────────────┤
 │ Opportunistic    │ Partial — appreciation test, auction-prefer, return-vs-cost loans, subsidy/inflation              │
 │ Trader           │ construction, renovate >15%, sell into declines; balanced portfolio and most regulation-driven    │
 │                  │ allocation are missing                                                                            │
 └──────────────────┴───────────────────────────────────────────────────────────────────────────────────────────────────┘

 ### §5 output

 ┌──────────────────────────────────────────────────────────────────────────────────────────┬───────────────────────────┐
 │ Item                                                                                     │ Verdict                   │
 ├──────────────────────────────────────────────────────────────────────────────────────────┼───────────────────────────┤
 │ Pre-game header, first line MONOPOLY-LK Simulation                                       │ Pass                      │
 ├──────────────────────────────────────────────────────────────────────────────────────────┼───────────────────────────┤
 │ Roll-off, dice, move, GO, buy, rent, house, hotel, loan, repay, default, insurance,      │ Pass (wording present;    │
 │ disaster, auction, event, regulation, depreciation, expiry, bankruptcy, round summary,   │ extra blank lines per     │
 │ market block, GAME OVER                                                                  │ D26)                      │
 ├──────────────────────────────────────────────────────────────────────────────────────────┼───────────────────────────┤
 │ Thousands separators                                                                     │ Pass (fmt_lkr)            │
 ├──────────────────────────────────────────────────────────────────────────────────────────┼───────────────────────────┤
 │ Turn-order list                                                                          │ Fail vs the §5 sample     │
 │                                                                                          │ (Finding 1)               │
 └──────────────────────────────────────────────────────────────────────────────────────────┴───────────────────────────┘

 ────────────────────────────────────────────────────────────────────────────────

 Findings

 ### 1. High — Turn order is rank-by-roll, not “highest then clockwise”

 Violated: Rule 2; §5 “Determining the First Player”.

 Rule 2 says the highest total begins, then “Play proceeds clockwise thereafter.” The §5 sample is unambiguous:

 - Rolls: Aggressive 9, Conservative 6, Risk Taker 11, Opportunist 5
 - Order printed: Risk Taker, Opportunistic Trader, Aggressive Investor, Conservative Banker (P3 → P4 → P1 → P2)

 That is seating order clockwise from the winner, not remaining rolls (which would be Risk, Aggressive, Conservative,
 Opportunist).

 Actual: determine_order sorts all four by dice descending.

 ```132:164:game.c
void determine_order(GameState *g)
{
    /* ... each player rolls, then sort_slice on scores ... */
    printf("%s will begin the game.\n", g->players[g->order[0]].name);
    printf("Turn order:\n");
    for (i = 0; i < NUM_PLAYERS; i++) {
        printf("%s\n", g->players[g->order[i]].name);
    }
}
 ```

 Seed 42: rolls 7 / 6 / 11 / 4 → printed order Risk, Aggressive, Conservative, Opportunist. Clockwise from Risk Taker
 would be Opportunist, Aggressive, Conservative.

 D8′ / R2.2 document a full ranking; the PDF sample contradicts that. Against the PDF this is a fail.

 Fix: Identify the first player by highest roll (reroll ties among those tied). Then set order[] to seating (first,
 first+1, first+2, first+3) % 4.

 ────────────────────────────────────────────────────────────────────────────────

 ### 2. Medium — Political Unrest drops “Business interruption claims increase”

 Violated: Rule-LK 18.

 Expected: riot probability doubles; hotel rent −50%; business-interruption claims increase.

 Actual: only hotel rent and riot weight:

 ```497:500:events.c
    { "Political Unrest",
      "Hotel rents fall by half.",
      { { EFF_HOTEL_RENT_MUL, SCOPE_GLOBAL, 0, -50 },
        { EFF_RIOT_RISK, SCOPE_GLOBAL, 0, +100 } }, 2 }
 ```

 No D-decision covers the BI-claim clause. Magnitude is unspecified, but the effect is simply absent (seed 1 fired
 Political Unrest three times).

 Fix: Add a timed multiplier on Business Interruption payouts (even a documented percentage, e.g. +50%) and apply it in
 settle_claim.

 ────────────────────────────────────────────────────────────────────────────────

 ### 3. Medium — Recession / Stock Market Boom charge interest twice

 Violated: Rule-LK 18 + D21’s own “do not charge the condition twice.”

 Expected (D21): new loans take Table 9’s row (Recession 15%, Boom 5%). The ±15%/−10% event shift applies to
 already-issued loans whose frozen rate did not come from that row.

 Actual: prevailing_condition already returns ECON_RECESSION / ECON_BOOM so grant_loan stores 15% or 5%, and the event
 still pushes EFF_INTEREST_MUL ±15%/−10%, which accrue_interest applies to every live loan, including one issued during
 the event:

 ```763:777:finance.c
        rate = pl->loan.ratePct + effect_modifier(g, EFF_INTEREST_ADD, -1, i);
        rate = apply_pct(rate, effect_modifier(g, EFF_INTEREST_MUL, -1, i));
 ```

 Seed 42 issued a loan at Interest Rate : 5% during Stock Market Boom; that loan then compounds at about 4.5% (5 × 0.90),
 not 5%. A recession loan would compound at about 17.25% (15 × 1.15), not 15%.

 current_loan_rate correctly omits EFF_INTEREST_MUL so the LK 36 block prints Table 9. Accrual does not.

 Fix: Apply EFF_INTEREST_MUL only when the issued ratePct is not already the Table 9 row for the active event (or stop
 pushing INTEREST_MUL for those two events and let Table 9 be the whole of the new-loan rate, with MUL reserved for loans
 issued earlier).

 ────────────────────────────────────────────────────────────────────────────────

 ### 4. Medium — Converting to a hotel wipes building decay (LK 28)

 Violated: Rule-LK 28; D38 (construction must not reset neglect).

 Expected: twenty-plus rounds without maintenance → structural damage (−15% value, −25% rent, +50% upkeep), regardless of
 later building. Rule 10 says a hotel replaces four houses, so it is the same fabric.

 Actual: after a hotel upgrade, only the new hotel’s condition is written, at 100%:

 ```820:841:game.c
        if (hotel) {
            s->houses = 0;
            s->hotel  = true;
            /* ... */
        } else {
            s->houses++;
            /* ... */
        }
        s->cond[s->hotel ? 0 : s->houses - 1] = 100;
 ```

 Aggressive Investor and Risk Taker hotel as soon as Rule 10 allows, so four decaying houses become a brand-new 100%
 hotel and LK 28 never fires. Seeds 42 and 7: 0 structural-damage messages. Seed 1 (229 rounds): 6, on properties that
 stayed as houses.

 Fix: On hotel conversion, set cond[0] to the average (or minimum) of the four houses being replaced, not 100. Do not
 treat the hotel as a newly built structure for LK 25/28.

 ────────────────────────────────────────────────────────────────────────────────

 ### 5. Medium — Inflation does not move railway or utility rents

 Violated: Rule-LK 13 (“Inflation modifies … Rental values”).

 Expected: railway 250/500/1000/2000 and utility 4×/10× dice scale by New = Old × (1 + inflation) like other rents.

 Actual: draw_inflation updates price, baseRent, houseCost, hotelCost, mortgageValue. Stations store baseRent = 0.
 square_rent uses a fixed table and dice multipliers:

 ```1025:1036:board.c
    case SQ_RAILWAY:
        rent = RAILWAY_RENT[count_owned(g, s->owner, SQ_RAILWAY) - 1];
        /* ... event multipliers only ... */
    case SQ_UTILITY:
        rent = diceTotal * (count_owned(g, s->owner, SQ_UTILITY) == 2
                            ? UTILITY_MULT_BOTH : UTILITY_MULT_ONE);
 ```

 Seed 42 still paid exact 250 / 500 / 1,000 / 2,000 when no event multiplier was active; 1,800 and 3,800 appeared only
 from Fuel Crisis / recession multipliers. Colour-property rents had already drifted.

 Fix: Keep running railway/utility rent bases on the square (or inflate the tables in draw_inflation) so LK 14 hits them
 too.

 ────────────────────────────────────────────────────────────────────────────────

 ### 6. Medium — Anti-Speculation Act: no “within five rounds” rule

 Violated: Rule-LK 24 last bullet.

 Expected: at most three undeveloped properties; extra purchases must be developed within five rounds.

 Actual: cap of 3 gates buy and auction; holding more than 3 forces decide_build to ignore personality. There is no
 five-round clock and no penalty if the player cannot pay. D25 records this as intentional (players.c 116–125).

 If a marker wants the PDF clause, add a per-property deadline and a stated consequence (forced development, fine, or
 forced sale).

 ────────────────────────────────────────────────────────────────────────────────

 ### 7. Low — Conservative Banker never prefers railways/utilities

 Violated: §3.2 “Prefers railway stations and utility companies due to their predictable income.” D44 states the bullet
 has no expression.

 buy_conservative is only “≥50% cash remains” and “not in recession.” Seed 1 purchases: NWSDB, Jaffna Town, Pettah, Kandy
 City, Maharagama — colour properties with no ranking toward stations.

 Fix: When several unowned options are not in play (this decision is per landing), still refuse colour property while a
 cheaper station/utility would have been the same landing — or, more usefully, bid more aggressively on
 stations/utilities than on colour groups.

 ────────────────────────────────────────────────────────────────────────────────

 ### 8. Low — Increasing a loan rewrites the frozen rate on the whole principal

 Violated: Rule-LK 13 “Existing loan rates remain unchanged.”

 increase_loan sets pl->loan.ratePct = current_loan_rate(g) on the combined balance (finance.c 591). The old principal
 starts compounding at today’s Table 9 row. Seed 1 increased a loan seven times.

 Fix: Keep the original ratePct for the old principal, or blend by weighted average; only the new slice should take the
 current rate.

 ────────────────────────────────────────────────────────────────────────────────

 ### 9. Low — Opportunistic Trader’s portfolio / regulation rules are thin

 Violated: §3.4 “balanced portfolio…”, “Prioritizes investments benefiting from current government regulations.”

 Construction does react to Housing Subsidy and positive inflation. There is no mix target across colour / railway /
 utility, and no use of Railway Modernization, Electricity Tariff, etc., when deciding what to buy or bid.

 ────────────────────────────────────────────────────────────────────────────────

 Testing performed

 ┌───────────────────────────────────────────┬──────────────────────────────────────────────────────────────────────────┐
 │ Test                                      │ Result                                                                   │
 ├───────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────┤
 │ gcc *.c -o monopoly                       │ Exit 0, empty stderr                                                     │
 ├───────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────┤
 │ gcc -std=c99 -Wall -Wextra -pedantic *.c  │ Exit 0, empty stderr                                                     │
 │ -o monopoly                               │                                                                          │
 ├───────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────┤
 │ gcc … -DDEBUG , ./debug 42                │ Exit 0, no invariant aborts                                              │
 ├───────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────┤
 │ ./monopoly 42 twice                       │ Byte-identical stdout                                                    │
 ├───────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────┤
 │ ./monopoly 42 /no/such/Rent.csv           │ Exit 1, 0 bytes on stdout, diagnostic on stderr                          │
 ├───────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────┤
 │ Seed 42                                   │ 78 rounds, Opportunistic Trader wins, 3 bankruptcies, 121 auctions, 7    │
 │                                           │ disasters, 129 event cards                                               │
 ├───────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────┤
 │ Seed 1                                    │ 229 rounds, Risk Taker wins; structural damage, renovation, rebuild,     │
 │                                           │ Anti-Speculation all observed                                            │
 ├───────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────┤
 │ Seed 7                                    │ 66 rounds, Aggressive Investor wins                                      │
 ├───────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────┤
 │ §5 strings in seed 42                     │ All required message types present; first line is MONOPOLY-LK Simulation │
 └───────────────────────────────────────────┴──────────────────────────────────────────────────────────────────────────┘

 Uncovered / rare in these seeds: Luxury Property Tax (implemented, never rolled while a hotel owner landed on square 4),
 a full 500-round no-bankruptcy game. Finding 3 is confirmed in seed 42’s boom-rate loan plus the accrual code.

 Edge cases that were exercised: jail doubles / bail / three-turn release; loan default; empty auction (“No bids
 received”); CSV missing; INT_MAX saturation path exists in money_round / credit (not hit in these seeds).

 ────────────────────────────────────────────────────────────────────────────────

 Assumptions and ambiguities (not scored as bugs)

 These are places the PDF is silent or self-contradictory and the code picks a documented reading:

 - D2′ Income Tax = 15% of current cash. PDF only says “the specified tax.” Seed 42 charged LKR 4,500 on LKR 30,000.
 - D7′ / Rent.csv Per-property prices; App B group purchase price is not what buyers pay.
 - D17 Square 2 levies 10% of colour-property value; it does not draw App A.
 - D30 / D34 A “round” is a lap of the board (every solvent player passing GO). Cadences (10/15/20) and the 500-round cap
   are therefore much coarser than “everyone takes one turn.” Jail stretches a round and gives opponents extra laps. If a
   marker uses the usual Monopoly “round,” this is a large deviation.
 - D4 Table 9 rates compound every lap, not annually.
 - D6′ Percentage math in double, rounded at the boundary (PDF §4 says integer calculations).
 - D31 Mortgages can be redeemed at the Bank (PDF never says how a mortgage ends).
 - D36 No renewal of a live policy, despite §1.2 “purchase or renew.”
 - D24 Luxury tax is paid on the Income Tax square while the regulation is active, not as a board-wide “annual” sweep.
 - D3 Building Collapse and Electrical Failure are uncovered except by Business Interruption; App E “Earthquake” never
   occurs.
 - D28 Net worth follows Rule 15 (no mortgage-liability term).
 - D42 No refinance action; Risk Taker “refinances” via Increase.
 - Railway/utility list price LKR 1,500 / mortgage 750 — not in the PDF.
 - LK 10 “may occur every ten rounds” is implemented as always, if any developed property exists.
 - Heavy Monsoon “premiums increase” has no percentage; code uses +20%.
 - LK 17 “increases rental” has no magnitude; code restores condition (which is really LK 27/29).
 - App A intro “effects … for 15 rounds” vs table durations of 5/3/2: code uses the table.
 - Repair cost (D1) = 50% of current building construction cost — PDF never prices repairs.
 - LK 36 sample “Southern Province (+20%)” vs Rule-LK 30–33 colour-group booms: code prints the colour group and the
   value multiplier (+20%).
 - Condition for Table 3 when several houses exist: average (D37).
 - No extra turn on doubles outside jail (PDF does not require one).
 - Rule 3 lists construction then financial transactions; the code sells first so Risk Taker proceeds can fund the same
   turn’s hotel (game.c 942–943).

 ────────────────────────────────────────────────────────────────────────────────

 Bottom line

 For a marker who accepts the written clarifications, this is a strong, mostly complete submission with a handful of real
 logic bugs (interest double-count, hotel condition reset, railway/utility rents frozen under inflation, missing
 Political Unrest BI clause).

 For a marker who uses only assets/Assignment_1_unlocked.pdf, add the turn-order mismatch (Finding 1), round = lap, 15%
 income tax, square 2 not drawing cards, and no insurance renewal as specification deviations.

 Highest-priority fixes: Finding 1 (visible in every run’s second block), Finding 3 (silent money error), Finding 4 (LK
 28 almost never triggers for the two builders).

 No code was changed. I can implement those three first if you want.