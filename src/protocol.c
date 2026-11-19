#include "protocol.h"

#include <string.h>

uint16_t crc16_ccitt(const uint8_t *data, size_t len, uint16_t crc)
{
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

size_t proto_encode(uint8_t type, const uint8_t *payload, uint16_t len, uint8_t *out, size_t out_cap)
{
    if (len > PROTO_MAX_PAYLOAD || out_cap < (size_t)len + PROTO_OVERHEAD) {
        return 0;
    }
    out[0] = PROTO_SOF;
    out[1] = type;
    put_u16(&out[2], len);
    if (len > 0u) {
        memcpy(&out[4], payload, len);
    }
    const uint16_t crc = crc16_ccitt(&out[1], (size_t)len + 3u, 0xFFFFu);
    put_u16(&out[4 + len], crc);
    return (size_t)len + PROTO_OVERHEAD;
}

void proto_parser_init(proto_parser_t *p) { memset(p, 0, sizeof(*p)); }

bool proto_feed(proto_parser_t *p, uint8_t byte, proto_frame_t *out)
{
    switch (p->state) {
    case PS_SOF:
        if (byte == PROTO_SOF) {
            p->state = PS_TYPE;
        }
        break;
    case PS_TYPE:
        p->type = byte;
        p->state = PS_LEN_LO;
        break;
    case PS_LEN_LO:
        p->len = byte;
        p->state = PS_LEN_HI;
        break;
    case PS_LEN_HI:
        p->len |= (uint16_t)((uint16_t)byte << 8);
        p->idx = 0;
        if (p->len > PROTO_MAX_PAYLOAD) {
            p->state = PS_SOF; /* impossible length: this was not a real frame */
        } else {
            p->state = (p->len == 0u) ? PS_CRC_LO : PS_PAYLOAD;
        }
        break;
    case PS_PAYLOAD:
        p->payload[p->idx++] = byte;
        if (p->idx >= p->len) {
            p->state = PS_CRC_LO;
        }
        break;
    case PS_CRC_LO:
        p->crc_rx = byte;
        p->state = PS_CRC_HI;
        break;
    case PS_CRC_HI: {
        p->crc_rx |= (uint16_t)((uint16_t)byte << 8);
        p->state = PS_SOF;
        uint8_t hdr[3] = {p->type, (uint8_t)(p->len & 0xFFu), (uint8_t)(p->len >> 8)};
        uint16_t crc = crc16_ccitt(hdr, 3, 0xFFFFu);
        crc = crc16_ccitt(p->payload, p->len, crc);
        if (crc != p->crc_rx) {
            p->crc_errors++;
            return false;
        }
        p->frames_ok++;
        out->type = p->type;
        out->len = p->len;
        out->payload = p->payload;
        return true;
    }
    }
    return false;
}

void put_u16(uint8_t *b, uint16_t v)
{
    b[0] = (uint8_t)(v & 0xFFu);
    b[1] = (uint8_t)(v >> 8);
}

void put_i16(uint8_t *b, int16_t v) { put_u16(b, (uint16_t)v); }

uint16_t get_u16(const uint8_t *b) { return (uint16_t)(b[0] | ((uint16_t)b[1] << 8)); }

int16_t get_i16(const uint8_t *b) { return (int16_t)get_u16(b); }
