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
    g->players[p].cash += amt;
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

    /* No else branch: charge now runs the D11 ladder and, failing that,
       prints Rule 14's bankruptcy block itself. A "cannot pay" line here
       would only repeat what has already been said, after it was said. */
    if (charge(g, p, due, -1)) {
        printf("Income Tax Paid : LKR %s.\n", fmt_lkr(b, due));
        printf("Remaining Balance : LKR %s.\n", fmt_lkr(b, g->players[p].cash));
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

/* D21: the rate a NEW loan is written at. Additive adjustments first --
 * LK 24's Reduce Loan Interest and Appendix A's rate cards are percentage
 * points -- then the relative shifts that large events apply.
 *
 * Square is -1 because the rate belongs to the economy rather than to any
 * square; only global and player-scoped effects can reach it. Both reads are
 * live now rather than deferred, so milestone 4 pushes its effects and this
 * function needs no edit.
 */
static int new_loan_rate(const GameState *g, int p)
{
    int rate = g->econ.interestRatePct + effect_modifier(g, EFF_INTEREST_ADD, -1, p);

    rate = apply_pct(rate, effect_modifier(g, EFF_INTEREST_MUL, -1, p));
    return rate < 0 ? 0 : rate;       /* a cut may reach zero, never below */
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
    rate = new_loan_rate(g, p);

    printf("%s obtained a secured loan.\n", pl->name);
    printf("Loan Amount : LKR %s.\n", fmt_lkr(b, amount));
    printf("Collateral :\n");
    pledge_collateral(g, p, amount);
    printf("Interest Rate : %d%%\n", rate);
    printf("Duration : %d Rounds\n", LOAN_ROUNDS);

    pl->loan.active      = true;
    pl->loan.principal   = amount;
    pl->loan.ratePct     = rate;               /* LK 13: frozen for life   */
    pl->loan.issuedRound = g->round;           /* D19: one clock           */
    pl->loan.termRounds  = LOAN_ROUNDS;

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
    rate = new_loan_rate(g, p);

    printf("%s increased the loan amount.\n", pl->name);
    printf("Loan Amount : LKR %s.\n", fmt_lkr(b, extra));
    printf("Collateral :\n");
    pledge_collateral(g, p, extra);
    printf("Interest Rate : %d%%\n", rate);
    printf("Duration : %d Rounds\n", pl->loan.termRounds);

    pl->loan.principal += extra;
    pl->loan.ratePct    = rate;
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
    pl->loan.termRounds += LOAN_ROUNDS;
    printf("%s extended the loan period.\n", pl->name);
    printf("Duration : %d Rounds\n", pl->loan.termRounds);
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
    if (pl->cash < amount || !charge(g, p, amount, -1)) {
        return;
    }

    pl->loan.principal -= amount;
    printf("%s repaid LKR %s.\n", pl->name, fmt_lkr(b, amount));

    if (pl->loan.principal <= 0) {
        pl->loan.active = false;
        unlock_collateral(g, p);
    }

    printf("Outstanding Balance :\n");
    printf("LKR %s.\n", fmt_lkr(b, pl->loan.principal));
}

/* LK 4 and D4, once per round for every live loan.
 *
 * At the rate the loan was ISSUED at, never the current one -- LK 13 freezes
 * it, and Loan owning its own ratePct is what makes that correct by
 * construction rather than by everyone remembering. Milestone 4's inflation
 * moves econ.interestRatePct and this line does not notice, which is the
 * point.
 */
void accrue_interest(GameState *g)
{
    int i;

    for (i = 0; i < NUM_PLAYERS; i++) {
        Player *pl = &g->players[i];

        if (pl->bankrupt || !pl->loan.active) {
            continue;
        }
        pl->loan.principal = apply_pct(pl->loan.principal, pl->loan.ratePct);

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
    s->policyRounds       = 0;
    s->conditionPct       = 100;
    s->unmaintainedRounds = 0;
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
        int     foreclosed[NUM_SQUARES];
        int     n = 0, k, sq;

        if (pl->bankrupt || !pl->loan.active) {
            continue;
        }
        if (g->round < pl->loan.issuedRound + pl->loan.termRounds) {
            continue;
        }
        if (pl->loan.principal <= 0) {
            pl->loan.active = false;           /* settled on the last round */
            unlock_collateral(g, i);
            continue;
        }

        printf("%s has defaulted.\n", pl->name);
        printf("Collateral has been foreclosed.\n");
        printf("Outstanding debt cleared.\n");

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
 * Repaying or refinancing a loan is deliberately NOT a rung. LK 5 and the
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
        s->hotel  = false;
        s->houses = MAX_HOUSES;
        printf("%s sold the hotel on %s.\n", g->players[p].name, s->name);
    } else {
        s->houses--;
        printf("%s sold a house on %s.\n", g->players[p].name, s->name);
    }

    credit(g, p, refund);
    printf("Received LKR %s.\n", fmt_lkr(b, refund));
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

    /* The one assignment to cash outside credit and charge. Zeroing a
       bankrupt player's balance is not a transaction -- there is no second
       party when the creditor is the Bank, and routing it through charge
       would re-enter the function that called this one. */
    if (creditor >= 0 && pl->cash > 0) {
        credit(g, creditor, pl->cash);
    }
    pl->cash = 0;

    pl->loan.active    = false;
    pl->loan.principal = 0;

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

        /* A mortgage is money already drawn against the square, so it comes
           straight back off. Rule 15's "- loans" names the LK 1-7 advance
           and says nothing about mortgages, but they are the same instrument
           at a different desk -- cash advanced against an asset the player
           keeps and still shows at full market value above. Omitting it
           makes mortgaging RAISE a player's net worth, and a balance sheet
           in which borrowing improves your position is arithmetically wrong
           before it is unfaithful to anything. Seed 42 had a player showing
           65,879 with nine of ten properties mortgaged and the tenth
           pledged. */
        if (s->mortgaged) {
            total -= mortgage_value(g, i);
        }
    }

    /* Rule 15's liabilities. Accrued interest needs no term of its own:
       LK 4 compounds it INTO the principal every round, so subtracting the
       principal subtracts every rupee of interest with it. Taxes due is
       likewise structurally zero -- both tax squares charge on the spot, so
       nothing is ever owed between turns -- but the field is subtracted
       rather than assumed, since milestone 4's regulations may defer one.
       Claims receivable is permanently 0 by D15. */
    total -= g->players[p].loan.principal;
    total -= g->players[p].taxesDue;

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
