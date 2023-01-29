/*
Tamaguachi group
Nieves Núñez and Isabel Martínez
*/

/* C Standard library */
#include <stdio.h>
#include <stdint.h>

#include <inttypes.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/hal/Seconds.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
//#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"
//#include "buzzer.c"


/*Global variables*/
char csv[80], *token;
const char s[2] = ",";
char command_to_send[30];


/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char buzzerTaskStack[STACKSIZE];

uint8_t uartBuffer[30];


/*Definition of the state machine for the flag */
enum state {DATA_READY, WAITING, LED_ON};
//SELECT_BUTTON and WARNING_DYING not needed for retake version
//READ_DATA also not needed because we don't turn SensorTag on and off for retake version
enum state programState = WAITING;


/*LEDS*/

static PIN_Handle hLed;
static PIN_State sLed;

PIN_Config ledConfig0[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};


/*MPU power pin global variables */
static PIN_Handle hMpuPin;
//static PIN_State MpuPinState;

static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};


//The LED connected to the pin Board_LED1 blinks with a delay of 1 second
void blink_led(){
    // configure the LED pin as output
    PIN_Handle ledHandle = PIN_open(&sLed, ledConfig0);
    PIN_setOutputValue(ledHandle, Board_LED1, 0);

    // loop to blink the LED
    while(1){
        PIN_setOutputValue(ledHandle, Board_LED1, 1);
        Task_sleep(1000000 / Clock_tickPeriod);
        PIN_setOutputValue(ledHandle, Board_LED1, 0);
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}


/* Task Functions */

//THIS TASK SENDS THE INFO TO THE BACKEND
//PREGUNTA: esa descripción está mal, no? Sería gets info from accelerometer
void sensorTaskAccGyro (UArg arg0, UArg arg1) {

    float ax,ay,az,gx,gy,gz;
    char print_msg[50];

    //I2C_Transaction i2cMessage;
    I2C_Handle i2cMPU;
    I2C_Params i2cMPUParams;

    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    // MPU power on
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    // Wait 100ms for the MPU sensor to power up
    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // MPU open i2c
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }

    System_flush();

    // MPU setup and calibration
    System_printf("MPU9250: Setup and calibration...\n");
    System_flush();

    //calling setup function to start the sensor and calibrate it
    mpu9250_setup(&i2cMPU);

    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();

    while (1) {

        if (programState==WAITING){
            // MPU ask data
            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

            if(ax > 1.3 || ax < -1.3){
                strcpy(command_to_send, "id:2401,MSG1:left-right,ping");
                programState = DATA_READY;
                sprintf(print_msg, "Horizontal move %s\n", command_to_send);
                System_printf(print_msg);
                }

            else if (ay> 1.3 || ay < -1.3){
                strcpy(command_to_send, "id:2401,MSG1:up-down,ping");
                programState = DATA_READY;
                sprintf(print_msg, "Vertical move %s\n", command_to_send);
                System_printf(print_msg);
                }

            else if (az> 1.3 || az < -1.3){
                strcpy(command_to_send, "id:2401,MSG1:front-back,ping");
                programState = DATA_READY;
                sprintf(print_msg, "Cross move %s\n", command_to_send);
                System_printf(print_msg);
            }
        }

            /*if(ax > 1.3 || ax < -1.3){
                sendToUART(command_to_send, "id:2401,MSG1:right,ping");
                programState = DATA_READY;
                sprintf(print_msg, "Horizontal move %s\n", command_to_send);
                System_printf(print_msg);
            }

            else if (ay> 1.3 || ay < -1.3){
                sendToUART(command_to_send, "id:2401,MSG1:up,ping");
                programState = DATA_READY;
                sprintf(print_msg, "Vertical move %s\n", command_to_send);
                System_printf(print_msg);
            }

            else if (az> 1.3 || az < -1.3){
                sendToUART(command_to_send, "id:2401,MSG1:top,ping");
                programState = DATA_READY;
                sprintf(print_msg, "Cross move %s\n", command_to_send);
                System_printf(print_msg);
            }
        }*/

       // Sleep 100ms
        System_flush();
        Task_sleep(100000 / Clock_tickPeriod);
    }
    // MPU close i2c
    //We close the i2cMPU so it doesn't disturb the line of i2c of the sensor
    I2C_close(i2cMPU);
    // MPU power off
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_OFF);
}

/*UART Functions*/

/*UART SensorAccGyro Function */
void uartTaskFxn(UArg arg0, UArg arg1) {

    UART_Handle uart;
    UART_Params uartParams;

    char echo_msg[50];
    char input[80];
    int found;

    // Setup here UART connection as 9600,8n1
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.baudRate = 9600;
    uartParams.readMode = UART_MODE_BLOCKING;
    uartParams.readEcho = UART_ECHO_OFF;
    //uartParams.readCallback = &uartFxn; //Decided to use Blocking method
    uartParams.dataLength = UART_LEN_8;
    uartParams.parityType = UART_PAR_NONE;
    uartParams.stopBits = UART_STOP_ONE;

    uart = UART_open(Board_UART0, &uartParams);

    if (uart == NULL) {
        System_abort("Error opening the UART");
    }

    while (1) {
        // Print out sensor data as string to debug window if the state is correct
        // Remember to modify state
        UART_read(uart, &input, 80);
        found = strstr(input, "pong");

        if(found!= NULL){
            sprintf(echo_msg, "Received: %s\n", input);
            System_printf(echo_msg);
            programState = LED_ON;
            blink_led();
        }
        System_printf(echo_msg);

        if (programState== DATA_READY){
            UART_write(uart,command_to_send, strlen(command_to_send)+1);
            Task_sleep(100000 / Clock_tickPeriod);
            programState = WAITING;
        }
        else if (programState == LED_ON) {
            blink_led();
            Task_sleep(1000000 / Clock_tickPeriod);
            programState = WAITING;
        }

        System_flush();

        // Once per second, you can modify this
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

Int main(void) {

    // Task variables
    Task_Handle sensorTaskHandle1;
    //Task_Handle sensorTaskHandle2;
    Task_Params sensorTaskParams;

    //UART Task variables
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    // Initialize board
    Board_initGeneral();

    //Initialize i2c bus
    Board_initI2C();
    //Initialize UART
    Board_initUART();

    // Open the shut/woke button and led pins
    // Remember to register the above interrupt handler for button

    hLed = PIN_open(&sLed, ledConfig0);
    if(!hLed) {
        System_abort("Error initializing LED pins\n");
    }


    /* Sensor Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority=2;
    sensorTaskHandle1 = Task_create(sensorTaskAccGyro, &sensorTaskParams, NULL);

    if (sensorTaskHandle1 == NULL) {
        System_abort("Task create failed failed for sensorTaskAccGyro!");
    }

    /* Uart Task */

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority=2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);

    if (uartTaskHandle == NULL) {
        System_abort("Task create failed for uartTaskFxn!");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();
    return (0);
}
