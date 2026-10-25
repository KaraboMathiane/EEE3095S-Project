#include "game_ai.h"

#include <stddef.h>

/* Centre, corners, then edges provide deterministic tie-breaking. */
static const uint8_t move_order[GAME_BOARD_SIZE] = {4U, 0U, 2U, 6U, 8U, 1U, 3U, 5U, 7U};

/* O maximises the score; X minimises it. Alpha-beta bounds skip branches
 * that cannot improve the result. Each recursion owns a small state copy;
 * there is no heap allocation and depth is bounded by the empty cells. */
static int minimax(const game_state_t *game, int depth, int alpha, int beta)
{
    int best = (game->current_player == 'O') ? -100 : 100;

    if (game->result == GAME_RESULT_O_WON) return 10 - depth;
    if (game->result == GAME_RESULT_X_WON) return depth - 10;
    if (game->result != GAME_RESULT_PLAYING) return 0;

    for (size_t i = 0U; i < GAME_BOARD_SIZE; i++)
    {
        const uint32_t index = move_order[i];
        game_state_t child;
        int score;

        if (game->board[index] != '-') continue;
        child = *game;
        (void)game_play(&child, index);
        score = minimax(&child, depth + 1, alpha, beta);

        if (game->current_player == 'O')
        {
            if (score > best) best = score;
            if (best > alpha) alpha = best;
        }
        else
        {
            if (score < best) best = score;
            if (best < beta) beta = best;
        }
        if (alpha >= beta) break;
    }
    return best;
}

uint32_t game_ai_choose_move(const game_state_t *game)
{
    uint32_t chosen = GAME_BOARD_SIZE;
    int best = -100;

    if ((game == NULL) || (game->connected == 0U) || (game->started == 0U) ||
        (game->result != GAME_RESULT_PLAYING) || (game->current_player != 'O'))
    {
        return chosen;
    }

    for (size_t i = 0U; i < GAME_BOARD_SIZE; i++)
    {
        const uint32_t index = move_order[i];
        game_state_t child;
        int score;

        if (game->board[index] != '-') continue;
        child = *game;
        (void)game_play(&child, index);
        score = minimax(&child, 0, -100, 100);
        if (score > best)
        {
            best = score;
            chosen = index;
        }
    }
    return chosen;
}
