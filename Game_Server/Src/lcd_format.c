#include "lcd.h"

#include <stddef.h>
#include <string.h>

void lcd_format_game(const game_state_t *game, char rows[2][17])
{
    const char *status = "Select new game";
    memset(rows[0], ' ', 16U);
    memset(rows[1], ' ', 16U);
    rows[0][16] = rows[1][16] = '\0';

    if ((game == NULL) || (game->connected == 0U))
    {
        memcpy(rows[0], "STM32 TicTacToe", 15U);
        memcpy(rows[1], "Waiting for PC", 14U);
        return;
    }
    if (game->started == 0U)
    {
        memcpy(rows[0], "PC connected", 12U);
    }
    else
    {
        /* All nine cells fit: three rows separated by '/', followed by mode. */
        for (size_t i = 0U; i < 9U; i++) rows[0][i + i / 3U] = game->board[i];
        rows[0][3] = rows[0][7] = '/';
        memcpy(&rows[0][12], game->ai_enabled ? "AI" : "2P", 2U);
        switch (game->result)
        {
            case GAME_RESULT_PLAYING:
                status = game->ai_enabled ? "Your turn: X" :
                         (game->current_player == 'X' ? "Turn: X" : "Turn: O");
                break;
            case GAME_RESULT_X_WON: status = "X wins!"; break;
            case GAME_RESULT_O_WON: status = game->ai_enabled ? "STM32 wins!" : "O wins!"; break;
            case GAME_RESULT_DRAW: status = "Draw!"; break;
            case GAME_RESULT_RESIGNED: status = "Game resigned"; break;
            default: break;
        }
    }
    memcpy(rows[1], status, strlen(status));
}
