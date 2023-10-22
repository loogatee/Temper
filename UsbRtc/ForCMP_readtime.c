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
static void           SetWbuf(uint8_t b0,uint8_t b1,uint8_t b2,uint8_t b3,uint8_t b4,uint8_t b5);
static ReadTime_Ret_t UsbRtc_ReadTime(int fd);





int main(void)
{
    int  FFD,i,f;

    i = 0;

/*
        if ((FFD=open("/dev/hidraw2",O_RDWR|O_NONBLOCK)) == -1)
        {
            perror("Error opening /dev/hidraw2");
            exit(1);
        } */


    while(1)
    {
        if ((FFD=open("/dev/hidraw2",O_RDWR|O_NONBLOCK)) == -1)
        {
            perror("Error opening /dev/hidraw2");
            exit(1);
        }

        usleep(500000);

        UsbRtc_InitBus(FFD);

        if (UsbRtc_ReadTime(FFD) == NEEDS_CANCEL_INIT)
        {
            printf("Calling _i2cCancel\n");
            UsbRtc_i2cCancel(FFD);
            close(FFD);
            usleep(200000);
            continue;
        }
        break;
    }

    //printf("Calling _i2cCancel.2\n");
    usleep(5000);
    //UsbRtc_i2cCancel(FFD);
    usleep(40000);
    close(FFD);
    usleep(500000);
    return 0;
}



static void SetWbuf(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)
{
    wbuf[0] = b0;
    wbuf[1] = b1;
    wbuf[2] = b2;
    wbuf[3] = b3;
    wbuf[4] = b4;
    wbuf[5] = b5;
}

static void UsbRtc_i2cCancel(int fd)
{
    usleep(10000);

    memset((void*)wbuf,0,BUFSIZE);
    memset((void*)rbuf,0,BUFSIZE);                                 // PyMCP2221A.I2C_Cancel

    SetWbuf(0, 0x10, 0, 0x10, 0, 0);

    write(fd,(void*)wbuf,65);  usleep(1000);
    read (fd,(void*)rbuf,65);  usleep(500000);
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

    SetWbuf(0, 0x10, 0, 0, 0x20, sparm);

    write(fd,(void*)wbuf,65);  usleep(2000);
    read (fd,(void*)rbuf,65);  usleep(100000);
}

/*
def ds3231ReadTime():
    #return bus.read_i2c_block_data(address,register,7);
    A=bus.read_i2c_block_data(address,0,4);
    B=bus.read_i2c_block_data(address,4,3);
    A.append(B[0])
    A.append(B[1])
    A.append(B[2])
    return A
*/

static ReadTime_Ret_t UsbRtc_ReadTime(int fd)
{
    uint8_t  addr = 0xff & (0x68 << 1);             //    0x68 is the addr

    // ---- # 1 -----------------------------------------------

    // A=bus.read_i2c_block_data(address,0,4);
    //       addrs = 0x68
    //       cmd   = 0
    //       size  = 4
    //                  .I2C_Write_No_Stop(0x68,[0])
    //                  ._i2c_write(0x68,[0],[0x00,0x94])    (addrs,data,buf)
    //                         buf[0] = 0x00;
    //                         buf[1] = 0x94
    //                         buf[2] = (len(data) & 0x00FF) = 1
    //                         buf[3] = (len(data) & 0xFF00) >> 8   = 0
    //                         buf[4] = 0xFF & (0x68 << 1)
    //                         Everything else 0's

    memset((void*)wbuf,0,BUFSIZE);                  // PyMCP2221A.I2C_Write_No_Stop 

    SetWbuf(0, 0x94, 1, 0, addr, 0);

    write(fd,(void*)wbuf,65);    usleep(2000);
    read (fd,(void*)rbuf,65);    usleep(3000);      // read and toss

    // ---- # 2 -----------------------------------------------

    // .I2C_Read_Repeated(0x68,4)        addrs, size
    //
    //     _i2c_read(0x68,4,[0x00,0x93])
    //                 buf[0] = 0x00
    //                 buf[1] = 0x93
    //                 buf[2] = (size & 0x00FF) = 4

    memset((void*)wbuf,0,BUFSIZE);                   // I2C_Read_Repeated
    rbuf[1] = 0;                                     //   only rbuf[1] is examined

    SetWbuf(0, 0x93, 7, 0, addr, 0);

    write(fd,(void*)wbuf,65);    usleep(2000);
    read (fd,(void*)rbuf,65);    usleep(3000);

    if (rbuf[1] != 0)
    {
        printf("Houston, we have a problem, rbuf[1] = 0x%x\n", rbuf[1]);
        close(fd);
        exit(1);
    }

    // ---- # 3 -----------------------------------------------

    memset((void*)wbuf,0,BUFSIZE);                    // _I2C_Read
    memset((void*)rbuf,0,BUFSIZE);

    SetWbuf(0, 0x40, 0, 0, 0, 0);

    write(fd,(void*)wbuf,65);      usleep(2000);
    read (fd,(void*)rbuf,65);

    if (rbuf[1] != 0)
    {
        printf("#1: Need Cancel and Init, rbuf[1] = 0x%x\n", rbuf[1]);
        return NEEDS_CANCEL_INIT;
    }

    if (rbuf[2] == 0 & rbuf[3] == 0)
    {
        printf("#2: Need Cancel and Init\n");
        return NEEDS_CANCEL_INIT;
    }

    if (rbuf[4] == 0x75)   // Seconds field:  "00:00:75 Sat 07/00/2000"
    {
        printf("#3: Need Cancel and Init\n");
        return NEEDS_CANCEL_INIT;
    }


    printf("     %02x:%02x:%02x,  %s %02x/%02x/20%02x\n",rbuf[6],rbuf[5],rbuf[4],gWeeks[rbuf[7]],rbuf[9],rbuf[8],rbuf[10]);

    return OK;
}



