
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "sensorlib/i2cm_drv.c"
#include "sensorlib/hw_mpu6050.h"
#include "sensorlib/mpu6050.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "inc/hw_i2c.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/debug.h"
#include "driverlib/interrupt.h"
#include "driverlib/i2c.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "IQmath/IQmathLib.h"
#include <math.h>
#include "utils/uartstdio.h"
#include "driverlib/uart.h"

volatile bool MPU6050_DONE;
#define ACCELEROMETER_SENSITIVITY 16384
#define GYROSCOPE_SENSITIVITY 131
#define SAMPLE_RATE 0.01
#define RATIO (180/3.14)
tMPU6050 sMPU6050;
tI2CMInstance I2CMaster; // I2C Master global instantiation
_iq16 Pitch = _IQ(0);
_iq16 Roll = _IQ(0);

int clkFreq;

void delayMS(int ms) {
    SysCtlDelay( (SysCtlClockGet()/(3*1000))*ms ) ;  // less accurate
}

void InitI2C0(void){
    //enable I2C module 0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);

    //reset module
    SysCtlPeripheralReset(SYSCTL_PERIPH_I2C0);

    //enable GPIO peripheral that contains I2C 0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

    // Configure the pin muxing for I2C0 functions on port B2 and B3.
    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);

    // Select the I2C function for these pins.
    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);

    // Enable and initialize the I2C0 master module.  Use the system clock for
    // the I2C0 module.
    // I2C data transfer rate set to 400kbps.
    I2CMasterInitExpClk(I2C0_BASE, SysCtlClockGet(), true);

    //clear I2C FIFOs
    HWREG(I2C0_BASE + I2C_O_FIFOCTL) = 80008000;

    // Initialize the I2C master driver.
    I2CMInit(&I2CMaster, I2C0_BASE, INT_I2C0, 0xff, 0xff, SysCtlClockGet());

}

void ConfigureUART(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
    UARTStdioConfig(0, 115200, 16000000);
}

void MPU6050Callback(void *pvCallbackData, uint_fast8_t ui8Status){
    if (ui8Status != I2CM_STATUS_SUCCESS){

    }
    MPU6050_DONE = true;
}

void I2CMasterIntHandler(void){
    I2CMIntHandler(&I2CMaster);
}

void Complementary_Filter(float *fAccel, float *fGyro)
{
    _iq16 ForceMagApprx, PitchAcc, RollAcc, sensitivity, Rat, JKL, LKJ;
    _iq16 GyroVal[3], Acc[3];

    Rat = _IQ16(RATIO);
    JKL = _IQ16(0.98);
    LKJ = _IQ16(0.02);
    GyroVal[0] = _IQ16(fGyro[0]);
    GyroVal[1] = _IQ16(fGyro[1]);
    GyroVal[2] = _IQ16(fGyro[2]);
    Acc[0] = _IQ16(fAccel[0]);
    Acc[1] = _IQ16(fAccel[1]);
    Acc[2] = _IQ16(fAccel[2]);
    sensitivity = _IQ16(GYROSCOPE_SENSITIVITY);

    Pitch += _IQ16mpy(_IQ16div(GyroVal[0],sensitivity), _IQ16(SAMPLE_RATE));
    Roll -= _IQ16mpy(_IQ16div(GyroVal[1],sensitivity), _IQ16(SAMPLE_RATE));

    ForceMagApprx = _IQabs(Acc[0]) + _IQabs(Acc[1]) + _IQabs(Acc[2]);
    UARTprintf("\nForce Mag Apprx in CF: %d\n\n",(int)(ForceMagApprx));

    if(ForceMagApprx > 1940371 && ForceMagApprx < 4940371){

        PitchAcc = _IQ16mpy(_IQ16atan2(Acc[1],Acc[2]), Rat);
        //UARTprintf("PitchAcc bet 2g and 4g: %d\n",PitchAcc);

        Pitch = _IQ16mpy(Pitch,JKL) + _IQ16mpy(PitchAcc,LKJ);
        UARTprintf("Pitch bet 2g and 4g: %d\n",(int)Pitch);

        RollAcc = _IQ16mpy(_IQ16atan2(Acc[0],Acc[2]), Rat);
        //UARTprintf("RollAcc bet 2g and 4g: %d\n",RollAcc);

        Roll = _IQ16mpy(Roll,JKL) + _IQ16mpy(RollAcc,LKJ);
        UARTprintf("Roll bet 2g and 4g: %d\n",Roll);
    }

}

void MPU_Init(void){
    MPU6050_DONE = false;
    MPU6050Init(&sMPU6050, &I2CMaster, 0x68, MPU6050Callback, &sMPU6050);
    while (!MPU6050_DONE){}
}

void MPURMW(void){
    MPU6050_DONE = false;
    MPU6050ReadModifyWrite(&sMPU6050, MPU6050_O_ACCEL_CONFIG, ~MPU6050_ACCEL_CONFIG_AFS_SEL_M, MPU6050_ACCEL_CONFIG_AFS_SEL_4G, MPU6050Callback, &sMPU6050);
    while (!MPU6050_DONE){}
}

void MPU_Reset(void){
    MPU6050_DONE = false;
    MPU6050ReadModifyWrite(&sMPU6050, MPU6050_O_PWR_MGMT_1, 0x00, 0b00000010 & MPU6050_PWR_MGMT_1_DEVICE_RESET, MPU6050Callback, &sMPU6050);
    while (!MPU6050_DONE){}
}

void MPU_R(void){
    MPU6050_DONE = false;
    MPU6050ReadModifyWrite(&sMPU6050, MPU6050_O_PWR_MGMT_2, 0x00, 0x00, MPU6050Callback, &sMPU6050);
    while (!MPU6050_DONE){}
}

void MPU_ReadData(void){
    MPU6050_DONE = false;
    MPU6050DataRead(&sMPU6050, MPU6050Callback, &sMPU6050);
    while (!MPU6050_DONE){}
}

void MPU_GetAccFloat(float *fAccel, float *fGyro){
    MPU6050DataAccelGetFloat(&sMPU6050, &fAccel[0], &fAccel[1], &fAccel[2]);
}

void MPU_GetGyroFloat(float *fAccel, float *fGyro){
    MPU6050DataGyroGetFloat(&sMPU6050, &fGyro[0], &fGyro[1], &fGyro[2]);
}

void CF(float *fAccel, float *fGyro){
    Complementary_Filter(fAccel, fGyro);
}

void MPU6050Example(void)
{
    float fAccel[3], fGyro[3];

    MPU_Init();
    // configure to get 4g on accelerometer
    MPURMW();
    // here we reset previouse device settings
    MPU_Reset();
    MPU_R();
    while (1){
        MPU_ReadData();
        // Get the new accelerometer and gyroscope readings.
        MPU_GetAccFloat(fAccel,fGyro);
        MPU_GetGyroFloat(fAccel,fGyro);
        CF(fAccel,fGyro);
        delayMS(1000);
    }
}

int main()
{
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_PLL | SYSCTL_OSC_INT | SYSCTL_XTAL_16MHZ);

    InitI2C0();
    ConfigureUART();
    MPU6050Example();
    return(0);
}


