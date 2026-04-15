#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

/* Collect the local player's current input as a bitmask.
 *
 * player_index – 0-based local player index (for gamepad selection etc.)
 *
 * The returned value uses the INPUT_* flags defined in game.h.
 * This function is only called ONCE per frame, before handing the result
 * to ggpo_add_local_input.  Remote player inputs arrive via GGPO and are
 * never polled here. */
uint32_t input_collect(int player_index);

#endif /* INPUT_H */
