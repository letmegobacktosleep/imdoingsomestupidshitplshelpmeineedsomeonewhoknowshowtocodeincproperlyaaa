/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#define HAL_USE_ADC TRUE
#define ADC_USE_WAIT TRUE

#define HAL_USE_SERIAL TRUE

#ifdef RGB_MATRIX_ENABLE
#define HAL_USE_PWM TRUE
#endif

#include_next <halconf.h>