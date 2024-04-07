/*
 * MS5611_I2C.c
 *
 *  Created on: Mar 26, 2024
 *      Author: phamt
 */

#include "stm32f4xx_hal.h"
#include "wire.h"
#include "math.h"
#include "DelayUs.h"
#include "stdlib.h"
#include "MS5611_I2C.h"

uint8_t counter_pressure = 0,temperature_counter = 0,average_temperature_mem_location = 0;
uint32_t raw_pressure, raw_temperature, temp, raw_temperature_rotating_memory[6], raw_average_temperature_total;
float actual_pressure, actual_pressure_slow, actual_pressure_fast, actual_pressure_diff;
int32_t pressure_rotating_mem[50], pressure_total_avarage;
uint8_t pressure_rotating_mem_location;
uint8_t  buffer[5];  //twi buffer, for now 32 elements, for other purpose (e.g. for OLED it is 1024) change as you wish
uint8_t  address=0x77, length=0; //0xEC
uint16_t calibrationData[7];
int64_t OFF, OFF2, SENS, SENS2;
uint32_t D1, D2;
int32_t dT, TEMP;
int  P, Pa,T2;
extern int flight_mode;
int pid_max_altitude = 400;


//Altitude PID variables
float pid_i_mem_altitude, pid_altitude_setpoint, pid_altitude_input, pid_output_altitude, pid_last_altitude_d_error, pid_error_temp;
uint8_t parachute_rotating_mem_location;
int32_t parachute_buffer[35], parachute_throttle;
float pressure_parachute_previous;
int32_t pressure_rotating_mem[50], pressure_total_avarage;
uint8_t pressure_rotating_mem_location;
float pressure_rotating_mem_actual;
float pid_error_gain_altitude, pid_throttle_gain_altitude;
uint8_t manual_altitude_change;

extern float pid_p_gain_altitude;           //Gain setting for the altitude P-controller (default = 1.4).
extern float pid_i_gain_altitude;           //Gain setting for the altitude I-controller (default = 0.2).
extern float pid_d_gain_altitude;

void setupSensor(void)
{
    twiSend(address, 0x1E,1); //just send 1 byte that tells MS5611 to reset
    HAL_Delay(20); //delay 10 mS needed for device to execute reset
    for (int i=1;i<=6;i++)
    {
    twiReceive(address, 0xA0+i*2, 2); //read all 14 bytes for callibration data from PROM
    HAL_Delay(5); //at least 40 uS
    calibrationData[i] = buffer[0]<<8|buffer[1]; //pair of bytes goes into each element of callibrationData[i], global variables, 14 uint8_t into 7 uint16_t
  }
}

int getPressure(void)
{
	counter_pressure ++;
	if (counter_pressure == 1)
	{
		if(temperature_counter == 0)
		{
			twiReceive(address, 0x00, 3);
			raw_average_temperature_total -= raw_temperature_rotating_memory[average_temperature_mem_location];
			raw_temperature_rotating_memory[average_temperature_mem_location] = buffer[0]<<16|buffer[1]<<8|buffer[2];
			raw_average_temperature_total += raw_temperature_rotating_memory[average_temperature_mem_location];
			average_temperature_mem_location++;
			if (average_temperature_mem_location == 5)average_temperature_mem_location = 0;
			raw_temperature = raw_average_temperature_total / 5;
		}
		else
		{
			twiReceive(address, 0x00, 3);
			raw_pressure = buffer[0]<<16|buffer[1]<<8|buffer[2];
		}
		temperature_counter ++;        //Increase the temperature_counter variable.
		if (temperature_counter == 20)
		{
			temperature_counter = 0;
			twiSend(address, 0x54,1);
		}
		else twiSend(address, 0x44,1);
	}

	if (counter_pressure == 2)
	{
		dT = raw_temperature - ((int)calibrationData[5] << 8);
		TEMP = (2000 + (((int64_t)dT * (int64_t)calibrationData[6]) >> 23));
		if (TEMP<2000)  //if temperature of the sensor goes below 20°C, it activates "second order temperature compensation"
		    {
		      T2=pow(dT,2)/2147483648;
		      OFF2=5*pow((TEMP-2000),2)/2;
		      SENS2=5*pow((TEMP-2000),2)/4;
		      if (TEMP<-1500) //if temperature of the sensor goes even lower, below -15°C, then additional math is utilized
		        {
		          OFF2=OFF2+7*pow((TEMP+1500),2);
		          SENS2=SENS2+11*pow((TEMP+1500),2)/2;
		        }
		    }
	   else { T2=0; OFF2=0; SENS2=0; }

		    TEMP = ((2000 + (((int64_t)dT * (int64_t)calibrationData[6]) >> 23))-T2); //second order compensation included
		    OFF = (((unsigned int)calibrationData[2] << 16) + (((int64_t)calibrationData[4] * dT) >> 7)-OFF2); //second order compensation included
		    SENS = (((unsigned int)calibrationData[1] << 15) + (((int64_t)calibrationData[3] * dT) >> 8)-SENS2); //second order compensation included
		    P = (((raw_pressure * SENS) >> 21) - OFF) >> 15;

		    //To get a smoother pressure value we will use a 20 location rotating memory.
		    pressure_total_avarage -= pressure_rotating_mem[pressure_rotating_mem_location];                          //Subtract the current memory position to make room for the new value.
		    pressure_rotating_mem[pressure_rotating_mem_location] = P;                                                //Calculate the new change between the actual pressure and the previous measurement.
		    pressure_total_avarage += pressure_rotating_mem[pressure_rotating_mem_location];                          //Add the new value to the long term avarage value.
            pressure_rotating_mem_location++;                                                                         //Increase the rotating memory location.
		    if (pressure_rotating_mem_location == 20)pressure_rotating_mem_location = 0;                              //Start at 0 when the memory location 20 is reached.
		    actual_pressure_fast = (float)pressure_total_avarage / 20.0;

		    //To get better results we will use a complementary fillter that can be adjusted by the fast average.
		    actual_pressure_slow = actual_pressure_slow * (float)0.985 + actual_pressure_fast * (float)0.015;
		    actual_pressure_diff = actual_pressure_slow - actual_pressure_fast;                                       //Calculate the difference between the fast and the slow avarage value.
		    if (actual_pressure_diff > 8)actual_pressure_diff = 8;                                                    //If the difference is larger then 8 limit the difference to 8.
		    if (actual_pressure_diff < -8)actual_pressure_diff = -8;                                                  //If the difference is smaller then -8 limit the difference to -8.
		    //If the difference is larger then 1 or smaller then -1 the slow average is adjuste based on the error between the fast and slow average.
		    if (actual_pressure_diff > 1 || actual_pressure_diff < -1)actual_pressure_slow -= actual_pressure_diff / 6.0;
		    actual_pressure = actual_pressure_slow;
	}
	if (counter_pressure == 3) {
		counter_pressure = 0;

	    if (manual_altitude_change == 1)	pressure_parachute_previous = actual_pressure * 10;                       //During manual altitude change the up/down detection is disabled.
	    parachute_throttle -= parachute_buffer[parachute_rotating_mem_location];                                  //Subtract the current memory position to make room for the new value.
	    parachute_buffer[parachute_rotating_mem_location] = actual_pressure * 10 - pressure_parachute_previous;   //Calculate the new change between the actual pressure and the previous measurement.
	    parachute_throttle += parachute_buffer[parachute_rotating_mem_location];                                  //Add the new value to the long term avarage value.
	    pressure_parachute_previous = actual_pressure * 10;                                                       //Store the current measurement for the next loop.
	    parachute_rotating_mem_location++;                                                                        //Increase the rotating memory location.
	    if (parachute_rotating_mem_location == 30)	parachute_rotating_mem_location = 0;

	    if (flight_mode == 2) {
	    	manual_altitude_change = 0;
	    	if (pid_altitude_setpoint == 0)	pid_altitude_setpoint = actual_pressure;

	        //Calculate the PID output of the altitude hold.
	        pid_altitude_input = actual_pressure;                                          //Set the setpoint (pid_altitude_input) of the PID-controller.
	        pid_error_temp = pid_altitude_input - pid_altitude_setpoint;                   //Calculate the error between the setpoint and the actual pressure value.

	        //To get better results the P-gain is increased when the error between the setpoint and the actual pressure value increases.
	        //The variable pid_error_gain_altitude will be used to adjust the P-gain of the PID-controller.
	        pid_error_gain_altitude = 0;                                                   //Set the pid_error_gain_altitude to 0.
	        if (pid_error_temp > 10 || pid_error_temp < -10) {                             //If the error between the setpoint and the actual pressure is larger than 10 or smaller then -10.
	          pid_error_gain_altitude = (abs(pid_error_temp) - 10) / 20.0;                 //The positive pid_error_gain_altitude variable is calculated based based on the error.
	          if (pid_error_gain_altitude > 3)pid_error_gain_altitude = 3;                 //To prevent extreme P-gains it must be limited to 3.
	        }

	        //In the following section the I-output is calculated. It's an accumulation of errors over time.
	        //The time factor is removed as the program loop runs at 250Hz.
	        pid_i_mem_altitude += (pid_i_gain_altitude / 100.0) * pid_error_temp;
	        if (pid_i_mem_altitude > pid_max_altitude)pid_i_mem_altitude = pid_max_altitude;
	        else if (pid_i_mem_altitude < pid_max_altitude * -1)pid_i_mem_altitude = pid_max_altitude * -1;
	        //In the following line the PID-output is calculated.
	        //P = (pid_p_gain_altitude + pid_error_gain_altitude) * pid_error_temp.
	        //I = pid_i_mem_altitude += (pid_i_gain_altitude / 100.0) * pid_error_temp (see above).
	        //D = pid_d_gain_altitude * parachute_throttle.
	        pid_output_altitude = (pid_p_gain_altitude + pid_error_gain_altitude) * pid_error_temp + pid_i_mem_altitude + pid_d_gain_altitude * parachute_throttle;
	        //To prevent extreme PID-output the output must be limited.
	        if (pid_output_altitude > pid_max_altitude)pid_output_altitude = pid_max_altitude;
	        else if (pid_output_altitude < pid_max_altitude * -1)pid_output_altitude = pid_max_altitude * -1;
	    }
	    else{
	        pid_altitude_setpoint = 0;                                                     //Reset the PID altitude setpoint.
	        pid_output_altitude = 0;                                                       //Reset the output of the PID controller.
	        pid_i_mem_altitude = 0;                                                        //Reset the I-controller.
	        manual_altitude_change = 1;
	    }
	}

  //return actual_pressure; //returns back pressure P
	return pid_output_altitude;
}
