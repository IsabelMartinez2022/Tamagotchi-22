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

typedef struct{
    char name[12];
    uint32_t pet;
    uint32_t exercise;
    uint32_t eat;
    uint32_t daylight;
    //time_t start;
} Pet;

typedef struct {
   uint8_t ax[8];
   uint8_t ay[8];
   uint8_t az[8];
} accValues;

typedef struct {
   uint8_t gx[8];
   uint8_t gy[8];
   uint8_t gz[8];
} gyroValues;

int  i = 0, j = 0, k = 0, num = 0; //may be used for loops

/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char buzzerTaskStack[STACKSIZE];

// Definition of the state machine
enum state { WAITING=1, DATA_READY };
enum state programState = WAITING;

// Global variable for ambient light
double ambientLight = -1000.0;

//Add pins RTOS-variables and configuration here
static PIN_Handle hButtonShut;
static PIN_State bStateShut;

static PIN_Handle hButtonSelect;
static PIN_State bStateSelect;

static PIN_Handle hLed;
static PIN_State sLed;

// MPU power pin global variables
static PIN_Handle hMpuPin;
static PIN_State  MpuPinState;

static PIN_Handle hBuzzer;
static PIN_State sBuzzer;

PIN_Config cBuzzer[] = {
  Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
  PIN_TERMINATE
};

PIN_Config buttonShut[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config buttonWake[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
   PIN_TERMINATE
};

PIN_Config buttonConfig[] = {
   Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};


PIN_Config ledConfig0[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

PIN_Config ledConfig1[] = {
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};


void simpleBuzzFxn(UArg arg0, UArg arg1) {

  while (1) {
    buzzerOpen(hBuzzer);
    buzzerSetFrequency(2000);
    Task_sleep(50000 / Clock_tickPeriod);
    buzzerClose();
    //?
    Task_sleep(950000 / Clock_tickPeriod);
  }

}

void jukeboxBuzzFxn(UArg arg0, UArg arg1){

    while (1) {
        buzzerOpen(hBuzzer);
        buzzerSetFrequency(525);
        Task_sleep(50000 / Clock_tickPeriod);
        buzzerSetFrequency(0);
        Task_sleep(20000 / Clock_tickPeriod);
        buzzerSetFrequency(590);
        Task_sleep(50000 / Clock_tickPeriod);
        buzzerSetFrequency(0);
        Task_sleep(20000 / Clock_tickPeriod);
        buzzerSetFrequency(660);
        Task_sleep(50000 / Clock_tickPeriod);
        buzzerSetFrequency(0);
        Task_sleep(20000 / Clock_tickPeriod);
        buzzerClose();

        Task_sleep(950000 / Clock_tickPeriod);
      }

}

//function for shutting it down
void shutFxn(PIN_Handle handle, PIN_Id pinId) {

   PINCC26XX_setWakeup(buttonShut);
   //CALL JUKEBOX
   Power_shutdown(NULL,0);
}

//function for turning it on
//?
void wakeFxn(PIN_Handle handle, PIN_Id pinId) {

   PINCC26XX_setWakeup(buttonWake);
   //CALL JUKEBOX
   Power_shutdown(NULL,0);
}

//or lightFxn
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

    //Blink either led of the device
    //
    if(pinId == Board_BUTTON0) {
        i++;
    }
    if(i > 3){
        i=0;
    }

    uint_t pinValue = PIN_getOutputValue(Board_LED0);
    pinValue = !pinValue;

    PIN_setOutputValue(handle, Board_LED0, pinValue );
}


/* Task Functions */
void uartTaskFxn(UArg arg0, UArg arg1) {

    char echo_msg [30];
    UART_Handle uart;
    UART_Params uartParams;


    // Setup here UART connection as 9600,8n1
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode=UART_MODE_BLOCKING;
    uartParams.baudRate = 9600;
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

        if(DATA_READY){
            /*char string[50];
            sprintf(string,"%.3f", ambientLight);
            System_printf(string);
            programState = WAITING;*/

            UARTAmbientLight(uart,uartParams);
            UARTAccGyro(uart,uartParams);

        // Send the same sensor data string with UART
        //UART_read(uart, &input, 1);
        /*sprintf(echo_msg,"Received: %.3f\n\r",ambientLight);

        UART_write(uart, echo_msg, strlen(echo_msg));*/
    }

        // Just for sanity check for exercise, you can comment this out
        System_printf(" uartTask\n");
        System_flush();

        // Once per second, you can modify this
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

void taskPet(UArg arg0, UArg arg1){

    double lux;

    //time
    //?
    //time_t start;

    //I2C_Transaction i2cMessage;

        I2C_Handle i2c;
        I2C_Params i2cParams;

   // I2C_MPU_Transaction i2c_MPU_Message
        I2C_Handle i2cMPU;
        I2C_Params i2cMPUParams;

   // Open the i2c bus

        I2C_Params_init(&i2cParams);
        i2cParams.bitRate = I2C_400kHz;

   // Open the i2cMPU bus

        I2C_Params_init(&i2cMPUParams);
        i2cMPUParams.bitRate = I2C_400kHz;
        i2cMPU = I2C_open(Board_I2C_TMP, &i2cMPUParams);
        if (i2cMPU == NULL) {
            System_abort("Error Initializing i2cMPU\n");
        }

    //sunlight
    while(1){
        //Display?
        //start = time (0);
        //System_printf("Receive sunlight", start);
        // OTHER SENSORS OPEN I2C
        i2c = I2C_open(Board_I2C_TMP, &i2cParams);
        if (i2c == NULL) {
            System_abort("Error Initializing I2C\n");
        }
        else {
            System_printf("I2C Initialized!\n");
        }
        Task_sleep(2000000/Clock_tickPeriod);
    }
}


void UARTAmbientLight (UART_Handle uart, UART_Params uartParams){

    char string[50];
    char echo_msg [30];
    sprintf(string,"%.3f", ambientLight);
    System_printf(string);
    programState = WAITING;
    sprintf(echo_msg,"Received: %.3f\n\r",ambientLight);

    UART_write(uart, echo_msg, strlen(echo_msg));

}


void UARTAccGyro (UART_Handle uart, UART_Params uartParams){
    //TODO (gr�ficas)
}



void sensorTaskAmbientLight(UArg arg0, UArg arg1) {

    double lux; //CREAR ARRAY?

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
           opt3001_setup(&i2c);

    while (1) {

        // Read sensor data and print it to the Debug window as string
        lux = opt3001_get_data(&i2c);
        char string[50];
        sprintf(string,"%f", lux);
        System_printf(string);

        // Save the sensor value into the global variable.   Remember to modify state
        ambientLight = lux;
        programState = DATA_READY;

        // Just for sanity check for exercise, you can comment this out
        System_printf(" sensorTask\n");
        System_flush();
        Task_sleep(1000000 / Clock_tickPeriod);

        //DESPU�S DE RECIBIR x valores / en x tiempo 0--> buttonShutFxn
    }
}

void sensorTaskAccGyro (UArg arg0, UArg arg1) {

    accValues acc;
    gyroValues gyro;
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

    mpu9250_setup(&i2cMPU);

    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();

           while (1) {
           // MPU ask data
           mpu9250_get_data(&i2cMPU, &acc.ax, &acc.ay, &acc.az, &gyro.gx, &gyro.gy, &gyro.gz);
           char string[50];
           sprintf(string,"%d", acc.ax);
           System_printf(string);
           sprintf(string,"%d", acc.ay);
           System_printf(string);
           sprintf(string,"%d", acc.az);
           System_printf(string);
           sprintf(string,"%d", gyro.gx);
           System_printf(string);
           sprintf(string,"%d", gyro.gy);
           System_printf(string);
           sprintf(string,"%d", gyro.gz);
           System_printf(string);
           // Sleep 100ms
           Task_sleep(100000 / Clock_tickPeriod);
           }

           // MPU close i2c
           I2C_close(i2cMPU);
           // MPU power off
           PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_OFF);
}


Int main(void) {

    // Task variables
    Task_Handle sensorTaskHandle1;
    Task_Handle sensorTaskHandle2;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;
    Task_Handle buzzerTaskHandle;
    Task_Params buzzerTaskParams;

    // Initialize board
    Board_initGeneral();
    //Init6LoWPAN();

    //Initialize i2c bus
    Board_initI2C();
    //Initialize UART
    Board_initUART();

       hBuzzer = PIN_open(&sBuzzer, cBuzzer);
       if (!hBuzzer) {
           System_abort("Error initializing buzzer pins\n");
       }
       hButtonSelect = PIN_open(&bStateSelect, buttonConfig);
       if(!hButtonSelect) {
          System_abort("Error initializing button pins\n");
       }
       hLed = PIN_open(&sLed, ledConfig0);
       if(!hLed) {
          System_abort("Error initializing LED pins\n");
       }
       hLed = PIN_open(&sLed, ledConfig1);
       if(!hLed) {
       System_abort("Error initializing LED pins\n");
       }

    // Open the shut/woke button and led pins
    // Remember to register the above interrupt handler for button

       hButtonShut = PIN_open(&bStateShut, buttonShut);

       if(!hButtonShut) {
          System_abort("Error initializing button shut pins\n");
       }
       if (PIN_registerIntCb(hButtonShut, &buttonShutFxn) != 0) {
          System_abort("Error registering button shut callback function");
       }
       if (PIN_registerIntCb(hButtonSelect, &buttonFxn) != 0) {
          System_abort("Error registering button select callback function");
       }


    /* Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority=2;
    sensorTaskHandle1 = Task_create(sensorTaskAmbientLight, &sensorTaskParams, NULL);
    sensorTaskHandle2 = Task_create(sensorTaskAccGyro, &sensorTaskParams, NULL);

    if (sensorTaskHandle1 == NULL) {
        System_abort("Task create failed!");
    }

    if (sensorTaskHandle2 == NULL) {
            System_abort("Task create failed!");
        }

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority=2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);

    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    //Another for buzzer?
    Task_Params_init(&buzzerTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &buzzerTaskStack;
    uartTaskParams.priority=2;
    uartTaskHandle = Task_create(buzzerTaskFxn, &buzzerTaskParams, NULL);

        if (buzzerTaskHandle == NULL) {
            System_abort("Task create failed!");
        }

    /* Sanity check */
    //?
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */

    BIOS_start();
    return (0);
}
