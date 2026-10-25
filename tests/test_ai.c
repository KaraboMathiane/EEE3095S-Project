/* Explore every human choice against the actual protocol and AI, rather than
 * testing only a few favourable games. Assertions use an independent winner
 * check so engine and AI cannot hide a missed X win from each other. */
#include "game_ai.h"
#include "server_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned terminals;

static int won(const char *b, char p)
{
    for (int i = 0; i < 3; i++)
    {
        if (b[3*i] == p && b[3*i+1] == p && b[3*i+2] == p) return 1;
        if (b[i] == p && b[i+3] == p && b[i+6] == p) return 1;
    }
    return (b[0] == p && b[4] == p && b[8] == p) ||
           (b[2] == p && b[4] == p && b[6] == p);
}

static void explore(const game_state_t *state)
{
    assert(!won(state->board, 'X'));
    if (state->result != GAME_RESULT_PLAYING)
    {
        assert(state->result == GAME_RESULT_O_WON || state->result == GAME_RESULT_DRAW);
        assert((state->result == GAME_RESULT_O_WON) == won(state->board, 'O'));
        if (state->result == GAME_RESULT_DRAW) assert(strchr(state->board, '-') == NULL);
        terminals++;
        return;
    }
    assert(state->current_player == 'X');
    for (unsigned i = 0; i < GAME_BOARD_SIZE; i++)
    {
        game_state_t next = *state;
        char command[16], reply[64];
        unsigned added_o = 0U;
        if (state->board[i] != '-') continue;
        (void)snprintf(command, sizeof(command), "MOVE:%u", i);
        assert(server_handle_command(&next, command, reply, sizeof(reply)));
        assert(next.board[i] == 'X');
        for (unsigned j = 0; j < GAME_BOARD_SIZE; j++)
        {
            if (state->board[j] != '-') assert(next.board[j] == state->board[j]);
            if (j != i && state->board[j] == '-' && next.board[j] != '-')
            {
                assert(next.board[j] == 'O');
                added_o++;
            }
        }
        assert(added_o <= 1U);
        if (next.result == GAME_RESULT_PLAYING) assert(added_o == 1U);
        if (next.result == GAME_RESULT_PLAYING) assert(strncmp(reply, "OK:BOARD:", 9) == 0);
        else assert(strcmp(reply, next.result == GAME_RESULT_DRAW ? "DRAW" : "WIN:O") == 0);
        explore(&next);
    }
}

int main(void)
{
    game_state_t game, before;
    char reply[64];
    game_init(&game);
    assert(game_ai_choose_move(NULL) == GAME_BOARD_SIZE);
    assert(game_ai_choose_move(&game) == GAME_BOARD_SIZE);
    assert(server_handle_command(&game, "NEW:AI", reply, sizeof(reply)));
    assert(strcmp(reply, "NACK:NOGAME") == 0);
    game_connect(&game);
    assert(server_handle_command(&game, "NEW:AI", reply, sizeof(reply)));
    assert(game.ai_enabled && strcmp(reply, "OK:BOARD:---------") == 0);
    explore(&game);
    assert(terminals > 0U);

    /* Search must not change the board, turn or result; centre blocks this row. */
    assert(game_play(&game, 0U) == GAME_MOVE_OK);
    before = game;
    assert(game_ai_choose_move(&game) == 4U);
    assert(memcmp(&game, &before, sizeof(game)) == 0);

    /* A client cannot submit O's turn, even if such an intermediate state is injected. */
    assert(server_handle_command(&game, "MOVE:1", reply, sizeof(reply)));
    assert(strcmp(reply, "NACK:NOTURN") == 0);
    assert(memcmp(&game, &before, sizeof(game)) == 0);

    /* Starting a standard game explicitly returns to two-human mode. */
    assert(server_handle_command(&game, "NEW", reply, sizeof(reply)));
    assert(!game.ai_enabled);
    assert(server_handle_command(&game, "MOVE:0", reply, sizeof(reply)));
    assert(strcmp(reply, "OK:BOARD:X--------") == 0);
    game_disconnect(&game);
    assert(!game.ai_enabled);
    printf("Explored %u terminal human-choice paths; no X wins.\n", terminals);
    return 0;
}
