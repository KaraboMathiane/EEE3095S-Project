#include "game.h"
#include "hardware_behavior.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static uint8_t tick_many(
    hardware_behavior_t *behavior,
    const game_state_t *game,
    uint8_t button_pressed,
    uint32_t count)
{
    uint8_t leds = 0U;

    for (uint32_t i = 0U; i < count; i++)
    {
        leds = hardware_behavior_tick(behavior, game, button_pressed);
    }

    return leds;
}

static void test_turn_leds_follow_server_turn(void)
{
    game_state_t game;
    hardware_behavior_t behavior;

    game_init(&game);
    game_connect(&game);
    assert(game_new(&game) == GAME_MOVE_OK);
    hardware_behavior_init(&behavior);

    assert(tick_many(&behavior, &game, 0U, 30U) == HARDWARE_LED_X);
    assert(game_play(&game, 0U) == GAME_MOVE_OK);
    assert(tick_many(&behavior, &game, 0U, 1U) == HARDWARE_LED_O);
}

static void test_button_debounces_and_toggles_indicators_once_per_press(void)
{
    game_state_t game;
    hardware_behavior_t behavior;

    game_init(&game);
    game_connect(&game);
    assert(game_new(&game) == GAME_MOVE_OK);
    hardware_behavior_init(&behavior);
    (void)tick_many(&behavior, &game, 0U, 30U);

    (void)tick_many(&behavior, &game, 1U, 10U);
    (void)tick_many(&behavior, &game, 0U, 10U);
    assert(tick_many(&behavior, &game, 0U, 1U) == HARDWARE_LED_X);

    assert(tick_many(&behavior, &game, 1U, 30U) == 0U);
    assert(tick_many(&behavior, &game, 1U, 100U) == 0U);

    (void)tick_many(&behavior, &game, 0U, 30U);
    assert(tick_many(&behavior, &game, 1U, 30U) == HARDWARE_LED_X);
    assert(game.board[0] == '-');
    assert(game.current_player == 'X');
    assert(game.result == GAME_RESULT_PLAYING);
}

static void test_game_over_blinks_both_leds(void)
{
    static const uint32_t moves[] = {0U, 3U, 1U, 4U, 2U};
    game_state_t game;
    hardware_behavior_t behavior;
    uint8_t leds = 0U;

    game_init(&game);
    game_connect(&game);
    assert(game_new(&game) == GAME_MOVE_OK);
    hardware_behavior_init(&behavior);

    for (size_t i = 0U; i < sizeof(moves) / sizeof(moves[0]); i++)
    {
        assert(game_play(&game, moves[i]) == GAME_MOVE_OK);
    }
    assert(game.result == GAME_RESULT_X_WON);

    assert(hardware_behavior_tick(&behavior, &game, 0U) == HARDWARE_LED_BOTH);
    leds = tick_many(&behavior, &game, 0U, 248U);
    assert(leds == HARDWARE_LED_BOTH);
    assert(hardware_behavior_tick(&behavior, &game, 0U) == 0U);
}

static void test_all_terminal_results_activate_game_over_indicators(void)
{
    static const game_result_t results[] = {
        GAME_RESULT_X_WON,
        GAME_RESULT_O_WON,
        GAME_RESULT_DRAW,
        GAME_RESULT_RESIGNED
    };

    for (size_t i = 0U; i < sizeof(results) / sizeof(results[0]); i++)
    {
        game_state_t game;
        hardware_behavior_t behavior;

        game_init(&game);
        game_connect(&game);
        assert(game_new(&game) == GAME_MOVE_OK);
        game.result = results[i];
        hardware_behavior_init(&behavior);

        assert(hardware_behavior_tick(&behavior, &game, 0U) == HARDWARE_LED_BOTH);
    }
}

static void test_progress_counts_cells_and_saturates(void)
{
    game_state_t game;
    hardware_behavior_t behavior;
    hardware_behavior_init(&behavior);
    game_init(&game);

    /* Disconnected/idle screens have no progress; null inputs are harmless. */
    assert(hardware_progress_leds(NULL, &game) == 0U);
    assert(hardware_progress_leds(&behavior, NULL) == 0U);
    assert(hardware_progress_leds(&behavior, &game) == 0U);
    game_connect(&game);
    assert(hardware_progress_leds(&behavior, &game) == 0U);
    assert(game_new(&game) == GAME_MOVE_OK);

    /* Display-only fixtures cover every count, including the ninth-cell limit. */
    for (uint32_t count = 0U; count <= 9U; count++)
    {
        const uint8_t expected = count >= 8U ? 0xFFU : (uint8_t)((1UL << count) - 1UL);
        assert(hardware_progress_leds(&behavior, &game) == expected);
        if (count < 9U) game.board[count] = count % 2U == 0U ? 'X' : 'O';
    }
    game.result = GAME_RESULT_DRAW;
    assert(hardware_progress_leds(&behavior, &game) == 0xFFU);

    /* Restart clears the bar and disconnect hides it. */
    assert(game_new(&game) == GAME_MOVE_OK);
    assert(hardware_progress_leds(&behavior, &game) == 0U);
    game_disconnect(&game);
    assert(hardware_progress_leds(&behavior, &game) == 0U);
}

static void test_progress_rejection_ai_and_button(void)
{
    game_state_t game;
    hardware_behavior_t behavior;
    game_init(&game);
    game_connect(&game);
    assert(game_new(&game) == GAME_MOVE_OK);
    hardware_behavior_init(&behavior);

    /* A failed move cannot advance the bar. */
    assert(game_play(&game, 0U) == GAME_MOVE_OK);
    assert(hardware_progress_leds(&behavior, &game) == 1U);
    assert(game_play(&game, 0U) == GAME_MOVE_OCCUPIED);
    assert(game_play(&game, 9U) == GAME_MOVE_RANGE);
    assert(hardware_progress_leds(&behavior, &game) == 1U);

    /* Two confirmed marks (as in an AI exchange) light two LEDs. */
    game.ai_enabled = 1U;
    assert(game_play(&game, 4U) == GAME_MOVE_OK);
    assert(hardware_progress_leds(&behavior, &game) == 3U);

    /* Debounced press hides status and progress; holding does not retoggle.
     * Releasing and pressing restores the current progress without game edits. */
    assert(tick_many(&behavior, &game, 1U, 30U) == 0U);
    assert(hardware_progress_leds(&behavior, &game) == 0U);
    (void)tick_many(&behavior, &game, 1U, 100U);
    assert(hardware_progress_leds(&behavior, &game) == 0U);
    (void)tick_many(&behavior, &game, 0U, 30U);
    (void)tick_many(&behavior, &game, 1U, 30U);
    assert(hardware_progress_leds(&behavior, &game) == 3U);
    assert(game.board[0] == 'X' && game.board[4] == 'O');

    /* Resignation retains the progress while the status lights blink. */
    assert(game_resign(&game) == GAME_MOVE_OK);
    assert(hardware_progress_leds(&behavior, &game) == 3U);
}

static void test_view_button_debounce_and_independence(void)
{
    hardware_behavior_t behavior;
    hardware_behavior_init(&behavior);
    assert(behavior.summary_view == 0U);
    assert(hardware_view_button_tick(NULL, 1U) == 0U);

    /* Short bouncing presses do not change the default board view. */
    for (unsigned i = 0U; i < 29U; i++)
        assert(hardware_view_button_tick(&behavior, 1U) == 0U);
    assert(hardware_view_button_tick(&behavior, 0U) == 0U);
    assert(behavior.summary_view == 0U);

    for (unsigned i = 0U; i < 29U; i++)
        assert(hardware_view_button_tick(&behavior, 1U) == 0U);
    assert(hardware_view_button_tick(&behavior, 1U) == 1U);
    assert(behavior.summary_view == 1U && behavior.indicators_enabled == 1U);

    /* Holding cannot repeat, and a bouncing release cannot re-arm the button. */
    for (unsigned i = 0U; i < 100U; i++)
        assert(hardware_view_button_tick(&behavior, 1U) == 0U);
    for (unsigned i = 0U; i < 29U; i++)
        assert(hardware_view_button_tick(&behavior, 0U) == 0U);
    assert(hardware_view_button_tick(&behavior, 1U) == 0U);
    assert(behavior.summary_view == 1U);
    for (unsigned i = 0U; i < 30U; i++)
        assert(hardware_view_button_tick(&behavior, 0U) == 0U);
    for (unsigned i = 0U; i < 29U; i++)
        assert(hardware_view_button_tick(&behavior, 1U) == 0U);
    assert(hardware_view_button_tick(&behavior, 1U) == 1U);
    assert(behavior.summary_view == 0U && behavior.indicators_enabled == 1U);
}

int main(void)
{
    test_view_button_debounce_and_independence();
    test_progress_counts_cells_and_saturates();
    test_progress_rejection_ai_and_button();
    test_turn_leds_follow_server_turn();
    test_button_debounces_and_toggles_indicators_once_per_press();
    test_game_over_blinks_both_leds();
    test_all_terminal_results_activate_game_over_indicators();
    return 0;
}
