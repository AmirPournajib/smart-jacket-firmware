/* Framed serial protocol between the jacket MCU and the Jetson Nano.
 *
 *   | 0xA5 | type | len lo | len hi | payload ... | crc lo | crc hi |
 *
 * CRC-16/CCITT-FALSE over type, length and payload. The parser is a byte-at-a-time
 * state machine, so it resynchronizes on its own after noise or a dropped byte.
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "sj_types.h"

#define PROTO_SOF 0xA5u
#define PROTO_MAX_PAYLOAD 512u
#define PROTO_OVERHEAD 6u

typedef enum {
    MSG_TELEMETRY = 0x01,   /* MCU -> Jetson, every second */
    MSG_EVENT = 0x02,       /* MCU -> Jetson, falls, alerts, faults */
    MSG_AUDIO_START = 0x10, /* MCU -> Jetson, wake the Jetson */
    MSG_AUDIO_CHUNK = 0x11, /* MCU -> Jetson, 10 ms of 16 kHz PCM */
    MSG_AUDIO_END = 0x12,
    MSG_INTENT = 0x20,      /* Jetson -> MCU, what the wearer asked for */
    MSG_STATUS = 0x21       /* MCU -> Jetson, reply to a status request */
} msg_type_t;

uint16_t crc16_ccitt(const uint8_t *data, size_t len, uint16_t crc);

/* Returns the frame length, or 0 if it does not fit in out_cap. */
size_t proto_encode(uint8_t type, const uint8_t *payload, uint16_t len, uint8_t *out, size_t out_cap);

typedef enum { PS_SOF = 0, PS_TYPE, PS_LEN_LO, PS_LEN_HI, PS_PAYLOAD, PS_CRC_LO, PS_CRC_HI } proto_state_t;

typedef struct {
    proto_state_t state;
    uint8_t type;
    uint16_t len;
    uint16_t idx;
    uint16_t crc_rx;
    uint8_t payload[PROTO_MAX_PAYLOAD];
    uint32_t crc_errors;
    uint32_t frames_ok;
} proto_parser_t;

typedef struct {
    uint8_t type;
    uint16_t len;
    const uint8_t *payload;
} proto_frame_t;

void proto_parser_init(proto_parser_t *p);
/* Feed one byte. Returns true when a complete, valid frame is in *out. */
bool proto_feed(proto_parser_t *p, uint8_t byte, proto_frame_t *out);

/* Little-endian helpers for payloads. */
void put_u16(uint8_t *b, uint16_t v);
void put_i16(uint8_t *b, int16_t v);
uint16_t get_u16(const uint8_t *b);
int16_t get_i16(const uint8_t *b);

#endif
