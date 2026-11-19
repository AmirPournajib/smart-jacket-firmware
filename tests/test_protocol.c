#include <string.h>

#include "protocol.h"
#include "test.h"

static int feed_all(proto_parser_t *p, const uint8_t *b, size_t n, proto_frame_t *last)
{
    int frames = 0;
    for (size_t i = 0; i < n; i++) {
        if (proto_feed(p, b[i], last)) {
            frames++;
        }
    }
    return frames;
}

void test_protocol(void)
{
    /* Known CRC-16/CCITT-FALSE check value. */
    CHECK(crc16_ccitt((const uint8_t *)"123456789", 9, 0xFFFFu) == 0x29B1u);

    uint8_t buf[64];
    const uint8_t payload[3] = {1, 2, 3};
    const size_t n = proto_encode(MSG_EVENT, payload, 3, buf, sizeof(buf));
    CHECK(n == 9);
    CHECK(proto_encode(MSG_EVENT, payload, 3, buf, 5) == 0);

    proto_parser_t p;
    proto_frame_t f;
    proto_parser_init(&p);
    CHECK(feed_all(&p, buf, n, &f) == 1);
    CHECK(f.type == MSG_EVENT && f.len == 3 && memcmp(f.payload, payload, 3) == 0);

    /* Plain line noise before a frame: the frame still gets through. */
    uint8_t noisy[80] = {0x00, 0x13, 0x7E, 0x99};
    memcpy(&noisy[4], buf, n);
    proto_parser_init(&p);
    CHECK(feed_all(&p, noisy, 4 + n, &f) == 1);

    /* Noise that contains a fake start byte can cost the next frame,
     * but the parser is back in sync for the one after it. */
    noisy[2] = PROTO_SOF;
    memcpy(&noisy[4 + n], buf, n);
    proto_parser_init(&p);
    CHECK(feed_all(&p, noisy, 4 + 2 * n, &f) >= 1);
    CHECK(f.type == MSG_EVENT);

    /* Flipped bit: rejected and counted. */
    uint8_t bad[64];
    memcpy(bad, buf, n);
    bad[5] ^= 0x01;
    proto_parser_init(&p);
    CHECK(feed_all(&p, bad, n, &f) == 0);
    CHECK(p.crc_errors == 1);

    /* Empty payload frames are valid (audio start/end). */
    const size_t m = proto_encode(MSG_AUDIO_END, NULL, 0, buf, sizeof(buf));
    proto_parser_init(&p);
    CHECK(feed_all(&p, buf, m, &f) == 1 && f.type == MSG_AUDIO_END && f.len == 0);

    put_i16(buf, -1234);
    CHECK(get_i16(buf) == -1234);
}
