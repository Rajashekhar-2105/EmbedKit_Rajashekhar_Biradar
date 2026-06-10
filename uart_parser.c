#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SOF                 (0xAAU)
#define MAX_PAYLOAD_LEN     (16U)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

typedef enum
{
    PARSER_IN_PROGRESS = 0,
    PARSER_FRAME_OK,
    PARSER_CHECKSUM_ERROR,
    PARSER_TIMEOUT_ERROR,
    PARSER_LENGTH_ERROR
} ParserResult;

typedef enum
{
    WAIT_FOR_SOF = 0,
    RECEIVE_CMD,
    RECEIVE_LEN,
    RECEIVE_PAYLOAD,
    RECEIVE_CHECKSUM
} ParserState;

typedef struct
{
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[MAX_PAYLOAD_LEN];
} UARTFrame;

typedef struct
{
    ParserState state;

    uint8_t cmd;
    uint8_t len;
    uint8_t payload[MAX_PAYLOAD_LEN];
    uint8_t payload_index;

    uint8_t checksum;

    uint32_t timeout_ms;
    uint32_t last_byte_time;
    uint32_t last_gap;
} UARTParser;

static void parser_reset(UARTParser *parser)
{
    parser->state = WAIT_FOR_SOF;
    parser->cmd = 0U;
    parser->len = 0U;
    parser->payload_index = 0U;
    parser->checksum = 0U;

    memset(parser->payload, 0, sizeof(parser->payload));
}

static void parser_init(UARTParser *parser, uint32_t timeout_ms)
{
    parser_reset(parser);

    parser->timeout_ms = timeout_ms;
    parser->last_byte_time = 0U;
    parser->last_gap = 0U;
}

static ParserResult parser_feed_byte(UARTParser *parser,
                                     uint8_t byte,
                                     uint32_t timestamp,
                                     UARTFrame *frame)
{
    if ((parser->timeout_ms != 0U) &&
        (parser->state != WAIT_FOR_SOF))
    {
        /* unsigned subtraction safely handles timestamp wraparound */
        uint32_t gap = timestamp - parser->last_byte_time;

        if (gap > parser->timeout_ms)
        {
            parser->last_gap = gap;
            parser_reset(parser);
            return PARSER_TIMEOUT_ERROR;
        }
    }

    switch (parser->state)
    {
        case WAIT_FOR_SOF:

            if (byte == SOF)
            {
                parser->state = RECEIVE_CMD;
                parser->last_byte_time = timestamp;
            }
            break;

        case RECEIVE_CMD:

            parser->cmd = byte;
            parser->checksum = byte;

            parser->state = RECEIVE_LEN;
            parser->last_byte_time = timestamp;
            break;

        case RECEIVE_LEN:

            parser->len = byte;
            parser->checksum ^= byte;
            parser->payload_index = 0U;

            if (parser->len > MAX_PAYLOAD_LEN)
            {
                parser_reset(parser);
                return PARSER_LENGTH_ERROR;
            }

            if (parser->len == 0U)
            {
                parser->state = RECEIVE_CHECKSUM;
            }
            else
            {
                parser->state = RECEIVE_PAYLOAD;
            }

            parser->last_byte_time = timestamp;
            break;

        case RECEIVE_PAYLOAD:

            parser->payload[parser->payload_index] = byte;
            parser->checksum ^= byte;

            parser->payload_index++;

            if (parser->payload_index >= parser->len)
            {
                parser->state = RECEIVE_CHECKSUM;
            }

            parser->last_byte_time = timestamp;
            break;

        case RECEIVE_CHECKSUM:

            parser->last_byte_time = timestamp;

            if (byte == parser->checksum)
            {
                frame->cmd = parser->cmd;
                frame->len = parser->len;

                memcpy(frame->payload,
                       parser->payload,
                       parser->len);

                parser_reset(parser);
                return PARSER_FRAME_OK;
            }

            parser_reset(parser);
            return PARSER_CHECKSUM_ERROR;

        default:

            parser_reset(parser);
            return PARSER_CHECKSUM_ERROR;
    }

    return PARSER_IN_PROGRESS;
}

static void print_frame(const UARTFrame *frame)
{
    uint8_t i;

    printf("FRAME OK CMD=0x%02X LEN=%u PAYLOAD=[",
           frame->cmd,
           frame->len);

    for (i = 0U; i < frame->len; i++)
    {
        printf("%02X", frame->payload[i]);

        if (i < (frame->len - 1U))
        {
            printf(" ");
        }
    }

    printf("]\n");
}

static void feed_stream(UARTParser *parser,
                        const uint8_t *bytes,
                        const uint32_t *timestamps,
                        uint32_t count)
{
    uint32_t i;
    UARTFrame frame;
    ParserResult result;

    for (i = 0U; i < count; i++)
    {
        result = parser_feed_byte(parser,
                                  bytes[i],
                                  timestamps[i],
                                  &frame);

        printf("t=%3ums byte=0x%02X -> ",
               timestamps[i],
               bytes[i]);

        switch (result)
        {
            case PARSER_IN_PROGRESS:

                if ((parser->state == WAIT_FOR_SOF) &&
                    (bytes[i] != SOF))
                {
                    printf("ignored (waiting for SOF)\n");
                }
                else
                {
                    printf("receiving...\n");
                }
                break;

            case PARSER_FRAME_OK:

                print_frame(&frame);
                break;

            case PARSER_CHECKSUM_ERROR:

                printf("CHECKSUM ERROR\n");
                break;

            case PARSER_TIMEOUT_ERROR:

                printf("TIMEOUT (%ums gap > %ums) -- parser reset\n",
                       parser->last_gap,
                       parser->timeout_ms);

                /* Re-feed current byte after timeout reset */
                result = parser_feed_byte(parser,
                                          bytes[i],
                                          timestamps[i],
                                          &frame);

                if (result == PARSER_IN_PROGRESS)
                {
                    printf("t=%3ums byte=0x%02X -> receiving... (re-fed after reset)\n",
                           timestamps[i],
                           bytes[i]);
                }

                break;

            case PARSER_LENGTH_ERROR:

                printf("INVALID LENGTH (> %u)\n",
                       MAX_PAYLOAD_LEN);
                break;

            default:
                break;
        }
    }
}

int main(void)
{
    UARTParser parser;

    printf("\n===== TEST 1 : CLEAN VALID FRAME =====\n");

    /*
     * NOTE:
     * Actual checksum:
     * 0x01 ^ 0x03 ^ 0x10 ^ 0x20 ^ 0x30 = 0x02
     *
     * The PDF mentions 0x22, which appears to be incorrect.
     */

    const uint8_t test1_bytes[] =
    {
        0xAA, 0x01, 0x03,
        0x10, 0x20, 0x30,
        0x02
    };

    const uint32_t test1_time[] =
    {
        0, 5, 10, 15, 20, 25, 30
    };

    parser_init(&parser, 50U);

    feed_stream(&parser,
                test1_bytes,
                test1_time,
                ARRAY_SIZE(test1_bytes));

    printf("\n===== TEST 2 : TIMEOUT AND RECOVERY =====\n");

    const uint8_t test2_bytes[] =
    {
        0xAA, 0x01, 0x03, 0x10,
        0xAA,
        0x05, 0x01, 0x7F, 0x7B
    };

    const uint32_t test2_time[] =
    {
        0, 5, 10, 15,
        200,
        200, 205, 210, 215
    };

    parser_init(&parser, 50U);

    feed_stream(&parser,
                test2_bytes,
                test2_time,
                ARRAY_SIZE(test2_bytes));

    printf("\n===== TEST 3 : TWO BACK-TO-BACK FRAMES =====\n");

    const uint8_t test3_bytes[] =
    {
        0xAA, 0x03, 0x01, 0x55, 0x57,
        0xAA, 0x04, 0x02, 0xAA, 0xBB, 0x17
    };

    const uint32_t test3_time[] =
    {
        0, 5, 10, 15, 20,
        25, 30, 35, 40, 45, 50
    };

    parser_init(&parser, 50U);

    feed_stream(&parser,
                test3_bytes,
                test3_time,
                ARRAY_SIZE(test3_bytes));

    printf("\n===== TEST 4 : TIMEOUT DISABLED =====\n");
    printf("Expected: No timeout. Partial frame eventually fails checksum.\n");

    parser_init(&parser, 0U);

    feed_stream(&parser,
                test2_bytes,
                test2_time,
                ARRAY_SIZE(test2_bytes));

    return 0;
}
