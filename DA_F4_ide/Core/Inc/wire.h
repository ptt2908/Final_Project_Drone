/*
 * wire.h
 *
 *  Created on: Mar 21, 2024
 *      Author: phamt
 */

#ifndef INC_WIRE_H_
#define INC_WIRE_H_
#include "stm32f4xx_hal.h"
#include <stdint.h>

extern uint8_t  address, command, length;
extern uint16_t calibrationData[];
extern uint8_t  buffer[];
extern uint8_t  status1;
extern uint8_t  status2;
void twiEnable(void);
void twiSend(uint8_t address, uint8_t command, uint8_t length);
void twiReceive(uint8_t address, uint8_t command, uint8_t length);


#endif /* INC_WIRE_H_ */
