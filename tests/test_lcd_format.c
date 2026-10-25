/* LCD content tests require no GPIO access. Compare all 16 characters so
 * shorter messages cannot leave stale characters from a previous screen. */
#include "lcd.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect(const game_state_t *game, const char *top, const char *bottom)
{
    char rows[2][17];
    lcd_format_game(game, rows);
    assert(strlen(rows[0]) == 16U && strlen(rows[1]) == 16U);
    assert(strcmp(rows[0], top) == 0);
    assert(strcmp(rows[1], bottom) == 0);
}

static void test_summary_view(void)
{
    game_state_t game;
    char rows[2][17];
    game_init(&game);
    lcd_format_summary(&game, rows);
    assert(strcmp(rows[1], "Waiting for PC  ") == 0);
    game_connect(&game);
    lcd_format_summary(&game, rows);
    assert(strcmp(rows[1], "Select new game ") == 0);
    assert(game_new(&game) == GAME_MOVE_OK);

    /* Exact padding, mode, move count and turn match the authoritative state. */
    lcd_format_summary(&game, rows);
    assert(strcmp(rows[0], "Mode: 2 players ") == 0);
    assert(strcmp(rows[1], "Moves: 0 Turn: X") == 0);
    assert(game_play(&game, 0U) == GAME_MOVE_OK);
    lcd_format_summary(&game, rows);
    assert(strcmp(rows[1], "Moves: 1 Turn: O") == 0);
    game.ai_enabled = 1U;
    assert(game_play(&game, 4U) == GAME_MOVE_OK);
    lcd_format_summary(&game, rows);
    assert(strcmp(rows[0], "Mode: STM32 AI  ") == 0);
    assert(strcmp(rows[1], "Moves: 2 Turn: X") == 0);

    /* Every terminal result retains the final move count; longest text fits. */
    const game_result_t results[] = {GAME_RESULT_X_WON, GAME_RESULT_O_WON,
        GAME_RESULT_DRAW, GAME_RESULT_RESIGNED};
    const char *expected[] = {"Result: X wins  ", "Result: O wins  ",
        "Result: Draw    ", "Result: Resigned"};
    for (unsigned i = 0U; i < 4U; i++)
    {
        game.result = results[i];
        lcd_format_summary(&game, rows);
        assert(strcmp(rows[0], expected[i]) == 0);
        assert(strcmp(rows[1], "Moves played: 2 ") == 0);
    }
}

int main(void)
{
    test_summary_view();
    game_state_t game;
    /* Null/disconnected states have a useful boot screen. */
    expect(NULL, "STM32 TicTacToe ", "Waiting for PC  ");
    game_init(&game);
    expect(&game, "STM32 TicTacToe ", "Waiting for PC  ");
    game_connect(&game);
    expect(&game, "PC connected    ", "Select new game ");

    /* Display every cell in row order, mode and both human turns. */
    assert(game_new(&game) == GAME_MOVE_OK);
    expect(&game, "---/---/--- 2P  ", "Turn: X         ");
    assert(game_play(&game, 0U) == GAME_MOVE_OK);
    expect(&game, "X--/---/--- 2P  ", "Turn: O         ");
    assert(game_play(&game, 4U) == GAME_MOVE_OK);
    expect(&game, "X--/-O-/--- 2P  ", "Turn: X         ");

    /* Terminal messages are derived from server result, including resignation. */
    game.result = GAME_RESULT_X_WON;
    expect(&game, "X--/-O-/--- 2P  ", "X wins!         ");
    game.result = GAME_RESULT_O_WON;
    expect(&game, "X--/-O-/--- 2P  ", "O wins!         ");
    game.result = GAME_RESULT_DRAW;
    expect(&game, "X--/-O-/--- 2P  ", "Draw!           ");
    game.result = GAME_RESULT_RESIGNED;
    expect(&game, "X--/-O-/--- 2P  ", "Game resigned   ");

    /* AI status distinguishes the human turn and MCU victory. */
    game.ai_enabled = 1U;
    game.result = GAME_RESULT_PLAYING;
    expect(&game, "X--/-O-/--- AI  ", "Your turn: X    ");
    game.result = GAME_RESULT_O_WON;
    expect(&game, "X--/-O-/--- AI  ", "STM32 wins!     ");
    game_disconnect(&game);
    expect(&game, "STM32 TicTacToe ", "Waiting for PC  ");
    puts("LCD screen formatting checks passed.");
    return 0;
}
