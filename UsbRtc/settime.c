/*
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

typedef unsigned char    uint8_t;
typedef unsigned int     uint32_t;

#define BUFSIZE   65


typedef enum 
{
   OK,
   NEEDS_CANCEL_INIT,
} ReadTime_Ret_t;


uint8_t NowTime[7] = {0x30,0x47,0x19,0x07,0x21,0x10,0x23};

static const char *gWeeks[8] =
{
   (const char *)"xxx",     // 0
   (const char *)"Sun",
   (const char *)"Mon",
   (const char *)"Tue",
   (const char *)"Wed",
   (const char *)"Thu",
   (const char *)"Fri",
   (const char *)"Sat"     // 7
};



static uint8_t  rbuf[BUFSIZE];
static uint8_t  wbuf[BUFSIZE];



static void           UsbRtc_i2cCancel(int fd);
static void           UsbRtc_InitBus(int fd);
static void           SetWbuf(uint8_t b0,uint8_t b1,uint8_t b2,uint8_t b3,uint8_t b4,uint8_t b5,uint8_t b6);
//static ReadTime_Ret_t UsbRtc_ReadTime(int fd);
static void           UsbRtc_SetTime(int fd);





int main(void)
{
    int  FFD,i,f;

    i = 0;

        if ((FFD=open("/dev/hidraw2",O_RDWR|O_NONBLOCK)) == -1)
        {
            perror("Error opening /dev/hidraw2");
            exit(1);
        }

        UsbRtc_InitBus(FFD);

        UsbRtc_SetTime(FFD);

    UsbRtc_i2cCancel(FFD);
    close(FFD);
    return 0;
}



static void SetWbuf(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6)
{
    wbuf[0] = b0;
    wbuf[1] = b1;
    wbuf[2] = b2;
    wbuf[3] = b3;
    wbuf[4] = b4;
    wbuf[5] = b5;
    wbuf[6] = b6;
}

static void UsbRtc_i2cCancel(int fd)
{
    usleep(10000);

    memset((void*)wbuf,0,BUFSIZE);
    memset((void*)rbuf,0,BUFSIZE);                                 // PyMCP2221A.I2C_Cancel

    SetWbuf(0, 0x10, 0, 0x10, 0, 0, 0);

    write(fd,(void*)wbuf,65);  usleep(1000);
    read (fd,(void*)rbuf,65);

    usleep(100000);
}


static void UsbRtc_InitBus(int fd)
{
    // Calculation for speed parameter:
    //     SPEED      = 100000
    //     speed_parm = ((12000000 / SPEED) - 3)
    //     speed_parm = 117
    uint8_t  sparm = 117;

    memset((void*)rbuf,0,BUFSIZE);                               // PyMCP2221A.I2C_Init
    memset((void*)wbuf,0,BUFSIZE);

    SetWbuf(0, 0x10, 0, 0, 0x20, sparm, 0);

    usleep(100000);
    write(fd,(void*)wbuf,65);  usleep(1000);
    read (fd,(void*)rbuf,65);  usleep(10000);
}


static void UsbRtc_SetTime(int fd)
{
    uint8_t  addr = 0xff & (0x68 << 1);             //    0x68 is the addr

    // ---- # 0 -----------------------------------------------

    //
    //   bus.write_byte_data(0x68, 0x00, NowTime[0])
    //      .I2C_Write(0x68, [0, 0])
    //              ._i2c_write(0x68,[0,0],[0,0x90])
    //                        buf[0] = 0
    //                        buf[1] = 0x90
    //                        buf[2] = (len(data) & 0x00FF)      = 2
    //                        buf[3] = (len(data) & 0xFF00) >> 8 = 0
    //                        buf[4] = 0xFF & (addrs << 1)       = addr, from calculation above
    //                        buf[5] = 0
    //                        buf[6] = 0
    //

    memset((void*)wbuf,0,BUFSIZE);                  // PyMCP2221A.I2C_Write_No_Stop 

    SetWbuf(0, 0x90, 2, 0, addr, 0, NowTime[0]);

    write(fd,(void*)wbuf,65);    usleep(1000);
    read (fd,(void*)rbuf,65);    usleep(1000);      // read and toss

    // ---- # 1 -----------------------------------------------

    memset((void*)wbuf,0,BUFSIZE);                  // PyMCP2221A.I2C_Write_No_Stop 

    SetWbuf(0, 0x90, 2, 0, addr, 1, NowTime[1]);

    write(fd,(void*)wbuf,65);    usleep(1000);
    read (fd,(void*)rbuf,65);    usleep(1000);      // read and toss


    // ---- # 2 -----------------------------------------------

    memset((void*)wbuf,0,BUFSIZE);                  // PyMCP2221A.I2C_Write_No_Stop 

    SetWbuf(0, 0x90, 2, 0, addr, 2, NowTime[2]);

    write(fd,(void*)wbuf,65);    usleep(1000);
    read (fd,(void*)rbuf,65);    usleep(1000);      // read and toss


    // ---- # 3 -----------------------------------------------

    memset((void*)wbuf,0,BUFSIZE);                  // PyMCP2221A.I2C_Write_No_Stop 

    SetWbuf(0, 0x90, 2, 0, addr, 3, NowTime[3]);

    write(fd,(void*)wbuf,65);    usleep(1000);
    read (fd,(void*)rbuf,65);    usleep(1000);      // read and toss

    // ---- # 4 -----------------------------------------------

    memset((void*)wbuf,0,BUFSIZE);                  // PyMCP2221A.I2C_Write_No_Stop 

    SetWbuf(0, 0x90, 2, 0, addr, 4, NowTime[4]);

    write(fd,(void*)wbuf,65);    usleep(1000);
    read (fd,(void*)rbuf,65);    usleep(1000);      // read and toss

    // ---- # 5 -----------------------------------------------

    memset((void*)wbuf,0,BUFSIZE);                  // PyMCP2221A.I2C_Write_No_Stop 

    SetWbuf(0, 0x90, 2, 0, addr, 5, NowTime[5]);

    write(fd,(void*)wbuf,65);    usleep(1000);
    read (fd,(void*)rbuf,65);    usleep(1000);      // read and toss

    // ---- # 6 -----------------------------------------------

    memset((void*)wbuf,0,BUFSIZE);                  // PyMCP2221A.I2C_Write_No_Stop 

    SetWbuf(0, 0x90, 2, 0, addr, 6, NowTime[6]);

    write(fd,(void*)wbuf,65);    usleep(1000);
    read (fd,(void*)rbuf,65);    usleep(1000);      // read and toss

    return;
}



