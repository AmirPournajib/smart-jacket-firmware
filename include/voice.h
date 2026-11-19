/* Voice front end on the jacket MCU (Microcontroller Unit).
 *
 * The MCU only decides "is someone speaking?" with a light voice activity
 * detector, and streams those segments to the Jetson Nano. The Jetson runs
 * the wake word, speech-to-text and the local language model, and sends a
 * short intent back. Keeping the heavy AI on the Jetson and the always-on
 * listening on the MCU saves battery: the Jetson can sleep until speech starts.
 */
#ifndef VOICE_H
#define VOICE_H

#include "sj_types.h"

#define VAD_START_FRAMES 3u     /* 30 ms of speech to open a segment */
#define VAD_HANGOVER_FRAMES 30u /* 300 ms of silence to close it */
#define VAD_RATIO 4.0f          /* speech energy vs noise floor (about 6 dB) */
#define VAD_MIN_ENERGY 2.0e4f   /* ignore very quiet rooms entirely */

typedef enum { VAD_NONE = 0, VAD_START, VAD_CONTINUE, VAD_END } vad_event_t;

typedef struct {
    float noise_floor;
    uint32_t speech_run;
    uint32_t silence_run;
    bool active;
    bool primed;
} vad_t;

void vad_init(vad_t *v);
vad_event_t vad_process(vad_t *v, const int16_t *frame, size_t n);

/* Intents the Jetson can send back. */
typedef enum {
    INTENT_NONE = 0,
    INTENT_HEAT_UP = 1,
    INTENT_HEAT_DOWN = 2,
    INTENT_HEAT_OFF = 3,
    INTENT_STATUS = 4,
    INTENT_CANCEL_ALERT = 5,
    INTENT_COUNT
} intent_t;

const char *intent_name(intent_t i);

#endif
