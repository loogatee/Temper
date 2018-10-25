/**
 * Copyright 2018 John Reed
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not  use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations  under the License.
 */

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



#define HIDNAME         "/dev/hidrawX"
#define TEMPERPIDFILE   "/var/tmp/temper.pid"
#define LOGFILE         "/home/johnr/tempdata.log"
#define LUTILS          "/opt/Temper/lutils.lua"


typedef unsigned char   u8;

typedef struct
{
    char       thedate[40];
    float      StoredReading;
    u8         usbdat[8];
    char       HIDname[40];
    int        firstflag;
    lua_State *LS1;
} Globes;

static Globes    Globals;








static void Fill_thedate( void ) 
{
    time_t      rawTime;
    struct tm  *Atime;
    Globes *G = &Globals;

    time( &rawTime );
    Atime = localtime( &rawTime );
    sprintf(G->thedate,"%02d/%02d/%d,%02d:%02d:%02d",
                        Atime->tm_mon+1,Atime->tm_mday,Atime->tm_year+1900,
                        Atime->tm_hour, Atime->tm_min, Atime->tm_sec);
}



static void Find_the_Hidraw_Device( void )
{
    char  tbuf[88];
    const char *rp;
    Globes *G = &Globals;

    lua_getglobal(G->LS1,"find_hidraw");
    lua_pcall    (G->LS1,0,1,0);
    lua_pushnil  (G->LS1);

    lua_next     (G->LS1,-2);
    rp = lua_tostring(G->LS1,-1);
    lua_pop(G->LS1, 1);

    if( strlen(rp) > 0 )
    {
        strcpy(G->HIDname,rp);
        Fill_thedate();
        sprintf(tbuf,"%s  Found this HIDraw device: %s\n",G->thedate,G->HIDname);
        write(2,tbuf,strlen(tbuf));
    }
}


static void Init_Globals( void )
{
    char   buf[80];
    Globes *G = &Globals;

    u8 Udat[8] = { 1,0x80,0x33,1,0,0,0,0 };    // magic byte sequence to read the temperature

    memcpy(G->usbdat,Udat,8);
    strcpy(G->HIDname,(const char *)HIDNAME);  // Default.  Can be overwritten by discovery

    G->StoredReading = -1000.0;                // Big number is an initial condition
    G->firstflag     = 0;


    G->LS1 = luaL_newstate();
    luaL_openlibs(G->LS1);                     //   you really do need this

    if( luaL_dofile(G->LS1, LUTILS) != 0 )     //   Can now extend the app with these lua functions
    {
        sprintf(buf,"luaL_dofile: ERROR opening %s\n", LUTILS);
        write(2,buf,strlen(buf));
        return;
    }

    Find_the_Hidraw_Device();
}


//
//   input data:  "10/18/18,10:52:45"
//
//   The year will be '69' if ntp date is not set
//
//   1=good, 0=toss
//
static int check_thedate( void )
{
    char *S = strrchr(Globals.thedate,'/');         // finds the last '/' in the string

    if( S == NULL || S[1] == '6' )
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

    memset(tbuf,0,8);          // only expect the 1st 8 bytes to change
    FD_ZERO(&rfds);
    FD_SET(fd,&rfds);

    tv.tv_sec  = 0;
    tv.tv_usec = 100000;       // input into the select call.  100,000usec = 100msec = .1sec
    llen       = 0;
    counter    = 0;

                               // All init up to this point. The action begins here
    write(fd,G->usbdat,8);     // This Write of 8 bytes triggers the temperature reading

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
    int               fd,lenr,temperature;
    unsigned int      secsdiff,usecsdiff,tot,BaseTimeStamp;
    char              tbuf[80];
    struct timeval    tv1,tv2;
    struct timezone   tz;
    float             tempF;
    Globes           *G = &Globals;

    if(fork()) {return 0;}                      // Parent returns. Child executes as background process

    Init_Globals();

    sprintf(tbuf,"%d",(unsigned int)getpid());
    fd = open(TEMPERPIDFILE,O_WRONLY|O_CREAT|O_TRUNC,0644);
    write(fd,tbuf,strlen(tbuf));
    close(fd);


    while(1)
    {
        if( (fd=open(G->HIDname,O_RDWR)) < 0 )
        {
            sprintf(tbuf, "ERROR opening %s  :", G->HIDname);
            perror(tbuf);
            sleep(20);
            Find_the_Hidraw_Device();
            continue;
        }

        Fill_thedate();                                        // date & time, accurate to seconds
        gettimeofday(&tv1,&tz);                                // because it has micro-seconds

        if( G->firstflag == 0 )
        {
            G->firstflag  = 1;
            BaseTimeStamp = tv1.tv_sec;                        // BaseTimeStamp is the keeper here
        }

        if( (lenr=TakeTemperatureReading(fd,tbuf)) >= 0 )      // return data is in tbuf
        {
            close(fd);

            if( lenr > 0 )                                     // StoredReading used if lenr==0
            {
                temperature      = ((u8)tbuf[2] << 8) + (u8)tbuf[3];
                tempF            = (((float)temperature/100.0) * 1.8) + 32.0;
                G->StoredReading = tempF;
            }

            sprintf(tbuf,"%ld,%s,%.1f\n",tv1.tv_sec-BaseTimeStamp,G->thedate,G->StoredReading);

            //
            // Queue up the data here.  4 every minute.   1hr=240 entries.  Memory is not an issue
            // Temperature data could be valid, but date info could be bad
            //

            if( check_thedate() == 1 )
            {
                //
                //   Different logfiles, based on date
                //
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




