#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */
#define MAX_PLAYERS     2
#define ARENA_WIDTH     800
#define ARENA_HEIGHT    600
#define PLAYER_W        40
#define PLAYER_H        60
#define PLAYER_SPEED    4.0f
#define JUMP_VEL       -12.0f
#define GRAVITY         0.5f
#define GROUND_Y        460.0f   /* Y of the ground surface (top edge) */
#define ATTACK_RANGE    80.0f
#define ATTACK_DAMAGE   10
#define ATTACK_COOLDOWN 20       /* frames before the same player can hit again */
#define START_HEALTH    100

/* -------------------------------------------------------------------------
 * Input bitmask – shared between input.c and game.c
 * ---------------------------------------------------------------------- */
#define INPUT_LEFT   (1u << 0)
#define INPUT_RIGHT  (1u << 1)
#define INPUT_UP     (1u << 2)   /* jump */
#define INPUT_ATTACK (1u << 3)

/* -------------------------------------------------------------------------
 * GameState
 *
 * The *entire* mutable state of the simulation lives here as a flat,
 * pointer-free struct.  GGPO's save_game_state / load_game_state callbacks
 * simply memcpy this struct to/from a heap buffer, so:
 *
 *   - no pointers
 *   - no heap allocations
 *   - no OS handles
 *   - no global variables that affect simulation outcome
 *
 * If you add any new simulation state (e.g. projectiles, powerups) it MUST
 * go inside this struct.
 * ---------------------------------------------------------------------- */
typedef struct {
    float    pos_x[MAX_PLAYERS];
    float    pos_y[MAX_PLAYERS];
    float    vel_x[MAX_PLAYERS];
    float    vel_y[MAX_PLAYERS];
    int      health[MAX_PLAYERS];
    int      attack_cooldown[MAX_PLAYERS]; /* frames remaining on cooldown */
    bool     on_ground[MAX_PLAYERS];
    int      frame;
    uint32_t rng_state; /* xorshift32 seed – kept in state for determinism */
} GameState;

/* -------------------------------------------------------------------------
 * API
 * ---------------------------------------------------------------------- */

/* Reset gs to start-of-game values. */
void     game_init(GameState *gs);

/* Advance the simulation by one frame given inputs for both players.
 * This function MUST be fully deterministic: identical inputs on any machine
 * must produce identical output.  Never call rand(), time(), or I/O here. */
void     game_advance(GameState *gs, uint32_t inputs[MAX_PLAYERS]);

/* Fletcher-32 checksum over the entire GameState.  GGPO uses this to detect
 * desync between peers. */
uint32_t game_checksum(const GameState *gs);

/* Returns true once a player's health reaches zero. */
bool     game_is_over(const GameState *gs);

#endif /* GAME_H */
