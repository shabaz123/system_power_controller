// includes
 #include <stdio.h>
 #include <string.h>
 #include <stdint.h>
 #include <stdlib.h>
 //extern "C" {
 #include <wiringPi.h>
 //}
 #include "i2ctest.h"
 #include "i2cfunc.h"
 #include <math.h>

int msp_handle;
unsigned char tbuf[2];
unsigned char buf[11];

int
main(void)
{
    int ret;
    int i;

    wiringPiSetupGpio();
    msp_handle = i2c_open(I2CBUS, MSP_ADDR);

    // write outputs
    printf("writing output\n");
    buf[0]=0x70;
    buf[1]=0x03;
    ret = i2c_write(msp_handle, buf, 2);

    // read inputs
    printf("Reading inputs\n");
    for (i=0; i<1; i++) {
        buf[0]=0x69;
        ret = i2c_write(msp_handle, buf, 1);
        ret = i2c_read(msp_handle, buf, 1);
        printf("input is 0x%02x\n", buf[0]);
    }
    

    for (i=0; i<11; i++) {
        buf[i]=i;
    }
    // write test
    printf("selecting address 0x00 and writing 10 bytes to RAM..\n");
    ret = i2c_write(msp_handle, buf, 11);
    if (ret<0) printf("error writing!\n");

    // read test
    printf("clearing local buffer..\n");
    for (i=0; i<11; i++) {
        buf[i]=0;
    }
    printf("reading 10 bytes from address 0 onward:\n");
    ret = i2c_write(msp_handle, buf, 1); // buf[0] contains 0
    ret = i2c_read(msp_handle, buf, 10); // read 10 bytes
    if (ret<0) printf("error reading!\n");
    printf("read %d bytes: 0x%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, \n", 
        ret, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9]);

    // set date
    printf("setting date to 27th June 2022\n");
    buf[0]=0x65; // date instruction
    buf[1]=22;
    buf[2]=6;
    buf[3]=27;
    ret = i2c_write(msp_handle, buf, 4);

    // set time
    printf("setting time to 1:19:01 AM\n");
    buf[0]=0x64; // time instruction
    buf[1]=0x01;
    buf[2]=0x19;
    buf[3]=0x01;
    buf[4]=0;
    ret = i2c_write(msp_handle, buf, 5);

    printf("delaying 4 sec..\n");
    for (i=0; i<4; i++) {
        delay_ms(999);
    }

    // read time
    buf[0]=100;
    ret = i2c_write(msp_handle, buf, 1);
    ret = i2c_read(msp_handle, buf, 4);
    printf("time is %02x:%02x:%02x %d\n", buf[0], buf[1], buf[2], buf[3]);

    // change wake rate to 10 sec
    printf("changing wake rate to 10 sec\n");
    buf[0]=0x66;
    buf[1]=10;
    ret = i2c_write(msp_handle, buf, 2);

    // finished
    ret = i2c_close(msp_handle);
    if (ret<0) printf("error closing I2C\n");


    return(0);
}

