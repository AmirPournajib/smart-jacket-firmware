/* Hardware abstraction layer. The application only talks to hardware through
 * these functions. src/hal_sim.c implements them for a laptop simulation; a
 * board port implements the same functions with real drivers (I2C/SPI sensors,
 * PWM timers, UART). Nothing above this layer changes between the two. */
#ifndef HAL_H
#define HAL_H

#include "sj_types.h"

bool hal_read_accel(sj_vec3_t *out);
float hal_read_ppg(void);
float hal_read_skin_temp(void);
float hal_read_ambient_temp(void);
float hal_read_pad_temp(sj_zone_t z);
uint8_t hal_battery_pct(void);
bool hal_button_pressed(void);

void hal_set_heater_duty(sj_zone_t z, uint8_t duty_pct);
void hal_haptic_buzz(uint16_t ms);
void hal_uart_write(const uint8_t *data, size_t len);

#endif
