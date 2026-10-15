#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define GAME_BOARD_SIZE 9U

typedef enum
{
    GAME_RESULT_NOT_STARTED = 0,
    GAME_RESULT_PLAYING,
    GAME_RESULT_X_WON,
    GAME_RESULT_O_WON,
    GAME_RESULT_DRAW,
    GAME_RESULT_RESIGNED
} game_result_t;

typedef enum
{
    GAME_MOVE_OK = 0,
    GAME_MOVE_NO_GAME,
    GAME_MOVE_RANGE,
    GAME_MOVE_OCCUPIED,
    GAME_MOVE_ILLEGAL
} game_move_status_t;

typedef struct
{
    char board[GAME_BOARD_SIZE + 1U];
    char current_player;
    uint8_t connected;
    uint8_t started;
    game_result_t result;
} game_state_t;

void game_init(game_state_t *game);
void game_connect(game_state_t *game);
void game_disconnect(game_state_t *game);
game_move_status_t game_new(game_state_t *game);
game_move_status_t game_play(game_state_t *game, uint32_t index);
game_move_status_t game_resign(game_state_t *game);

#endif
