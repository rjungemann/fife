#include "game.h"

#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Deterministic xorshift32 PRNG
 *
 * The seed is stored *inside* GameState so both peers always advance the
 * same sequence regardless of timing.  Call this instead of rand().
 * ---------------------------------------------------------------------- */
static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Silence unused-function warning when PRNG isn't needed yet in advance(). */
static inline void rng_unused_suppress(GameState *gs) { (void)gs; (void)xorshift32; }

/* -------------------------------------------------------------------------
 * game_checksum
 *
 * Fletcher-32 over the raw bytes of GameState.  GGPO passes the value
 * returned here to the remote peer; if they disagree the session is
 * declared desynced.  A strong checksum catches non-determinism bugs early.
 * ---------------------------------------------------------------------- */
uint32_t game_checksum(const GameState *gs)
{
    const uint8_t *data = (const uint8_t *)gs;
    size_t len = sizeof(GameState);
    uint32_t sum1 = 0, sum2 = 0;
    for (size_t i = 0; i < len; i++) {
        sum1 = (sum1 + data[i]) % 65535u;
        sum2 = (sum2 + sum1)    % 65535u;
    }
    return (sum2 << 16) | sum1;
}

/* -------------------------------------------------------------------------
 * game_init
 * ---------------------------------------------------------------------- */
void game_init(GameState *gs)
{
    memset(gs, 0, sizeof(*gs));

    /* Player 0 starts on the left, player 1 on the right. */
    gs->pos_x[0] = 160.0f;  gs->pos_y[0] = GROUND_Y;
    gs->pos_x[1] = 560.0f;  gs->pos_y[1] = GROUND_Y;

    gs->health[0] = START_HEALTH;
    gs->health[1] = START_HEALTH;

    gs->rng_state = 12345u;
    gs->frame     = 0;
}

/* -------------------------------------------------------------------------
 * game_advance
 *
 * One deterministic simulation tick.  Called both from the normal main loop
 * AND from GGPO's advance_frame callback during rollback re-simulation.
 * ---------------------------------------------------------------------- */
void game_advance(GameState *gs, uint32_t inputs[MAX_PLAYERS])
{
    rng_unused_suppress(gs);

    /* --- Per-player movement and physics --- */
    for (int p = 0; p < MAX_PLAYERS; p++) {

        /* Horizontal movement */
        if      (inputs[p] & INPUT_LEFT)  gs->vel_x[p] = -PLAYER_SPEED;
        else if (inputs[p] & INPUT_RIGHT) gs->vel_x[p] =  PLAYER_SPEED;
        else                               gs->vel_x[p] =  0.0f;

        /* Jump – only when touching the ground */
        if ((inputs[p] & INPUT_UP) && gs->on_ground[p]) {
            gs->vel_y[p]     = JUMP_VEL;
            gs->on_ground[p] = false;
        }

        /* Gravity */
        gs->vel_y[p] += GRAVITY;

        /* Integrate */
        gs->pos_x[p] += gs->vel_x[p];
        gs->pos_y[p] += gs->vel_y[p];

        /* Ground collision */
        if (gs->pos_y[p] >= GROUND_Y) {
            gs->pos_y[p]     = GROUND_Y;
            gs->vel_y[p]     = 0.0f;
            gs->on_ground[p] = true;
        }

        /* Arena left/right walls */
        if (gs->pos_x[p] < 0.0f)
            gs->pos_x[p] = 0.0f;
        if (gs->pos_x[p] > (float)(ARENA_WIDTH - PLAYER_W))
            gs->pos_x[p] = (float)(ARENA_WIDTH - PLAYER_W);

        /* Tick down attack cooldown */
        if (gs->attack_cooldown[p] > 0)
            gs->attack_cooldown[p]--;
    }

    /* --- Attack resolution ---
     * Iterate every player as a potential attacker.  Using a separate pass
     * ensures both players' attacks are checked against the state *before*
     * either resolves, avoiding ordering artefacts. */
    for (int attacker = 0; attacker < MAX_PLAYERS; attacker++) {
        int target = 1 - attacker;
        if ((inputs[attacker] & INPUT_ATTACK) &&
            gs->attack_cooldown[attacker] == 0)
        {
            float dx = gs->pos_x[target] - gs->pos_x[attacker];
            if (fabsf(dx) < ATTACK_RANGE) {
                gs->health[target] -= ATTACK_DAMAGE;
                if (gs->health[target] < 0)
                    gs->health[target] = 0;

                gs->attack_cooldown[attacker] = ATTACK_COOLDOWN;

                /* Knockback – deterministic, no RNG needed here */
                gs->vel_x[target]    = (dx >= 0.0f) ?  6.0f : -6.0f;
                gs->vel_y[target]    = -4.0f;
                gs->on_ground[target] = false;
            }
        }
    }

    gs->frame++;
}

/* -------------------------------------------------------------------------
 * game_is_over
 * ---------------------------------------------------------------------- */
bool game_is_over(const GameState *gs)
{
    return gs->health[0] <= 0 || gs->health[1] <= 0;
}
