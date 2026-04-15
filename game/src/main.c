/*
 * main.c – Raylib window initialisation and the GGPO-aware main loop.
 *
 * Usage
 * -----
 *   ./game <local_port> <remote_ip> <remote_port> [input_delay]
 *
 *   Player 1 (left side):   ./game 7001 127.0.0.1 7002 2
 *   Player 2 (right side):  ./game 7002 127.0.0.1 7001 2
 *
 * The player with the lower port number is assigned to player slot 0.
 *
 * Main loop structure (the GGPO contract)
 * ----------------------------------------
 *   Each frame:
 *
 *   1. Poll local Raylib input → bitmask.
 *   2. net_begin_frame()
 *        a. ggpo_add_local_input()        — queue our input for this frame
 *        b. ggpo_synchronize_input()      — may internally perform rollback:
 *             • save_game_state callback  — snapshot current gs
 *             • load_game_state callback  — restore an older snapshot
 *             • advance_frame callback    — re-simulate a frame
 *        c. returns merged inputs[MAX_PLAYERS] for the current frame
 *   3. game_advance(&gs, inputs)          — one deterministic simulation tick
 *   4. net_end_frame()                    — ggpo_advance_frame notification
 *   5. render(&gs)                        — draw the current (confirmed) state
 *   6. net_idle()                         — let GGPO flush/receive UDP packets
 *
 * Rendering is always driven by the live GameState.  When GGPO rolls back it
 * restores an older snapshot, re-simulates forward, then returns.  By the
 * time render() is called the state is already at the correct frame.
 */

#include "raylib.h"
#include "game.h"
#include "input.h"
#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Compile-time settings
 * ---------------------------------------------------------------------- */
#define WINDOW_WIDTH  ARENA_WIDTH
#define WINDOW_HEIGHT ARENA_HEIGHT
#define TARGET_FPS    60
#define GAME_NAME     "ggpo-raylib-demo"

/* -------------------------------------------------------------------------
 * Rendering
 * ---------------------------------------------------------------------- */
static Color s_player_color[MAX_PLAYERS] = { RED, BLUE };

static void render_player(const GameState *gs, int p)
{
    DrawRectangle(
        (int)gs->pos_x[p],
        (int)gs->pos_y[p],
        PLAYER_W, PLAYER_H,
        s_player_color[p]
    );

    /* Health bar above the player sprite */
    int bar_x = (int)gs->pos_x[p];
    int bar_y = (int)gs->pos_y[p] - 14;
    DrawRectangle(bar_x, bar_y, PLAYER_W, 8, DARKGRAY);
    DrawRectangle(bar_x, bar_y,
                  (int)(PLAYER_W * gs->health[p] / (float)START_HEALTH),
                  8, GREEN);
}

static void render_hud(const GameState *gs)
{
    DrawText(TextFormat("P1 HP: %d", gs->health[0]),
             10, 10, 20, RED);
    DrawText(TextFormat("P2 HP: %d", gs->health[1]),
             WINDOW_WIDTH - 130, 10, 20, BLUE);
    DrawText(TextFormat("Frame: %d", gs->frame),
             WINDOW_WIDTH / 2 - 50, 10, 20, LIGHTGRAY);
}

static void render(const GameState *gs)
{
    BeginDrawing();
    ClearBackground(BLACK);

    /* Ground platform */
    DrawRectangle(0, (int)GROUND_Y + PLAYER_H, WINDOW_WIDTH, 20, DARKGRAY);

    for (int p = 0; p < MAX_PLAYERS; p++)
        render_player(gs, p);

    render_hud(gs);

    if (game_is_over(gs)) {
        int winner = (gs->health[0] <= 0) ? 2 : 1;
        const char *msg = TextFormat("PLAYER %d WINS!", winner);
        int tw = MeasureText(msg, 40);
        DrawText(msg, WINDOW_WIDTH / 2 - tw / 2, WINDOW_HEIGHT / 2 - 20,
                 40, YELLOW);
    }

    EndDrawing();
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr,
            "Usage: %s <local_port> <remote_ip> <remote_port> [input_delay]\n"
            "\n"
            "  Player 1:  %s 7001 127.0.0.1 7002 2\n"
            "  Player 2:  %s 7002 127.0.0.1 7001 2\n",
            argv[0], argv[0], argv[0]);
        return 1;
    }

    unsigned short local_port  = (unsigned short)atoi(argv[1]);
    const char    *remote_ip   = argv[2];
    unsigned short remote_port = (unsigned short)atoi(argv[3]);
    int            input_delay = (argc >= 5) ? atoi(argv[4]) : 2;

    /* Assign player slots by port order so both sides agree without a
     * separate handshake: lower port → player 0, higher port → player 1. */
    int local_player = (local_port < remote_port) ? 0 : 1;

    /* -------------------------------------------------------------------------
     * Initialise game state
     * ---------------------------------------------------------------------- */
    GameState gs;
    game_init(&gs);

    /* -------------------------------------------------------------------------
     * Initialise GGPO
     * ---------------------------------------------------------------------- */
    NetConfig cfg = {
        .game_name    = GAME_NAME,
        .num_players  = MAX_PLAYERS,
        .local_player = local_player,
        .local_port   = local_port,
        .remote_ip    = remote_ip,
        .remote_port  = remote_port,
        .input_delay  = input_delay,
    };

    NetSession *session = net_start(&cfg, &gs);
    if (!session) {
        fprintf(stderr, "Failed to start GGPO session.\n");
        return 1;
    }

    /* -------------------------------------------------------------------------
     * Raylib window
     * ---------------------------------------------------------------------- */
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, GAME_NAME);
    SetTargetFPS(TARGET_FPS);

    /* -------------------------------------------------------------------------
     * Main loop
     * ---------------------------------------------------------------------- */
    while (!WindowShouldClose() && !game_is_over(&gs)) {
        /* Step 1 – collect this frame's local input using Raylib.
         * This is the ONLY place we read keyboard/gamepad state.  Remote
         * inputs always arrive through GGPO. */
        uint32_t local_input = input_collect(local_player);

        /* Step 2 – hand input to GGPO and get back synchronised inputs.
         * Internally GGPO may call save/load/advance_frame callbacks to
         * perform rollback before returning merged inputs for both players. */
        uint32_t inputs[MAX_PLAYERS] = {0};
        bool ok = net_begin_frame(session, local_input, inputs);

        /* Step 3 – advance the simulation by one deterministic tick. */
        if (ok) {
            game_advance(&gs, inputs);
        }

        /* Step 4 – tell GGPO we finished advancing this frame. */
        net_end_frame(session);

        /* Step 5 – render the current game state. */
        render(&gs);

        /* Step 6 – let GGPO flush outgoing and process incoming UDP packets.
         * Passing 0 ms means "don't block; just poll once". */
        net_idle(session, 0);
    }

    /* Show winner screen briefly before exiting. */
    if (game_is_over(&gs)) {
        double deadline = GetTime() + 3.0;
        while (!WindowShouldClose() && GetTime() < deadline)
            render(&gs);
    }

    /* -------------------------------------------------------------------------
     * Cleanup
     * ---------------------------------------------------------------------- */
    CloseWindow();
    net_shutdown(session);
    return 0;
}
