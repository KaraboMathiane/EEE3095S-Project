#include "server_protocol.h"

#include <stdint.h>
#include <string.h>

static int response_append(char *response, size_t capacity, size_t *length, const char *text)
{
    const size_t text_length = strlen(text);

    if ((text_length >= capacity) || (*length > capacity - text_length - 1U))
    {
        return 0;
    }

    memcpy(&response[*length], text, text_length);
    *length += text_length;
    response[*length] = '\0';
    return 1;
}

static int response_append_board(
    char *response,
    size_t capacity,
    size_t *length,
    const game_state_t *game,
    const char *prefix)
{
    const size_t prefix_length = strlen(prefix);

    if ((prefix_length + GAME_BOARD_SIZE >= capacity) ||
        (*length > capacity - prefix_length - GAME_BOARD_SIZE - 1U))
    {
        return 0;
    }

    memcpy(&response[*length], prefix, prefix_length);
    *length += prefix_length;
    memcpy(&response[*length], game->board, GAME_BOARD_SIZE);
    *length += GAME_BOARD_SIZE;
    response[*length] = '\0';
    return 1;
}

static const char *move_error(game_move_status_t status)
{
    switch (status)
    {
        case GAME_MOVE_NO_GAME:
            return "NACK:NOGAME";
        case GAME_MOVE_RANGE:
            return "NACK:RANGE";
        case GAME_MOVE_OCCUPIED:
            return "NACK:OCCUPIED";
        case GAME_MOVE_ILLEGAL:
            return "NACK:ILLEGAL";
        case GAME_MOVE_OK:
        default:
            return "NACK:BAD";
    }
}

static int handle_move(
    game_state_t *game,
    const char *command,
    char *response,
    size_t capacity)
{
    uint32_t index = 0U;
    size_t response_length = 0U;
    size_t position = 5U;
    game_move_status_t status;

    if (command[position] == '\0')
    {
        return response_append(response, capacity, &response_length, "NACK:BAD");
    }

    while (command[position] != '\0')
    {
        const char digit = command[position];

        if ((digit < '0') || (digit > '9'))
        {
            return response_append(response, capacity, &response_length, "NACK:BAD");
        }

        if (index <= GAME_BOARD_SIZE)
        {
            index = (index * 10U) + (uint32_t)(digit - '0');
        }
        position++;
    }

    if (index >= GAME_BOARD_SIZE)
    {
        return response_append(response, capacity, &response_length, "NACK:RANGE");
    }

    status = game_play(game, index);
    if (status != GAME_MOVE_OK)
    {
        return response_append(response, capacity, &response_length, move_error(status));
    }

    if (game->result == GAME_RESULT_X_WON)
    {
        return response_append(response, capacity, &response_length, "WIN:X");
    }
    if (game->result == GAME_RESULT_O_WON)
    {
        return response_append(response, capacity, &response_length, "WIN:O");
    }
    if (game->result == GAME_RESULT_DRAW)
    {
        return response_append(response, capacity, &response_length, "DRAW");
    }

    return response_append_board(response, capacity, &response_length, game, "OK:BOARD:");
}

int server_handle_command(
    game_state_t *game,
    const char *command,
    char *response,
    size_t response_capacity)
{
    size_t command_length = 0U;
    size_t response_length = 0U;
    game_move_status_t status;

    if ((game == NULL) || (command == NULL) || (response == NULL) ||
        (response_capacity == 0U))
    {
        return 0;
    }

    response[0] = '\0';
    while ((command[command_length] != '\0') &&
           (command_length <= SERVER_MAX_COMMAND_LENGTH))
    {
        command_length++;
    }

    if (command_length > SERVER_MAX_COMMAND_LENGTH)
    {
        return response_append(response, response_capacity, &response_length, "NACK:BAD");
    }

    if (strcmp(command, "CONNECT") == 0)
    {
        game_connect(game);
        return response_append(response, response_capacity, &response_length, "OK");
    }
    if (strcmp(command, "DISCONNECT") == 0)
    {
        game_disconnect(game);
        return response_append(response, response_capacity, &response_length, "OK");
    }
    if (strcmp(command, "NEW") == 0)
    {
        status = game_new(game);
        if (status != GAME_MOVE_OK)
        {
            return response_append(response, response_capacity, &response_length,
                                   move_error(status));
        }
        return response_append_board(response, response_capacity, &response_length,
                                     game, "OK:BOARD:");
    }
    if (strcmp(command, "STATE") == 0)
    {
        if ((game->connected == 0U) || (game->started == 0U))
        {
            return response_append(response, response_capacity, &response_length,
                                   "NACK:NOGAME");
        }
        return response_append_board(response, response_capacity, &response_length,
                                     game, "BOARD:");
    }
    if (strcmp(command, "RESIGN") == 0)
    {
        status = game_resign(game);
        if (status != GAME_MOVE_OK)
        {
            return response_append(response, response_capacity, &response_length,
                                   move_error(status));
        }
        return response_append(response, response_capacity, &response_length,
                               "OK:GAMEOVER");
    }
    if (strncmp(command, "MOVE:", 5U) == 0)
    {
        return handle_move(game, command, response, response_capacity);
    }

    return response_append(response, response_capacity, &response_length, "NACK:BAD");
}
