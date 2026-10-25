#ifndef LCD_H
#define LCD_H

#include "game.h"

/* Two padded 16-character rows plus NUL; pure formatting, testable on a PC. */
void lcd_format_game(const game_state_t *game, char rows[2][17]);

/* Call after the 8 MHz, 1 ms SysTick has been configured. */
void lcd_init(void);
void lcd_show_game(const game_state_t *game);

#endif
