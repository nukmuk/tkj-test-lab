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
Char ledTaskStack[STACKSIZE];

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

#include "queue.h"
#include "util.h"

Void uartReadCallbackFxn(UART_Handle handle, void *rxBuf, size_t len);

#define TASK_SLEEP_DURATION 40 // 25hz

const int DOT_TIME = 100;
const int DASH_TIME = 300;
const int SPACE_TIME = 500;

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

enum state { MENU=1, SEND /*,RECEIVE*/ };
enum state programState = MENU;

char characterToSend = NULL;
char characterToBuzz = NULL;

static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle powerButtonHandle;
static PIN_State powerButtonState;
static PIN_Handle ledHandle;
static PIN_State ledState;

// Pin configurations, with separate configuration for each pin
// The constant BOARD_BUTTON_0 corresponds to one of the buttons

PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE // The configuration table is always terminated with this constant
};
PIN_Config powerButtonConfig[] = {
   Board_BUTTON1  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
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

Queue receiveQueue;

/* Task Functions */
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

    if (characterToSend != NULL) return;    

    characterToSend = ' ';
    characterToBuzz = ' ';
}

void uart_sendCharacter(UART_Handle uart, char c) {
    char x[4];

    sprintf(x, "%c\r\n\0", characterToSend);

    UART_write(uart, x, 4);
}


Char uartRxBuf[16];
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
    uartParams.readMode=UART_MODE_CALLBACK;
    uartParams.baudRate = 9600; // nopeus 9600baud
    uartParams.dataLength = UART_LEN_8; // 8
    uartParams.parityType = UART_PAR_NONE; // n
    uartParams.stopBits = UART_STOP_ONE; // 1

    uartParams.readCallback  = &uartReadCallbackFxn; // Käsittelijäfunktio

    // Avataan yhteys laitteen sarjaporttiin vakiossa Board_UART0
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
       System_abort("Error opening the UART");
    }

    UART_read(uart, uartRxBuf, 1);

    while (1) {

        if (characterToSend != NULL) {
            uart_sendCharacter(uart, characterToSend);
            characterToSend = NULL;

        }
        sleepms(TASK_SLEEP_DURATION);
    }
}

void buzzCharacter(char c) {
    int baseFrequency = 262;
    int delta = 100;

    if (c == '.') {
        playNoteForMs(hBuzzer, baseFrequency + delta * 2, TASK_SLEEP_DURATION / 2);
    } else if (c == '-') {
        characterToBuzz = NULL;
        playNoteForMs(hBuzzer, baseFrequency + delta, TASK_SLEEP_DURATION / 2);
    } else if (c == ' ') {
        characterToBuzz = NULL;
        playNoteForMs(hBuzzer, baseFrequency, TASK_SLEEP_DURATION / 2);
    }
}

Void buzzerTaskFxn(UArg arg0, UArg arg1) {
    while (1) {

        if (characterToBuzz == NULL) {
            sleepms(TASK_SLEEP_DURATION / 2);
            continue;
        }

        buzzCharacter(characterToBuzz);
        characterToBuzz = NULL;
    }
}

void uartReadCallbackFxn(UART_Handle handle, void *rxBuf, size_t len) {
    char* input = (char*)rxBuf;
    size_t i = 0;
    while (i < len) {
        switch (input[i]) {
            case '.':
                // print(".");
                queue_push(&receiveQueue, '.');
                // PIN_setOutputValue(ledHandle, Board_LED0, !PIN_getOutputValue(Board_LED0));
                break;
            case '-':
                // print("-");
                queue_push(&receiveQueue, '-');
                break;
            case ' ':
                // print(" ");
                queue_push(&receiveQueue, ' ');
                break;
        }
        i++;
    }
    UART_read(handle, rxBuf, 1);
}

Void ledTaskFxn(UArg arg0, UArg xarg1) {
    while (1) {
        if (queue_is_empty(&receiveQueue)) {
            sleepms(TASK_SLEEP_DURATION);
            continue;
        }
        sleepms(10);
        char c;
        queue_pop(&receiveQueue, &c);

        // PIN_setOutputValue(ledHandle, Board_LED0, 1);

        switch (c) {
            case '.':
                // print("piste");
                playNoteForMs(hBuzzer, 500, DOT_TIME);
                break;
            case '-':
                // print("dash");
                playNoteForMs(hBuzzer, 500, DASH_TIME);
                break;
            case ' ':
                // print("space");
                playNoteForMs(hBuzzer, 500, SPACE_TIME);
                break;
        }
        // sleepms(SPACE_TIME);
    }   
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

        float threshold = 100.0;

        if (gx > threshold) {
            characterToSend = '-';
            characterToBuzz = '-';
        } else if (gy > threshold) {
            characterToSend = '.';
            characterToBuzz = '.';
        }

        sleepms(TASK_SLEEP_DURATION);
    }

}


// Void startupChimeTaskFxn(UArg arg0, UArg arg1) {

//     struct Note notes[] = {
//         {NOTE_B5, 8},
//         {NOTE_FS6, 8},
//         {NOTE_B6, 8},
//         {NOTE_AS6, 2},
//         {0, 2},
//         {NOTE_FS6, 4},
//         {0, 2}
//     };
//     struct Chime chime = {
//         &notes, 7, 150
//     };

//     playChime(hBuzzer, &chime);
// }


Int main(void) {

    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;
    Task_Handle buzzerTaskHandle;
    Task_Params buzzerTaskParams;
    Task_Handle ledTaskHandle;
    Task_Params ledTaskParams;

    // Initialize board
    Board_initGeneral();
    Board_initUART();

    queue_init(&receiveQueue);

    // Enable the pins for use in the program
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if(!buttonHandle) {
       System_abort("Error initializing button pins\n");
    }
    powerButtonHandle = PIN_open(&powerButtonState, powerButtonConfig);
    if(!powerButtonHandle) {
       System_abort("Error initializing powerButton pins\n");
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
    if (PIN_registerIntCb(powerButtonHandle, &buttonFxn) != 0) {
       System_abort("Error registering powerButton callback function");
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
    buzzerTaskParams.priority=2;
    buzzerTaskHandle = Task_create(buzzerTaskFxn, &buzzerTaskParams, NULL);
    if (buzzerTaskHandle == NULL) {
        System_abort("Buzzer task create failed!");
    }


    Task_Params_init(&ledTaskParams);
    ledTaskParams.stackSize = STACKSIZE;
    ledTaskParams.stack = &ledTaskStack;
    ledTaskParams.priority=1;
    ledTaskHandle = Task_create(ledTaskFxn, &ledTaskParams, NULL);
    if (ledTaskHandle == NULL) {
        System_abort("LED task create failed!");
    }


    

    // Char chimeStack[128];
    // Task_Handle chimeTaskHandle;
    // Task_Params chimeTaskParams;
    
    // Task_Params_init(&chimeTaskParams);
    // chimeTaskParams.stackSize = 128;
    // chimeTaskParams.stack = &chimeStack;
    // chimeTaskParams.priority=2;
    // chimeTaskHandle = Task_create(startupChimeTaskFxn, &chimeTaskParams, NULL);
    // if (chimeTaskHandle == NULL) {
    //     System_abort("Task create failed!");
    // }
    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
