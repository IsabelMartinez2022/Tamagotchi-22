/* C Standard library */
#include <stdio.h>

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

/*Global variables*/
char csv[80], *token;
const char s[2] = ",";
char command_to_send[30];
//char sendingStates[10];

/*
 * Having the data as an array would be more managable in order to know previous values
 * typedef struct {
   uint8_t ax[8];
   uint8_t ay[8];
   uint8_t az[8];
} accValues;

typedef struct {
   uint8_t gx[8];
   uint8_t gy[8];
   uint8_t gz[8];
} gyroValues;*/

int  i = 0, j = 0, k = 0, num = 0; //may be used for loops

/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char buzzerTaskStack[STACKSIZE];

uint8_t uartBuffer[30];

// Definition of the state machine
enum state { WAITING=1, DATA_READY, READ_DATA, SELECT_BUTTON};
enum state programState = WAITING;

enum myState {AWAKE_MODE, SLEEP_MODE}; //used for jukebox
enum myState petState= AWAKE_MODE; //awake_mode by defect

//FLAG
enum msgState {SEND, READING, DETECT};
enum msgState sendingStates;

// Global variable for ambient light
double ambientLight = -1000.0;

//Add pins RTOS-variables and configuration here
//Buttons and leds handle state & configuration

/*BUTTON SHUT*/
static PIN_Handle hButtonShut;
static PIN_State bStateShut;

PIN_Config buttonShut[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config buttonWake[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
   PIN_TERMINATE
};

/*BUTTON SELECT*/
static PIN_Handle hButtonSelect;
static PIN_State bStateSelect;

PIN_Config buttonConfig[] = {
   Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

/*LEDS*/

static PIN_Handle hLed;
static PIN_State sLed;

PIN_Config ledConfig0[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

PIN_Config ledConfig1[] = {
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

//Handle & state for the buzzer
static PIN_Handle hBuzzer;
static PIN_State sBuzzer;

PIN_Config cBuzzer[] = {
  Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
  PIN_TERMINATE
};

// MPU power pin global variables
static PIN_Handle hMpuPin;
static PIN_State  MpuPinState;

static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

/* Task Functions */

//THIS TASK SENDS THE INFO TO THE BACKEND
void sensorTaskAccGyro (UArg arg0, UArg arg1) {

    float ax,ay,az,gx,gy,gz;

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

    // MPU setup and calibration
    System_printf("MPU9250: Setup and calibration...\n");
    System_flush();

    //calling setup function to start the sensor and calibrate it
    mpu9250_setup(&i2cMPU);

    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();

    while (1) {

        if (programState==READ_DATA){
           // MPU ask data

            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

           /* Hacemos un movimiento horizontal para dar de comer al tamagotchi
            * Si el movimiento es vertical se le acaricia
            * Si el movimiento es           hace ejercicio
            * */

            if(ax > 0.1){
               // command_to_send[0]='id: /*sensor id*/, EAT,2';
                strcpy(command_to_send, "id: 2401, EAT: 2");
                strcpy(sendingStates, "SEND");
                sprintf(print_msg, "Current State %s\n", sendingStates);
                System_printf(print_msg);

            }else if(ay > 0.1){

                strcpy(command_to_send, "id: 2401, PET: 2");
                strcpy(sendingStates, "SEND");
                sprintf(print_msg, "Current State %s\n", sendingStates);
                System_printf(print_msg);

            }else if(az >0.1){

                strcpy(command_to_send, "id: 2401, EXCERCISE: 2");
                strcpy(sendingStates, "SEND");
                sprintf(print_msg, "Current State %s\n", sendingStates);
                System_printf(print_msg);
            }

            char string[50];
            sprintf(string,"%d, %d, %d, %d, %d, %d", ax, ay, az, gx, gy, gz);
            System_printf(string);
        }

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

void sensorTaskAmbientLight(UArg arg0, UArg arg1) {

    //local variable
    double lux;

    //I2C_Transaction i2cMessage;

    I2C_Handle i2c;
    I2C_Params i2cParams;

    // Open the i2c bus
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);

    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }

    //Setup the OPT3001 sensor for use
    //Before calling the setup function, insert 100ms delay with Task_sleep
    Task_sleep(100000 / Clock_tickPeriod);

    //configurating the light
    opt3001_setup(&i2c);

    while (1) {
        if (programState==WAITING){
            // Read sensor data and print it to the Debug window as string
            lux = opt3001_get_data(&i2c);
            char string[50];
            sprintf(string,"%f", lux);
            System_printf(string);

            // Save the sensor value into the global variable. Remember to modify state
            ambientLight = lux;

            //Hacemos la media de los primeros 10 valores que coge el sensor, si son menores que 1 pasamos al estado de dormir

            for (i=0; i<10;i++){
                ambientLight+= ambientLight;
            }
            ambientLight= ambientLight/10;
            if (ambientLight<1){
                petState= SLEEP_MODE;
            }

            //cerramos el i2c para q se puedda utilizar el MPU
            I2C_close(i2c);
            programState = DATA_READY;
        }

        // Just for sanity check for exercise, you can comment this out
        System_printf(" Ambient Light sensorTask\n");
        System_flush();
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

/* Shut Function */
void shutFxn() {
   PINCC26XX_setWakeup(buttonShut);
   Power_shutdown(NULL,0);
}

/*UART Functions*/

/*UART SensorAccGyro Function */
void uartTaskFxn(UArg arg0, UArg arg1) {

    UART_Handle uart;
    UART_Params uartParams;

    // Setup here UART connection as 9600,8n1
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.baudRate = 9600;
    uartParams.readMode = UART_MODE_CALLBACK;
    uartParams.readCallback = &uartFxn; //TODO Function to read the backend
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

        string_compare=strcmp(sendingState, "SEND");

        if (string_compare==0){
            UART_write(uart,command_to_send, strlen(command_to_send)+1);
            Task_sleep(100000 / Clock_tickPeriod);
            strcpy(sendingState, "DETECT");
        }

        // Just for sanity check for exercise, you can comment this out
       // System_printf(" uartTask\n");
        System_flush();

        // Once per second, you can modify this
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

/*UART Function AmbientLight*/

void UARTAmbientLight (UART_Handle uart, UART_Params uartParams){

    char string[50];
    char echo_msg [30];
    sprintf(string,"%.3f", ambientLight);
    System_printf(string);
    programState = WAITING;
    sprintf(echo_msg,"Received: %.3f\n\r",ambientLight);

    UART_write(uart, echo_msg, strlen(echo_msg)+1);
}

//EXTRA

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

    if(programState == WAITING){
        PIN_setOutputValue(hLed, Board_LED0, 0 );
        programState = DATA_READY;
    }else{
        PIN_setOutputValue(hLed, Board_LED0, 1 );
        programState = WAITING;
    }
    programState = SELECT_BUTTON;
}

/* Buzz Function */
/* Jukebox Function */

void buzzerTaskFxn(UArg arg0, UArg arg1){

    while (1) {
        if (petState== SLEEP_MODE){
        buzzerOpen(hBuzzer);
        buzzerSetFrequency(660);
        Task_sleep(50000 / Clock_tickPeriod);
        buzzerSetFrequency(0);
        Task_sleep(20000 / Clock_tickPeriod);
        buzzerSetFrequency(512);
        Task_sleep(50000 / Clock_tickPeriod);
        buzzerSetFrequency(0);
        Task_sleep(20000 / Clock_tickPeriod);
        buzzerSetFrequency(590);
        Task_sleep(50000 / Clock_tickPeriod);
        buzzerSetFrequency(0);
        Task_sleep(20000 / Clock_tickPeriod);
        buzzerClose();
        //Calling shutdown
        shutFxn();
        return 0;
        }
        if(programState == SELECT_BUTTON){
            buzzerOpen(hBuzzer);
            buzzerSetFrequency(2000);
            Task_sleep(50000 / Clock_tickPeriod);
            buzzerClose();
            programState= READ_DATA;
        }else if(programState == ){ //añadir el estado de leer backend
            buzzerOpen(hBuzzer);
            buzzerSetFrequency(3000);
            Task_sleep(50000 / Clock_tickPeriod);
            buzzerClose();
        }
        Task_sleep(100000 / Clock_tickPeriod);
     }
}

Int main(void) {

    // Task variables
    Task_Handle sensorTaskHandle1;
    Task_Handle sensorTaskHandle2;
    Task_Params sensorTaskParams;

    //UART Task variables
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    //Buzzer Task variables & init
    Task_Handle buzzerTaskHandle;
    Task_Params buzzerTaskParams;

    hBuzzer = PIN_open(&sBuzzer, cBuzzer);
    if (!hBuzzer) {
        System_abort("Error initializing buzzer pins\n");
    }

    // Initialize board
    Board_initGeneral();

    //Initialize i2c bus
    Board_initI2C();
    //Initialize UART
    Board_initUART();

    // Open the shut/woke button and led pins
    // Remember to register the above interrupt handler for button

    hButtonSelect = PIN_open(&bStateSelect, buttonConfig);
    if(!hButtonSelect) {
        System_abort("Error initializing button pins\n");
    }

    if (PIN_registerIntCb(hButtonSelect, &buttonFxn) != 0) {
        System_abort("Error registering button select callback function");
    }

    hButtonShut = PIN_open(&bStateShut, buttonShut);
    if(!hButtonShut) {
        System_abort("Error initializing button shut pins\n");
    }

    hLed = PIN_open(&sLed, ledConfig0);
    if(!hLed) {
        System_abort("Error initializing LED pins\n");
    }

    hLed = PIN_open(&sLed, ledConfig1);
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
        System_abort("Task create failed!");
    }

    /* AmbientLight Task */

    Task_Params_init(&sensorTaskParams);
        sensorTaskParams.stackSize = STACKSIZE;
        sensorTaskParams.stack = &sensorTaskStack;
        sensorTaskParams.priority=2;
        sensorTaskHandle2 = Task_create(sensorTaskAmbientLight, &sensorTaskParams, NULL);

    if (sensorTaskHandle2 == NULL) {
        System_abort("Task create failed!");
    }

    /* Uart Task */

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority=2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);

    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    /*UARTAmbientLight*/

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority=2;
    uartTaskHandle = Task_create(UARTAmbientLight, &uartTaskParams, NULL);

    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    /*EXTRA*/

    /* Buzzer Task */

        Task_Params_init(&buzzerTaskParams);
        uartTaskParams.stackSize = STACKSIZE;
        uartTaskParams.stack = &buzzerTaskStack;
        uartTaskParams.priority=2;
        uartTaskHandle = Task_create(buzzerTaskFxn, &buzzerTaskParams, NULL);

        if (buzzerTaskHandle == NULL) {
            System_abort("Task create failed!");
        }

    /* Sanity check */

    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */

    BIOS_start();
    return (0);
}
