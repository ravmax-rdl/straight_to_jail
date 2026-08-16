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

#include <limits.h>   /* INT_MAX, in the DEBUG principal guard only */
#include <stdio.h>
#include <stdlib.h>   /* abort(), in the DEBUG guards only */

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
    double r = v + (v >= 0.0 ? 0.5 : -0.5);

    /* Saturate rather than convert out of range. Casting a double outside
       int's range is undefined behaviour in C, not merely a wrong number,
       and milestone 4's inflation puts it within reach of ordinary play:
       D21 compounds the loan rate with every draw, D4 compounds the
       principal every round at that rate, and LK 5 sets no limit on how
       often a term may be extended. Seed 1 reached a balance of 1.2 billion
       by round 384 that way.

       A debt that large is unpayable whatever number is printed beside it,
       so the value is not what matters here -- what matters is that the D6'
       boundary always yields a valid int, and that a saturated result is a
       recognisable sentinel rather than an arbitrary wrapped one. */
    if (r >= (double)INT_MAX) {
        return INT_MAX;
    }
    if (r <= (double)INT_MIN) {
        return INT_MIN;
    }
    return (int)r;
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
    /* D29 saturates rather than wraps. money_round clamps every ratio, but
       a credit adds to a balance that is already an int, so a saturated
       loan paid out or a bankruptcy sweeping a saturated estate could still
       overflow here -- signed overflow being undefined, not merely wrong. */
    int *cash = &g->players[p].cash;

    if (amt > 0 && *cash > INT_MAX - amt) {
        *cash = INT_MAX;
    } else {
        *cash += amt;
    }
}

/* Move amt from p to toPlayer, or to the Bank when toPlayer is -1.
 *
 * The single place in the program where insolvency is detected. A player
 * short of cash is put through the D11 recovery ladder first, and only a
 * player the ladder cannot save is declared bankrupt -- so every rule that
 * charges anything gets Rule 11's "normal debt recovery" and Rule 14's
 * bankruptcy for free, and none of them has to ask.
 *
 * Returns false only when the charge bankrupted the payer. Callers need not
 * announce the shortfall themselves: by the time this returns false the
 * bankruptcy block has already printed, and Rule 14 has moved whatever the
 * player had left to the creditor.
 */
bool charge(GameState *g, int p, int amt, int toPlayer)
{
    if (amt <= 0) {
        return true;
    }

    if (g->players[p].cash < amt && !raise_funds(g, p, amt)) {
        declare_bankrupt(g, p, toPlayer);        /* Rule 14                 */
        return false;
    }

    g->players[p].cash -= amt;
    if (toPlayer >= 0) {
        credit(g, toPlayer, amt);        /* D29 saturates there          */
    }
    return true;
}

/* The optional-spend gate. See types.h for why an affordable-only charge is a
   different act from a compelled one; the pre-check is what keeps the D11
   ladder out of a purchase the player could simply have declined. */
bool pay_if_affordable(GameState *g, int p, int cost)
{
    return g->players[p].cash >= cost && charge(g, p, cost, -1);
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

/* Rule 11 as D2' restates it: 15% of the player's CURRENT CASH, charged the
 * moment they land and not before.
 *
 * There is no accrued balance and no "taxes due". An earlier reading assessed
 * the tax every round into a running total that square 4 then settled, which
 * gave Rule 15 a Taxes Due term to subtract -- but Rule 11 makes landing the
 * whole of the event, and a tax that is charged on arrival is never
 * outstanding. The term is dead and the field is gone.
 *
 * The 15% does not drift. LK 13 lists what inflation modifies -- prices,
 * building and hotel costs, rents, premiums, repair costs, loan interest --
 * and income tax is not among them. The one thing that does move it is LK
 * 24's Increase Property Tax, which says so outright ("Income Tax increases
 * by 50%"), and that arrives as EFF_TAX_MUL. Square is -1 because a tax
 * belongs to the player, not to the square they are standing on.
 */
void pay_income_tax(GameState *g, int p)
{
    char b[FMT_BUF];
    int  rate = apply_pct(INCOME_TAX_PCT,
                          effect_modifier(g, EFF_TAX_MUL, -1, p));
    int  due  = pct_of(g->players[p].cash, rate);

    printf("%s landed on Income Tax.\n", g->players[p].name);

    if (due <= 0) {
        printf("No tax is payable.\n");
        end_block();
        levy_luxury_tax(g, p);
        return;
    }

    /* No else branch: charge runs the D11 ladder and, failing that, prints
       Rule 14's bankruptcy block itself. A "cannot pay" line here would only
       repeat what has already been said, after it was said. */
    if (charge(g, p, due, -1)) {
        printf("Income Tax Paid : LKR %s.\n", fmt_lkr(b, due));
        printf("Remaining Balance : LKR %s.\n", fmt_lkr(b, g->players[p].cash));
        end_block();
    }

    /* LK 24's Luxury Property Tax is levied on this square too, while the
       regulation is in force -- the tax square is where a tax is paid. */
    levy_luxury_tax(g, p);
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
        end_block();
    }
}

/* ------------------------------------------------------------ auctions -- */

/* LK 19's opening price: half of market value, then LK 32's -25% while the
 * group is in decline.
 *
 * Static, with one caller. It was public so that decide_bid could reason
 * about the opening without recomputing it, but that need disappeared when
 * milestone 2 changed decide_bid to take minBid rather than the drafted
 * currentBid: run_auction hands the opening straight to the first bidder, so
 * a strategy already has the figure. */
static int auction_opening(const GameState *g, int sq)
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
    printf("\n");
    printf("Property :\n");
    printf("%s\n", g->board[sq].name);
    printf("\n");
    printf("Opening Bid :\n");
    printf("LKR %s.\n", fmt_lkr(b, opening));
    printf("\n");

    /* D23: start immediately after the anchor, then clockwise -- through
       order[], which is the seating the Rule 2 roll-off decided. Rotating
       raw player numbers instead walked 0,1,2,3 regardless of where the
       roll-off had put them, so the bidding order was only correct when
       the roll-off happened to leave the players in index order. */
    for (seat = 0; seat < NUM_PLAYERS; seat++) {
        if (g->order[seat] == anchorPlayer) {
            break;
        }
    }
    seat = (seat + 1) % NUM_PLAYERS;

    /* Each pass either removes a bidder or raises the price by at least
       AUCTION_INC, so the loop cannot spin: withdrawals are bounded by four
       players and raises by the bidders' cash. It ends when the last
       standing bidder is also the high bidder, or when everyone has gone. */
    while (remaining > 0) {
        int p      = g->order[seat];
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
        end_block();
        return;
    }

    charge(g, highBidder, highBid, -1);
    g->board[sq].owner          = highBidder;
    g->board[sq].purchasedRound = g->round;       /* D19                   */
    printf("%s wins the auction.\n", g->players[highBidder].name);
    end_block();
}

/* --------------------------------------------------------------- loans -- */
/*
 * LK 1-7. A secured loan against board assets, at a rate frozen on the day it
 * is issued, compounding every round, repayable only at the Bank square.
 *
 * D4 is what gives this section its teeth. LK 4 adds interest every round
 * while Table 9 labels the rate annual, and taking LK 4 literally means 8%
 * per round -- x4.66 over a twenty-round term, x16.4 at 15%. Combined with
 * R1.8, which makes landing on square 38 the only route to repayment, a loan
 * is a genuine gamble rather than cheap money. Default is meant to happen.
 */

/* LK 1: buildings are never collateral, and LK 3 bars an asset already
   pledged. A mortgaged asset has had its value drawn down already, so it
   cannot secure a second advance either. */
bool eligible_collateral(const GameState *g, int p, int sq)
{
    const Square *s = &g->board[sq];

    return s->owner == p && is_purchasable(g, sq) && !s->mortgaged && !s->loanLocked;
}

/* LK 2 and D5: 75% of the summed mortgage value of everything still free to
 * pledge. Because eligible_collateral excludes what is already locked, this
 * is the REMAINING capacity, which is exactly what the LK 5 increase action
 * needs to know.
 *
 * The 75% is taken once over the sum rather than per asset, and
 * pledge_collateral compares against the same running sum, so the two can
 * never disagree by a rupee of rounding and ask for one asset too many.
 */
int loan_capacity(const GameState *g, int p)
{
    int i, total = 0;

    for (i = 0; i < NUM_SQUARES; i++) {
        if (eligible_collateral(g, p, i)) {
            total += mortgage_value(g, i);
        }
    }
    return pct_of(total, LOAN_LTV_PCT);
}

/* LK 5 permits one loan at a time, so an indebted player can borrow nothing
   more as a NEW loan -- their route to more cash is the increase action,
   which draws on loan_capacity directly. */
int max_loan(const GameState *g, int p)
{
    if (g->players[p].loan.active) {
        return 0;
    }
    return loan_capacity(g, p);
}

/* D22: lock the minimum set of assets, dearest first, whose 75% LTV covers
 * the advance. Prints each name as it goes, under the "Collateral :" header
 * the caller has already written.
 *
 * LK 3 is silent on how much a loan pledges, and locking only what is needed
 * is strictly the more playable reading: it never exceeds LK 2's cap, and it
 * leaves the rest of the portfolio free to be mortgaged later when the D11
 * ladder comes calling. Dearest first is what keeps the set minimal.
 *
 * Terminates because each pass locks one more square, and a locked square is
 * no longer eligible.
 */
static void pledge_collateral(GameState *g, int p, int amount)
{
    int collateral = 0;

    while (pct_of(collateral, LOAN_LTV_PCT) < amount) {
        int i, best = -1, bestValue = 0;

        for (i = 0; i < NUM_SQUARES; i++) {
            int v;

            if (!eligible_collateral(g, p, i)) {
                continue;
            }
            v = mortgage_value(g, i);
            if (best < 0 || v > bestValue) {
                best      = i;
                bestValue = v;
            }
        }
        if (best < 0) {
            return;              /* capacity exhausted; the caller capped   */
        }

        g->board[best].loanLocked = true;
        collateral += bestValue;
        printf("%s\n", g->board[best].name);
    }
}

/* Free every asset p pledged. Called on full settlement and on foreclosure,
   the only two ways a loan ever ends. */
static void unlock_collateral(GameState *g, int p)
{
    int i;

    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].owner == p) {
            g->board[i].loanLocked = false;
        }
    }
}

/* Appendix D, Table 9, in the table's own order -- indexed by
   EconomicCondition. This is the whole of the table's contribution and the
   only place these five figures appear. */
static const int TABLE9_RATE[] = { 5, 8, 10, 12, 15 };

/* D21: the rate a NEW loan would be written at right now.
 *
 * Table 9 is a lookup, not a seed: "loan interest rates vary according to the
 * prevailing economic conditions", so the condition picks the row and the row
 * IS the rate. Nothing drifts.
 *
 * The percentage-point adjustments still land on top. LK 24's Reduce Loan
 * Interest and Appendix A's Interest Rate Cut and Increase each name a direct
 * 2-point move, and they describe no condition Table 9 has a row for -- so
 * they modify the row rather than being expressed by it, and the issued rate
 * can sit outside the table's band. EFF_INTEREST_MUL is deliberately absent:
 * Economic Recession and Stock Market Boom already choose their own rows, and
 * applying their shift here would charge the condition twice. Those two reach
 * existing loans instead, in accrue_interest.
 *
 * Square is -1 because the rate belongs to the economy rather than to any
 * square, and there is no player parameter for the same reason: the rate is
 * the economy's, identical for every borrower, so a player-scoped effect can
 * never reach it and the LK 36 block can print the same figure it issues.
 */
int current_loan_rate(const GameState *g)
{
    return TABLE9_RATE[prevailing_condition(g)];
}

int loan_due_lap(const Player *pl)
{
    return pl->loan.issuedLap + pl->loan.termLaps;
}

/* LK 2-4, 13. Credits the cash immediately and freezes the rate for life. */
void grant_loan(GameState *g, int p, int amount)
{
    char    b[FMT_BUF];
    Player *pl  = &g->players[p];
    int     cap = max_loan(g, p);
    int     rate;

    if (pl->loan.active || amount <= 0 || cap <= 0) {
        return;
    }
    if (amount > cap) {
        amount = cap;                          /* LK 2 is a hard ceiling   */
    }
    rate = current_loan_rate(g);

    /* Written BEFORE the block prints, because the block reports the term
       and there was none to report: printing pl->loan.termLaps ahead of this
       announced "Duration : 0 Rounds" on every first loan in the game. The
       increase and extend blocks read the same field and were always correct,
       having a live loan to read. */
    pl->loan.active      = true;
    pl->loan.principal   = amount;
    pl->loan.ratePct     = rate;               /* LK 13: frozen for life   */
    pl->loan.issuedLap   = pl->laps;           /* D34: the borrower's own  */
    pl->loan.termLaps    = LOAN_ROUNDS;

    printf("%s obtained a secured loan.\n", pl->name);
    printf("\n");
    printf("Loan Amount : LKR %s.\n", fmt_lkr(b, amount));
    printf("\n");
    printf("Collateral :\n");
    pledge_collateral(g, p, amount);
    printf("\n");
    printf("Interest Rate : %d%%\n", rate);
    printf("Duration : %d Rounds\n", pl->loan.termLaps);
    end_block();

    credit(g, p, amount);
}

/* LK 5's increase action: top up against whatever collateral is still free.
 *
 * The rate is re-frozen on the combined balance, since the whole debt is now
 * one advance written today. The maturity date deliberately does not move --
 * extending the term is a separate LK 5 action with its own cost in a Bank
 * landing, and rolling it into a top-up would hand the player both for one.
 */
void increase_loan(GameState *g, int p, int extra)
{
    char    b[FMT_BUF];
    Player *pl  = &g->players[p];
    int     cap = loan_capacity(g, p);
    int     rate;

    if (!pl->loan.active || extra <= 0 || cap <= 0) {
        return;
    }
    if (extra > cap) {
        extra = cap;
    }
    rate = current_loan_rate(g);

    printf("%s increased the loan amount.\n", pl->name);
    printf("\n");
    printf("Loan Amount : LKR %s.\n", fmt_lkr(b, extra));
    printf("\n");
    printf("Collateral :\n");
    pledge_collateral(g, p, extra);
    printf("\n");
    printf("Interest Rate : %d%%\n", rate);
    printf("Duration : %d Rounds\n", pl->loan.termLaps);
    end_block();

    /* Saturating add, for the reason D29 gives. money_round keeps the
       compounding in accrue_interest inside int's range by clamping at
       INT_MAX, but a top-up adds to that clamped figure directly, and
       INT_MAX + extra is signed overflow -- undefined behaviour, not merely
       a wrong number. Seed 51 reached it: the Risk Taker increases its loan at
       every opportunity under 3.3, and once its balance had saturated the next
       increase wrapped the principal to INT_MIN, which the DEBUG guard in
       accrue_interest caught. */
    if (pl->loan.principal > INT_MAX - extra) {
        pl->loan.principal = INT_MAX;
    } else {
        pl->loan.principal += extra;
    }

    pl->loan.ratePct = rate;
    credit(g, p, extra);
}

/* LK 5's extend action. Buys twenty more rounds before the default check
   starts looking, and nothing else -- the principal keeps compounding. */
void extend_loan(GameState *g, int p)
{
    Player *pl = &g->players[p];

    if (!pl->loan.active) {
        return;
    }
    pl->loan.termLaps += LOAN_ROUNDS;
    printf("%s extended the loan period.\n", pl->name);
    printf("Duration : %d Rounds\n", pl->loan.termLaps);
    end_block();
}

/* LK 5's two repayment actions, part and full, which differ only in amount.
 *
 * Cash is tested before charging. Repayment is voluntary, and D11 explicitly
 * refuses to make it a rung of the recovery ladder -- a player selling
 * buildings to service a loan is the exact spiral LK 5 confines to the Bank
 * square to prevent.
 */
void repay_loan(GameState *g, int p, int amount)
{
    char    b[FMT_BUF];
    Player *pl = &g->players[p];

    if (!pl->loan.active || amount <= 0) {
        return;
    }
    if (amount > pl->loan.principal) {
        amount = pl->loan.principal;
    }
    if (!pay_if_affordable(g, p, amount)) {
        return;
    }

    pl->loan.principal -= amount;
    printf("%s repaid LKR %s.\n", pl->name, fmt_lkr(b, amount));
    printf("\n");

    if (pl->loan.principal <= 0) {
        pl->loan.active = false;
        unlock_collateral(g, p);
    }

    printf("Outstanding Balance :\n");
    printf("LKR %s.\n", fmt_lkr(b, pl->loan.principal));
    end_block();
}

/* Forward declarations for the two disposal helpers defined further down:
   reset_to_bank with the loan section, since foreclosure was its first
   caller, and sell_one_building with the D11 ladder. D32's voluntary sale
   reuses both rather than restating what either does. */
static void reset_to_bank(Square *s);
static void sell_one_building(GameState *g, int p, int sq);

/* D32: sell a property outright to the Bank.
 *
 * Section 3 requires this of two personalities in so many words -- the Risk
 * Taker "sells lower-value properties to finance premium developments", the
 * Opportunistic Trader "sells properties expected to decrease in value" --
 * and forbids it of a third, which only means anything if the act exists.
 * The spec names no price for it.
 *
 * The price is the current square_value: the Bank buys back at what it sells
 * for. That symmetry is the whole argument, and the alternative fails on its
 * own terms -- paying mortgage_value would make selling strictly worse than
 * mortgaging, which yields the same cash and keeps the property, so no
 * rational player would ever sell and all three bullets would be dead
 * letters. At market value a sale is a real choice: it realises no profit on
 * its own, but it moves capital out of an asset the market is about to mark
 * down, which is exactly what section 3.4 describes.
 *
 * Buildings come down first at D11's 50%, the price the ladder already uses.
 * LK 3 bars a loan-locked square from being sold at all. A mortgaged square
 * is refused too, rather than netted off: the player has already drawn cash
 * against it and D31 gives them a way to settle that first.
 */
void sell_property(GameState *g, int p, int sq)
{
    char    b[FMT_BUF];
    Square *s = &g->board[sq];
    int     proceeds;

    if (s->owner != p || s->loanLocked || s->mortgaged) {
        return;
    }

    while (development_level(g, sq) > 0) {
        sell_one_building(g, p, sq);
    }

    proceeds = square_value(g, sq);
    reset_to_bank(s);
    credit(g, p, proceeds);

    printf("%s sold %s to the Bank.\n", g->players[p].name, s->name);
    printf("\n");
    printf("Sale Price : LKR %s.\n", fmt_lkr(b, proceeds));
    printf("Remaining Balance : LKR %s.\n", fmt_lkr(b, g->players[p].cash));
    end_block();
}

/* D31: lift a mortgage by repaying it, at the Bank square.
 *
 * The spec leaves this open. Mortgaging is defined -- LK 2 prices it, Rule 7
 * suppresses the rent, R3.3 bars it on loan-locked assets -- but no rule says
 * how the state ends, which left it permanent and made D11's ladder a
 * one-way door: a property mortgaged once earned nothing again and barred its
 * colour group from development for the rest of the game.
 *
 * The price is the CURRENT mortgage value, the same choke point that priced
 * the advance. That makes redemption track inflation and LK 32's decline
 * exactly as the advance did, and it invents no figure. No interest accrues
 * in between, because unlike a loan the spec gives a mortgage neither a rate
 * nor a term to accrue over -- inventing one would be a second new rule to
 * support the first.
 *
 * Cash is tested before charging, on the usual principle: redemption is
 * voluntary, and selling buildings to lift a mortgage is not a trade the D11
 * ladder should be asked to make.
 */
void redeem_mortgage(GameState *g, int p, int sq)
{
    char    b[FMT_BUF];
    Square *s   = &g->board[sq];
    int     due = mortgage_value(g, sq);

    if (s->owner != p || !s->mortgaged) {
        return;
    }
    if (!pay_if_affordable(g, p, due)) {
        return;
    }

    s->mortgaged = false;

    printf("%s redeemed %s.\n", g->players[p].name, s->name);
    printf("\n");
    printf("Redemption Cost : LKR %s.\n", fmt_lkr(b, due));
    printf("Remaining Balance : LKR %s.\n", fmt_lkr(b, g->players[p].cash));
    end_block();
}

/* LK 4 and D4, once per round for every live loan.
 *
 * From the rate the loan was ISSUED at, never a fresh Table 9 reading -- LK 13
 * freezes it, and Loan owning its own ratePct is what makes that correct by
 * construction rather than by everyone remembering. Inflation reaches new
 * loans by moving the prevailing condition, and this line cannot see it.
 *
 * The one thing that does move a live loan is D21's pair of economy-wide
 * conditions; see the local below.
 */
void accrue_interest(GameState *g)
{
    int i, laps, from;

    for (i = 0; i < NUM_PLAYERS; i++) {
        Player *pl = &g->players[i];
        int     rate;

        if (pl->bankrupt || !pl->loan.active) {
            continue;
        }

        /* D21. LK 13 freezes the issued rate for the loan's life, and Table 9
           never revisits it -- the table chose the row at issue and is
           finished. The two economy-wide conditions do still reach a live
           loan: a recession makes an existing debt compound harder while it
           lasts, a boom eases it. Read once per round because the modifier
           cannot change between one lap and the next, and applied to a local
           so the frozen rate stays frozen. */
        /* D21: every live adjustment lands here and none of them at
           issue. The percentage-point instruments -- LK 24's Reduce Loan
           Interest and Appendix A's rate cards -- move an existing debt
           alongside the relative shifts a recession or boom applies.
           Additive first, then relative, as D21 has always ordered them;
           the frozen ratePct is never written, so LK 13 still holds. */
        rate = pl->loan.ratePct + effect_modifier(g, EFF_INTEREST_ADD, -1, i);
        rate = apply_pct(rate, effect_modifier(g, EFF_INTEREST_MUL, -1, i));
        if (rate < 0) {
            rate = 0;                 /* a cut may reach zero, never below */
        }

        /* D34: a loan is a single-player instrument, so it compounds on the
           borrower's clock -- once per lap they completed this round, not
           once per game round. That is what makes the instrument coherent:
           its term is twenty of the borrower's laps under D34, so it now
           accrues exactly twenty times over that term however fast or slow
           they move round the board. Compounding per game round while the
           term counted laps meant a quick player paid fewer periods than a
           slow one for the same twenty-lap loan.

           Applied once per lap rather than as one compound step, so D6's
           rounding still happens at each period exactly as LK 4 describes,
           and a player who laps twice is charged twice rather than being
           charged once at a rate raised to a power. */
        /* From the loan's own start, not the round's. A player who passed
           GO and then borrowed later in the same round was charged for the
           lap they had already run -- a period the loan did not exist for,
           and one its twenty-lap term never counted. */
        from = pl->lapsPrev < pl->loan.issuedLap ? pl->loan.issuedLap
                                                 : pl->lapsPrev;
        for (laps = pl->laps - from; laps > 0; laps--) {
            pl->loan.principal = apply_pct(pl->loan.principal, rate);
        }

#ifdef DEBUG
        /* Milestone 3 asserted a ceiling here on the reasoning that real
           terms cap out in the low hundreds of thousands. Inflation retired
           that premise: D21 lifts the rate every draw and LK 5 lets a term be
           extended without limit, so a balance in the billions is now a
           reachable state rather than a symptom. money_round saturates
           instead of overflowing, which leaves one invariant still worth
           asserting -- a balance that compounds upward can never come back
           negative, and if it does the arithmetic has genuinely broken. */
        if (pl->loan.principal < 0) {
            fprintf(stderr, "R%d: %s principal went negative (%d)\n",
                    g->round, pl->name, pl->loan.principal);
            abort();
        }
#endif
    }
}

/* Return a square to the Bank, stripped of everything ownership carried.
   Shared by foreclosure and by Rule 14's bankruptcy, which dispose of assets
   identically -- buildings demolished, policy cancelled, condition reset for
   the next owner. */
static void reset_to_bank(Square *s)
{
    s->owner              = -1;
    s->purchasedRound     = -1;
    s->houses             = 0;
    s->hotel              = false;
    s->mortgaged          = false;
    s->loanLocked         = false;
    s->damaged            = false;
    s->structDamaged      = false;
    s->policy             = INS_NONE;
    s->policyLap          = 0;
    s->policyWarned       = false;
    restore_condition(s);
    s->depreciationPct    = 0;
}

static bool owns_any_square(const GameState *g, int p)
{
    int i;

    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].owner == p) {
            return true;
        }
    }
    return false;
}

/* LK 6-7, run immediately after accrue_interest so a loan can default on the
 * interest that has just compounded -- which, at D4's per-round rate, is
 * usually how it happens.
 *
 * The pledged assets go to the Bank and then straight to auction under
 * LK 19. The debt is cleared rather than carried: LK 6 treats foreclosure as
 * settlement in full however little the collateral fetched, and the Bank
 * wearing the shortfall is what stops a defaulted player being pursued into
 * a bankruptcy the rule does not call for.
 *
 * Collecting the squares before auctioning any of them matters. run_auction
 * assigns ownership, so a single sweep that auctioned as it went would hand
 * a square to a new owner and then find it again on a later pass.
 */
void check_loan_default(GameState *g)
{
    int i;

    for (i = 0; i < NUM_PLAYERS; i++) {
        Player *pl = &g->players[i];
        char    b[FMT_BUF];
        int     foreclosed[NUM_SQUARES];
        int     n = 0, k, sq;

        if (pl->bankrupt || !pl->loan.active) {
            continue;
        }
        if (pl->laps < loan_due_lap(pl)) {
            continue;
        }
        if (pl->loan.principal <= 0) {
            pl->loan.active = false;           /* settled on the last round */
            unlock_collateral(g, i);
            continue;
        }

        printf("%s has defaulted.\n", pl->name);
        printf("\n");
        printf("Collateral has been foreclosed.\n");
        printf("\n");
        printf("Outstanding debt cleared.\n");
        printf("\n");
        /* Appended rather than interleaved: section 5 templates three lines
           here and no figure, so the template stays consecutive and this
           follows it. Without the balance the block states that a debt was
           written off and never says how much -- and the last summary before
           it is already stale, LK 4 having compounded once more in the same
           scheduler pass before the default check ran. */
        printf("Amount Written Off : LKR %s.\n", fmt_lkr(b, pl->loan.principal));
        end_block();

        for (sq = 0; sq < NUM_SQUARES; sq++) {
            if (g->board[sq].owner == i && g->board[sq].loanLocked) {
                reset_to_bank(&g->board[sq]);
                foreclosed[n++] = sq;
            }
        }

        pl->loan.active    = false;
        pl->loan.principal = 0;

        for (k = 0; k < n; k++) {
            run_auction(g, foreclosed[k], i);  /* LK 19                     */
        }

        /* LK 7: foreclosure alone does not bankrupt anyone. A player who
           keeps a property or a rupee plays on; one left with neither has
           nothing to play with. The creditor is the Bank -- LK 6 cleared the
           debt above, so nobody is owed anything by this point. */
        if (!owns_any_square(g, i) && pl->cash <= 0) {
            declare_bankrupt(g, i, -1);
        }
    }
}

/* ----------------------------------------------------------- insurance -- */
/*
 * Section 1.2 and LK 8-11. Three tiers, priced off the property's current
 * value, valid twenty rounds, and consumed by the first claim they pay.
 */

/* Appendix E's three premium rates, indexed by InsuranceType so INS_NONE
   costs nothing and no branch is needed to say so. */
static const int PREMIUM_PCT[] = {
    0, INS_BASIC_PCT, INS_COMPREHENSIVE_PCT, INS_BUSINESS_PCT
};

/* Section 1.2's tier names, for the purchase block. Kept static: buy_policy
   below is the only place a tier is ever named in output, and an accessor
   exposing this to other modules had no callers. */
static const char *TIER_NAMES[] = {
    "None", "Basic", "Comprehensive", "Business Interruption"
};

/* Appendix E: 5%, 10% or 15% of the property's CURRENT value, then LK 24's
 * Insurance Regulation and Heavy Monsoon through EFF_PREMIUM_MUL.
 *
 * Reading square_value is the whole design. A premium quoted off the stored
 * price would drift away from the market the first time a boom or an
 * inflation draw landed; quoted off the choke point it tracks both, and
 * milestone 4's systems reach it without knowing it exists.
 */
int premium(const GameState *g, int sq, InsuranceType tier)
{
    int base = pct_of(square_value(g, sq), PREMIUM_PCT[tier]);

    return apply_pct(base, effect_modifier(g, EFF_PREMIUM_MUL, sq, g->board[sq].owner));
}

/* LK 8-9. One policy per property; buying again replaces what was there and
 * restarts the twenty rounds, which is what "renewal" means for a square that
 * already carries a policy.
 *
 * Cash is tested before charging, on the same principle as construction and
 * upkeep: a premium is voluntary, and selling buildings to insure a building
 * is not a trade the D11 ladder should ever be asked to make.
 */
void buy_policy(GameState *g, int p, int sq, InsuranceType tier)
{
    char    b[FMT_BUF];
    Square *s   = &g->board[sq];
    int     due = premium(g, sq, tier);

    if (tier == INS_NONE || s->owner != p) {
        return;
    }
    if (!pay_if_affordable(g, p, due)) {
        return;
    }

    s->policy       = tier;
    s->policyLap    = g->players[p].laps;   /* D34: the owner's own clock  */
    s->policyWarned = false;

    printf("%s Insurance purchased.\n", TIER_NAMES[tier]);
    printf("\n");
    printf("Property : %s\n", s->name);
    printf("Premium : LKR %s.\n", fmt_lkr(b, due));
    end_block();
}

/* LK 9, once per round. Warns three rounds out and lapses at zero.
 *
 * The warning fires on the exact equality rather than on "three or fewer", so
 * each policy announces itself once. A >= test would repeat the same warning
 * for four consecutive rounds, which reads as four different policies about
 * to lapse rather than one.
 */
void tick_insurance(GameState *g)
{
    int i;

    for (i = 0; i < NUM_SQUARES; i++) {
        Square *s = &g->board[i];
        int     left;

        if (s->policy == INS_NONE || s->owner < 0) {
            continue;
        }

        /* D34: a policy covers one property held by one player, so it ages
           on that player's clock. Derived from the stored baseline rather
           than counted down, so it cannot drift from the lap count it is
           measured against. */
        left = INS_ROUNDS - (g->players[s->owner].laps - s->policyLap);

        if (left <= INS_WARN_ROUNDS && left > 0 && !s->policyWarned) {
            s->policyWarned = true;
            /* LK 9 states the notice, not a countdown: "players receive
               renewal reminders three rounds before expiry". The figure is
               therefore the rule's, printed as section 5 shows it, and does
               not track a remaining count that the rule never mentions. */
            printf("Insurance policy on %s expires in %d rounds.\n",
                   s->name, INS_WARN_ROUNDS);
            end_block();
        }
        if (left <= 0) {
            s->policy       = INS_NONE;
            s->policyWarned = false;
        }
    }
}

/* ------------------------------------------- debt recovery, bankruptcy -- */
/*
 * Rule 11 charges taxes "immediately" and Rule 14 declares bankruptcy when
 * liabilities exceed assets, but neither says what happens in between. D11
 * settles it as a two-rung ladder, taken in this order and no other:
 *
 *   1. sell buildings back to the Bank at 50% of construction cost
 *   2. mortgage assets that are neither mortgaged nor pledged
 *   3. still short -- bankrupt
 *
 * Repaying or increasing a loan is deliberately NOT a rung. LK 5 and the
 * clarification confine every loan action to the Bank square, and letting a
 * cornered player restructure their debt from wherever they happen to be
 * standing would make that square pointless.
 */

/* Half of what the topmost building on this square cost to put up. */
static int demolition_refund(const GameState *g, int sq)
{
    return pct_of(building_cost(g, sq, g->board[sq].hotel), 50);
}

/* Sell exactly one level of development, the reverse of one build_step pass.
 *
 * Public because R4.3 and R4.4 both describe selling as a deliberate act
 * rather than a forced one, and this is the only price the spec states for
 * disposing of anything: D11's 50% of construction cost. A voluntary sale
 * that invented its own figure would be a new rule; this one is the ladder's
 * own rung, used on purpose.
 *
 * A hotel goes back to the four houses it replaced rather than to bare land,
 * which is Rule 10 read backwards and keeps the refund honest: the owner paid
 * four house costs and then a hotel cost, so unwinding in the same two stages
 * returns half of each. Dropping straight to zero would refund half a hotel
 * for buildings worth a hotel plus four houses.
 */
static void sell_one_building(GameState *g, int p, int sq)
{
    char    b[FMT_BUF];
    Square *s      = &g->board[sq];
    int     refund = demolition_refund(g, sq);

    if (s->hotel) {
        /* Rule 10 backwards. The four houses that reappear are the same
           fabric the hotel was, so they inherit its rating rather than
           arriving new -- otherwise building a hotel and selling it back
           would be the cheapest overhaul on the board. */
        spread_condition(s);
        s->hotel  = false;
        s->houses = MAX_HOUSES;
        printf("%s sold the hotel on %s.\n", g->players[p].name, s->name);
    } else {
        s->houses--;
        printf("%s sold a house on %s.\n", g->players[p].name, s->name);
    }

    /* LK 11: damage belongs to the BUILDINGS, so selling the last one
       takes the damage with it. Leaving the flag on a bare lot left a
       square that could not collect rent and could not be repaired
       either -- D1 prices repair off the buildings standing, and there
       are none. reset_to_bank already clears it on the foreclosure and
       bankruptcy paths; this is the one route that emptied a square
       while its owner kept it. */
    if (!s->hotel && s->houses == 0) {
        s->damaged = false;
    }

    credit(g, p, refund);
    printf("Received LKR %s.\n", fmt_lkr(b, refund));
    end_block();
}

/* D11, called by charge and by nothing else. Returns whether p can now cover
 * `needed`.
 *
 * Rung 1 always takes from the MOST developed square, which is the builder
 * run in reverse and keeps groups even on the way down just as it does on the
 * way up.
 *
 * Rung 2 mortgages cheapest first. Mortgage value and rent both scale with
 * the property, so raising a given sum from the bottom of the portfolio
 * sacrifices the smallest rent stream, and the properties worth holding are
 * the last to go.
 *
 * The ordering also makes rung 2 safe without a check of its own. Rung 1 runs
 * until the money is found or no building is left standing, so by the time
 * anything is mortgaged the board carries no buildings at all -- which is the
 * ordinary rule that a developed property cannot be mortgaged, obtained
 * structurally rather than asserted.
 */
bool raise_funds(GameState *g, int p, int needed)
{
    int i;

    while (g->players[p].cash < needed) {
        int best = -1, bestLevel = 0;

        for (i = 0; i < NUM_SQUARES; i++) {
            int level;

            if (g->board[i].owner != p) {
                continue;
            }
            level = development_level(g, i);
            if (level > bestLevel) {
                best      = i;
                bestLevel = level;
            }
        }
        if (best < 0) {
            break;                          /* nothing left to demolish    */
        }
        sell_one_building(g, p, best);
    }

    while (g->players[p].cash < needed) {
        char b[FMT_BUF];
        int  best = -1, bestValue = 0, value;

        for (i = 0; i < NUM_SQUARES; i++) {
            const Square *s = &g->board[i];

            if (s->owner != p || s->mortgaged || s->loanLocked) {
                continue;
            }
            if (!is_purchasable(g, i)) {
                continue;
            }
            value = mortgage_value(g, i);
            if (best < 0 || value < bestValue) {
                best      = i;
                bestValue = value;
            }
        }
        if (best < 0) {
            break;                          /* nothing left to pledge      */
        }

        g->board[best].mortgaged = true;
        credit(g, p, bestValue);
        printf("%s mortgaged %s.\n", g->players[p].name, g->board[best].name);
        printf("Received LKR %s.\n", fmt_lkr(b, bestValue));
        end_block();
    }

    return g->players[p].cash >= needed;
}

/* Rule 14, and the end of a player's game.
 *
 * Section 5's block says the assets go to the Bank, and that is literally
 * what happens to every square: ownership reverts, buildings come down,
 * policies lapse, and LK 19 then puts each one up for auction. Rule 14's
 * transfer to the creditor is of the CASH, which is all a bankrupt player has
 * left that anyone can be handed directly.
 *
 * The loan dies with the player rather than following the estate. LK 6
 * already treats foreclosure as settlement in full however little the
 * collateral fetched, so there is no reading of the rules under which a debt
 * outlives the debtor.
 */
void declare_bankrupt(GameState *g, int p, int creditor)
{
    Player *pl = &g->players[p];
    int     assets[NUM_SQUARES];
    int     n = 0, k, sq;

    if (pl->bankrupt) {
        return;                     /* already gone; do not auction twice   */
    }
    pl->bankrupt = true;

    printf("%s has been declared bankrupt.\n", pl->name);
    printf("Remaining assets transferred to the Bank.\n");
    end_block();

    /* The one assignment to cash outside credit and charge. Zeroing a
       bankrupt player's balance is not a transaction -- there is no second
       party when the creditor is the Bank, and routing it through charge
       would re-enter the function that called this one. */
    if (creditor >= 0 && pl->cash > 0) {
        credit(g, creditor, pl->cash);
    }
    pl->cash = 0;

    /* The whole record, not just the two fields anything reads today. A
       bankrupt player is skipped everywhere, so a stale rate or jail turn
       is invisible until some future reader forgets to check -- and the
       cost of not leaving one behind is six assignments. */
    pl->loan.active    = false;
    pl->loan.principal = 0;
    pl->loan.ratePct   = 0;
    pl->loan.issuedLap = 0;
    pl->loan.termLaps  = 0;
    pl->jailed         = false;
    pl->jailTurns      = 0;
    pl->sufferedLoss   = false;

    for (sq = 0; sq < NUM_SQUARES; sq++) {
        if (g->board[sq].owner == p) {
            reset_to_bank(&g->board[sq]);
            assets[n++] = sq;
        }
    }

    /* Collected first, auctioned second, for the reason foreclosure does the
       same: run_auction assigns ownership, and one sweep doing both would
       find a square it had already sold. */
    for (k = 0; k < n; k++) {
        run_auction(g, assets[k], p);       /* LK 19                        */
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
       square_value already knows how to price each type. */
    for (i = 0; i < NUM_SQUARES; i++) {
        const Square *s = &g->board[i];

        if (s->owner != p) {
            continue;
        }
        total += square_value(g, i);

        /* Buildings at book value, which Rule 15 counts separately from the
           land. Read through building_cost rather than the stored field so
           the book keeps pace with what a building currently costs to put
           up. A hotel replaced its four houses, so exactly one of these
           branches contributes. */
        if (s->hotel) {
            total += building_cost(g, i, true);
        } else if (s->houses > 0) {
            total += building_cost(g, i, false) * s->houses;
        }

        /* No mortgage term. D28 settles which of the spec's two statements
           of net worth governs, and it is Rule 15's: the intro paragraph
           lists "outstanding loans, and mortgage liabilities" while Rule 15
           itself gives the formula without any mortgage term. Rule 15 is the
           numbered rule and wins. The consequence is real and intended --
           a mortgaged square is still carried at full market value here, so
           mortgaging raises the reported figure by the cash it releases. */
    }

    /* Rule 15's liabilities. Accrued interest needs no term of its own:
       LK 4 compounds it INTO the principal every round, so subtracting the
       principal subtracts every rupee of interest with it. Taxes due is
       likewise structurally zero -- both tax squares charge on the spot, so
       nothing is ever owed between turns -- but the field is subtracted
       rather than assumed, since milestone 4's regulations may defer one.
       Claims receivable is permanently 0 by D15. */
    total -= g->players[p].loan.principal;

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
