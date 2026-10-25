#ifndef GAME_AI_H
#define GAME_AI_H

#include "game.h"

/* Return an optimal legal O move, or GAME_BOARD_SIZE if none is available.
 * Selection never modifies the supplied game. */
uint32_t game_ai_choose_move(const game_state_t *game);

#endif
