#ifndef SERVER_PROTOCOL_H
#define SERVER_PROTOCOL_H

#include <stddef.h>

#include "game.h"

#define SERVER_MAX_COMMAND_LENGTH 32U
#define SERVER_RESPONSE_CAPACITY 32U

int server_handle_command(
    game_state_t *game,
    const char *command,
    char *response,
    size_t response_capacity);

#endif
