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
