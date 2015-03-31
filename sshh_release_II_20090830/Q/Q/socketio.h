/*  Copyright 2009 Stu Mark 
    This file is part of sshh.

    sshh is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    sshh is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with sshh. If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef INCL_SOCKETIO_H
#define INCL_SOCKETIO_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <time.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <errno.h>

/* Instructions.

   For use with acceptor or connector, a couple of functions that handle the sending
   and receiving of data over a TCP socket.

   Constructor:
   socketio (int Infd, int Outfd, int timeout, int appropriate);
     This is the socket you got back from Acceptor.accept();
     or from Connector.victim();
     This object takes ownership of the socket you give it. Normally
     you have one bi-directional socket and you pass it as both parameters
     to this constructor.
     Note: This object owns the socket(s) you pass in the constructor which
     means when this object goes away, this half of the connection(s) close.
     The timeout value is how long to wait on a read or write before it
     returns with an error. The default is 20 seconds, I think.
     Addendum: the approriate (ap-pro-pree-ate) parameter defautls to true.
     This tells socketio whether or not to take ownership of the socket.
     You may someday find yourself not owning the socket, but still wanting
     to use socketio, set this to false, and socketio will not close the
     file descriptiors upon destruction.

     Example:
       socketio sco(con.victim(), con.victim(), ar[TXT_SOCKETTIMEOUT]);
     or
       int s = ac.accept();
       socketio sco(s, s, ar[TXT_SOCKETTIMEOUT]);

   If both ends of the socket are accessed through this class, it will
   take care of endian-ness of get/putint, so you don't have to. I.E. it works
   between AIX and Linux.

   These return TRUE/FALSE for success/fail
   If you get a FALSE back from any of these functions, you can call
     int errorstate(void);
   to return the errno of the cause of the failure. The internal error number
   is reset to 0 when errorstate is called.

   int sendstr(const char *s, int len = -1); // must be read with getstr.
   int getstr(char **s);  // THIS IS A FACTORY. YOU MUST FREEMEM THE POINTER YOU GET BACK.
   int getstr(Memory &s); // This is not a factory. Contents of s are replaced.

     Memory d1;
     sco.sendstr("Hi from stu");
     sco.getstr(d1.consume()); // this will set d1 to the data coming in on the stream, and
                               // the Memory object will free it on desctruction.
     sco.getstr(d1); // a cleaner version of above.

   int senddata(void *s, int len); // send a block of data of len length
   int getdata(void *s, int len); // receive a block of data, you supply the memory.

   // this pair has the feature of being endian observant. You can pass UINTs
   // between intel and RS6000 and it will come out ok, but -1 is used as
   // a flag for getint not working.
   UINT32 getint(void) // returns an int from stream, -1 if there's an error
   int putint(UINT32 x) // sends an int.
   int getint(UINT32 &x) // more correct version of getint that supports endianness and
                         // an error return.

   void settimeout(int t); // allows you to set the timeout value after construction.

   int getline(Memory &line, char delimeter = '\n') // buffered line based socket reader. Will ignore all \r
                             // and denote eol by \n
   int sendline(char *line); // send text line and appends a CRLF and sends that too.
   // good for oh, say, talking to sendmail
*/

#define TXT_SOCKETTIMEOUT "sockettimeout"

#define GETLINEBUFFERSIZE 65536 // how much should we try to read at once. Biggest I've seen is 16K, but you never know

class Memory;

class socketio
  {
    public:
      socketio (int Infd, int Outfd, int timeout = 0, int appropriate = TRUE);
      ~socketio()
        {
          if (xbuf != NULL)
            freemem(xbuf);
          if (appropriated == FALSE)
            return; // not our sockets to kill
          socketioclose();
        }
      void socketioclose(void)
        {
          close (fdin);
          if (fdin != fdout)
            close (fdout);
          fdin = 0;
          fdout = 0;
        }
      int errorstate(void) { int r = err; err = 0; return r; }
      void settimeout(int timeout); // set read timeout value in seconds
      int gettimeout(void) { return sockettimeout; }
      void setwritetimeout(int timeout); // set write timeout value in seconds
      /* Returns TRUE/FALSE for success/fail */
      int sendstr(const char *s, int len = -1);
      int getstr(char **s);  /* THIS IS A FACTORY. YOU MUST FREEMEM THE POINTER YOU GET BACK */
      int getstr(Memory &s); // this is a better version of the above.
      int senddata(void *s, int len);
      int getdata(void *s, int len, int &inlen, int takeanything = FALSE, int waitatall = TRUE);
      int getalldata(Memory &out, int &retlen);

      /* These were a long time in coming */
      int getline(Memory &line, char delimeter = '\n');
      int sendline(char *line);
      int bufferedgetline(Memory &line, char delimeter = '\n', int immediatereturn = FALSE); // faster version of above
      int bufferedlineavailable(char delimeter = '\n');
      UINT32 getint(void)
        {
          UINT32 dat;
          int retlen;
          if (getdata(&dat, sizeof(dat), retlen) == FALSE)
            return (UINT32)-1; // flag error. this will make unaware programs blow up when they try and malloc this
          if (need.flip)
            dat = flip(dat);
          return dat;
        }
      int getint(UINT32 &dat)
        {
          int retlen;
          if (getdata(&dat, sizeof(dat), retlen) == FALSE)
            return FALSE;
          if (need.flip)
            dat = flip(dat);
          return TRUE;
        }
      int putint(UINT32 dat)
        {
          if (need.flip)
            dat = flip(dat);
          return senddata(&dat, sizeof(dat));
        }
      SOCKET getfd(void) { return fdin; }

      ULONG unixtime(void) { return time(0); }
      UINT32 flip(UINT32 s);
      union
        {
          char flip;
          int iflip;
        } need;

    private:
      int fdin;
      int fdout;
      UINT sockettimeout;
      UINT socketwritetimeout;
      int appropriated;
      int err;
      char *xbuf; // for bulk reading bufferedgetline
      int xbufpos; // current read position in buffer
      int xbufend; // position of last byte in buffer
  };


#endif

