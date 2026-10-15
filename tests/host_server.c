#include "server_protocol.h"

#include <stdio.h>

int main(void)
{
    game_state_t game;
    char line[SERVER_MAX_COMMAND_LENGTH + 1U];
    char response[SERVER_RESPONSE_CAPACITY];

    game_init(&game);

    for (;;)
    {
        size_t length = 0U;
        int invalid_frame = 0;
        int pending_cr = 0;
        int received_bytes = 0;
        int reached_eof = 0;

        for (;;)
        {
            const int character = getchar();

            if (character == EOF)
            {
                reached_eof = 1;
                break;
            }
            if (character == '\n')
            {
                break;
            }

            received_bytes = 1;
            if (pending_cr != 0)
            {
                invalid_frame = 1;
                pending_cr = 0;
            }
            if (character == '\r')
            {
                pending_cr = 1;
            }
            else if ((character < 0x20) || (character > 0x7E))
            {
                invalid_frame = 1;
            }
            else if (length < SERVER_MAX_COMMAND_LENGTH)
            {
                line[length++] = (char)character;
            }
            else
            {
                invalid_frame = 1;
            }
        }

        if ((received_bytes == 0) && (pending_cr == 0) && (reached_eof != 0))
        {
            break;
        }
        if ((pending_cr != 0) && (reached_eof != 0))
        {
            invalid_frame = 1;
        }
        if (invalid_frame != 0)
        {
            line[0] = '\0';
        }
        else
        {
            line[length] = '\0';
        }

        if (server_handle_command(&game, line, response, sizeof(response)) == 0)
        {
            fputs("server response formatting failed\n", stderr);
            return 1;
        }

        puts(response);
        if (reached_eof != 0)
        {
            break;
        }
    }

    return ferror(stdin) ? 1 : 0;
}
