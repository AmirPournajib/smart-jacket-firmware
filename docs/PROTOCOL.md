# Jacket to Jetson messages

Every message is one frame:

| Byte | Field |
|---|---|
| 0 | Start byte `0xA5` |
| 1 | Message type |
| 2-3 | Payload length, little endian (max 512) |
| 4.. | Payload |
| last 2 | CRC-16/CCITT-FALSE over type, length and payload |

## Message types

| Type | Direction | Payload |
|---|---|---|
| `0x01` Telemetry | Jacket to Jetson, every second | 22 bytes, see below |
| `0x02` Event | Jacket to Jetson | `code, arg, arg2` |
| `0x10` Audio start | Jacket to Jetson | empty, wakes the Jetson |
| `0x11` Audio chunk | Jacket to Jetson | 160 samples of 16 kHz, 16-bit PCM (10 ms) |
| `0x12` Audio end | Jacket to Jetson | empty |
| `0x20` Intent | Jetson to jacket | `intent` (1 byte) |
| `0x21` Status | Jacket to Jetson | same 22 bytes as telemetry, sent on request |

## Telemetry (22 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | Activity: 0 still, 1 walk, 2 run |
| 1 | 1 | Model confidence, % |
| 2 | 1 | Heart rate, bpm (0 = no reading) |
| 3 | 1 | Skin contact, 0/1 |
| 4 | 2 | Skin temperature, C x 100 |
| 6 | 2 | Air temperature, C x 100 |
| 8 | 6 | Pad temperatures chest, back, collar, C x 100 |
| 14 | 3 | Heater duty per zone, % |
| 17 | 1 | Battery, % |
| 18 | 2 | Vital flags (bit 0 high HR at rest, 1 low HR, 2 skin cold, 3 no contact) |
| 20 | 1 | Heat level 0 to 5 |
| 21 | 1 | Heater fault mask per zone |

## Events

| Code | Meaning | Args |
|---|---|---|
| 1 | Fall detected | seconds to cancel |
| 2 | Fall alert cancelled | 0 button, 1 voice |
| 3 | Fall alert sent | |
| 4 | New vital flags | flags |
| 5 | Heater fault | zone, fault (1 over-temp, 2 sensor, 3 no rise) |
| 6 | Activity changed | activity, confidence % |
| 7 | Heat level changed | level |

## Intents

| Value | Intent |
|---|---|
| 1 | Heat up |
| 2 | Heat down |
| 3 | Heat off |
| 4 | Status |
| 5 | Cancel fall alert |

Unknown values are ignored by the jacket.
