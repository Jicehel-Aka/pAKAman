#pragma once
#ifndef __HARDWARE_PWM_H__
#define __HARDWARE_PWM_H__

#include <stdint.h>

// Initialisation PWM pour le rétroéclairage LCD
void lcd_init_pwm();

// Mise à jour du duty cycle (0..255)
void lcd_update_pwm(uint8_t duty);

#endif // __HARDWARE_PWM_H__
