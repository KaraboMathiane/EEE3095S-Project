#include "hardware_behavior.h"

#include <stddef.h>

#define BUTTON_SAMPLE_INTERVAL_MS 10U
#define BUTTON_DEBOUNCE_SAMPLES 3U
#define GAME_OVER_BLINK_INTERVAL_MS 250U

void hardware_behavior_init(hardware_behavior_t *behavior)
{
    if (behavior == NULL)
    {
        return;
    }

    behavior->sample_elapsed_ms = 0U;
    behavior->blink_elapsed_ms = 0U;
    behavior->candidate_pressed = 0U;
    behavior->stable_pressed = 0U;
    behavior->debounce_samples = 0U;
    behavior->indicators_enabled = 1U;
    behavior->blink_on = 1U;
    behavior->view_stable_pressed = 0U;
    behavior->view_debounce_ms = 0U;
    behavior->summary_view = 0U;
    behavior->previous_result = GAME_RESULT_NOT_STARTED;
}

uint8_t hardware_view_button_tick(hardware_behavior_t *behavior, uint8_t pressed)
{
    if (behavior == NULL) return 0U;
    pressed = pressed != 0U ? 1U : 0U;

    /* A transition must persist for 30 samples. A hold triggers once;
     * release must also debounce before another press can toggle the view. */
    if (pressed == behavior->view_stable_pressed)
    {
        behavior->view_debounce_ms = 0U;
    }
    else if (++behavior->view_debounce_ms >= 30U)
    {
        behavior->view_stable_pressed = pressed;
        behavior->view_debounce_ms = 0U;
        if (pressed != 0U)
        {
            behavior->summary_view ^= 1U;
            return 1U;
        }
    }
    return 0U;
}

static void update_button(hardware_behavior_t *behavior, uint8_t button_pressed)
{
    behavior->sample_elapsed_ms++;
    if (behavior->sample_elapsed_ms < BUTTON_SAMPLE_INTERVAL_MS)
    {
        return;
    }

    behavior->sample_elapsed_ms = 0U;
    button_pressed = (button_pressed != 0U) ? 1U : 0U;
    if (button_pressed != behavior->candidate_pressed)
    {
        behavior->candidate_pressed = button_pressed;
        behavior->debounce_samples = 1U;
    }
    else if (behavior->debounce_samples < BUTTON_DEBOUNCE_SAMPLES)
    {
        behavior->debounce_samples++;
    }

    if ((behavior->debounce_samples >= BUTTON_DEBOUNCE_SAMPLES) &&
        (behavior->stable_pressed != behavior->candidate_pressed))
    {
        behavior->stable_pressed = behavior->candidate_pressed;
        if (behavior->stable_pressed != 0U)
        {
            behavior->indicators_enabled =
                (behavior->indicators_enabled == 0U) ? 1U : 0U;
        }
    }
}

uint8_t hardware_progress_leds(
    const hardware_behavior_t *behavior,
    const game_state_t *game)
{
    uint32_t occupied = 0U;

    if ((behavior == NULL) || (game == NULL) ||
        (behavior->indicators_enabled == 0U) ||
        (game->connected == 0U) || (game->started == 0U))
    {
        return 0U;
    }

    /* Derive progress from authoritative cells rather than counting requests:
     * invalid moves do not advance it; an AI exchange can add two marks. */
    for (uint32_t i = 0U; i < GAME_BOARD_SIZE; i++)
    {
        if ((game->board[i] == 'X') || (game->board[i] == 'O')) occupied++;
    }
    if (occupied >= 8U) return 0xFFU;
    return (uint8_t)((1UL << occupied) - 1UL);
}

static uint8_t game_leds(
    hardware_behavior_t *behavior,
    const game_state_t *game)
{
    if ((game == NULL) || (game->connected == 0U) || (game->started == 0U))
    {
        behavior->blink_elapsed_ms = 0U;
        behavior->blink_on = 1U;
        return 0U;
    }

    if (game->result == GAME_RESULT_PLAYING)
    {
        behavior->blink_elapsed_ms = 0U;
        behavior->blink_on = 1U;
        return (game->current_player == 'X') ? HARDWARE_LED_X : HARDWARE_LED_O;
    }

    if ((game->result == GAME_RESULT_X_WON) ||
        (game->result == GAME_RESULT_O_WON) ||
        (game->result == GAME_RESULT_DRAW) ||
        (game->result == GAME_RESULT_RESIGNED))
    {
        if (behavior->previous_result != game->result)
        {
            behavior->previous_result = game->result;
            behavior->blink_elapsed_ms = 0U;
            behavior->blink_on = 1U;
        }

        behavior->blink_elapsed_ms++;
        if (behavior->blink_elapsed_ms >= GAME_OVER_BLINK_INTERVAL_MS)
        {
            behavior->blink_elapsed_ms = 0U;
            behavior->blink_on = (behavior->blink_on == 0U) ? 1U : 0U;
        }

        return (behavior->blink_on != 0U) ? HARDWARE_LED_BOTH : 0U;
    }

    behavior->blink_elapsed_ms = 0U;
    behavior->blink_on = 1U;
    return 0U;
}

uint8_t hardware_behavior_tick(
    hardware_behavior_t *behavior,
    const game_state_t *game,
    uint8_t button_pressed)
{
    uint8_t leds;

    if (behavior == NULL)
    {
        return 0U;
    }

    update_button(behavior, button_pressed);
    leds = game_leds(behavior, game);
    return (behavior->indicators_enabled != 0U) ? leds : 0U;
}
