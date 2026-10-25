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

int main(void)
{
    test_turn_leds_follow_server_turn();
    test_button_debounces_and_toggles_indicators_once_per_press();
    test_game_over_blinks_both_leds();
    test_all_terminal_results_activate_game_over_indicators();
    return 0;
}
