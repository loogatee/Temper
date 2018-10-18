#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


char           outdata[80];
char           thedate[40];
float          PreviousReading;

unsigned char  usbdat[8] = { 1,0x80,0x33,1,0,0,0,0 };
char          *fname     = "/dev/hidraw1";


//
//   /tmp/tmpdate example:  10/18/18,10:52:45
//
//   The year will be '69' if ntp date is not set
//
//   1=good, 0=toss
//
int read_thedate()
{
    int   lfd,lenr;
    char *S;

    lfd  = open("/tmp/tmpdate",O_RDONLY);
    lenr = read(lfd,thedate,80);
    close(lfd);

    thedate[lenr-1] = 0;

    S = strrchr(thedate,'/');

    if( S == NULL || *(S+1) == '6' )
        return 0;
    else
        return 1;
}



int main()
{
    int             fd,retv,counter,lenr,temperature,ok_to_log;
    char            tbuf[80];
    struct timeval  tv;
    unsigned char   high_byte,low_byte;
    float           tempC,tempF;
    pid_t           temperPid;
    fd_set          rfds;

    if( (temperPid = fork()) != 0 )
        return;

    PreviousReading = -1000.0;

    temperPid = getpid();
    sprintf(tbuf,"%d",(unsigned int)temperPid);
    fd = open("/tmp/temper.pid",O_WRONLY|O_CREAT);
    write(fd,tbuf,strlen(tbuf));
    close(fd);

    while(1)
    {
        fd = open(fname,O_RDWR);

        if( fd < 0 )
        {
            sprintf(tbuf, "ERROR opening %s\n\r", fname);
            perror(tbuf);
            return 1;
        }

        FD_ZERO(&rfds);
        FD_SET(fd,&rfds);

        tv.tv_sec  = 0;
        tv.tv_usec = 100000;
        counter    = 0;
        lenr       = 0;

        memset(tbuf,0,8);
        memset(thedate,0,40);

        system("/bin/date +%D,%T > /tmp/tmpdate");
        write(fd,usbdat,8);

        while(1)
        {
             retv = select(fd+1,&rfds,NULL,NULL,&tv);

             if( retv == -1 )
             {
                 perror("select()");
                 return 1;
             }
             else if( retv )
             {
                 if( (retv = read(fd,&tbuf[lenr],8)) < 0 )
                 {
                     perror("read()");
                     return 1;
                 }
                 lenr += retv;
             }
             else
             {
                 if( ++counter > 3)
                     break;
             }
        }

        close(fd);

        if( read_thedate() == 1 )              // toss if the date is bad
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

                sprintf(outdata,"%s,%.1f\n",thedate,tempF);
            }
            else
            {
                if( PreviousReading < -100.0 )
                    ok_to_log = 0;
                else
                    sprintf(outdata,"%s,%.1f\n",thedate,PreviousReading);
            }

            if( ok_to_log == 1 )
            {
                fd = open("/home/johnr/tempdata.log",O_WRONLY|O_APPEND);
                write(fd,outdata,strlen(outdata));
                close(fd);
            }
        }

        sleep(10);
    }
}
















