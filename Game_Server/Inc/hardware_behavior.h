#ifndef HARDWARE_BEHAVIOR_H
#define HARDWARE_BEHAVIOR_H

#include <stdint.h>

#include "game.h"

#define HARDWARE_LED_X 0x01U
#define HARDWARE_LED_O 0x02U
#define HARDWARE_LED_BOTH (HARDWARE_LED_X | HARDWARE_LED_O)

typedef struct
{
    uint16_t sample_elapsed_ms;
    uint16_t blink_elapsed_ms;
    uint8_t candidate_pressed;
    uint8_t stable_pressed;
    uint8_t debounce_samples;
    uint8_t indicators_enabled;
    uint8_t blink_on;
    uint8_t view_stable_pressed;
    uint8_t view_debounce_ms;
    uint8_t summary_view;
    game_result_t previous_result;
} hardware_behavior_t;

void hardware_behavior_init(hardware_behavior_t *behavior);
/* Call once per serviced millisecond. Returns one only when the view changes. */
uint8_t hardware_view_button_tick(hardware_behavior_t *behavior, uint8_t pressed);
/* PB0..PB7 progress bar: one bit per occupied cell, saturating at eight.
 * Kept separate from the two status bits returned by tick(). */
uint8_t hardware_progress_leds(
    const hardware_behavior_t *behavior,
    const game_state_t *game);

uint8_t hardware_behavior_tick(
    hardware_behavior_t *behavior,
    const game_state_t *game,
    uint8_t button_pressed);

#endif
