/*
    Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
    Copyright (C) 2012 - 2018 Xilinx, Inc. All Rights Reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy of
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software. If you wish to use our Amazon
    FreeRTOS name, please do so in a fair use way that does not cause confusion.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    http://www.FreeRTOS.org
    http://aws.amazon.com/freertos


    1 tab == 4 spaces!
*/
/*
 * rtos_software_timer.c
 *
 * Created on: 	10 November 2020 (based on rtos_task_management.c)
 *     Author: 	Leomar Duran
 *    Version: 	2.1
 */

/*
 * rtos_task_management.c
 *
 * Created on: 	16 September 2020 (based on FreeRTOS_Hello_World.c)
 *     Author: 	Leomar Duran
 *    Version: 	1.5
 */

/********************************************************************************************
* VERSION HISTORY
********************************************************************************************
* 	v2.1 - 10 November 2020
* 		Re-implemented LED blinker with the software timer, 500 ms -> 5 s.
*
* 	v2.0 - 10 November 2020
* 		Set up GPIOs and implemented LED blinker with `vTaskDelay`.
*
* 	v1.5 - 16 September 2020
* 		Added TaskBTN feature that controls TaskSW.
*
* 	v1.4 - 16 September 2020
* 		Added TaskSW  feature that controls TaskLED and TaskBTN.
*
* 	v1.3 - 16 September 2020
* 		Stubbed TaskSW.  Optimized TaskBTN.
*
* 	v1.2 - 16 September 2020
* 		Added TaskBTN feature that controls TaskLED.
*
* 	v1.1 - 15 September 2020
* 		Set up LED counter.
*
* 	v1.0 - 2017
* 		Started with FreeRTOS_Hello_World.c
*
*******************************************************************************************/

/********************************************************************************************
* TASK DESCRIPTION
********************************************************************************************
* TIMERtask := a blinker between 0b1100 and 0b0011, displayed in the LEDs.
*
* BTNtask := reads the buttons to control the other tasks
*
* SWtask  := reads the switches to control the other tasks
*
*******************************************************************************************/

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
/* Xilinx includes. */
#include "xil_printf.h"
#define	printf	xil_printf
#include "xparameters.h"
#include "xgpio.h"
#include "xstatus.h"

/* task definitions */
#define	DO_TIMER_TASK	1								/* whether to do TIMERtask */
#define	DO_BTN_TASK		0								/* whether to do BTNtask */
#define	DO_SW_TASK 		0								/* whether to do SWtask */

/* GPIO definitions */
#define	LD_BTN_DEVICE_ID	XPAR_AXI_GPIO_0_DEVICE_ID	/* GPIO device for LEDs, Buttons */
#define	SW_DEVICE_ID		XPAR_AXI_GPIO_1_DEVICE_ID	/* GPIO device for switches */
#define BTN_DELAY	250UL							/* button delay length for debounce */
#define LED_CHANNEL	1								/* GPIO port for LEDs */
#define BTN_CHANNEL	2								/* GPIO port for buttons */
#define  SW_CHANNEL	1								/* GPIO port for switches */

#define	TIMER_TASK_ID				1
#define	TIMER_TASK_CHECK_THRESHOLD	9
#define	TIMER_DELAY	5000UL							/* LED delay length (in ms) */

/* GPIO instances */
XGpio LdBtnInst;					/* GPIO Device driver instance for LEDs, Buttons */
XGpio SwInst;						/* GPIO Device driver instance for switches */

/* leds */
#define	LED_INIT	0b1100						/* initial value of LED */

/* bit masks for on */
#define	      ON4 	0b1111						/* 4 bit on  */
#define	BTN10_ON  	0b1100
#define	 BTN2_ON  	0b1011
#define	 BTN3_ON  	0b0111
#define	 SW10_ON  	0b1100
#define	  SW2_ON  	0b1011
#define	  SW3_ON  	0b0111

/* bit masks for off */
#define       OFF4	0b0000						/* 4 bit off */
#define	BTN10_OFF 	0b0011
#define	 BTN2_OFF 	0b0100
#define	 BTN3_OFF 	0b1000
#define	 SW10_OFF 	0b0011
#define	  SW2_OFF 	0b0100
#define	  SW3_OFF 	0b1000
/*-----------------------------------------------------------*/

/* The tasks as described at the top of this file. */
static void prvBTNtask( void *pvParameters );
static void prvSWtask ( void *pvParameters );
static void vTIMERtask( TimerHandle_t pxTimer );
/*-----------------------------------------------------------*/

/* The task handles to control other tasks. */
static TaskHandle_t xBTNtask;
static TaskHandle_t xSWtask;
static TimerHandle_t xTIMERtask = NULL;
long RxtaskCntr = 0;
/* The LED blinker. */
int ledBlnkr = LED_INIT;
/* The last value of button. */
int btn;

int main( void )
{
	int Status;
	const TickType_t xTIMERticks = pdMS_TO_TICKS( TIMER_DELAY );

	if (DO_BTN_TASK) {
		printf( "Starting BTNtask. . .\r\n" );
		/* Create BTNtask with priority 1. */
		xTaskCreate(
					prvBTNtask,						/* The function implementing the task. */
				( const char * ) "BTNtask",			/* Text name provided for debugging. */
					configMINIMAL_STACK_SIZE,		/* Not much need for a stack. */
					NULL,							/* The task parameter, not in use. */
					( UBaseType_t ) 1,				/* The next to lowest priority. */
					&xBTNtask );
		printf( "\tSuccessful\r\n" );
	}

	if (DO_SW_TASK) {
		printf( "Starting SWtask . . .\r\n" );
		/* Create SWtask with priority 1. */
		xTaskCreate(
					prvSWtask,						/* The function implementing the task. */
				( const char * ) "SWtask",			/* Text name provided for debugging. */
					configMINIMAL_STACK_SIZE,		/* Not much need for a stack. */
					NULL,							/* The task parameter, not in use. */
					( UBaseType_t ) 1,				/* The next to lowest priority. */
					&xSWtask );
		printf( "\tSuccessful\r\n" );
	}

	if (DO_TIMER_TASK) {
		printf( "Starting TIMERtask. . .\r\n" );
		/* Create a timer with a timer expiry of 10 seconds. The timer would expire
		 after 10 seconds and the timer call back would get called. In the timer call back
		 checks are done to ensure that the tasks have been running properly till then.
		 The tasks are deleted in the timer call back and a message is printed to convey that
		 the example has run successfully.
		 The timer expiry is set to 10 seconds and the timer set to not auto reload. */
		xTIMERtask = xTimerCreate( (const char *) "TIMERtask",
								xTIMERticks,
								pdTRUE,				/* this is a multiple shot timer */
								(void *) TIMER_TASK_ID,
								vTIMERtask);
		/* Check the timer was created. */
		configASSERT( xTIMERtask );

		/* start the timer with a block time of 0 ticks. This means as soon
		   as the schedule starts the timer will start running and will expire after
		   10 seconds */
		xTimerStart( xTIMERtask, 0 );
		printf( "\tSuccessful\r\n" );
	}

	/* initialize the GPIO driver for the LEDs */
	Status = XGpio_Initialize(&LdBtnInst, LD_BTN_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	/* set LEDs direction to output */
	XGpio_SetDataDirection(&LdBtnInst, LED_CHANNEL, 0x00);
	/* set buttons direction to input */
	XGpio_SetDataDirection(&LdBtnInst, BTN_CHANNEL, 0xFF);

	/* initialize the GPIO driver for the buttons */
	Status = XGpio_Initialize(&SwInst, SW_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	/* set switches to input direction to input */
	XGpio_SetDataDirection(&SwInst,  SW_CHANNEL, 0xFF);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following line
	will never be reached.  If the following line does execute, then there was
	insufficient FreeRTOS heap memory available for the idle and/or timer tasks
	to be created.  See the memory management section on the FreeRTOS web site
	for more details. */
	for( ;; );
}


/*-----------------------------------------------------------*/
static void vTIMERtask( TimerHandle_t pxTimer )
{
	static const TickType_t LEDseconds = pdMS_TO_TICKS( TIMER_DELAY );

	static long lTimerId;
	configASSERT( pxTimer );

	// get the ID of the timer
	lTimerId = ( long ) pvTimerGetTimerID( pxTimer );

	if (lTimerId != TIMER_TASK_ID) {
		xil_printf("TIMERtask FAILED: Unexpected timer.");
		return;
	}

	/* display the blinker */
	XGpio_DiscreteWrite(&LdBtnInst, LED_CHANNEL, ledBlnkr);
	printf("TIMERtask: blink := 0b%d%d%d%d\r\n",
				((ledBlnkr >> 3) & 1),
				((ledBlnkr >> 2) & 1),
				((ledBlnkr >> 1) & 1),
				((ledBlnkr >> 0) & 1)
			);

	/* update the blinker */
	ledBlnkr = ~ledBlnkr;

	/* reset the timer */
	xTimerReset( vTIMERtask, LEDseconds );

	/* If the RxtaskCntr is updated every time the Rx task is called. The
	 Rx task is called every time the Tx task sends a message. The Tx task
	 sends a message every 1 second.
	 The timer expires after 10 seconds. We expect the RxtaskCntr to at least
	 have a value of 9 (TIMER_CHECK_THRESHOLD) when the timer expires. */
//	if (RxtaskCntr >= TIMER_TASK_CHECK_THRESHOLD) {
//		xil_printf("FreeRTOS Hello World Example PASSED");
//	} else {
//		xil_printf("FreeRTOS Hello World Example FAILED");
//	}

//	vTaskDelete( xRxTask );
//	vTaskDelete( xTxTask );
}


/*-----------------------------------------------------------*/
static void prvBTNtask( void *pvParameters )
{
const TickType_t BTNseconds = pdMS_TO_TICKS( BTN_DELAY );
	int nextBtn;	/* Hold the new button value. */
	for( ;; )
	{
		/* Read input from the buttons. */
		nextBtn = XGpio_DiscreteRead(&LdBtnInst, BTN_CHANNEL);

		/* Debounce: */
		/* If the button has changed, */
		if ( btn != nextBtn ) {
			btn = nextBtn;	/* store the old value */
			vTaskDelay( BTNseconds );	/* delay until the end of the bounce */
			nextBtn = XGpio_DiscreteRead(&LdBtnInst, BTN_CHANNEL );	/* read again */
			/* if the button value is still the same, continue */
			if ( btn == nextBtn ) {
				printf("BTNtask: Button changed to 0x%x.\r\n", btn);

				btn = nextBtn;	/* update btn */
				/* If BTN2 is depressed, regardless of the
				 * status of BTN0 and BTN1, then TIMERtask is
				 * resumed.  So BTN2 gets priority. */
				if ( ( btn | BTN2_ON ) == ON4 ) {
					vTaskResume(xTIMERtask);
					printf("BTNtask: TIMERtask is resumed.\r\n");
				}
				/* Otherwise if BTN0 and BTN1 are depressed at
				 * some point together then TIMERtask is
				 * suspended */
				else if ( ( btn | BTN10_ON ) == ON4 ) {
					vTaskSuspend(xTIMERtask);
					printf("BTNtask: TIMERtask is suspended.\r\n");
				}

				/* This logic below is independent from those above. */
				/* If BTN3 is depressed then SWtask is suspended */
				if ( ( btn | BTN3_ON ) == ON4 ) {
					vTaskSuspend(xSWtask);
					printf("BTNtask: SWtask  is suspended.\r\n");
				}
				/* Either BTN3 is depressed or it is released. */
				/* If BTN3 is released then SWtask is resumed */
				else {
					vTaskResume(xSWtask);
					printf("BTNtask: SWtask  is resumed.\r\n");
				}

			} /* end if ( btn == nextBtn ) check if button is consistent */
		} /* end if ( btn != nextBtn ) check if button has changed since last call */
	} /* end for( ;; ) */
}


/*-----------------------------------------------------------*/
static void prvSWtask( void *pvParameters )
{
	int sw;	/* Hold the current switch value. */
	for( ;; )
	{
		/* Read input from the switches. */
		sw = XGpio_DiscreteRead(&LdBtnInst,  SW_CHANNEL);

		/* If SW0 and SW1 are ON together at some point then
		 * BTNtask is suspended */
		if ( ( sw | SW10_ON ) == ON4 ) {
			vTaskSuspend(xBTNtask);
		}
		/* Switches 0 and 1 cannot both be off if they're both on. */
		/* If SW0 and SW1 are OFF then BTNtask is resumed. */
		else if ( (sw & SW10_OFF ) == OFF4 ) {
			vTaskResume(xBTNtask);
		}

		/* This logic below is independent from those above. */
		/* If SW3 is ON then TIMERtask is suspended. */
		if ( ( sw | SW3_ON ) == ON4 ) {
			vTaskSuspend(xTIMERtask);
			/* If SW3 is then turned OFF, then resume TIMERtask. */
			sw = XGpio_DiscreteRead(&LdBtnInst,  SW_CHANNEL);
			if ( ( sw & SW3_OFF ) == OFF4 ) {
				vTaskResume(xTIMERtask);
				printf("SWtask : TIMERtask is resumed.\r\n");
			}
		}
	}
}
