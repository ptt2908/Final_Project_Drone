/*
 * MS5611_I2C.h
 *
 *  Created on: Mar 26, 2024
 *      Author: phamt
 */

#ifndef INC_MS5611_I2C_H_
#define INC_MS5611_I2C_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include "wire.h"

extern float actual_pressure;
void setupSensor(void);
int getPressure(void);

#endif /* INC_MS5611_I2C_H_ */
