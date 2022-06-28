/***********************************************
 * System Controller
 * maincode.c
 * rev 1 - shabaz - June 2022
 * *********************************************/

// includes
#include <msp430.h>				
#include "RTC_Calendar.h"
#include "USI_I2CSlave.h"

// ******** defines ********
// change to suit requirements
// MINPWRSEC is a value from 1 to 59
#define MINPWRSEC 2
#define DEFAULTINTERVALSEC 30
#define I2CADDR 0x40
// P1.3 LED
#define LED_ON P1OUT |= (0x01 << 3)
#define LED_OFF P1OUT &= ~(0x01 << 3)
#define LED_TOGGLE P1OUT ^= (0x01 << 3)
#define PWR_ENABLE P2OUT |= (0x01 << 3)
#define PWR_DISABLE P2OUT &= ~(0x01 << 3)
#define STAYAWAKE ((P2IN & 0x10)!=0)
// I2C commands
#define ACTMIN 0x64
#define TIME 0x64
#define DATE 0x65
#define WAKEPERIODSEC 0x66
#define WAKEPERIODMIN 0x67
#define PLACEHOLDER1 0x68
#define READINPUTS 0x69
#define WRITEOUTPUTS 0x70
#define ACTMAX 0x70
// external input events
#define INEVENT_NONE 0
#define INEVENT_START 1
#define INEVENT_PROGRESS 2

#define STORAGE_MAX 64
// action states
#define IDLE 0
#define STORAGE 1
// other
#define RXMAX 4
#define PWROFF 0
#define ON1 1
#define ON2 2

// global vars
unsigned char xctr;
unsigned char rxbuf[4];
unsigned char txctr;
unsigned char wakeperiodsec;
unsigned char wakeperiodmin;
unsigned char stored_sec;
unsigned char stored_min;
//I2C-accessible storage
unsigned char storage[STORAGE_MAX];
unsigned char storage_idx;
// other
unsigned char action;
unsigned char pwrstate;
unsigned char input_event;

// function prototypes
int i2c_rx_callback(unsigned char);
int i2c_tx_callback(int*);
void i2c_start_callback();

// ********* interrupt service routines *********
// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
    incrementSeconds();
    __bic_SR_register_on_exit(LPM3_bits);
}

#pragma vector=PORT1_VECTOR
__interrupt void port1_isr(void)
{
    P1IFG = 0; // reset interrupt
    if (input_event==INEVENT_NONE) {
        input_event=INEVENT_START;
    }
    __bic_SR_register_on_exit(LPM3_bits);
}

// functions
void i2c_start_callback()
{
    xctr = 0;
}

int i2c_rx_callback(unsigned char RxData)
{
    if (xctr == 0) { // read the command that has arrived
        if (RxData < STORAGE_MAX) { // command is storage related
            action = STORAGE;
            storage_idx = RxData;
        } else if ((RxData>=ACTMIN) && (RxData<=ACTMAX)) {
            action = RxData;
        }
    } else { // handle the subsequent data that arrives
        if (action == STORAGE) {
            if (storage_idx<STORAGE_MAX) {
                storage[storage_idx] = RxData;
                storage_idx++;
            }
        } else if (action == TIME) { // we expect 4 bytes (BCD hour, min, sec, and AM/PM indication)
            if (xctr<5) {
                rxbuf[xctr-1] = RxData;
                if (xctr==4) {
                    setTime(rxbuf[0], rxbuf[1], rxbuf[2], rxbuf[3]);
                    action = IDLE;
                }
            }
        } else if (action == DATE) { // we expect 3 bytes (year from 2000, month, date)
            if (xctr<4) {
                rxbuf[xctr-1] = RxData;
                if (xctr==3) {
                    setDate(((int)rxbuf[0])+2000, rxbuf[1], rxbuf[2]);
                    action = IDLE;
                }
            }
        } else if (action == WAKEPERIODSEC) { // new wake period in seconds
            wakeperiodsec = RxData;
            wakeperiodmin = 0;
            action = IDLE;
        } else if (action == WAKEPERIODMIN) { // new wake period in minutes
            wakeperiodsec = 0;
            wakeperiodmin = RxData;
            action = IDLE;
        } else if (action == WRITEOUTPUTS) { // value to write to output pins
            if (RxData & 0x01) {
                P2OUT |= 0x01;
            } else {
                P2OUT &= ~0x01;
            }
            if (RxData & 0x02) {
                P2OUT |= 0x02;
            } else {
                P2OUT &= ~0x02;
            }
        }
    }
    xctr++;
    return TI_USI_STAY_LPM ; // stay in LPM
}

int i2c_tx_callback(int* TxDataPtr)
{
    if (action == STORAGE) { // send contents of storage
        *(unsigned char*)TxDataPtr = storage[storage_idx];
        storage_idx++;
#ifdef MOREFLASH // Need more Flash to implement this
        if (storage_idx >= STORAGE_MAX) {
            storage_idx = 0; // wrap around
        }
#endif
    } else if (action == TIME) {
        switch(xctr) {
            case 0:
                *(unsigned char*)TxDataPtr = TI_hour;
                break;
            case 1:
                *(unsigned char*)TxDataPtr = TI_minute;
                break;
            case 2:
                *(unsigned char*)TxDataPtr = TI_second;
                break;
            case 3:
                *(unsigned char*)TxDataPtr = TI_PM;
                break;
            default:
                break;
        }
    } else if (action == DATE) {
        switch(xctr) {
            case 0:
                *(unsigned char*)TxDataPtr = (unsigned char)(TI_year-2000);
                break;
            case 1:
                *(unsigned char*)TxDataPtr = TI_month;
                break;
            case 2:
                *(unsigned char*)TxDataPtr = TI_day;
                break;
            default:
                break;
        }
    } else if (action == READINPUTS) {
        *(unsigned char*)TxDataPtr = P1IN;
    }

    xctr++;  // Increment tx pointer
    return TI_USI_STAY_LPM ; // stay in LPM
}

// ************* main function ******************
void main (void)
{
    WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer
    BCSCTL3 |= XCAP_3;    // select 12pF caps
    //_delay_cycles(10000); // allow xtal time to start up

    // global var initialization
    xctr = 0;
    storage_idx = 0;
    action = IDLE;
    wakeperiodsec = DEFAULTINTERVALSEC;
    wakeperiodmin = 0;
    setDate( 2022, 6, 25 );
    setTime( 0x12, 0, 0, 0); // initialize time to 12:00:00 AM
    stored_sec= 0;
    stored_min = 0;
    pwrstate = PWROFF;
    input_event = INEVENT_NONE;

    // input/output initialization
    P1OUT = 0;
    P2OUT = 0;
    P1DIR = 0x3c; // Set P1.2, 1.3, 1.4, 1.5 as outputs
    P2DIR = 0x2f; // Set P2.0, 2.1, 2.2, 2.3, 2.5 as outputs
    P1REN |= 0x03; // resistor enabled for P1.0, 1.1 (to act as pull-down)
    P2REN |= 0x10; // resistor enabled for P2.4 (to act as pull-down)
    P1IES |= 0x01; // interrupt on low-to-high transition of P1.0
    P1IE |= 0x01; // interrupt enable for P1.0

    TI_USI_I2C_SlaveInit(I2CADDR, i2c_start_callback, i2c_rx_callback, i2c_tx_callback);

    CCR0 = 32768-1;
    TACTL = TASSEL_1+MC_1; // ACLK, upmode
    CCTL0 |= CCIE; // enable CCRO interrupt
    __bis_SR_register(GIE); // enable interrupts

    while(1) {
        __bis_SR_register(LPM3_bits); // enter LPM3, clock will be updated
        stored_sec++;
        if (input_event==INEVENT_START) {
            input_event=INEVENT_PROGRESS;
            stored_sec=0;
            pwrstate = ON1;
            PWR_ENABLE;
        }
        if (wakeperiodmin>0) {
            if (stored_sec>=60) {
                stored_min++;
                if (stored_min>=wakeperiodmin) {
                    pwrstate = ON1;
                    PWR_ENABLE;
                    stored_min=0;
                    stored_sec=0;
                }
            }
        } else {
            if (stored_sec>=wakeperiodsec) {
                stored_sec=0;
                pwrstate = ON1;
                PWR_ENABLE;
            }
        }
        if (pwrstate==ON1) {
            if (stored_sec>=MINPWRSEC) {
                if (STAYAWAKE) {
                    pwrstate = ON2;
                } else {
                    PWR_DISABLE;
                    if (input_event==INEVENT_PROGRESS) {
                        input_event = INEVENT_NONE;
                    }
                }
            }
        }
        if (pwrstate == ON2) {
            if (STAYAWAKE) {
                //
            } else {
                pwrstate = PWROFF;
                PWR_DISABLE;
                if (input_event==INEVENT_PROGRESS) {
                    input_event = INEVENT_NONE;
                }
            }
        }

        _NOP(); // set breakpoint here to see 1 second int.
    }
}


