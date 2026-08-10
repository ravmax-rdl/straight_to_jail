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
