#include "input.h"
#include "game.h"     /* INPUT_* bitmask constants */
#include "raylib.h"

/* Translate Raylib keyboard / gamepad state into the game's input bitmask.
 *
 * Only the *local* player's input is collected here.  GGPO delivers the
 * remote player's inputs through its own synchronisation path; this function
 * is never called for remote players.
 *
 * Extend the switch-statement for gamepad support or a second local player
 * if you want local-vs-AI or local-vs-local modes without networking.     */
uint32_t input_collect(int player_index)
{
    (void)player_index; /* reserved for multi-gamepad setups */

    uint32_t input = 0;

    if (IsKeyDown(KEY_LEFT))  input |= INPUT_LEFT;
    if (IsKeyDown(KEY_RIGHT)) input |= INPUT_RIGHT;
    if (IsKeyDown(KEY_UP))    input |= INPUT_UP;
    if (IsKeyDown(KEY_SPACE)) input |= INPUT_ATTACK;

    return input;
}
