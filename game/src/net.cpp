/*
 * net.cpp – GGPO session management and rollback callbacks.
 *
 * This file is compiled as C++ because ggponet.h uses C++ constructs.  It
 * exposes a plain-C API (via extern "C") so that main.c can stay pure C.
 *
 * Design principles
 * -----------------
 *  - All GGPO state (session pointer, player handles) lives here; no other
 *    translation unit should include ggponet.h.
 *  - The GameState pointer passed to net_start() is stored as s_gs.  Every
 *    GGPO callback reads/writes through that pointer, making rollback
 *    transparent to the rest of the codebase.
 *  - The callbacks follow the contract described in the GGPO documentation:
 *      save_game_state  – heap-allocate a snapshot, fill checksum
 *      load_game_state  – restore from snapshot
 *      free_buffer      – release the heap snapshot
 *      advance_frame    – re-simulate one frame during rollback
 */

#include "net.h"

#include <ggponet.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>

/* -------------------------------------------------------------------------
 * Module-level state
 * ---------------------------------------------------------------------- */
static GameState     *s_gs           = nullptr;
static GGPOSession   *s_session      = nullptr;
static GGPOPlayerHandle s_local_handle = GGPO_INVALID_HANDLE;

/* -------------------------------------------------------------------------
 * GGPO callbacks
 * ---------------------------------------------------------------------- */

/* Allocate and fill a snapshot of the current game state.
 * GGPO owns the returned buffer and will call free_buffer when done.      */
static bool cb_save_game_state(unsigned char **buffer,
                                int           *len,
                                int           *checksum,
                                int            /*frame*/)
{
    *len    = sizeof(GameState);
    *buffer = static_cast<unsigned char *>(malloc(*len));
    if (!*buffer) return false;

    memcpy(*buffer, s_gs, *len);
    *checksum = static_cast<int>(game_checksum(s_gs));
    return true;
}

/* Restore a previously saved snapshot. */
static bool cb_load_game_state(unsigned char *buffer, int len)
{
    if (len != sizeof(GameState)) return false;
    memcpy(s_gs, buffer, len);
    return true;
}

/* Release the buffer allocated in cb_save_game_state. */
static bool cb_free_buffer(void *buffer)
{
    free(buffer);
    return true;
}

/* Called by GGPO during rollback to re-simulate a single frame.
 *
 * The sequence inside this callback mirrors the normal per-frame sequence
 * in main.c, EXCEPT that we do NOT poll Raylib for input – GGPO supplies
 * the inputs for the frame being re-simulated via ggpo_synchronize_input.  */
static bool cb_advance_frame(int /*flags*/)
{
    uint32_t inputs[MAX_PLAYERS] = {0};
    int disconnect_flags = 0;

    GGPOErrorCode rc = ggpo_synchronize_input(
        s_session,
        inputs,
        sizeof(uint32_t) * MAX_PLAYERS,
        &disconnect_flags
    );

    if (GGPO_SUCCEEDED(rc)) {
        game_advance(s_gs, inputs);
        ggpo_advance_frame(s_session);
    }
    return true;
}

/* Log GGPO peer events to stdout for debugging. */
static bool cb_on_event(GGPOEvent *info)
{
    switch (info->code) {
        case GGPO_EVENTCODE_CONNECTED_TO_PEER:
            printf("[ggpo] connected to peer %d\n",
                   info->u.connected.player);
            break;
        case GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER:
            printf("[ggpo] synchronizing with peer %d (%d/%d)\n",
                   info->u.synchronizing.player,
                   info->u.synchronizing.count,
                   info->u.synchronizing.total);
            break;
        case GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER:
            printf("[ggpo] synchronized with peer %d\n",
                   info->u.synchronized.player);
            break;
        case GGPO_EVENTCODE_RUNNING:
            printf("[ggpo] running\n");
            break;
        case GGPO_EVENTCODE_DISCONNECTED_FROM_PEER:
            printf("[ggpo] disconnected from peer %d\n",
                   info->u.disconnected.player);
            break;
        case GGPO_EVENTCODE_TIMESYNC:
            /* GGPO is telling us to stall for info->u.timesync.frames_ahead
             * frames.  A real implementation would sleep here. */
            break;
        default:
            break;
    }
    return true;
}

static bool cb_begin_game(const char * /*game*/)   { return true; }
static bool cb_log_game_state(char * /*filename*/,
                               unsigned char * /*buf*/,
                               int /*len*/)        { return true; }

/* -------------------------------------------------------------------------
 * net_start
 * ---------------------------------------------------------------------- */
extern "C"
NetSession *net_start(NetConfig *cfg, GameState *gs)
{
    s_gs = gs;

    /* Fill the callbacks table. */
    GGPOSessionCallbacks cb;
    memset(&cb, 0, sizeof(cb));
    cb.begin_game      = cb_begin_game;
    cb.save_game_state = cb_save_game_state;
    cb.load_game_state = cb_load_game_state;
    cb.log_game_state  = cb_log_game_state;
    cb.free_buffer     = cb_free_buffer;
    cb.advance_frame   = cb_advance_frame;
    cb.on_event        = cb_on_event;

    GGPOSession *session = nullptr;
    GGPOErrorCode rc = ggpo_start_session(
        &session,
        &cb,
        cfg->game_name,
        cfg->num_players,
        static_cast<int>(sizeof(uint32_t)), /* input_size per player */
        cfg->local_port
    );
    if (!GGPO_SUCCEEDED(rc)) {
        fprintf(stderr, "[ggpo] ggpo_start_session failed: %d\n", rc);
        return nullptr;
    }

    ggpo_set_disconnect_timeout(session, 3000);
    ggpo_set_disconnect_notify_start(session, 1000);

    /* Register all players (both local and remote). */
    for (int i = 0; i < cfg->num_players; i++) {
        GGPOPlayer player;
        memset(&player, 0, sizeof(player));
        player.player_num = i + 1; /* GGPO uses 1-based player numbers */

        if (i == cfg->local_player) {
            player.type = GGPO_PLAYERTYPE_LOCAL;
        } else {
            player.type = GGPO_PLAYERTYPE_REMOTE;
            strncpy(player.u.remote.ip_address,
                    cfg->remote_ip,
                    sizeof(player.u.remote.ip_address) - 1);
            player.u.remote.port = cfg->remote_port;
        }

        GGPOPlayerHandle handle;
        rc = ggpo_add_player(session, &player, &handle);
        if (!GGPO_SUCCEEDED(rc)) {
            fprintf(stderr,
                    "[ggpo] ggpo_add_player failed for player %d: %d\n",
                    i, rc);
            ggpo_close_session(session);
            return nullptr;
        }

        /* Store the local handle so net_begin_frame can reference it. */
        if (i == cfg->local_player) {
            s_local_handle = handle;
            /* A small fixed input delay reduces the rollback window.
             * 2 frames is a common starting point; tune to your target
             * network latency. */
            ggpo_set_frame_delay(session, handle, cfg->input_delay);
        }
    }

    s_session = session;
    return static_cast<NetSession *>(session);
}

/* -------------------------------------------------------------------------
 * net_begin_frame
 *
 * Normal per-frame call sequence:
 *   1. ggpo_add_local_input  – queue this frame's local input
 *   2. ggpo_synchronize_input – merge local + remote inputs; may trigger
 *                               save/load/advance_frame callbacks internally
 *                               to perform rollback before returning.
 * ---------------------------------------------------------------------- */
extern "C"
bool net_begin_frame(NetSession *session,
                     uint32_t    local_input,
                     uint32_t    out_inputs[])
{
    GGPOSession *ggpo = static_cast<GGPOSession *>(session);

    /* Feed local input into GGPO.  GGPO timestamps it and queues it for
     * transmission to the remote peer. */
    GGPOErrorCode rc = ggpo_add_local_input(
        ggpo,
        s_local_handle,
        &local_input,
        sizeof(local_input)
    );
    if (!GGPO_SUCCEEDED(rc)) return false;

    /* Retrieve synchronised inputs for all players.  GGPO may stall here
     * (waiting for remote input) and/or trigger rollback callbacks before
     * returning.  On return, out_inputs[p] holds the confirmed input for
     * player p (0-based, matching our GameState arrays). */
    int disconnect_flags = 0;
    rc = ggpo_synchronize_input(
        ggpo,
        out_inputs,
        sizeof(uint32_t) * MAX_PLAYERS,
        &disconnect_flags
    );
    return GGPO_SUCCEEDED(rc);
}

/* -------------------------------------------------------------------------
 * net_end_frame
 * ---------------------------------------------------------------------- */
extern "C"
void net_end_frame(NetSession *session)
{
    ggpo_advance_frame(static_cast<GGPOSession *>(session));
}

/* -------------------------------------------------------------------------
 * net_idle
 * ---------------------------------------------------------------------- */
extern "C"
void net_idle(NetSession *session, int timeout_ms)
{
    ggpo_idle(static_cast<GGPOSession *>(session), timeout_ms);
}

/* -------------------------------------------------------------------------
 * net_shutdown
 * ---------------------------------------------------------------------- */
extern "C"
void net_shutdown(NetSession *session)
{
    if (session) {
        ggpo_close_session(static_cast<GGPOSession *>(session));
        s_session      = nullptr;
        s_gs           = nullptr;
        s_local_handle = GGPO_INVALID_HANDLE;
    }
}
