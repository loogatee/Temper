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
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zmq.h>
#include <assert.h>
#include <pthread.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>



#define HIDNAME         "/dev/hidrawX"
#define TEMPERPIDFILE   "/var/tmp/temper.pid"
#define LOGFILE_BASE    "/usr/share/monkey/temper"
#define LUTILS          "lutils.lua"

#define HIDNAMELEN      40
#define LOGNAMELEN      80
#define INITIAL_KDAY    99

#define ZMQPORT_CMDCHANNEL     "ipc:///var/tmp/TemperCommandChannel"
#define ZMQPORT_SIGNALER       "ipc:///var/tmp/TemperSignalChannel"


typedef unsigned char   u8;

typedef struct
{
    char       thedate[40];
    float      StoredReading;
    char       HIDname[HIDNAMELEN];
    char       LOGfilename[LOGNAMELEN];
    int        Kyear,Kmonth,Kday;
    void       *CmdChan,*SignalChan;
    char       MostRecentData[256];
    lua_State *LS1;
} Globes;

static Globes    Globals;


static void Find_the_Hidraw_Device( void );


/* ------------------------------------ */





//
//    Runs in a seperate thread.   See the pthread_create() call in main.
//    Signals main at a constant frequency.
//
static void *TheSignaler( void *dummy )
{
    int    rc;
    void   *lcontext,*lSigChan;
    char   dbuf[60];

    lcontext = zmq_ctx_new();
    lSigChan = zmq_socket ( lcontext,  ZMQ_PUSH         );
    rc       = zmq_connect( lSigChan,  ZMQPORT_SIGNALER );   assert(rc == 0);

    dbuf[0] = 'A';     // for now, anything
    dbuf[1] = 0;       //   make it a proper 'C' string

    while( 1 )
    {
        usleep( 15000000 );
        rc = zmq_send( lSigChan, dbuf, 2, 0 );
    }

    return 0;      // its never gonna get here.  This statement makes the compiler happy
}



static void Init_Globals( char *Pstr )
{
    char         buf[80],*S,buf1[100];
    Globes *G = &Globals;

    G->StoredReading = -1000.0;                    // Big number is an initial condition
    G->Kday          = INITIAL_KDAY;               // A number that is not a day of the month 

    strcpy(G->HIDname,(const char *)HIDNAME);      // Default.  Can be overwritten by discovery

    S  = strrchr((const char *)Pstr,'/');          // Pstr is the entire path of the temper executable
    *S = 0;                                        // effectively lops off '/temper', leaving only the path
    sprintf(buf,"%s/%s",Pstr,LUTILS);              // new name is <path>/lutils.lua
    sprintf(buf1,"lutils full path: %s\n\r",buf);  // provide context to this write to stderr
    write(STDERR_FILENO,buf1,strlen(buf1));        // send to stderr if you want to look at the name
    

    G->LS1 = luaL_newstate();                      // Set up the Lua environment
    luaL_openlibs(G->LS1);                         //   doesn't seem to work without this

    if( luaL_dofile(G->LS1, buf) != 0 )            // Can now extend the app with these lua functions
    {
        sprintf(buf,"luaL_dofile: ERROR opening %s\n", LUTILS);
        write(STDERR_FILENO,buf,strlen(buf));
        return;
    }

    strcpy(G->MostRecentData,"<empty>");           // for Data Requests from the Command Channel

    Find_the_Hidraw_Device();                      // the 'Temper' USB device
}


static void Init_PacketHandler_ZMQs( void )
{
    char    dbuf[60];
    int     rc;
    void   *contextBCh,*contextSig;
    Globes *G = &Globals;

    contextSig    = zmq_ctx_new();
    G->SignalChan = zmq_socket ( contextSig,     ZMQ_PULL            );
    rc            = zmq_bind   ( G->SignalChan,  ZMQPORT_SIGNALER    );   assert(rc == 0);

    contextBCh  = zmq_ctx_new();
    G->CmdChan  = zmq_socket ( contextBCh,  ZMQ_REP            );
    rc          = zmq_bind   ( G->CmdChan,  ZMQPORT_CMDCHANNEL );    assert(rc == 0);

    strcpy((void *)dbuf, ZMQPORT_SIGNALER);      chmod((void *)&dbuf[6], 0777);
    strcpy((void *)dbuf, ZMQPORT_CMDCHANNEL);    chmod((void *)&dbuf[6], 0777);
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

//
//   With BASE = '/usr/share/monkey/temper':
//   And Date  = '10/28/2018'
//
//      YearDirName = /usr/share/monkey/temper/y2018
//      LOGfilename = /usr/share/monkey/temper/y2018/d10_28.txt
//
//      These 2 directories would get created if needed:
//          1. /usr/share/monkey/temper
//          2. /usr/share/monkey/temper/y2018
//
//   Once BASE is created, the Year Directory should only need
//   to get created after that, which will happen yearly.
//
//        ****  The LOGfilename will change everyday  ****
//
static void Make_Dirs_and_Assign_LOGfilename( void )
{
    char         YearDirName[120];
    Globes *G = &Globals;

    G->LOGfilename[0] = 0;

    if( check_and_make(LOGFILE_BASE) ) {return;}                   // non-zero return: Error with mkdir

    if( G->Kyear >= 2018 )
    {
        sprintf(YearDirName,"%s/y%d",LOGFILE_BASE,G->Kyear);           // name for the 'year' directory.  Example:  'y2018'

        if( check_and_make(YearDirName) ) {return;}                    // non-zero return: Error with mkdir
    
        sprintf(G->LOGfilename,"%s/d%02d_%02d.txt",YearDirName,G->Kmonth,G->Kday);    // full path-name is this
    }
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

    if( G->Kday == INITIAL_KDAY || G->Kday != Atime->tm_mday )
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

    if( S == NULL || S[1] == '1' )
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

static void do_cmdchannel_response( void )
{
    char   Sbuf[80];
    Globes *G = &Globals;


    zmq_recv( G->CmdChan, Sbuf, 40, 0 );

    // Can ignore the incoming, because there's only 1 thing to do for now

    zmq_send( G->CmdChan, G->MostRecentData, strlen(G->MostRecentData), 0 );
}



int main( int argc, char *argv[] )
{
    int               fd,lenr,temperature,fd1,lenslr;
    //unsigned int      secsdiff,usecsdiff;
    time_t            midniteSecs,nowSecs;
    char              tbuf[240];
    char              tbuf2[181];
    pthread_t         Sign_threadID;
    zmq_pollitem_t    PollItems[2];
    Globes           *G = &Globals;

    signal(SIGCHLD, SIG_IGN);
    if(fork()) {return 0;}                      // Parent returns. Child executes as background process

    Init_Globals(argv[0]);
    Init_PacketHandler_ZMQs();

    sprintf(tbuf,"%d",(unsigned int)getpid());                 // hey, you pid!
    fd = open(TEMPERPIDFILE,O_WRONLY|O_CREAT|O_TRUNC,0644);    // create if not there. Trunc if there
    write(fd,tbuf,strlen(tbuf));                               // note confidence: no error checking!
    close(fd);                                                 // done. Close.  The pid was written!

    pthread_create(&Sign_threadID, 0, TheSignaler, 0);

    PollItems[0].socket  = G->SignalChan;
    PollItems[0].fd      = 0;
    PollItems[0].events  = ZMQ_POLLIN;
    PollItems[0].revents = 0;

    PollItems[1].socket  = G->CmdChan;
    PollItems[1].fd      = 0;
    PollItems[1].events  = ZMQ_POLLIN;
    PollItems[1].revents = 0;


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

        system("/usr/bin/python /opt/epsolar-tracer/info.py > /var/tmp/tmpabcX");   // stash the output somewhere
        fd1 = open("/var/tmp/tmpabcX",O_RDONLY);                                    // open same file we just wrote
        lenslr=read(fd1,tbuf2,180);                                                 // read it
        if( lenslr >= 0 ) { tbuf2[lenslr] = 0; }                                    // NEED string termination
        close(fd1);                                                                 // tidy up
        unlink("/var/tmp/tmpabcX");                                                 // remove file

        if( (fd>0) && ((lenr=TakeTemperatureReading(fd,tbuf)) >= 0) )   // return data of 8 bytes is in tbuf
        {
            close(fd);

            if( lenr > 0 )                                     // StoredReading used if lenr==0
            {
                temperature      = ((u8)tbuf[2] << 8) + (u8)tbuf[3];
                G->StoredReading = (((float)temperature/100.0) * 1.8) + 32.0;
            }

            // NOTE:  if lenr <= 0, old value from G->StoredReading will be used

            if( lenslr > 0 )
                sprintf(tbuf,"%ld,%s,%.1f,%s",(nowSecs-midniteSecs),G->thedate,G->StoredReading,tbuf2);
            else
                sprintf(tbuf,"%ld,%s,%.1f,<nosolar>\n",(nowSecs-midniteSecs),G->thedate,G->StoredReading);

        }
        else
        {
            if( fd>0 ) close(fd);     // close down if possible

            if( lenslr > 0 )
                sprintf(tbuf,"%ld,%s,XX,%s",(nowSecs-midniteSecs),G->thedate,tbuf2);
            else
                sprintf(tbuf,"%ld,%s,XX,<nosolar>\n",(nowSecs-midniteSecs),G->thedate);
        }


        strcpy(G->MostRecentData,tbuf);

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



        while( 1 )
        {
            zmq_poll( PollItems, 2, -1 );

            if(   PollItems[1].revents & ZMQ_POLLIN  ) { do_cmdchannel_response(); }
            if( !(PollItems[0].revents & ZMQ_POLLIN) ) { continue; }

            zmq_recv( G->SignalChan, tbuf, 2, 0 );       // receive a couple of dummy bytes
            break;
        }

    }
}




/*
   Notes on Solar Charger RTC.

   This date:    11/04/2018, 15:13:02

   Is this on the charger:

     10022     6923       3331
    0x2726    0x1b0b      0x0d03

    39 38     27 11       13 3


                3 = April, where Jan = 0
               13 = 2013,  where 00 = 2000
               11 = hrs in day, where 00 = midnite
               27 = April 28th, where 00 = 1st day of month
               38 = seconds
               39 = minutes

   The 2 dates:
       11/04/2018, 15:13:02
       04/28/2013, 11:39:38

       Time difference = 174,195,204 seconds
*/

