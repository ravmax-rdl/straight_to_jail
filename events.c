/* events.c -- the economy: timed effects, cadenced systems, the card deck.
 *
 * Milestone 2 needs only one thing from this file: effect_modifier, in its
 * final shape but stubbed to zero. That is what lets the choke points in
 * board.c be written once, now, rather than written for a static board and
 * retrofitted in milestone 4 when the first timed effect appears.
 *
 * The registry itself (D12) arrives in milestone 4.
 */

#include "types.h"

/* Sum the magnitudes of every active effect of this kind that reaches this
 * square and this player, as a signed percentage for apply_pct.
 *
 * LK 34 requires concurrent effects to be cumulative, so the composition is
 * a sum rather than a chain of multiplications: one rounding, and expiry
 * subtracts exactly what activation added. The architecture document
 * section 5 works through why that matters.
 *
 * player is the participant the value is being read *for* -- the owner of
 * the square, in practice. Appendix A cards attach to the player who drew
 * them, so an effect with owner == -1 reaches everyone while one with a
 * real owner reaches only that player's holdings.
 *
 * MILESTONE 4 replaces this body. The signature is final; callers written
 * against it now will not change.
 */
int effect_modifier(const GameState *g, EffectKind kind, int square, int player)
{
    (void)g;
    (void)kind;
    (void)square;
    (void)player;
    return 0;
}
