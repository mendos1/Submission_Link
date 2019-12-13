
//----------------------------------------
// BIOS header files
//----------------------------------------
#include <xdc/std.h>                        //mandatory - have to include first, for BIOS types
#include <ti/sysbios/BIOS.h>                //mandatory - if you call APIs like BIOS_start()
#include <xdc/runtime/Log.h>                //needed for any Log_info() call
#include <xdc/cfg/global.h>                 //header file for statically defined objects/handles


//------------------------------------------
// TivaWare Header Files
//------------------------------------------
#include <stdint.h>
#include <stdbool.h>

#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "inc/hw_ints.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "driverlib/adc.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "utils/uartstdio.h"
#include "utils/uartstdio.c"


//----------------------------------------
// Prototypes
//----------------------------------------
void HWI_init(void);
void ledToggle(void);
void Timer_ISR(void);
void ADC_init();
void ADCrx(void);
void Cons_init(void);
void UART_ADC_tx(void);


//---------------------------------------
// Globals have to be declared as volatile
//---------------------------------------
volatile int16_t Toggle_Counter = 0;
volatile int16_t Inst_Counter = 0;

// ADCValues stores the ADC values from the TIvaC and the size has to match the
// FIFO depth
uint32_t ADCValues[1];


// ADC_out is used to store the ADC output var to UART
uint32_t ADC_out ;

//---------------------------------------------------------------------------
// main()
//---------------------------------------------------------------------------
void main(void){
    HWI_init();
    ADC_init();
    Cons_init();
    BIOS_start();
}

//---------------------------------------------------------------------------
// HWI_init()
//---------------------------------------------------------------------------
void HWI_init(void){
    uint32_t Period;

    //Set CPU Clock to 40MHz. 400MHz PLL/2 = 200 DIV 5 = 40MHz
    SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|SYSCTL_OSC_MAIN);

    // ADD Tiva-C GPIO setup - enables port, sets pins 1-3 (RGB) pins for output
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);

    // Turn on the LED
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 4);

    // Timer 2 setup code
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);       // enable Timer 2 periph clks
    TimerConfigure(TIMER2_BASE, TIMER_CFG_PERIODIC);    // cfg Timer 2 mode - periodic

    Period = (SysCtlClockGet() / 20);                   // period = CPU clk div 20 (50ms)
    TimerLoadSet(TIMER2_BASE, TIMER_A, Period);         // set Timer 2 period

    TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);    // enables Timer 2 to interrupt CPU

    TimerEnable(TIMER2_BASE, TIMER_A);                  // enable Timer 2
}

//---------------------------------------------------------------------------
// ledToggle()
//---------------------------------------------------------------------------
void ledToggle(void){
    while(1){
        Semaphore_pend(LEDSem, BIOS_WAIT_FOREVER);

        if(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_2)){
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 0);
        }
        else{
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 4);
        }

        Toggle_Counter += 1;                                    // keep track of #toggles

        Log_info1("LED TOGGLED [%u] TIMES",Toggle_Counter);     // send toggle count to UIA
    }

}

//---------------------------------------------------------------------------
// Timer ISR - called by BIOS Hwi (see app.cfg)
//---------------------------------------------------------------------------
void Timer_ISR(void){
    TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);         // must clear timer flag FROM timer

    if(Inst_Counter == 10) {
        Semaphore_post(ADC3Sem);
    }

    else if (Inst_Counter == 20) {
        Semaphore_post(UARTSem);
    }

    else if(Inst_Counter == 30) {
        Semaphore_post(LEDSem);                                     // post LEDSwi
        Inst_Counter = 0;
    }
    Inst_Counter++;
}

//---------------------------------------------------------------------------
//ADC_init
//---------------------------------------------------------------------------
void ADC_init() {

    // The PE0 peripheral must be enabled for use.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlDelay(3);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlDelay(3);

    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0);    //Configure ADC pin: PE0

    // Sample from ADC0_BASE using sequencer 3
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);

    // Here sequence 3 is configured to be zero steps when it samples from adc
    // channel 3 with the interrupt flag set when done sampling and set the
    // ADC_CLT_END.
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH3  | ADC_CTL_IE | ADC_CTL_END);

    ADCSequenceEnable(ADC0_BASE, 3);

    // clear any previous flags
    ADCIntClear(ADC0_BASE, 3);
}

//---------------------------------------------------------------------------
//ADCrx
//---------------------------------------------------------------------------
void ADCrx(void) {

    while(1) {
        Semaphore_pend(ADC3Sem, BIOS_WAIT_FOREVER);

        // tell processor to trigger the ADC0_BASE
        ADCProcessorTrigger(ADC0_BASE, 3);

        // wait for completion
        while(!ADCIntStatus(ADC0_BASE, 3, false)){}

        // Clear ADC flag
        ADCIntClear(ADC0_BASE, 3);

        // store ADC0_BASE value into ADCValues
        ADCSequenceDataGet(ADC0_BASE, 3, ADCValues);
        ADC_out = ADCValues[0];
    }

}

//---------------------------------------------------------------------------
//Cons_init
//---------------------------------------------------------------------------
void Cons_init(void){

    // Enable GPIO port A to use with UART
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);

    // Enable UART0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    // set default 16MHz frequency
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // config UART0 at BAUD: 115200 F:16MHz
    UARTStdioConfig(0, 115200, 16000000);
}

//---------------------------------------------------------------------------
//UART_ADC_tx
//---------------------------------------------------------------------------
void UART_ADC_tx(void) {
    while(1) {
        Semaphore_pend(UARTSem, BIOS_WAIT_FOREVER);
        UARTprintf("ADC_out: %d\n\n", ADC_out);
    }
}
