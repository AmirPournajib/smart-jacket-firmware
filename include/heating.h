/* Heated zones: one PI (Proportional-Integral) controller per pad, wrapped in safety rules.
 * Safety rules always win over comfort: they run after the controller and can only
 * lower the duty cycle, never raise it. */
#ifndef HEATING_H
#define HEATING_H

#include "sj_types.h"

#define HEAT_LEVEL_MAX 5
#define HEAT_SETPOINT_MAX_C 45.0f   /* highest pad target */
#define HEAT_CUTOFF_C 48.0f         /* hard cut-off, latches a fault */
#define HEAT_SENSOR_MIN_C (-20.0f)  /* readings outside this range mean a broken sensor */
#define HEAT_SENSOR_MAX_C 90.0f
#define HEAT_LOW_BATTERY_PCT 15u
#define HEAT_CRITICAL_BATTERY_PCT 5u
#define HEAT_LOW_BATTERY_DUTY 40u
#define HEAT_NO_RISE_MS 60000u      /* heater on hard but pad not warming: open circuit or detached sensor */

typedef enum {
    HEAT_FAULT_NONE = 0,
    HEAT_FAULT_OVERTEMP,
    HEAT_FAULT_SENSOR,
    HEAT_FAULT_NO_RISE
} heat_fault_t;

typedef struct {
    float integ;
    float setpoint;
    uint8_t duty;          /* 0..100 % PWM (Pulse Width Modulation) */
    heat_fault_t fault;    /* latched until heating_clear_faults() */
    float rise_ref_c;      /* temperature when hard heating started */
    uint32_t hard_on_ms;   /* how long the duty has been above 50 % */
} heat_zone_t;

typedef struct {
    heat_zone_t zone[SJ_HEAT_ZONES];
    int level; /* 0 = off, 1..5 */
} heating_t;

typedef struct {
    float pad_c[SJ_HEAT_ZONES];
    float ambient_c;
    float skin_c;
    sj_activity_t activity;
    uint8_t battery_pct;
} heat_inputs_t;

void heating_init(heating_t *h);
void heating_set_level(heating_t *h, int level);
void heating_clear_faults(heating_t *h);
/* Returns a bitmask of zones that faulted during this update (for events). */
uint8_t heating_update(heating_t *h, const heat_inputs_t *in, uint32_t dt_ms);
float heating_target(const heating_t *h, sj_zone_t z, const heat_inputs_t *in);
const char *heating_fault_name(heat_fault_t f);

#endif
