/* C Standard library */
#include <stdio.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"


/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char buzzerTaskStack[STACKSIZE];

/* MPU */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include "Board.h"
#include "sensors/mpu9250.h"

Void sleepms(int ms);

static const int SLEEP_DURATION = 40; // 25hz

// MPU power pin global variables
static PIN_Handle hMpuPin;
static PIN_State  MpuPinState;

// MPU power pin
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

// MPU uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

// enum state { WAITING=1, DATA_READY };
// enum state programState = WAITING;

// enum character_state {NO_CHARACTER=0,WAITING_FOR_BUZZ,READY_TO_SEND};
char characterToSend = NULL;
char characterToBuzz = NULL;
// enum character_state characterToSend_State = NO_CHARACTER;

static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;

// Pin configurations, with separate configuration for each pin
// The constant BOARD_BUTTON_0 corresponds to one of the buttons

PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE // The configuration table is always terminated with this constant
};

// The constant Board_LED0 corresponds to one of the LEDs
PIN_Config ledConfig[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE // The configuration table is always terminated with this constant
};
Char buff[20];

// UART-kirjaston asetukset
UART_Handle uart;
UART_Params uartParams;

/* Buzzer globals */
static PIN_Handle hBuzzer;
static PIN_State sBuzzer;
PIN_Config cBuzzer[] = {
  Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
  PIN_TERMINATE
};

/* Task Functions */
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

    if (characterToSend != NULL) return;

    // System_printf("button press so send space");
    // System_flush();

    characterToSend = ' ';
    characterToBuzz = ' ';
    // characterToSend_State = WAITING_FOR_BUZZ;

    // System_printf("sended space");
    // System_flush();
}

Void uartTaskFxn(UArg arg0, UArg arg1) {

    Char buff[20];

    // UART-kirjaston asetukset
    UART_Handle uart;
    UART_Params uartParams;

    // Alustetaan sarjaliikenne
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode=UART_MODE_BLOCKING;
    uartParams.baudRate = 9600; // nopeus 9600baud
    uartParams.dataLength = UART_LEN_8; // 8
    uartParams.parityType = UART_PAR_NONE; // n
    uartParams.stopBits = UART_STOP_ONE; // 1

    // Avataan yhteys laitteen sarjaporttiin vakiossa Board_UART0
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
       System_abort("Error opening the UART");
    }

    while (1) {

        if (characterToSend != NULL) {
            char x[4];

            sprintf(x, "%c\r\n\0", characterToSend);
            characterToSend = NULL;

            // System_printf("%s", x);
            UART_write(uart, x, 4);
        }

        sleepms(SLEEP_DURATION);
    }
}

// blocks the task for the duration!!!!!!!!!
Void playNoteForMs(PIN_Handle hBuzzer, int frequency, int ms) {
    if (frequency == 0)
        buzzerClose();
    else {
        buzzerOpen(hBuzzer);
        buzzerSetFrequency(frequency);
        sleepms(ms);
        buzzerClose();
    }
}

Void buzzerTaskFxn(UArg arg0, UArg arg1) {
    while (1) {

        if (characterToBuzz == NULL) {
            sleepms(SLEEP_DURATION / 2);
            continue;
        }

        if (characterToBuzz == '.'){   
        characterToBuzz = NULL;
            playNoteForMs(hBuzzer, 262 + 200, SLEEP_DURATION / 2);
            // playNoteForMs(hBuzzer, 330, 200);
            // playNoteForMs(hBuzzer, 392, 200);
        } else if (characterToBuzz == '-') {
        characterToBuzz = NULL;
            playNoteForMs(hBuzzer, 262 + 100, SLEEP_DURATION / 2);
            // playNoteForMs(hBuzzer, 311, 200);
            // playNoteForMs(hBuzzer, 392, 200);
        } else if (characterToBuzz == ' ') {
        characterToBuzz = NULL;

            playNoteForMs(hBuzzer, 262, SLEEP_DURATION / 2);
            // playNoteForMs(hBuzzer, 311, 200);
            // playNoteForMs(hBuzzer, 392, 200);
        }

    }
}

Void sleepms(int ms) {
    Task_sleep(ms * 1000 / Clock_tickPeriod);
}

Void sensorTaskFxn(UArg arg0, UArg arg1) {

    float ax, ay, az, gx, gy, gz;

    I2C_Handle      i2cMPU;
    I2C_Params      i2cParams;

    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    i2cParams.custom = (uintptr_t) &i2cMPUCfg;

    // MPU power on
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    // Wait 100ms for the MPU sensor to power up
    sleepms(100);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // MPU open i2c
    i2cMPU = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2cMPU == NULL) {
       System_abort("Error Initializing I2C\n");
    }

    // MPU setup and calibration
	System_printf("MPU9250: Setup and calibration...\n");
	System_flush();

	mpu9250_setup(&i2cMPU);

	System_printf("MPU9250: Setup and calibration OK\n");
	System_flush();

    while (1) {

        mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);

        char x[100];

        float threshold = 100.0;

        if (gx > threshold) {
            // System_printf("detected -");
            // System_flush();
            characterToSend = '-';
            characterToBuzz = '-';
            // characterToSend_State = WAITING_FOR_BUZZ;
        } else if (gy > threshold) {
            characterToSend = '.';
            characterToBuzz = '.';
            // System_printf("detected .");
            // System_flush();
            // characterToSend_State = WAITING_FOR_BUZZ;
        }

        sleepms(SLEEP_DURATION);
    }

}

Int main(void) {

    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;
    Task_Handle buzzerTaskHandle;
    Task_Params buzzerTaskParams;

    // Initialize board
    Board_initGeneral();
    Board_initUART();

    // Enable the pins for use in the program
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if(!buttonHandle) {
       System_abort("Error initializing button pins\n");
    }
    ledHandle = PIN_open(&ledState, ledConfig);
    if(!ledHandle) {
       System_abort("Error initializing LED pins\n");
    }
    hBuzzer = PIN_open(&sBuzzer, cBuzzer);
    if (hBuzzer == NULL) {
        System_abort("Pin open failed!");
    }

    // Set the button pin’s interrupt handler to function buttonFxn
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
       System_abort("Error registering button callback function");
    }

    /* Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority=2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL) {
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

    
    Task_Params_init(&buzzerTaskParams);
    buzzerTaskParams.stackSize = STACKSIZE;
    buzzerTaskParams.stack = &buzzerTaskStack;
    buzzerTaskHandle = Task_create((Task_FuncPtr)buzzerTaskFxn, &buzzerTaskParams, NULL);
    if (buzzerTaskHandle == NULL) {
        System_abort("Buzzer task create failed!");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
