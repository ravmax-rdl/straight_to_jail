/* game.c -- the simulation engine.
 *
 * The only module that orchestrates. It owns the round and turn loops, the
 * landing dispatch, the end-of-round scheduler, and every block-format
 * output. Everything else answers questions; this file asks them.
 */

#include <stdio.h>
#include <stdlib.h>   /* abort(), in the DEBUG invariant guards only */
#include <string.h>

#include "types.h"

/* Player 1-4 order, fixed by the section 5 pre-game block. This is the
   order they are *introduced* in, not the order they play in -- turn order
   is decided by the roll-off below. */
static const char *PLAYER_NAMES[NUM_PLAYERS] = {
    "Aggressive Investor",
    "Conservative Banker",
    "Risk Taker",
    "Opportunistic Trader"
};

static const Strategy PLAYER_STRATS[NUM_PLAYERS] = {
    STRAT_AGGRESSIVE, STRAT_CONSERVATIVE, STRAT_RISKTAKER, STRAT_OPPORTUNIST
};

/* ------------------------------------------------------- initialisation -- */

/* Returns false if Rent.csv could not be loaded (D27). board_init has
   already explained why on stderr; the caller must abandon the run rather
   than play on a board whose properties have no prices. */
bool game_init(GameState *g, const char *csvPath)
{
    int i;

    memset(g, 0, sizeof *g);
    if (!board_init(g, csvPath)) {
        return false;
    }

    for (i = 0; i < NUM_PLAYERS; i++) {
        Player *p = &g->players[i];

        p->name  = PLAYER_NAMES[i];
        p->strat = PLAYER_STRATS[i];
        p->cash  = START_CASH;      /* Rule 1                               */
        p->pos   = SQ_IDX_GO;

        g->order[i] = i;            /* replaced by determine_order          */
    }

    /* Table 9's Stable Economy is where the economy starts (D21). Both rates
       drift from here: the loan rate for new loans, the tax rate for
       Income Tax (D2'). */
    g->econ.interestRatePct = BASE_INTEREST_PCT;
    g->econ.incomeTaxPct    = INCOME_TAX_PCT;

    /* -1 is "none yet" for all three. GRP_NONE happens to be -1 too, which
       is what makes the LK 30 consecutive-repeat check work on round 10. */
    g->econ.activeRegulation  = -1;
    g->econ.lastBoomGroup     = GRP_NONE;
    g->econ.lastDeclineGroup  = GRP_NONE;
    g->econ.activeCard        = -1;

    /* Shuffled once, then walked in order for the rest of the game (App A). */
    deck_init(g);

    g->round = 0;
    return true;
}

/* --------------------------------------------------------- turn order --- */

/* Selection sort of order[lo..hi) into descending score. Four elements at
   most, so the algorithm is irrelevant; what matters is that order[] and
   score[] stay in lockstep, since score[k] is the roll of player order[k]
   rather than of player k. */
static void sort_slice(int *order, int *score, int lo, int hi)
{
    int i, j, best, tmp;

    for (i = lo; i < hi - 1; i++) {
        best = i;
        for (j = i + 1; j < hi; j++) {
            if (score[j] > score[best]) {
                best = j;
            }
        }
        if (best != i) {
            tmp = order[i]; order[i] = order[best]; order[best] = tmp;
            tmp = score[i]; score[i] = score[best]; score[best] = tmp;
        }
    }
}

/* D8': resolve a run of equal rolls.
 *
 * The subtlety the rule turns on is that a reroll decides only the
 * positions of the tied players *among themselves*. Untied players keep
 * their ranks. Because this only ever touches order[lo..hi), a player who
 * was not tied cannot be displaced by someone else's reroll -- which is
 * precisely what the lecturer's worked example requires when players 2 and
 * 3 reroll and players 1 and 4 stay put.
 *
 * A reroll can tie again, so this recurses on any run that survives. Depth
 * is bounded in practice by probability and in code by the guard, which
 * cannot fire in a real game but stops a pathological RNG from hanging the
 * simulation.
 */
static void resolve_ties(GameState *g, int *order, int *score, int lo, int hi, int depth)
{
    int k, runStart, runEnd;

    if (hi - lo < 2 || depth > 32) {
        return;
    }

    for (k = lo; k < hi; k++) {
        int d1, d2;
        score[k] = roll_dice(&d1, &d2);
        printf("%s rolls %d.\n", g->players[order[k]].name, score[k]);
    }

    sort_slice(order, score, lo, hi);

    runStart = lo;
    while (runStart < hi) {
        runEnd = runStart + 1;
        while (runEnd < hi && score[runEnd] == score[runStart]) {
            runEnd++;
        }
        resolve_ties(g, order, score, runStart, runEnd, depth + 1);
        runStart = runEnd;
    }
}

/* Rule 2 and section 5 "Determining the First Player". */
void determine_order(GameState *g)
{
    int score[NUM_PLAYERS];
    int i, runStart, runEnd;

    for (i = 0; i < NUM_PLAYERS; i++) {
        int d1, d2;
        g->order[i] = i;
        score[i]    = roll_dice(&d1, &d2);
        printf("%s rolls %d.\n", g->players[i].name, score[i]);
    }

    sort_slice(g->order, score, 0, NUM_PLAYERS);

    runStart = 0;
    while (runStart < NUM_PLAYERS) {
        runEnd = runStart + 1;
        while (runEnd < NUM_PLAYERS && score[runEnd] == score[runStart]) {
            runEnd++;
        }
        resolve_ties(g, g->order, score, runStart, runEnd, 0);
        runStart = runEnd;
    }

    printf("\n");
    printf("%s will begin the game.\n", g->players[g->order[0]].name);
    printf("\n");
    printf("Turn order:\n");
    for (i = 0; i < NUM_PLAYERS; i++) {
        printf("%s\n", g->players[g->order[i]].name);
    }
}

/* ------------------------------------------------------- block output ---- */

/* Rule-line widths, measured from the section 5 templates rather than
   guessed: the round summary uses 45 characters and the market conditions
   block uses 41. Extracting the PDF's text confirmed both, and confirmed
   that the round summary's field lines are consecutive -- the vertical gaps
   in the rendered PDF are LaTeX spacing, not content. That is D26. */
#define SUMMARY_RULE 45
#define MARKET_RULE  41

static void rule_line(char c, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        putchar(c);
    }
    putchar('\n');
}

/* Property and hotel counts are read straight off the board rather than
   cached on the Player, so the figures cannot drift from reality after a
   foreclosure or an auction. count_owned lives in board.c and does the
   property half; only hotels need a local helper. */

static int count_hotels(const GameState *g, int p)
{
    int i, n = 0;
    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].owner == p && g->board[i].hotel) {
            n++;
        }
    }
    return n;
}

/* Milestone 2 swaps the stored price for square_value once the choke point
   exists; until then nothing is owned and both read zero. */
static int total_property_value(const GameState *g, int p)
{
    int i, total = 0;
    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].owner == p) {
            total += g->board[i].price;
        }
    }
    return total;
}

/* Section 5 "End of Every Round". Players print in turn order, and the
   separator falls between players only -- there is none after the last one,
   because the closing rule already terminates the block. */
void round_summary(const GameState *g)
{
    char b[FMT_BUF];
    int  i;

    rule_line('=', SUMMARY_RULE);
    printf("Round %d Summary\n", g->round);
    rule_line('=', SUMMARY_RULE);

    for (i = 0; i < NUM_PLAYERS; i++) {
        const Player *pl = &g->players[g->order[i]];
        int           p  = g->order[i];

        printf("%s\n", pl->name);
        printf("Cash : LKR %s\n", fmt_lkr(b, pl->cash));
        printf("Net Worth : LKR %s\n", fmt_lkr(b, net_worth(g, p)));
        printf("Properties : %d\n", count_owned(g, p, SQ_PROPERTY));
        printf("Hotels : %d\n", count_hotels(g, p));

        /* "None", never "LKR 0" -- the template is explicit about this and
           a zero would also be a lie, since a settled loan is not a loan. */
        if (pl->loan.active) {
            printf("Outstanding Loan : LKR %s\n", fmt_lkr(b, pl->loan.principal));
        } else {
            printf("Outstanding Loan : None\n");
        }

        if (i < NUM_PLAYERS - 1) {
            rule_line('-', SUMMARY_RULE);
        }
    }

    rule_line('=', SUMMARY_RULE);
}

/* Rule-LK 36, printed at the end of every round immediately after the
 * summary.
 *
 * The underline widths are literal. They are not label length plus a
 * constant -- the five run 13, 16, 23, 12 and 23 against labels of 11, 14,
 * 20, 9 and 21 -- so they are transcribed from the template rather than
 * computed, and a comment says so to stop a later reader "fixing" them.
 *
 * Milestone 4 fills the first three sections from the effect registry. Until
 * then only the two scalar rates have anything to report, and a section with
 * nothing active prints nothing at all.
 */
void market_conditions(const GameState *g)
{
    const char *card;
    int         grp, left, pct = 0;

    rule_line('=', MARKET_RULE);
    printf("Current Market Conditions\n");
    rule_line('=', MARKET_RULE);
    printf("\n");

    /* A section with nothing active prints nothing at all -- header, rule and
       body together. LK 36 reports what is in force, and an empty heading
       reports the absence of something rather than the presence of nothing. */
    grp = boom_group(g, &left);
    if (grp != GRP_NONE) {
        printf("Market Boom\n");
        rule_line('-', 13);
        printf("%s : %d rounds remaining\n", group_name((PropertyGroup)grp), left);
        printf("\n");
    }

    grp = decline_group(g, &left);
    if (grp != GRP_NONE) {
        printf("Market Decline\n");
        rule_line('-', 16);
        printf("%s : %d rounds remaining\n", group_name((PropertyGroup)grp), left);
        printf("\n");
    }

    card = active_card(g, &pct, &left);
    if (card != NULL) {
        printf("Regional Development\n");
        rule_line('-', 23);
        printf("%s : %+d%%, %d rounds remaining\n", card, pct, left);
        printf("\n");
    }

    printf("Inflation\n");
    rule_line('-', 12);
    printf("%+d%%\n", g->econ.inflationPct);
    printf("\n");

    /* D21: the rate a new loan would be written at now, not the stored one.
       Section 5's sample prints 9% during an Economic Recession, which is the
       stable 8% with the event's relative +15% applied -- so the block has to
       report the adjusted figure. Player -1 keeps a player-scoped Appendix A
       rate card out of a board-wide reading. */
    printf("Current Loan Interest\n");
    rule_line('-', 23);
    printf("%d%%\n", current_loan_rate(g, -1));
    printf("\n");

    rule_line('=', MARKET_RULE);
}

/* Section 5 "End of Game". Note this block puts labels and values on
   separate lines, unlike the round summary's "Label : value" pairs. */
void final_report(const GameState *g)
{
    char b[FMT_BUF];
    int  i, winner = g->order[0];

    /* Rule 15 has two endings and one tiebreak between them. If a single
       player is still solvent they win outright; otherwise 500 rounds have
       elapsed and the highest net worth takes it. Scanning for the best net
       worth among the solvent covers both cases in one pass. */
    for (i = 0; i < NUM_PLAYERS; i++) {
        if (g->players[i].bankrupt) {
            continue;
        }
        if (g->players[winner].bankrupt || net_worth(g, i) > net_worth(g, winner)) {
            winner = i;
        }
    }

    rule_line('=', SUMMARY_RULE);
    printf("GAME OVER\n");
    printf("Winner\n");
    printf("%s\n", g->players[winner].name);
    printf("Total Cash\n");
    printf("LKR %s\n", fmt_lkr(b, g->players[winner].cash));
    printf("Total Property Value\n");
    printf("LKR %s\n", fmt_lkr(b, total_property_value(g, winner)));
    printf("Outstanding Loans\n");
    if (g->players[winner].loan.active) {
        printf("LKR %s\n", fmt_lkr(b, g->players[winner].loan.principal));
    } else {
        printf("None\n");
    }
    printf("Net Worth\n");
    printf("LKR %s\n", fmt_lkr(b, net_worth(g, winner)));
    rule_line('=', SUMMARY_RULE);
}

/* --------------------------------------------------------- the loops ----- */

/* Rule 3. Steps 2 and 3 only for now; the remaining six arrive with the
   systems they depend on, in the order the milestones introduce them. */
/* ----------------------------------------------------------- landing --- */

/* Rule 3 step 4: resolve whatever the square the player stopped on does.
 *
 * Every SquareType is listed explicitly and there is no default label. That
 * is deliberate: adding a type later produces
 *   warning: enumeration value 'SQ_X' not handled in switch
 * at exactly the places that need updating. A default: would compile
 * silently and do nothing, which is the failure this switch exists to
 * prevent.
 */
/* Rules 5 and 7: an unowned purchasable square is bought or auctioned; an
 * owned one charges rent unless its owner is standing on it.
 *
 * Rule 5 gives no third option. Declining is not "nothing happens" -- it is
 * an auction, immediately, which is why the else branch is not empty.
 */
/* LK 17. Pays a tenth of current market value to undo the wear and restart
 * the clock.
 *
 * Resetting purchasedRound is what "resets age" means under D19's single
 * clock: age is derived from that field, so there is no separate age counter
 * to forget. Clearing depreciationPct alone would leave the property
 * immediately eligible to depreciate again on the next five-round tick.
 */
static void renovate_step(GameState *g, int p, int sq)
{
    char    b[FMT_BUF];
    Square *s = &g->board[sq];
    int     cost;

    if (!decide_renovate(g, p, sq)) {
        return;
    }

    /* LK 29's rebuild and LK 17's renovation are different jobs at different
       prices, and decide_renovate already ranked them: structural damage is
       the worse of the two, so a square carrying it gets rebuilt first. */
    if (s->structDamaged) {
        cost = structural_renovation_cost(g, sq);
        if (g->players[p].cash < cost || !charge(g, p, cost, -1)) {
            return;
        }

        /* LK 29 restores value, rent and condition together, which is three
           fields because the three penalties are read in three places. */
        s->structDamaged      = false;
        s->conditionPct       = 100;
        s->unmaintainedRounds = 0;

        printf("%s rebuilt %s.\n", g->players[p].name, s->name);
        printf("Renovation Cost : LKR %s.\n", fmt_lkr(b, cost));
        return;
    }

    cost = pct_of(square_value(g, sq), RENOVATE_PCT);
    if (g->players[p].cash < cost || !charge(g, p, cost, -1)) {
        return;
    }

    s->depreciationPct = 0;
    s->purchasedRound  = g->round;      /* D19: the age resets with it     */

    printf("%s renovated %s.\n", g->players[p].name, s->name);
    printf("Renovation Cost : LKR %s.\n", fmt_lkr(b, cost));
}

static void land_on_purchasable(GameState *g, int p, int sq, int diceTotal)
{
    char    b[FMT_BUF];
    Square *s = &g->board[sq];
    int     rent;

    if (s->owner < 0) {
        int price = square_value(g, sq);

        if (decide_buy(g, p, sq) && charge(g, p, price, -1)) {
            s->owner          = p;
            s->purchasedRound = g->round;    /* D19: age starts at purchase */
            printf("%s purchased %s for LKR %s.\n", g->players[p].name, s->name,
                   fmt_lkr(b, price));
            printf("Remaining Balance : LKR %s.\n", fmt_lkr(b, g->players[p].cash));
        } else {
            /* Rule 5 gives no third option: a declined square goes straight
               to auction, and LK 19 lets the decliner bid in it. */
            run_auction(g, sq, p);
        }
        return;
    }

    if (s->owner == p) {
        /* LK 17 permits renovation here and nowhere else -- landing on your
           own property is the opportunity, which is why this is the one
           branch of land_on that does something for a square the player
           already holds. */
        renovate_step(g, p, sq);
        return;
    }

    rent = square_rent(g, sq, diceTotal);
    if (rent <= 0) {
        return;                              /* mortgaged -- Rule 7       */
    }

    printf("%s landed on %s.\n", g->players[p].name, s->name);

    /* No else branch. charge now runs the D11 ladder and prints Rule 14's
       bankruptcy block if the ladder cannot save the payer, by which point
       the creditor has already received whatever was left. */
    if (charge(g, p, rent, s->owner)) {
        printf("Rent Paid : LKR %s.\n", fmt_lkr(b, rent));
        printf("Owner : %s.\n", g->players[s->owner].name);
    }
}

/* R1.8 and LK 5. One action per landing, and the only route to repayment
 * there is -- a player who never lands on square 38 inside their term
 * defaults, which is the clarified rule rather than an oversight.
 *
 * Every case is listed and there is no default label, for the same reason
 * land_on has none: adding a sixth action should break the build here.
 */
static void bank_visit(GameState *g, int p)
{
    int        amount = 0;
    BankAction act    = decide_bank(g, p, &amount);

    switch (act) {
    case BANK_OBTAIN:     grant_loan(g, p, amount);     break;
    case BANK_REPAY_PART:
    case BANK_REPAY_FULL: repay_loan(g, p, amount);     break;
    case BANK_EXTEND:     extend_loan(g, p);            break;
    case BANK_INCREASE:   increase_loan(g, p, amount);  break;
    case BANK_NONE:                                     break;
    }
}

void land_on(GameState *g, int p, int sq, int diceTotal)
{
    switch (g->board[sq].type) {
    case SQ_PROPERTY:
    case SQ_RAILWAY:
    case SQ_UTILITY:
        land_on_purchasable(g, p, sq, diceTotal);
        break;

    case SQ_TAX:
        pay_income_tax(g, p);
        break;

    case SQ_COMMUNITY:                      /* D17: levies, never draws */
        pay_community_fund(g, p);
        break;

    /* Nothing to resolve. GO already paid during movement (Rule 4); Jail
       here is Just Visiting; Free Parking does nothing in this ruleset. */
    case SQ_GO:
    case SQ_JAIL:
    case SQ_PARKING:
        break;

    /* Rule 12: transferred, not walked. Deliberately not move_player -- that
       would pay the GO salary on the way round, and Rule 12 says the player
       does not collect it. */
    case SQ_GOTOJAIL:
        g->players[p].pos       = SQ_IDX_JAIL;
        g->players[p].jailed    = true;
        g->players[p].jailTurns = 0;
        printf("%s was sent to Jail.\n", g->players[p].name);
        break;

    case SQ_BANK:
        bank_visit(g, p);
        break;

    /* Appendix A. Squares 7, 22 and 36 only -- square 2 is SQ_COMMUNITY and
       levies rather than draws (D17), which is why there are three card
       squares on this board and not four. */
    case SQ_EVENT:
        draw_event_card(g, p);
        break;

    /* R1.9 and S1.2: one policy, one property, per landing -- the same
       one-action-per-square shape the Bank has. */
    case SQ_INSURANCE: {
        InsuranceType tier;
        int           target = decide_insurance(g, p, &tier);

        if (target >= 0) {
            buy_policy(g, p, target, tier);
        }
        break;
    }
    }
}

/* ---------------------------------------------------------------- jail -- */

/* Rule 3 step 1, for a jailed player. Returns whether they are free to move
 * this turn; the dice have already been rolled by the caller.
 *
 * Rule 13 gives three ways out, and this takes them in the order that costs
 * the player least: doubles are free, bail costs 300, and waiting costs a
 * turn. D10 settles what the rule leaves open -- after the third failed
 * turn bail is paid automatically, so nobody sits in Jail forever.
 *
 * The dice come in rather than being rolled here. Rolling separately would
 * give a released player two rolls in one turn, and returning "the turn is
 * over" after a doubles release would skip Rule 3 steps 4-7 -- the player
 * would move and then sail past whatever they landed on, paying no rent and
 * buying nothing. One roll, one move, one landing.
 */
static bool resolve_jail(GameState *g, int p, int d1, int d2)
{
    char    b[FMT_BUF];
    Player *pl = &g->players[p];

    if (!pl->jailed) {
        return true;
    }

    if (d1 == d2) {                             /* Rule 13: doubles       */
        pl->jailed = false;
        printf("%s rolled doubles and left Jail.\n", pl->name);
        return true;
    }

    if (charge(g, p, JAIL_BAIL, -1)) {          /* Rule 13: pay the bail  */
        pl->jailed = false;
        printf("%s paid LKR %s bail.\n", pl->name, fmt_lkr(b, JAIL_BAIL));
        return true;
    }

    pl->jailTurns++;
    if (pl->jailTurns >= JAIL_MAX_TURNS) {      /* D10: served the wait   */
        pl->jailed    = false;
        pl->jailTurns = 0;
        printf("%s has served three turns and leaves Jail.\n", pl->name);
        return true;
    }

    printf("%s is in Jail.\n", pl->name);
    return false;
}

/* ---------------------------------------------------------- invariants --- */

/* State that must hold after every turn, checked only in the debug build.
 * Both failures are silent in ordinary play -- they surface as rents and
 * loan capacities that are merely wrong -- so they are worth catching at the
 * turn that caused them.
 *
 * Rule 9's evenness is NOT here, and the omission is deliberate. See
 * assert_builds_evenly.
 */
#ifdef DEBUG
static void assert_invariants(const GameState *g)
{
    int i;

    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].houses > 0 && g->board[i].hotel) {
            fprintf(stderr, "R%d: square %d holds houses and a hotel (Rule 10)\n",
                    g->round, i);
            abort();
        }

        /* A lock outliving its loan would silently sterilise an asset:
           eligible_collateral would refuse to pledge it and the D11 ladder
           would refuse to mortgage it, forever. Both ends that clear a loan
           unlock, so this catches a third one appearing. */
        if (g->board[i].loanLocked) {
            int owner = g->board[i].owner;

            if (owner < 0 || !g->players[owner].loan.active) {
                fprintf(stderr, "R%d: square %d is loan-locked without a loan (LK 3)\n",
                        g->round, i);
                abort();
            }
        }
    }
}

/* Rule 9, checked where it actually applies: the moment a building is added.
 *
 * Deviation from the plan, which states this as a board invariant --
 * max(houses) - min(houses) <= 1 across every group, asserted each turn. It
 * cannot be one. Foreclosure demolishes the buildings on the squares a loan
 * pledged and leaves their groupmates untouched, so LK 6 itself produces a
 * group standing at 0 and 4; a player who buys the stripped square back at
 * auction then re-assembles a monopoly that is legitimately five levels
 * apart, and the builder needs several turns to level it up again. Every one
 * of those states is one the rules asked for, and the plan's invariant fired
 * on all of them -- on six of the seven seeds tried.
 *
 * The plan's wording is also incompatible with hotels on its own terms: a
 * hotel stores houses == 0, so a group holding one hotel and two four-house
 * properties reads max 4, min 0 and fails an invariant it satisfies. That is
 * why the comparison below is on development_level.
 *
 * What Rule 9 does guarantee is that no building is ever ADDED to a square
 * standing above its group's minimum. That is the property the even-building
 * rule is really about, it holds without exception, and it is what catches
 * the failure this guard exists for -- decide_build picking anything other
 * than the least developed square.
 */
static void assert_builds_evenly(const GameState *g, int sq)
{
    PropertyGroup grp   = g->board[sq].group;
    int           level = development_level(g, sq);
    int           i;

    for (i = 0; i < NUM_SQUARES; i++) {
        if (g->board[i].group == grp && development_level(g, i) < level) {
            fprintf(stderr,
                    "R%d: building on square %d at level %d over square %d at level %d (Rule 9)\n",
                    g->round, sq, level, i, development_level(g, i));
            abort();
        }
    }
}
#endif

/* --------------------------------------------------------- construction -- */

/* Rule 3 step 6. Builds until the strategy stops asking, which is what lets
 * Rule 9's "max houses immediately" personalities exist at all -- the rule
 * puts no cap on how many buildings one turn may raise.
 *
 * The loop terminates because every iteration raises one square's development
 * level by one and the levels are bounded: at most 5 per square across 22
 * properties, and each level costs money the player must already hold.
 *
 * Note the explicit cash test before charge(). Every other charge in the
 * program is a debt the rules impose, and reaching for the D11 recovery
 * ladder to meet one is correct. Construction is not a debt -- a player who
 * would have to sell buildings to fund a building is simply not building, so
 * this checks first and never lets a voluntary spend touch the ladder.
 */
static void build_step(GameState *g, int p)
{
    char b[FMT_BUF];
    int  sq;

    while ((sq = decide_build(g, p)) >= 0) {
        Square *s     = &g->board[sq];
        bool    hotel = (s->houses == MAX_HOUSES);
        int     cost  = building_cost(g, sq, hotel);

#ifdef DEBUG
        assert_builds_evenly(g, sq);
#endif

        if (g->players[p].cash < cost || !charge(g, p, cost, -1)) {
            return;
        }

        if (hotel) {
            /* Rule 10: a hotel REPLACES the four houses rather than joining
               them. Both fields are written so the two can never coexist. */
            s->houses = 0;
            s->hotel  = true;
            printf("%s upgraded %s to a Hotel.\n", g->players[p].name, s->name);
            /* Section 5 gives the hotel upgrade no cost line, unlike house
               construction. The asymmetry is in the template, not an
               oversight here. */
        } else {
            s->houses++;
            printf("%s constructed one house on %s.\n", g->players[p].name, s->name);
            printf("Construction Cost : LKR %s.\n", fmt_lkr(b, cost));
        }

        s->conditionPct       = 100;    /* LK 25: new work begins sound      */
        s->unmaintainedRounds = 0;
    }
}

/* --------------------------------------------------------- maintenance -- */

/* Rule 3 step 1 and LK 27. The rule is emphatic that maintenance happens
 * here and nowhere else -- not on landing, not at the end of the round -- so
 * a property that decays past a band edge stays there until its owner's next
 * turn comes round.
 *
 * Loops for the same reason build_step does: LK 27 allows any number of
 * buildings to be serviced if the owner can afford them. It terminates
 * because each pass restores one square to 100%, which puts it above the
 * threshold decide_maintenance selects on.
 *
 * Cash is tested before charging, on the same principle as construction:
 * upkeep is voluntary, and selling a building to fund the maintenance of
 * another building is not a trade the D11 ladder should ever be asked to
 * make.
 */
static void maintenance_step(GameState *g, int p)
{
    char b[FMT_BUF];
    int  sq;

    while ((sq = decide_maintenance(g, p)) >= 0) {
        Square *s    = &g->board[sq];
        int     cost = maintenance_cost(g, sq);

        if (g->players[p].cash < cost || !charge(g, p, cost, -1)) {
            return;
        }

        s->conditionPct       = 100;
        s->unmaintainedRounds = 0;    /* LK 28's clock restarts             */
        printf("%s maintained %s.\n", g->players[p].name, s->name);
        printf("Maintenance Cost : LKR %s.\n", fmt_lkr(b, cost));
    }
}


/* ------------------------------------------------------------- a turn --- */

/* Rule 3's eight steps. Steps arrive as the milestones implement them; the
   numbering below tracks the rule so the gaps stay visible. */
void play_turn(GameState *g, int p)
{
    int d1, d2, total;

    maintenance_step(g, p);                         /* 1. upkeep (LK 27)  */

    total = roll_dice(&d1, &d2);                    /* 2. roll two dice   */
    printf("%s rolled %d.\n", g->players[p].name, total);

    /* 1. resolve outstanding penalties. Also step 1, and it lands after the
       roll only because Rule 13's doubles test needs the dice. Maintenance
       above needs none, so it keeps its place at the head of the turn --
       and a player who stays in Jail has still had their upkeep, which is
       right: LK 27 ties maintenance to the turn, not to the movement. */
    if (!resolve_jail(g, p, d1, d2)) {
        return;
    }

    move_player(g, p, total);                       /* 3. move clockwise  */
    land_on(g, p, g->players[p].pos, total);        /* 4. landing action  */
    build_step(g, p);                               /* 6. construction    */

#ifdef DEBUG
    assert_invariants(g);
#endif
}

/* One round is one turn for every solvent player, in order[] sequence --
   which is the clarification's definition, and the reason D19 needs only a
   single clock: a player's own round count and the game's advance together,
   diverging only when the player leaves the game entirely. */
void play_round(GameState *g)
{
    int i;

    g->round++;

    for (i = 0; i < NUM_PLAYERS; i++) {
        int p = g->order[i];
        if (!g->players[p].bankrupt) {
            play_turn(g, p);
        }
    }

    /* D13 puts these last and in this order. The economic cadences slot in
       between the tick and the summary as the milestones introduce them.

       Interest before the default check is the one ordering that matters:
       LK 4 compounds first, so a loan defaults on the round its balance
       finally outgrows its term rather than a round later. */
    accrue_interest(g);             /* LK 4, D4                            */
    check_loan_default(g);          /* LK 6-7                              */
    condition_tick(g);              /* LK 25: buildings age by the round   */
    tick_insurance(g);              /* LK 9: policies lapse and warn       */
    auto_repairs(g);                /* LK 11: damage is a pause, not an end */

    /* The registry ages BEFORE this round's cadences fire, so that nothing
       created below is docked a round the moment it is created. D13 writes
       the tick after the cadences; see the note on tick_effects for why that
       defeats its own stated intent. */
    tick_effects(g);                /* D12, LK 35                          */
    tick_cooldowns(g);              /* LK 33                               */

    /* The cadenced systems, in D13's order. Each fires on its own clock and
       none of them knows about the others -- the registry is what lets them
       compose without a line of coordination here. */
    if (g->round % DEPREC_EVERY == 0) {
        depreciation_tick(g);       /* LK 16                               */
    }
    if (g->round % INFLATION_EVERY == 0) {
        draw_inflation(g);          /* LK 12-14                            */
    }
    if (g->round % MARKET_EVERY == 0) {
        market_review(g);           /* LK 30-33                            */
        fire_disaster(g);           /* LK 10, App E                        */
    }
    if (g->round % EVENT_EVERY == 0) {
        national_event(g);          /* LK 18: the whole board              */
        regional_card(g);           /* Table 4: named squares              */
    }
    if (g->round % REGULATION_EVERY == 0) {
        government_regulation(g);   /* LK 24                               */
    }

    round_summary(g);
    market_conditions(g);
}

/* Rule 15's first ending: the game stops when only one player is still
   solvent. Nobody can go bankrupt yet, so this is always false until
   milestone 3 -- but game_run is written against it now so the ending does
   not need retrofitting later. */
bool game_over(const GameState *g)
{
    int i, solvent = 0;

    for (i = 0; i < NUM_PLAYERS; i++) {
        if (!g->players[i].bankrupt) {
            solvent++;
        }
    }
    return solvent < 2;
}

void game_run(GameState *g)
{
    determine_order(g);

    while (g->round < MAX_ROUNDS && !game_over(g)) {
        play_round(g);
    }

    final_report(g);
}
