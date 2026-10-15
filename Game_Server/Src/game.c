#include "game.h"

#include <stddef.h>

static const uint8_t winning_lines[8][3] = {
    {0U, 1U, 2U},
    {3U, 4U, 5U},
    {6U, 7U, 8U},
    {0U, 3U, 6U},
    {1U, 4U, 7U},
    {2U, 5U, 8U},
    {0U, 4U, 8U},
    {2U, 4U, 6U}
};

static game_result_t game_check_result(const game_state_t *game)
{
    for (size_t i = 0U; i < sizeof(winning_lines) / sizeof(winning_lines[0]); i++)
    {
        const uint8_t first = winning_lines[i][0];
        const char mark = game->board[first];

        if ((mark != '-') &&
            (mark == game->board[winning_lines[i][1]]) &&
            (mark == game->board[winning_lines[i][2]]))
        {
            return (mark == 'X') ? GAME_RESULT_X_WON : GAME_RESULT_O_WON;
        }
    }

    for (size_t i = 0U; i < GAME_BOARD_SIZE; i++)
    {
        if (game->board[i] == '-')
        {
            return GAME_RESULT_PLAYING;
        }
    }

    return GAME_RESULT_DRAW;
}

void game_init(game_state_t *game)
{
    if (game == NULL)
    {
        return;
    }

    for (size_t i = 0U; i < GAME_BOARD_SIZE; i++)
    {
        game->board[i] = '-';
    }
    game->board[GAME_BOARD_SIZE] = '\0';
    game->current_player = 'X';
    game->connected = 0U;
    game->started = 0U;
    game->result = GAME_RESULT_NOT_STARTED;
}

void game_connect(game_state_t *game)
{
    if ((game == NULL) || (game->connected != 0U))
    {
        return;
    }

    game_init(game);
    game->connected = 1U;
}

void game_disconnect(game_state_t *game)
{
    game_init(game);
}

game_move_status_t game_new(game_state_t *game)
{
    if ((game == NULL) || (game->connected == 0U))
    {
        return GAME_MOVE_NO_GAME;
    }

    for (size_t i = 0U; i < GAME_BOARD_SIZE; i++)
    {
        game->board[i] = '-';
    }
    game->board[GAME_BOARD_SIZE] = '\0';
    game->current_player = 'X';
    game->started = 1U;
    game->result = GAME_RESULT_PLAYING;
    return GAME_MOVE_OK;
}

game_move_status_t game_play(game_state_t *game, uint32_t index)
{
    if (index >= GAME_BOARD_SIZE)
    {
        return GAME_MOVE_RANGE;
    }
    if ((game == NULL) || (game->connected == 0U) || (game->started == 0U))
    {
        return GAME_MOVE_NO_GAME;
    }
    if (game->result != GAME_RESULT_PLAYING)
    {
        return GAME_MOVE_ILLEGAL;
    }
    if (game->board[index] != '-')
    {
        return GAME_MOVE_OCCUPIED;
    }

    game->board[index] = game->current_player;
    game->result = game_check_result(game);
    if (game->result == GAME_RESULT_PLAYING)
    {
        game->current_player = (game->current_player == 'X') ? 'O' : 'X';
    }
    return GAME_MOVE_OK;
}

game_move_status_t game_resign(game_state_t *game)
{
    if ((game == NULL) || (game->connected == 0U) || (game->started == 0U))
    {
        return GAME_MOVE_NO_GAME;
    }
    if (game->result != GAME_RESULT_PLAYING)
    {
        return GAME_MOVE_ILLEGAL;
    }

    game->result = GAME_RESULT_RESIGNED;
    return GAME_MOVE_OK;
}
