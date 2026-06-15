#pragma once
#ifndef __HARDWARE_ADC_H__
#define __HARDWARE_ADC_H__

#include <stdint.h>

// Initialise les 3 canaux ADC (batterie + joystick X/Y)
int adc_init();

// Lecture batterie en mV
int adc_read_vbatt();

// Lecture batterie en %
int adc_read_vbatt_percent();

// Lecture joystick X en mV (0..3300)
int adc_read_joyx();

// Lecture joystick Y en mV (0..3300)
int adc_read_joyy();

#endif // __HARDWARE_ADC_H__
