#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


#define HIDNAME         "/dev/hidraw1"
#define TEMPERPIDFILE   "/var/tmp/temper.pid"
#define LOGFILE         "/home/johnr/tempdata.log"


typedef unsigned char   u8;

typedef struct
{
    char   thedate[40];
    float  PrevReading;
    u8     usbdat[8];
    char   HIDname[40];
    int    firstflag;
} Globes;

static Globes    Globals;


static void Init_Globals( void )
{
    Globes *G = &Globals;

    u8 Udat[8] = { 1,0x80,0x33,1,0,0,0,0 };    // magic byte sequence to read the temperature

    memcpy(G->usbdat,Udat,8);
    strcpy(G->HIDname,(const char *)HIDNAME);

    G->PrevReading = -1000.0;              // Big number is an initial condition
    G->firstflag   = 0;
}


//
//   input data:  "10/18/18,10:52:45"
//
//   The year will be '69' if ntp date is not set
//
//   1=good, 0=toss
//
static int check_thedate()
{
    char *S;

    S = strrchr(Globals.thedate,'/');         // finds the last '/' in the string

    if( S == NULL || *(S+1) == '6' )
        return 0;
    else
        return 1;
}

//   returns:
//        -1  on error from select() or read()
//         0  if the device never put any data on the wire
//        >0  probly got some valid data
//
static int TakeTemperatureReading(int fd, char *tbuf)
{
    int             retv,llen,counter;
    fd_set          rfds;
    struct timeval  tv;
    Globes         *G = &Globals;

    FD_ZERO(&rfds);
    FD_SET(fd,&rfds);

    tv.tv_sec  = 0;
    tv.tv_usec = 100000;    // input into the select call.  100,000usec = 100msec = .1sec
    llen       = 0;
    counter    = 0;

    memset(tbuf,0,8);



    write(fd,G->usbdat,8);     // this Write of 8 bytes triggers the temperature reading

    while(1)
    {
         retv = select(fd+1,&rfds,NULL,NULL,&tv);

         if( retv == -1 )
         {
             perror("select on hidraw device");
             return -1;
         }
         else if( retv )
         {
             if( (retv = read(fd,&tbuf[llen],8)) < 0 )
             {
                 perror("read from hidraw device");
                 return -1;
             }
             llen += retv;
             if( llen > 16 ) {break;}     // failsafe
         }
         else if( ++counter > 2)
             break;
    }

    return llen;
}



int main( void )
{
    int             fd,lenr,temperature;
    unsigned int    secsdiff,usecsdiff,tot,BaseTimeStamp;
    char            tbuf[80];
    struct timeval  tv1,tv2;
    struct timezone tz;
    float           tempF;
    time_t          rawTime;
    struct tm       *Atime;
    Globes          *G = &Globals;

    if(fork()) {return 0;}                      // child executes as background process

    Init_Globals();

    sprintf(tbuf,"%d",(unsigned int)getpid());
    fd = open(TEMPERPIDFILE,O_WRONLY|O_CREAT,0644);
    write(fd,tbuf,strlen(tbuf));
    close(fd);


    while(1)
    {
        fd = open(G->HIDname,O_RDWR);
        if( fd < 0 )
        {
            sprintf(tbuf, "ERROR opening %s\n\r", G->HIDname);
            perror(tbuf);
            sleep(20);
            break;
        }


        time( &rawTime );
        Atime = localtime( &rawTime );
        sprintf(G->thedate,"%02d/%02d/%d,%02d:%02d:%02d",
                            Atime->tm_mon+1,Atime->tm_mday,Atime->tm_year+1900,
                            Atime->tm_hour, Atime->tm_min, Atime->tm_sec);

        gettimeofday(&tv1,&tz);                                // has micro-seconds

        if( G->firstflag == 0 )
        {
            G->firstflag  = 1;
            BaseTimeStamp = tv1.tv_sec;                        // BaseTimeStamp is the keeper here
        }

        if( (lenr=TakeTemperatureReading(fd,tbuf)) >= 0 )      // return data is in tbuf
        {
            close(fd);

            if( lenr > 0 )
            {
                temperature    = ((u8)tbuf[2] << 8) + (u8)tbuf[3];
                tempF          = (((float)temperature/100.0) * 1.8) + 32.0;
                G->PrevReading = tempF;
            }

            sprintf(tbuf,"%ld,%s,%.1f\n",tv1.tv_sec-BaseTimeStamp,G->thedate,G->PrevReading);

            if( check_thedate() == 1 )
            {
                if( (fd=open(LOGFILE,O_WRONLY|O_APPEND|O_CREAT,0666)) > 0 )
                {
                    write(fd,tbuf,strlen(tbuf));
                    close(fd);
                }
            }
        }
        else
        {
            close(fd);         // error on select() or read()
            sleep(20);
        }

        gettimeofday(&tv2,&tz);

        secsdiff  = tv2.tv_sec - tv1.tv_sec;
        usecsdiff = (secsdiff * 1000000) + (tv2.tv_usec - tv1.tv_usec + 70000);   // 60100 for wally
        tot       = 15000000 - usecsdiff;                                         // going for 15 seconds

        //printf("secs: %ld   usecs: %ld,   usecsdiff: %d    tot: %d\n", tv1.tv_sec, tv1.tv_usec, usecsdiff, tot);

        usleep(tot);
    }
}


















/*
    log directory needs to be accessible by monkey

       separate directory for year
       separate file for each day
       accept request from socket, and send data back

       New Program:
       ??     - reads from socket 1:    reads latest date/temp.  Stores in memory
       ??     - reads from socket 2:    writes date/temp from memory
              - Web-page to keep getting the temp
                    has button to get on request
                    ajax transaction.  Send request, get response

*/











