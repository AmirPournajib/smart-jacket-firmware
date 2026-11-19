/* Controls for the simulated world behind src/hal_sim.c. */
#ifndef HAL_SIM_H
#define HAL_SIM_H

#include "protocol.h"
#include "sj_types.h"

typedef void (*sim_uart_sink_t)(const proto_frame_t *frame, void *ctx);

void sim_init(uint32_t seed);
void sim_advance(uint32_t ms);          /* move simulated time forward */
uint32_t sim_now_ms(void);

void sim_set_activity(sj_activity_t a);
void sim_trigger_fall(void);            /* free fall, impact, then lying still */
void sim_set_ambient(float c);
void sim_set_battery(uint8_t pct);
void sim_break_pad_sensor(sj_zone_t z); /* sensor starts reading nonsense */
void sim_press_button(uint32_t hold_ms);
void sim_remove_jacket_contact(bool removed);
void sim_audio_frame(int16_t *out, size_t n, bool speaking);

uint8_t sim_heater_duty(sj_zone_t z);
uint32_t sim_buzz_count(void);

/* Every frame the firmware writes to the UART is decoded and passed here. */
void sim_set_uart_sink(sim_uart_sink_t sink, void *ctx);

#endif
