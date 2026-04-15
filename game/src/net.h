#ifndef NET_H
#define NET_H

/* net.h – C-compatible interface to the GGPO networking layer.
 *
 * net.cpp is compiled as C++ (because ggponet.h is a C++ header) but
 * exposes a plain-C API so that main.c can remain pure C.
 *
 * Internals of GGPO (GGPOSession*, player handles, etc.) are hidden behind
 * an opaque NetSession pointer to avoid leaking C++ types into C translation
 * units.                                                                    */

#ifdef __cplusplus
extern "C" {
#endif

#include "game.h"
#include <stdbool.h>

/* Opaque handle – callers treat this as a cookie. */
typedef void NetSession;

/* Session configuration passed to net_start(). */
typedef struct {
    const char    *game_name;
    int            num_players;
    int            local_player;    /* 0-based index of the local player    */
    unsigned short local_port;
    const char    *remote_ip;
    unsigned short remote_port;
    int            input_delay;     /* frames of deliberate input delay (1-3) */
} NetConfig;

/* Start a GGPO peer-to-peer session.
 *
 * gs must remain valid for the lifetime of the session; GGPO callbacks read
 * and write it directly.
 *
 * Returns NULL on failure. */
NetSession *net_start(NetConfig *cfg, GameState *gs);

/* Per-frame networking – call once at the top of the game loop:
 *
 *   1. Feeds local_input into GGPO.
 *   2. Calls ggpo_synchronize_input, which may trigger save/load/advance
 *      callbacks internally to perform rollback before returning.
 *   3. Fills out_inputs[MAX_PLAYERS] with the confirmed inputs for this frame.
 *
 * Returns false when the session is lost or inputs cannot be synchronised. */
bool net_begin_frame(NetSession *session,
                     uint32_t    local_input,
                     uint32_t    out_inputs[/* MAX_PLAYERS */]);

/* Notify GGPO that this frame has been fully simulated.
 * Call once per frame, after game_advance(). */
void net_end_frame(NetSession *session);

/* Let GGPO send/receive pending UDP packets.
 * Pass the remaining budget for this frame (milliseconds); 0 is safe. */
void net_idle(NetSession *session, int timeout_ms);

/* Tear down the GGPO session. */
void net_shutdown(NetSession *session);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NET_H */
