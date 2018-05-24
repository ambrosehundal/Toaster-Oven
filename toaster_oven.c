// **** Include libraries here ****
// Standard libraries

//CMPE13 Support Library
#include "BOARD.h"

// Microchip libraries
#include <xc.h>
#include <plib.h>
#include "Oled.h"
#include "OledDriver.h"
#include "Adc.h"
#include "Buttons.h"
#include "leds.h"



// **** Set any macros or preprocessor directives here ****
// Set a macro for resetting the timer, makes the code a little clearer.
#define TIMER_2HZ_RESET() (TMR1 = 0)
#define DEFAULT_TEMP 300 //default temperature
#define LONG_PRESS 5

// **** Declare any datatypes here ****

//Cooking Modes
typedef enum CookingState {
    BAKE, TOAST, BROIL
} cookingMode;

//Ovenstates
typedef enum OvenState {
    RESET, START, COUNTDOWN, PENDING_RESET, PENDING_SELECTOR_CHANGE
} ovenstate;

//whether Oven is ON/OFF
typedef enum powerstate {
    OFF, ON
} power;


//INPUT SELECTION
typedef enum InputSelection {
    TIME, TEMP
} input;

typedef struct {
    uint8_t cookingTimeLeft;    // countdown time left once timer is started
    uint16_t InitCookTime;      // Initial cooking time set
    int temp;                   // Cooking temperature
    cookingMode cookingMode;    // BAKE, TOAST or BROIL mode
    ovenstate ovenState;        // state of the oven machine
    power power;                //whether oven is on/off
    uint8_t buttonPressCounter; //counter for comparing elapsedtime and LONG PRESS
    input inputSelector;
} OvenData;

// **** Define any module-level, global, or external variables here ****


static uint16_t FreeRunningCounter;   //counter to see if BTN pressed > 1s
uint16_t startTime;   // time when countdown was started
uint8_t bEvent = 0;  //ButtonEvent flag check
static uint8_t led;  //state of the LEDs at a given point
static uint8_t twoHertz; //2Hz timer flag
static int cookTime;  // to store 0.5 seconds every 2Hz event

// Configuration Bit settings

/*OLED UPDATE HELPER FUNCTION*/
void OledDisplay(char a[]) {
    OledDrawString(a);
    OledUpdate();
}

/*function that populates OLED with struct*/
void UpdateDisplay(OvenData oven) {
    const char TOP_ON[] = "\x01\x01\x01\x01\x01";
    const char TOP_OFF[] = "\x02\x02\x02\x02\x02";
    const char BOTTOM_ON[] = "\x03\x03\x03\x03\x03";
    const char BOTTOM_OFF[] = "\x04\x04\x04\x04\x04";
    char s[100];

    if (oven.cookingMode == BAKE) {
        //if BAKE mode & in countdown
        if (oven.ovenState == COUNTDOWN) {
            sprintf(s, "|%s|\tMode: BAKE\n|     |\tTime: %d:%02d\n|-----|\tTemp: %d\xF8 F\n|%s|",
                    TOP_ON, (oven.cookingTimeLeft / 60), (oven.cookingTimeLeft % 60), oven.temp, BOTTOM_ON);

        } else {
            //if input selector is TIME
            if (oven.inputSelector == TIME) {
              sprintf(s, "|%s|\tMode: BAKE\n|     |\t>Time: %d:%02d\n|-----|\tTemp: %d\xF8 F\n|%s|",
                    TOP_OFF, (oven.InitCookTime / 60), (oven.InitCookTime % 60), oven.temp, BOTTOM_OFF);
            //if input selector is TEMP
            } else if (oven.inputSelector == TEMP) {
              
                  sprintf(s, "|%s|\tMode: BAKE\n|     |\tTime: %d:%02d\n|-----|\t>Temp: %d\xF8 F\n|%s|",
                    TOP_OFF, (oven.InitCookTime / 60), (oven.InitCookTime % 60), oven.temp, BOTTOM_OFF);
            }
        }


    } else if (oven.cookingMode == TOAST) {
        if(oven.ovenState == COUNTDOWN || oven.ovenState == PENDING_RESET){
            sprintf(s, "|%s|\tMode: TOAST\n|     |\tTime: %d:%02d\n|-----|\n|%s|",
                    TOP_OFF, (oven.cookingTimeLeft / 60), (oven.cookingTimeLeft % 60), BOTTOM_ON);
        }
        else{
            sprintf(s, "|%s|\tMode: TOAST\n|     |\tTime: %d:%02d\n|-----|\n|%s|",
                    TOP_OFF, (oven.InitCookTime / 60), (oven.InitCookTime % 60), BOTTOM_OFF);
        }
      
    } 
    else if (oven.cookingMode == BROIL) {
        if(oven.ovenState == COUNTDOWN || oven.ovenState == PENDING_RESET){
            sprintf(s, "|%s|\tMode: BROIL\n|     |\tTime: %d:%02d\n|-----|\tTemp: %d\xF8 F\n|%s|",
                    TOP_OFF, (oven.cookingTimeLeft / 60), (oven.cookingTimeLeft % 60), oven.temp, BOTTOM_ON);
        }
        else{
            sprintf(s, "|%s|\tMode: BROIL\n|     |\tTime: %d:%02d\n|-----|\tTemp: %d\xF8 F\n|%s|",
                    TOP_OFF, (oven.InitCookTime / 60), (oven.InitCookTime % 60), oven.temp, BOTTOM_OFF);
        }
      
    } 
    OledClear(OLED_COLOR_BLACK); //clear screen
    OledDrawString(s);
    OledUpdate();

}

//function to print values for testing

void TestCase(OvenData oven) {
    char m [100];
    sprintf(m, "CM:%d\n", oven.cookingMode);

    OledDrawString(m);
    OledUpdate();
}

int main() {
    BOARD_Init();

    // Configure Timer 1 using PBCLK as input. We configure it using a 1:256 prescalar, so each timer
    // tick is actually at F_PB / 256 Hz, so setting PR1 to F_PB / 256 / 2 yields a 0.5s timer.
    OpenTimer1(T1_ON | T1_SOURCE_INT | T1_PS_1_256, BOARD_GetPBClock() / 256 / 2);

    // Set up the timer interrupt with a medium priority of 4.
    INTClearFlag(INT_T1);
    INTSetVectorPriority(INT_TIMER_1_VECTOR, INT_PRIORITY_LEVEL_4);
    INTSetVectorSubPriority(INT_TIMER_1_VECTOR, INT_SUB_PRIORITY_LEVEL_0);
    INTEnable(INT_T1, INT_ENABLED);

    // Configure Timer 2 using PBCLK as input. We configure it using a 1:16 prescalar, so each timer
    // tick is actually at F_PB / 16 Hz, so setting PR2 to F_PB / 16 / 100 yields a .01s timer.
    OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_16, BOARD_GetPBClock() / 16 / 100);

    // Set up the timer interrupt with a medium priority of 4.
    INTClearFlag(INT_T2);
    INTSetVectorPriority(INT_TIMER_2_VECTOR, INT_PRIORITY_LEVEL_4);
    INTSetVectorSubPriority(INT_TIMER_2_VECTOR, INT_SUB_PRIORITY_LEVEL_0);
    INTEnable(INT_T2, INT_ENABLED);

    // Configure Timer 3 using PBCLK as input. We configure it using a 1:256 prescalar, so each timer
    // tick is actually at F_PB / 256 Hz, so setting PR3 to F_PB / 256 / 5 yields a .2s timer.
    OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_256, BOARD_GetPBClock() / 256 / 5);

    // Set up the timer interrupt with a medium priority of 4.
    INTClearFlag(INT_T3);
    INTSetVectorPriority(INT_TIMER_3_VECTOR, INT_PRIORITY_LEVEL_4);
    INTSetVectorSubPriority(INT_TIMER_3_VECTOR, INT_SUB_PRIORITY_LEVEL_0);
    INTEnable(INT_T3, INT_ENABLED);

    /***************************************************************************************************
     * Your code goes in between this comment and the following one with asterisks.
     **************************************************************************************************/
    /*Initialize ADC value, OLED and LEDs */
    OledInit();
    AdcInit();
    LEDS_INIT();

    /*Initialize struct*/
    OvenData t1;
    t1.cookingMode = BAKE;
    t1.temp = 300;
    t1.InitCookTime = 0;
    t1.ovenState = START;
    t1.inputSelector = TIME;



    while (1) {
        switch (t1.ovenState) {
            case RESET:
                t1.inputSelector = TIME;
                t1.temp = DEFAULT_TEMP;
                t1.power = OFF;
                t1.InitCookTime = 1;

                t1.ovenState = START;
                LEDS_SET(0x0);
                UpdateDisplay(t1);
                break;



            case START:
                  //Update ADC value if potentiometer changed
                if (AdcChanged()) {
                    if (t1.inputSelector == TEMP) {
                        t1.temp = (AdcRead() >> 2) + 300;
                    }
                    if (t1.inputSelector == TIME) {
                        t1.InitCookTime = (AdcRead() >> 2) + 1;
                    }


                    UpdateDisplay(t1);


                }
                /*BUTTON_EVENT_4DOWN event*/
                if (bEvent & BUTTON_EVENT_4DOWN) {
                    TIMER_2HZ_RESET();
                    cookTime = (t1.InitCookTime) *2;
                    t1.power = ON;
                    bEvent = 0;
                    UpdateDisplay(t1);
                    t1.ovenState = COUNTDOWN;
                }




                /*BUTTON_EVENT_3DOWN */
                if (bEvent & BUTTON_EVENT_3DOWN) {
                    startTime = FreeRunningCounter;
                    t1.ovenState = PENDING_SELECTOR_CHANGE;
                    bEvent = 0;
                }
                break;
            case PENDING_SELECTOR_CHANGE:
                
                // if BTN3 was pressed < 1s
                if ((bEvent & BUTTON_EVENT_3UP) && (FreeRunningCounter - startTime < LONG_PRESS)) {


                    //BAKE MODE to TOAST
                    if (t1.cookingMode == BAKE) {
                        t1.temp = 0;
                        t1.InitCookTime = 0;
                        t1.cookingMode = TOAST;
                        // TestCase(t1);
                        t1.inputSelector = TIME;
                        UpdateDisplay(t1);
                        bEvent = 0;
                        t1.ovenState = START;


                    }//TOAST to BROIL
                    else if (t1.cookingMode == TOAST) {
                        t1.temp = 500;
                        t1.InitCookTime = 0;
                        t1.cookingMode = BROIL;
                        t1.inputSelector = TIME;
                        UpdateDisplay(t1);
                        bEvent = 0;
                        t1.ovenState = START;

                        //BROIL to BAKE
                    } else if (t1.cookingMode == BROIL) {
                        t1.temp = 0;
                        t1.InitCookTime = 0;
                        t1.cookingMode = BAKE;
                        t1.inputSelector = TIME;
                        UpdateDisplay(t1);
                        bEvent = 0;
                        t1.ovenState = START;

                    }


                }       
                    //if BTN3 was pressed > 1s
                else if ((bEvent & BUTTON_EVENT_3UP) &&
                        (FreeRunningCounter - startTime >= LONG_PRESS)) {
                 //Change input selection based off previous set input selection
                    if (t1.cookingMode == BAKE) {
                        if (t1.inputSelector == TIME) {
                            t1.inputSelector = TEMP;
                        } else if (t1.inputSelector == TEMP) {
                            t1.inputSelector = TIME;
                        }
                    }
                    //update TEMP
                    if (t1.cookingMode == BAKE) {
                        if (t1.inputSelector == TEMP) {

                            t1.temp = (AdcRead() >> 2) + 300;
                            UpdateDisplay(t1);

                        }

                    }
                    //update TIME
                    if (t1.inputSelector == TIME) {

                        t1.InitCookTime = (AdcRead() >> 2) + 1;
                        UpdateDisplay(t1);


                    }




                    bEvent = 0;
                    t1.ovenState = START;

                }





                break;

            case COUNTDOWN:
                if ((bEvent & BUTTON_EVENT_4DOWN)) {
                    startTime = FreeRunningCounter;
                    bEvent = 0;
                    t1.ovenState = PENDING_RESET;
                
                } else if ((t1.cookingTimeLeft > 0) && (twoHertz == TRUE)) {
                  //after every 2Hz timer event, decrement the cooking time left 
                   t1.cookingTimeLeft -= 1;
                   //LEDs light setting based on how much cookingTimeLeft
                    led = ((t1.cookingTimeLeft * 8 / t1.InitCookTime) + 1);
                    if (led == 8) {
                        LEDS_SET(0xFF);
                    } else if (led == 7) {
                        LEDS_SET(0xFE);
                    } else if (led == 6) {
                        LEDS_SET(0xFC);
                    } else if (led == 5) {
                        LEDS_SET(0xF8);
                    } else if (led == 4) {
                        LEDS_SET(0xF0);
                    } else if (led == 3) {
                        LEDS_SET(0xE0);
                    } else if (led == 2) {
                        LEDS_SET(0xC0);
                    } else if (led == 1) {
                        LEDS_SET(0x80);
                    }
                    UpdateDisplay(t1);
                    t1.ovenState = COUNTDOWN;
                    twoHertz = 0;
                // if countdown over, change state to RESET
                } else if ((t1.cookingTimeLeft == 0) && (twoHertz == TRUE)) {
                    UpdateDisplay(t1);
                    twoHertz = 0;
                    t1.ovenState = RESET;

                }

                break;

            case PENDING_RESET:
               
                if (bEvent & BUTTON_EVENT_4UP) {
                    bEvent = 0;
                    t1.ovenState = COUNTDOWN;
                }
                if ((t1.cookingTimeLeft > 0) && (twoHertz == TRUE)) {
                
                    t1.cookingTimeLeft -=1;
                    led = ((t1.cookingTimeLeft * 8) / t1.InitCookTime) + 1;
                    if (led == 8) {
                        LEDS_SET(0xFF);
                    } else if (led == 7) {
                        LEDS_SET(0xFE);
                    } else if (led == 6) {
                        LEDS_SET(0xFC);
                    } else if (led == 5) {
                        LEDS_SET(0xF8);
                    } else if (led == 4) {
                        LEDS_SET(0xF0);
                    } else if (led == 3) {
                        LEDS_SET(0xE0);
                    } else if (led == 2) {
                        LEDS_SET(0xC0);
                    } else if (led == 1) {
                        LEDS_SET(0x80);
                    }



                    UpdateDisplay(t1);
                    t1.ovenState = PENDING_RESET;
                    twoHertz = 0;
                } else if ((t1.cookingTimeLeft == 0) && (twoHertz == TRUE)) {

                    twoHertz = 0;
                    t1.ovenState = RESET;

                }
                if (FreeRunningCounter - startTime >= LONG_PRESS) {
                    t1.ovenState = RESET;
                }

                break;

        }
    }




    /***************************************************************************************************
     * Your code goes in between this comment and the preceding one with asterisks
     **************************************************************************************************/
    while (1);
}

void __ISR(_TIMER_1_VECTOR, ipl4auto) TimerInterrupt2Hz(void) {
    // Clear the interrupt flag.

    IFS0CLR = 1 << 4;
    twoHertz = TRUE;

}

void __ISR(_TIMER_3_VECTOR, ipl4auto) TimerInterrupt5Hz(void) {
    // Clear the interrupt flag.

    IFS0CLR = 1 << 12;
    FreeRunningCounter++;


}

void __ISR(_TIMER_2_VECTOR, ipl4auto) TimerInterrupt100Hz(void) {
    // Clear the interrupt flag.
    IFS0CLR = 1 << 8;
    bEvent = ButtonsCheckEvents();
}
