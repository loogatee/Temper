#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
    log directory needs to be accessible by monkey

       seperate directory for year
       seperate file for each day
       write data to socket

       New Program:
          - reads from socket 1:    reads latest date/temp.  Stores in memory
          - reads from socket 2:    writes date/temp from memory

*/


char           outdata[80];
char           thedate[40];
float          PreviousReading;

unsigned char  usbdat[8] = { 1,0x80,0x33,1,0,0,0,0 };    // magic byte sequence to read the temperature

char          *HIDname       = "/dev/hidraw2";
char          *TemperPidfile = "/var/tmp/temper.pid";
char          *logfile       = "/home/johnr/tempdata.log";

//
//   input data:  "10/18/18,10:52:45"
//
//   The year will be '69' if ntp date is not set
//
//   1=good, 0=toss
//
int check_thedate()
{
    char *S;

    S = strrchr(thedate,'/');         // finds the last '/' in the string

    if( S == NULL || *(S+1) == '6' )
        return 0;
    else
        return 1;
}



int main()
{
    int             fd,retv,counter,lenr,temperature,ok_to_log;
    unsigned int    secsdiff,usecsdiff,tot,BaseTimeStamp,firstflag;
    char            tbuf[80];
    struct timeval  tv,tv1,tv2;
    struct timezone tz;
    unsigned char   high_byte,low_byte;
    float           tempC,tempF;
    fd_set          rfds;
    time_t          rawTime;
    struct tm       *Atime;


    if( fork() )                       // child executes as background process
        return 0;

    firstflag       = 0;
    PreviousReading = -1000.0;         // Big number is an initial condition

    sprintf(tbuf,"%d",(unsigned int)getpid());
    fd = open(TemperPidfile,O_WRONLY|O_CREAT,0644);
    write(fd,tbuf,strlen(tbuf));
    close(fd);


    while(1)
    {
        fd = open(HIDname,O_RDWR);
        if( fd < 0 )
        {
            sprintf(tbuf, "ERROR opening %s\n\r", HIDname);
            perror(tbuf);
            sleep(20);
            break;
        }

        FD_ZERO(&rfds);
        FD_SET(fd,&rfds);

        tv.tv_sec  = 0;
        tv.tv_usec = 100000;    // input into the select call.  100,000usec = 100msec = .1sec
        counter    = 0;
        lenr       = 0;

        memset(tbuf,0,8);
        memset(thedate,0,40);

        time( &rawTime );
        Atime = localtime( &rawTime );
        sprintf(thedate,"%02d/%02d/%d,%02d:%02d:%02d",
                        Atime->tm_mon+1,Atime->tm_mday,Atime->tm_year+1900,
                        Atime->tm_hour, Atime->tm_min, Atime->tm_sec);

        gettimeofday(&tv1,&tz);

        if( firstflag == 0 )
        {
            firstflag     = 1;
            BaseTimeStamp = tv1.tv_sec;
        }

        write(fd,usbdat,8);

        while(1)
        {
             retv = select(fd+1,&rfds,NULL,NULL,&tv);

             if( retv == -1 )
             {
                 perror("select()");
                 close(fd);
                 sleep(20);
                 goto try_again;
             }
             else if( retv )
             {
                 if( (retv = read(fd,&tbuf[lenr],8)) < 0 )
                 {
                     perror("read()");
                     close(fd);
                     sleep(20);
                     goto try_again;
                 }
                 lenr += retv;
             }
             else if( ++counter > 2)
                 break;
        }

        close(fd);

        if( check_thedate() == 1 )              // toss if the date is bad
        {
            ok_to_log = 1;

            if( lenr > 0 )
            {
                high_byte = tbuf[2];
                low_byte  = tbuf[3];


                temperature     = (high_byte << 8) + low_byte;
                tempC           = (float)temperature / 100.0;
                tempF           = (tempC * 1.8) + 32.0;
                PreviousReading = tempF;

                sprintf(outdata,"%ld,%s,%.1f\n",tv1.tv_sec-BaseTimeStamp,thedate,tempF);
            }
            else
            {
                if( PreviousReading < -100.0 )
                    ok_to_log = 0;
                else
                    sprintf(outdata,"%ld,%s,%.1f,*\n",tv1.tv_sec-BaseTimeStamp,thedate,PreviousReading);
            }

            if( ok_to_log == 1 )
            {
                fd = open(logfile,O_WRONLY|O_APPEND|O_CREAT,0666);
                write(fd,outdata,strlen(outdata));
                close(fd);
            }
        }

try_again:

        gettimeofday(&tv2,&tz);

        secsdiff  = tv2.tv_sec - tv1.tv_sec;
        usecsdiff = (secsdiff * 1000000) + (tv2.tv_usec - tv1.tv_usec + 60100);   // 60100 for wally
        tot       = 10000000 - usecsdiff;

        //printf("secs: %ld   usecs: %ld,   usecsdiff: %d    tot: %d\n", tv1.tv_sec, tv1.tv_usec, usecsdiff, tot);

        usleep(tot);
    }
}
















