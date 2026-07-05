
//
// Included Files
//
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "c2000ware_libraries.h"

#define TARGET_CMP 1000
#define ISR_TICKS 50
#define RESULTS_BUFFER_SIZE 256
#define TARGET_VOLTAGE 24

volatile int done=0;
volatile int counter=0;
volatile int starting_duty_cycle=0;

volatile uint16_t index = 0;
volatile uint16_t ADC_reading = 0;
volatile float output_voltage = 0.0f;
uint16_t myADC0Results[RESULTS_BUFFER_SIZE];

float KP=0.1f;
float KI=0.001f;
float proportional=0.0f;
float integral=0.0f;
float target_voltage=24.0f;
float duty_cycle=0.0f;


__interrupt void epwm1ISR(void)
{

    if (!done){

        counter++;

        if (counter>=ISR_TICKS){
            counter=0;

            if (starting_duty_cycle<TARGET_CMP){
                EPWM_setCounterCompareValue(myEPWM1_BASE, EPWM_COUNTER_COMPARE_A, starting_duty_cycle);
                starting_duty_cycle+=1;
            }

            else {
                EPWM_setCounterCompareValue(myEPWM1_BASE, EPWM_COUNTER_COMPARE_A, TARGET_CMP);
                done=1;
            }

        }
    }

    EPWM_clearEventTriggerInterruptFlag(myEPWM1_BASE);

    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);

}

__interrupt void adcA1ISR(void)
{

    uint16_t latest_result = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);

        ADC_reading = latest_result;
        myADC0Results[index++] = latest_result;
        output_voltage = (float)latest_result * 0.01476301476f *0.98f;

    if(RESULTS_BUFFER_SIZE <= index)
    {
        index = 0;
    }

    if (done==1){

        float error = TARGET_VOLTAGE - output_voltage;
        proportional = KP * error;
        integral += KI * error;

        duty_cycle= 0.50f + proportional + integral;

        if (duty_cycle>0.65){
            duty_cycle = 0.65;
            integral -= KI * error;
        }

        if (duty_cycle<0.15){
            duty_cycle=0.15;
            integral -=KI * error;
        }

        uint16_t new_cmp = (uint16_t)(duty_cycle * 2000.0f);
        EPWM_setCounterCompareValue(myEPWM1_BASE, EPWM_COUNTER_COMPARE_A, new_cmp);

    }

    //
    // Clear the interrupt flag
    //
    ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);

    //
    // Check if overflow has occurred
    //
    if(true == ADC_getInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER1))
    {
        ADC_clearInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);
    }

    //
    // Acknowledge the interrupt
    //
    Interrupt_clearACKGroup(INT_myADC0_1_INTERRUPT_ACK_GROUP);
}

void main(void)
{

    //
    // Initialize device clock and peripherals
    //

    Device_init();

    //
    // Disable pin locks and enable internal pull-ups.
    //
    Device_initGPIO();

    //
    // Initialize PIE and clear PIE registers. Disables CPU interrupts.
    //
    Interrupt_initModule();

    //
    // Initialize the PIE vector table with pointers to the shell Interrupt
    // Service Routines (ISR).
    //
    Interrupt_initVectorTable();

    //
    // PinMux and Peripheral Initialization
    //

    Interrupt_register(INT_EPWM1, &epwm1ISR);
    Interrupt_register(INT_myADC0_1, &adcA1ISR);

    Board_init();

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);


    Interrupt_enable(INT_EPWM1);
    Interrupt_enable(INT_myADC0_1);

    for(index = 0; index < RESULTS_BUFFER_SIZE; index++)
    {
        myADC0Results[index] = 0;
    }

    EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
    EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_UP);

    //
    // Enable Global Interrupt (INTM) and real time interrupt (DBGM)
    //
    EINT;
    ERTM;

    for(;;)
    {

    }

}

//
// End of File
//
