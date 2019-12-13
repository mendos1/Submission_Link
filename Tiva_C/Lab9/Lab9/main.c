#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom.h"

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

#define SERIES_LENGTH 1000 //size of buff array
float gSeriesData[SERIES_LENGTH]; //used to store the array of floats (1000)
 int32_t i32DataCount = 0;

int main(void) {
    float fRad_mid_part, fRad_end_part; //used to calculate the intermediate vars 0f the equation
    ROM_FPULazyStackingEnable();//enable lazy stacking
    ROM_FPUEnable();// enable the FPU
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

    fRad_mid_part = (2*M_PI*50)/SERIES_LENGTH;  // used to model the equations that
    fRad_end_part = (2*M_PI*200)/SERIES_LENGTH; // generates the 50Hz

    // here we calculate the values of the wave
    while(i32DataCount < SERIES_LENGTH) {
        gSeriesData[i32DataCount] = 1.5+(1.0*sinf(fRad_mid_part * i32DataCount)+0.5*cosf(fRad_end_part * i32DataCount));
        i32DataCount++;
    }

    //endless loop
    while(1)
    {

    }
}
