#ifndef TYPES_H
#define TYPES_H

/* ------------------------------------------------------------------------
 * Straight to Jail -- an implementation of the MONOPOLY-LK specification
 * (SCS 1301 take-home). Spec: assets/Assignment_1_unlocked.pdf
 * Supplemental per-property values: assets/Rent.csv
 *
 * SPEC-GAP DECISIONS. Full rationale in docs/REQUIREMENTS.md section D.
 * Every one of these is implemented exactly once, at a choke point, and the
 * implementation site carries a comment citing its ID.
 *
 * The three marked SUPERSEDES override the PDF; they come from the
 * lecturer's clarification set, which is later and authoritative.
 *
 * D1  Repair cost     50% of the current construction cost of the buildings
 *                     standing on the property                        [LK 10]
 * D2' Income Tax      SUPERSEDES the PDF. 15% of taxable assets (D16),
 *                     the rate itself market-adjusted; x1.5 under the
 *                     Increase Property Tax regulation               [Rule 11]
 * D3  Coverage        Basic {Fire,Flood} @80%; Comprehensive
 *                     {Fire,Flood,Riot,Vandalism} @100%; Business
 *                     Interruption all perils @100% + 5 rounds of hotel
 *                     rent. Building Collapse and Electrical Failure are
 *                     uncovered below Business Interruption. App E's
 *                     "Earthquake" never occurs             [LK 10, App E]
 * D4  Interest        The Table 9 rate applies EVERY ROUND; the "annual"
 *                     label is ignored as inconsistent with LK 4. The
 *                     issued rate is frozen for the loan's life  [LK 4, 13]
 * D5  Max loan        75% of the mortgage value of properties + railways +
 *                     utilities (LK 2 beats the narrower 1.1.4)      [LK 2]
 * D6' Rounding        SUPERSEDES the PDF. Money is STORED as int; ratio
 *                     and interest arithmetic is computed in double and
 *                     rounded to nearest by money_round(). No math.h
 * D7' Values          SUPERSEDES the PDF. Individual purchase price and
 *                     base rent come from Rent.csv; the group table
 *                     supplies construction cost and mortgage value
 * D8' Tie-break       Only tied players reroll, and the reroll permutes
 *                     only their own positions                      [Rule 2]
 * D9  Valuation proxy "estimated market value" = square_value()    [sec. 3]
 * D10 Jail            After the 3rd failed turn bail is auto-paid; doubles
 *                     have no effect outside jail                  [Rule 13]
 * D11 Debt recovery   sell buildings @50% -> mortgage free assets ->
 *                     bankrupt, assets auctioned. Repaying a loan is NOT
 *                     a rung: that needs the Bank square      [Rule 11, 14]
 * D12 Effects         Permanent effects mutate stored values; temporary
 *                     effects live in the registry and are read at
 *                     access time                          [LK 14, 34, 35]
 * D13 Round order     interest -> default -> condition -> insurance ->
 *                     repairs -> cadences -> tick -> summary -> market
 * D14 Region tags     see REGION_* below                   [LK 18, Table 4]
 * D15 Claims recv.    always 0 -- compensation is credited immediately
 *                                                              [Rule 15]
 * D16 Taxable assets  sum of square_value over the 22 coloured properties
 *                     owned. Buildings, railways and utilities excluded.
 *                     Income Tax takes 15%, Community Dev. Fund takes 10%
 * D17 Square 2        its own SQ_COMMUNITY type -- it draws no card. Card
 *                     squares are 7, 22 and 36 only              [Table 1]
 * D18 Value bases     individual price -> purchase, market value, rent
 *                     basis, renovation, tax base, auction opening.
 *                     group base price -> mortgage value only, i.e. loan
 *                     capacity and debt-recovery proceeds
 * D19 One clock       everything counts game rounds. A loan matures at
 *                     issuedRound + 20; property age is
 *                     round - purchasedRound and starts at purchase
 * D20 Single claim    a payout consumes the policy, whatever rounds remain
 * D21 Interest rate   seeds at Table 9 Stable 8%. Inflation applies LK 14
 *                     multiplicatively. Large event shifts are relative
 *                     (EFF_INTEREST_MUL); the explicit +/-2% adjustments
 *                     are percentage points (EFF_INTEREST_ADD), since a
 *                     relative 2% rounds to a no-op. ADD applies first
 * D22 Loan pledge     only the minimum set of assets, highest mortgage
 *                     value first, whose 75% LTV covers the amount
 * D23 Auction order   starts with the player immediately after the current
 *                     player, then clockwise. The decliner may bid
 * D24 Luxury tax      charged once when the regulation activates, at 25%
 *                     of each hotel property's value including buildings
 * D25 Anti-Spec. Act  the <=3 undeveloped cap alone; enforcing it strictly
 *                     makes LK 24's five-round clause unreachable
 * D26 Output spacing  blocks are emitted compact. The gaps in the PDF are
 *                     LaTeX spacing between verbatim chunks, not content
 * ------------------------------------------------------------------------ */

#endif /* TYPES_H */
