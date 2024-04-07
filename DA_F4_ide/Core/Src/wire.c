/*
 * wire.c
 *
 *  Created on: Mar 21, 2024
 *      Author: phamt
 */


#include "stm32f4xx_hal.h"
#include "wire.h"
#include "DelayUs.h"
void twiSend(uint8_t address, uint8_t command, uint8_t length)
{
    I2C2->CR1 |= I2C_CR1_START; //START condition
    while(!(I2C2->SR1 & I2C_SR1_SB));
    I2C2->DR=(address<<1|0); //sending address of the device, 0 = sending
    while(!(I2C2->SR1 & I2C_SR1_ADDR)|!(I2C2->SR2));
    I2C2->DR=command; //filling data register with byte, if single - command, multiple - command(s) and data
    for (uint8_t i=0;i<length;i++)
    {
        I2C2->DR=buffer[i]; //filling buffer with command or data
        delay(60);
    }
    I2C2->CR1 |= I2C_CR1_STOP;
}

void twiReceive(uint8_t address, uint8_t command, uint8_t length)
{
    I2C2->CR1 |= I2C_CR1_ACK;
  I2C2->CR1 |= I2C_CR1_START; //start pulse
    while(!(I2C2->SR1 & I2C_SR1_SB));
    I2C2->DR=(address<<1|0); //sending address of the device, 0 = sending
    while(!(I2C2->SR1 & I2C_SR1_ADDR)|!(I2C2->SR2 & I2C_SR2_BUSY));
    I2C2->DR=command; //sending command to the device in order to request data
    I2C2->CR1 |= I2C_CR1_START; //REPEATED START condition to change from sending address + command to receive data
    while(!(I2C2->SR1 & I2C_SR1_SB));
    I2C2->DR=(address<<1|1); //sending address of the device, 1 = reading
    while(!(I2C2->SR1 & I2C_SR1_ADDR)|!(I2C2->SR2));

if (length==1)  //receiving single byte, N=1
    {
        while(!(I2C2->SR1)|!(I2C2->SR2));
        I2C2->CR1 &= ~I2C_CR1_ACK; //this will send later NAK (not acknowledged) to signal it is last byte
        I2C2->CR1 |= I2C_CR1_STOP; //issuing STOP condition before (!) reading byte
        buffer[0]=I2C2->DR; //single byte is read AFTER NAK (!) and STOP condition
    }
    if (length==2) //receiving two bytes, N=2
    {
        while(!(I2C2->SR1)|!(I2C2->SR2));
        I2C2->CR1 &= ~I2C_CR1_ACK; //this will send later NAK (not acknowledged) before last byte
    I2C2->CR1 |= I2C_CR1_STOP;
        buffer[0]=I2C2->DR; //reading N-1 byte, next to last byte is in DR, last one still in shift register
        while(!(I2C2->SR1 & I2C_SR1_RXNE)|!(I2C2->SR2));
        buffer[1]=I2C2->DR; //read last N byte now available
    }
  if (length>2) //receiving more than two bytes, N>2
    {

      for (uint8_t i=0;i<length;i++)
      {

          if (i<(length-3))      // if it is not N-2, then read all bytes
            {
                while(!(I2C2->SR1 & I2C_SR1_RXNE)|!(I2C2->SR2));
                buffer[i]=I2C2->DR;
            }
          else if (i==length-3) // if it is N-2 then read
            {
                while(!(I2C2->SR1)|!(I2C2->SR2));
                buffer[i]=I2C2->DR;
                while(!(I2C2->SR1 & I2C_SR1_RXNE)|!(I2C2->SR2));
                I2C2->CR1 &= ~I2C_CR1_ACK; //this will send later NAK (not acknowledged) before last byte
                I2C2->CR1 |= I2C_CR1_STOP;
            }
        else if (i==length-2) // if it is N-1 then read
            {
                while(!(I2C2->SR1 & I2C_SR1_RXNE)|!(I2C2->SR2));
                buffer[i]=I2C2->DR;
            }
            else if (i==length-1) // else it is N byte
            {
                while(!(I2C2->SR1 & I2C_SR1_RXNE)|!(I2C2->SR2)){};
            buffer[i]=I2C2->DR;
            }
    }
 }
}
