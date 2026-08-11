/* finance.c -- money.
 *
 * This file owns the boundary between arithmetic and stored balances. Spec
 * section 4 requires monetary values to be integers; the lecturer's
 * clarification requires interest and similar ratios to be computed in
 * floating point and rounded. D6' reconciles the two: money is STORED as
 * int, ratios are COMPUTED as double, and exactly one function converts
 * between them.
 *
 * The three helpers below are the only functions in the entire program that
 * are permitted to contain a double. Milestone 6 greps for violations.
 */

#include <stdio.h>
#include <stdlib.h>   /* abort(), in the DEBUG auction guard only */

#include "types.h"

/* D6'. Rounds half away from zero.
 *
 * Deliberately not round() from math.h: that can require linking -lm, and
 * spec section 4 mandates the build line 'gcc *.c -o monopoly', which does
 * not supply it. A program that needs an extra flag to link has failed R0.2
 * regardless of how correct its arithmetic is. See R0.9.
 */
int money_round(double v)
{
    return (int)(v + (v >= 0.0 ? 0.5 : -0.5));
}

/* Scale a value by a signed percentage: apply_pct(1000, 15) == 1150,
   apply_pct(1000, -15) == 850. This is LK 14's New = Previous x (1 + rate). */
int apply_pct(int value, int percent)
{
    return money_round((double)value * (1.0 + percent / 100.0));
}

/* Take a percentage OF a value: pct_of(1000, 15) == 150. Used wherever a
   rule charges a fraction rather than adjusting a price. */
int pct_of(int value, int percent)
{
    return money_round((double)value * (percent / 100.0));
}

/* ------------------------------------------------------- money movement -- */
/*
 * Every rupee that moves in this simulation moves through credit or charge.
 * Nothing else assigns to Player.cash. That is what makes the D11 debt
 * recovery ladder a milestone-3 change to one function rather than an audit
 * of forty call sites, and what makes "cash went negative" a question with
 * exactly one place to look.
 */

void credit(GameState *g, int p, int amt)
{
    g->players[p].cash += amt;
}

/* Move amt from p to toPlayer, or to the Bank when toPlayer is -1.
 *
 * Returns false, having moved nothing, if p cannot cover it. Milestone 3
 * replaces that with the D11 ladder -- sell buildings at half, mortgage what
 * is free, and only then declare bankruptcy -- which is why the caller is
 * given a bool rather than this function simply clamping. Until then an
 * unpayable charge is reported and skipped, so no balance can go negative
 * behind our back.
 */
bool charge(GameState *g, int p, int amt, int toPlayer)
{
    if (amt <= 0) {
        return true;
    }
    if (g->players[p].cash < amt) {
        return false;
    }

    g->players[p].cash -= amt;
    if (toPlayer >= 0) {
        g->players[toPlayer].cash += amt;
    }
    return true;
}

/* D16. The Community Development Fund's base and nothing else's: the sum of
 * square_value over the 22 COLOURED properties p owns. Buildings, railways
 * and utilities are excluded.
 *
 * Reading square_value is what makes the clarification's "the 10% will also
 * be affected by the market fluctuations" true for free -- when milestone 4
 * lands booms and declines, this number moves with them and no code here
 * changes.
 */
int total_assets(const GameState *g, int p)
{
    int i, total = 0;

    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].type == SQ_PROPERTY && g->board[i].owner == p) {
            total += square_value(g, i);
        }
    }
    return total;
}

/* ------------------------------------------------------------ the taxes -- */
/*
 * Two squares, two bases, two functions. Parameterising one helper would
 * hide the only interesting thing about them, which is that they tax
 * different things and therefore punish different strategies: the Fund bites
 * landholders, Income Tax bites hoarders -- which is the Conservative
 * Banker's whole plan.
 */

/* Rule 11 as the clarification restates it (D2'): 15% of the player's
 * CURRENT CASH, not of a fixed sum and not of their assets.
 *
 * The rate lives in econ.incomeTaxPct so that milestone 4's inflation can
 * move it the same way it moves the loan rate (D21). EFF_TAX_MUL scales it
 * again at charge time -- x1.5 while the Increase Property Tax regulation is
 * active.
 */
void pay_income_tax(GameState *g, int p)
{
    char b[FMT_BUF];
    int  rate = apply_pct(g->econ.incomeTaxPct,
                          effect_modifier(g, EFF_TAX_MUL, g->players[p].pos, p));
    int  due  = pct_of(g->players[p].cash, rate);

    printf("%s landed on Income Tax.\n", g->players[p].name);
    if (charge(g, p, due, -1)) {
        printf("Income Tax Paid : LKR %s.\n", fmt_lkr(b, due));
        printf("Remaining Balance : LKR %s.\n", fmt_lkr(b, g->players[p].cash));
    } else {
        printf("%s cannot pay LKR %s.\n", g->players[p].name, fmt_lkr(b, due));
    }
}

/* D16/D17. Square 2 is a Community Development Fund square, not a card
 * square: Table 1 types it "Event", but it levies rather than draws, which
 * is why the three card squares are 7, 22 and 36 only.
 *
 * A player owning nothing pays nothing, which is correct -- the levy is on
 * landholding.
 */
void pay_community_fund(GameState *g, int p)
{
    char b[FMT_BUF];
    int  assets = total_assets(g, p);
    int  due    = pct_of(assets, COMMUNITY_PCT);

    printf("%s landed on the Community Development Fund.\n", g->players[p].name);
    printf("Total Property Assets : LKR %s.\n", fmt_lkr(b, assets));
    if (charge(g, p, due, -1)) {
        printf("Contribution Paid : LKR %s.\n", fmt_lkr(b, due));
        printf("Remaining Balance : LKR %s.\n", fmt_lkr(b, g->players[p].cash));
    } else {
        printf("%s cannot pay LKR %s.\n", g->players[p].name, fmt_lkr(b, due));
    }
}

/* ------------------------------------------------------------ auctions -- */

/* LK 19's opening price: half of market value, then LK 32's -25% while the
 * group is in decline. Shared with decide_bid so a strategy can reason about
 * the opening without recomputing it -- one definition, two readers. */
int auction_opening(const GameState *g, int sq)
{
    int open = pct_of(square_value(g, sq), AUCTION_OPEN_PCT);

    return apply_pct(open, effect_modifier(g, EFF_AUCTION_OPEN_MUL, sq, -1));
}

/* LK 19-23. An English ascending auction, run to exhaustion.
 *
 * anchorPlayer is whoever's turn triggered this. D23 puts the first bid with
 * the player immediately after them and proceeds clockwise, so the trigger
 * does not also get first refusal -- though they may bid, since Rule 5's
 * decliner is not excluded.
 *
 * Withdrawal is permanent for this auction (LK 21), which is what
 * guarantees termination: each pass round either raises the price by at
 * least AUCTION_INC or removes a bidder, and both are bounded -- the price
 * by the bidders' cash, the bidders by there being four of them.
 *
 * LK 23: if nobody bids even the opening, the Bank keeps it and nothing is
 * charged. That is a real outcome, not an error.
 */
void run_auction(GameState *g, int sq, int anchorPlayer)
{
    char b[FMT_BUF];
    bool active[NUM_PLAYERS];
    int  i, seat, remaining = 0;
#ifdef DEBUG
    int  guard = 0;
#endif
    int  highBid = 0, highBidder = -1;
    int  opening = auction_opening(g, sq);

    for (i = 0; i < NUM_PLAYERS; i++) {
        active[i] = !g->players[i].bankrupt;      /* LK 19: all solvent    */
        if (active[i]) {
            remaining++;
        }
    }
    if (remaining == 0) {
        return;
    }

    printf("Auction Started.\n");
    printf("Property :\n");
    printf("%s\n", g->board[sq].name);
    printf("Opening Bid :\n");
    printf("LKR %s.\n", fmt_lkr(b, opening));

    /* D23: start immediately after the anchor, then clockwise. */
    seat = (anchorPlayer + 1) % NUM_PLAYERS;

    /* Each pass either removes a bidder or raises the price by at least
       AUCTION_INC, so the loop cannot spin: withdrawals are bounded by four
       players and raises by the bidders' cash. It ends when the last
       standing bidder is also the high bidder, or when everyone has gone. */
    while (remaining > 0) {
        int p      = seat;
        int minBid = (highBidder < 0) ? opening : highBid + AUCTION_INC;
        int bid;

        seat = (seat + 1) % NUM_PLAYERS;

#ifdef DEBUG
        if (++guard > 200) {
            fprintf(stderr, "R%d: auction on square %d did not converge\n", g->round, sq);
            abort();
        }
#endif

        if (!active[p]) {
            continue;
        }
        if (p == highBidder) {
            if (remaining == 1) {
                break;                    /* everyone else has withdrawn   */
            }
            continue;                     /* no bidding against yourself   */
        }

        bid = decide_bid(g, p, sq, minBid);

        /* LK 22 is enforced here rather than trusted to the strategy: a bid
           may never exceed cash on hand, and no loan may be raised
           mid-auction. Anything short of the minimum is a withdrawal, and
           LK 21 makes it permanent for this auction. */
        if (bid < minBid || bid > g->players[p].cash) {
            active[p] = false;
            remaining--;
            printf("%s withdraws.\n", g->players[p].name);
            continue;
        }

        highBid    = bid;
        highBidder = p;
        printf("%s bids LKR %s.\n", g->players[p].name, fmt_lkr(b, bid));
    }

    if (highBidder < 0) {
        printf("No bids received. %s remains with the Bank.\n", g->board[sq].name);
        return;
    }

    charge(g, highBidder, highBid, -1);
    g->board[sq].owner          = highBidder;
    g->board[sq].purchasedRound = g->round;       /* D19                   */
    printf("%s wins the auction.\n", g->players[highBidder].name);
}

/* Rule 15's balance sheet:
 *   cash + property + buildings + railway + utility + claims receivable
 *        - loans - accrued interest - taxes due
 *
 * Built up across three milestones without ever changing this signature.
 * Version 1 is cash alone, because cash is all a player can own yet.
 * Milestone 2 adds owned squares at square_value; milestone 3 adds building
 * book value and subtracts the loan. Claims receivable is permanently 0 by
 * D15 -- LK 10 credits compensation immediately, so nothing is ever
 * outstanding to receive.
 */
int net_worth(const GameState *g, int p)
{
    int i, total = g->players[p].cash;

    /* Every held square at market value -- properties, railways and
       utilities alike. Rule 15 lists them separately but sums them, and
       square_value already knows how to price each type. Buildings are not
       here yet; they arrive with construction in milestone 3, at book
       value, and the loan is subtracted there too. */
    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].owner == p) {
            total += square_value(g, i);
        }
    }

    return total;
}

/* R5.3: render an amount with thousands separators, no currency prefix --
 * callers supply "LKR ". Returns buf so it can be used inline in a printf.
 *
 * buf must hold at least FMT_BUF bytes. The widest possible result is
 * "-2,147,483,648" plus a terminator, which is 15.
 */
const char *fmt_lkr(char *buf, int amount)
{
    char digits[12];
    int  n = 0, i, j = 0;
    bool negative = (amount < 0);

    /* Negate in unsigned arithmetic rather than as int. Two's-complement
       negation of INT_MIN overflows a signed type, and long is only 32 bits
       on Windows, so casting up is not a portable escape. */
    unsigned v = (unsigned)amount;
    if (negative) {
        v = ~v + 1u;
    }

    if (v == 0u) {
        digits[n++] = '0';
    }
    while (v > 0u) {
        digits[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }

    if (negative) {
        buf[j++] = '-';
    }
    for (i = n - 1; i >= 0; i--) {
        buf[j++] = digits[i];
        if (i > 0 && i % 3 == 0) {
            buf[j++] = ',';
        }
    }
    buf[j] = '\0';

    return buf;
}
