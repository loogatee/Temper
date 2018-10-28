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
#define LOGFILE_BASE    "/usr/share/monkey/temper"
#define LUTILS          "lutils.lua"

#define HIDNAMELEN      40
#define LOGNAMELEN      80


typedef unsigned char   u8;

typedef struct
{
    char       thedate[40];
    float      StoredReading;
    char       HIDname[HIDNAMELEN];
    char       LOGfilename[LOGNAMELEN];
    int        Kyear,Kmonth,Kday;
    lua_State *LS1;
} Globes;

static Globes    Globals;


static void Find_the_Hidraw_Device( void );


/* ------------------------------------ */



static void Init_Globals( char *Pstr )
{
    char         buf[80],*S;
    Globes *G = &Globals;

    G->StoredReading = -1000.0;                // Big number is an initial condition
    G->Kday          = 99;

    strcpy(G->HIDname,(const char *)HIDNAME);  // Default.  Can be overwritten by discovery

    S  = strrchr((const char *)Pstr,'/');      // Pstr is the entire path of the temper executable
    *S = 0;                                    // effectively lops off '/temper', leaving only the path
    sprintf(buf,"%s/%s",Pstr,LUTILS);          // new name is <path>/lutils.lua
    write(STDERR_FILENO,buf,strlen(buf));      // send to stderr if you want to look at the name
    write(STDERR_FILENO,"\n\r",2);             // make the log look nice
    

    G->LS1 = luaL_newstate();                  // Set up the Lua environment
    luaL_openlibs(G->LS1);                     //   doesn't seem to work without this

    if( luaL_dofile(G->LS1, buf) != 0 )        // Can now extend the app with these lua functions
    {
        sprintf(buf,"luaL_dofile: ERROR opening %s\n", LUTILS);
        write(STDERR_FILENO,buf,strlen(buf));
        return;
    }

    Find_the_Hidraw_Device();
}


static int check_and_make( char *istr )
{
    char          bufx[120];
    struct stat   sb;

    if (stat(istr, &sb) || !S_ISDIR(sb.st_mode))           // IF directory does not exist
    {
        if( mkdir(istr,0755) < 0)                          // THEN make it
        {
            sprintf(bufx,"ERROR mkdir (%s)", istr);        //    show error and return if can't make the directory
            perror(bufx);                                  //    Error message shown on stdout (see /etc/init.d/temper)
            return 1;                                      //    1 means return error
        }
    }
    return 0;                                              // return good
}


static void Make_Dirs_and_Assign_LOGfilename( void )
{
    char         YearDirName[120];
    Globes *G = &Globals;

    G->LOGfilename[0] = 0;

    if( check_and_make(LOGFILE_BASE) ) {return;}                   // non-zero return: Error with mkdir

    sprintf(YearDirName,"%s/y%d",LOGFILE_BASE,G->Kyear);           // name for the 'year' directory.  Example:  'y2018'

    if( check_and_make(YearDirName) ) {return;}                    // non-zero return: Error with mkdir
    
    sprintf(G->LOGfilename,"%s/y%d/d%02d_%02d.txt",LOGFILE_BASE,G->Kyear,G->Kmonth,G->Kday);    // full path-name is this
}


static time_t Get_Midnite_Seconds( void )
{
    time_t      rawTime;
    struct tm  *Atime;

    time( &rawTime );
    Atime = localtime( &rawTime );

    Atime->tm_sec  = 0;
    Atime->tm_min  = 0;
    Atime->tm_hour = 0;

    return mktime(Atime);
}


static int Fill_thedate( time_t *nowTime ) 
{
    time_t      rawTime;
    struct tm  *Atime;
    Globes     *G = &Globals;

    time( &rawTime );
    *nowTime = rawTime;
    Atime    = localtime( &rawTime );

    sprintf(G->thedate,"%02d/%02d/%d,%02d:%02d:%02d",
                        Atime->tm_mon+1,Atime->tm_mday,Atime->tm_year+1900,
                        Atime->tm_hour, Atime->tm_min, Atime->tm_sec);

    if( G->Kday == 99 || G->Kday != Atime->tm_mday )
    {
        G->Kyear  = Atime->tm_year+1900;
        G->Kmonth = Atime->tm_mon+1;
        G->Kday   = Atime->tm_mday;
        return 1;
    }

    return 0;
}



static void Find_the_Hidraw_Device( void )
{
    const char *rp;
    Globes *G = &Globals;

    lua_getglobal(G->LS1,"find_hidraw");           // function to call (see lutils.lua)
    if( lua_isnil(G->LS1, -1) == 1 ) {return;}     // means 'find_hidraw' was not found, should not happen

    lua_pcall(G->LS1,0,1,0);                       // calls 'find_hidraw'.  The 1 says: one return value
    rp = lua_tostring(G->LS1,-1);                  // the return value is a string: rp points to it

    if( strlen(rp) > 0 )                           // returned string will be NULL if device not found
    {
        strncpy(G->HIDname,rp,HIDNAMELEN);         // copy at most HIDNAMELEN
        G->HIDname[HIDNAMELEN-1] = 0;              // guarantes string termination
    }

    lua_pop(G->LS1, 1);                            // cleans up the stack
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
    int             retS,retR,llen,counter;
    fd_set          rfds;
    struct timeval  tv;
    static const u8 ReadTemp_ByteSequence[8] = { 1,0x80,0x33,1,0,0,0,0 };    // magic byte sequence to read the temperature


    memset(tbuf,0,8);          // only expect the 1st 8 bytes to change
    FD_ZERO(&rfds);
    FD_SET(fd,&rfds);

    tv.tv_sec  = 0;
    tv.tv_usec = 100000;       // input into the select call.  100,000usec = 100msec = .1sec
    llen       = 0;
    counter    = 0;

                                           // All init up to this point. The action begins here
    write(fd,ReadTemp_ByteSequence,8);     // This Write of 8 bytes triggers the temperature reading

    while( counter < 2 && llen < 8 )
    {
         retS = select(fd+1,&rfds,NULL,NULL,&tv);
         if( retS == -1 )
         {
             perror("select on hidraw device");
             return -1;
         }
         else if( retS )
         {
             if( (retR = read(fd,&tbuf[llen],8)) < 0 )
             {
                 perror("read from hidraw device");
                 return -1;
             }
             llen += retR;
         }
         else ++counter;
    }

    return llen;
}



int main( int argc, char *argv[] )
{
    int               fd,lenr,temperature;
    unsigned int      secsdiff,usecsdiff;
    time_t            midniteSecs,nowSecs;
    char              tbuf[180];
    struct timeval    tv1,tv2;
    struct timezone   tz;
    Globes           *G = &Globals;


    if(fork()) {return 0;}                      // Parent returns. Child executes as background process

    Init_Globals(argv[0]);

    sprintf(tbuf,"%d",(unsigned int)getpid());                 // hey, you pid!
    fd = open(TEMPERPIDFILE,O_WRONLY|O_CREAT|O_TRUNC,0644);    // create if not there. Trunc if there
    write(fd,tbuf,strlen(tbuf));                               // note confidence: no error checking!
    close(fd);                                                 // done. Close.  The pid was written!


    while(1)
    {
        if( (fd=open(G->HIDname,O_RDWR)) < 0 )
        {
            sprintf(tbuf, "ERROR open HIDname (%s):", G->HIDname);
            perror(tbuf);
            Find_the_Hidraw_Device();                          // keep testing every 15 seconds
        }

        if( Fill_thedate( &nowSecs ) )                         // returning non-zero means the date changed
        { 
            Make_Dirs_and_Assign_LOGfilename();
            midniteSecs = Get_Midnite_Seconds();
        }
        gettimeofday(&tv1,&tz);                                // because it has micro-seconds

        if( (fd>0) && ((lenr=TakeTemperatureReading(fd,tbuf)) >= 0) )   // return data of 8 bytes is in tbuf
        {
            close(fd);

            if( lenr > 0 )                                     // StoredReading used if lenr==0
            {
                temperature      = ((u8)tbuf[2] << 8) + (u8)tbuf[3];
                G->StoredReading = (((float)temperature/100.0) * 1.8) + 32.0;
            }

            sprintf(tbuf,"%ld,%s,%.1f\n",(nowSecs-midniteSecs),G->thedate,G->StoredReading);

            //
            // Queue up the data here.  4 every minute.   1hr=240 entries.  Memory is not an issue
            // Temperature data could be valid, but date info could be bad
            //

            if( check_thedate() == 1 )
            {
                if( (fd=open(G->LOGfilename,O_WRONLY|O_APPEND|O_CREAT,0666)) > 0 )
                {
                    write(fd,tbuf,strlen(tbuf));
                    close(fd);
                }
                else
                {
                    sprintf(tbuf, "ERROR Opening LOGfilename (%s)", G->LOGfilename);
                    perror(tbuf);
                }
            }
        }
        else
        {
            if( fd>0 ) close(fd);     // close down if possible
        }

        gettimeofday(&tv2,&tz);

        secsdiff  = tv2.tv_sec - tv1.tv_sec;
        usecsdiff = (secsdiff * 1000000) + (tv2.tv_usec - tv1.tv_usec + 70000);   // 60100 for wally

        //printf("secs: %ld   usecs: %ld,   usecsdiff: %d\n", tv1.tv_sec, tv1.tv_usec, usecsdiff);

        usleep( 15000000 - usecsdiff );
    }
}




