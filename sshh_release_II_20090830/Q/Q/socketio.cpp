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
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <strings.h>
#include <sys/time.h>

#include "memm.h"
#include "socketio.h"


socketio::socketio (int Infd, int Outfd, int timeout, int appropriate)
  {
    fdin = Infd;
    fdout = Outfd;
    // a timeout of 0 is special, it means never time out
    sockettimeout = timeout; // default is 20 seconds
    socketwritetimeout = timeout; // default is 20 seconds
    need.iflip = 1; // detect endianness
    appropriated = appropriate;
    err = 0;
    xbuf = NULL;
  }

void socketio::settimeout(int timeout)
  {
    sockettimeout = timeout;
  }

void socketio::setwritetimeout(int timeout)
  {
    if (timeout == 0)
      timeout = 20;
    socketwritetimeout = timeout;
  }

UINT32 socketio::flip(UINT32 s)
  {
    union
      {
        char ary[4];
        UINT32 num;
      } t;

    t.num = s;
    char c = t.ary[0]; t.ary[0] = t.ary[3]; t.ary[3] = c;
    c = t.ary[1]; t.ary[1] = t.ary[2]; t.ary[2] = c;
    return t.num;
  }

int socketio::sendstr(const char *s, int len)
  {
    int size;
    int size2;
    if (len == -1)
      size = strlen(s)+1; /* Send the NT */
    else
      size = len;
    if (need.flip)
      size2 = flip(size);
    else
      size2 = size;
    if (senddata(&size2, sizeof(size2)) == FALSE)
      return FALSE;
    return senddata((char *)s, size);
  }

int socketio::getstr(char **s)
  {
    /* THIS IS A FACTORY. YOU MUST FREEMEM THE POINTER YOU GET BACK */
    /* Your buffer better be big enough */
    int size;

    *s = NULL;
    int retlen;
    if (getdata(&size, sizeof(size), retlen) == FALSE)
      return FALSE;
    if (need.flip)
      size = flip(size);
    if (size > (1024 * 1024))
      { // assume it's bad, we never get strings bigger than a meg
        err = EINVAL; // got a better idea?
        return FALSE;
      }

    *s = getmem (size); // they are sending the NT
    if (getdata(*s, size, retlen) == FALSE)
      {
        freemem (*s);
        *s = NULL;
        return FALSE;
      }
    return TRUE;
  }

int socketio::getstr(Memory &s)
  { // this is a better version of the above.
    int size;
    int retlen;
    s = "";
    if (getdata(&size, sizeof(size), retlen) == FALSE)
      return FALSE;
    if (need.flip)
      size = flip(size);
    if (size > (1024 * 1024))
      { // assume it's bad, we never get strings bigger than a meg
        err = EINVAL;
        return FALSE;
      }
    Memory m(size); // they are sending the NT
    if (getdata(m, size, retlen) == FALSE)
      {
        return FALSE;
      }
    s = m;
    return TRUE;
  }

int socketio::senddata(void *s2, int len)
  {
    fd_set fd;
    struct timeval tme;
    int offset = 0;
    ULONG starttime;
    char *s = (char *)s2;

    if (err != 0) // quick bailout if we're in a bad state
      return FALSE;
    if (fdout == 0)
      return FALSE; // has been closed

    /* 3/19/02 All time stupid stu award. I was waiting 1millisecond instead of 1 second.
       Really bad things happen when it backs up */
    starttime = unixtime();

    while (offset < len)
      {
        if ((unixtime() - starttime) > socketwritetimeout)
          {
            //(*ls)(LSERR) << "Timed out waiting on socket write. " << socketwritetimeout <<
            //              " seconds have passed." << endl;
            err = ETIMEDOUT;
            return FALSE; /* took too long */
          }
        FD_ZERO(&fd);
        FD_SET(fdout, &fd);
        // 1/3/03 linux bug.
        tme.tv_sec = 1;
        tme.tv_usec = 0; /* Don't hang the machine, give it some sleep time, if no data ready */
        int ret = select (fdout+1, 0, &fd, 0, &tme); // for write
        if (ret == 0)
          continue;
        if (FD_ISSET(fdout, &fd) == 0)
          continue; // our socket isn't avail to write yet
        /* Write to the socket what we can */
        starttime = unixtime();
        ret = write (fdout, s+offset, len-offset); // try and write everything
        if (ret == -1)
          {
            //(*ls)(LSERR) << "Can't write to socket. " << strerror(errno) << endl;
            err = errno;
            return FALSE; // tried to do something...
          }
        offset += ret;
      }
    return TRUE;
  }

int socketio::getdata(void *s2, int len, int &retdatasize, int takeanything, int waitatall)
  {
    fd_set fd;
    struct timeval tme;
    int offset;
    ULONG starttime;
    char *s = (char *)s2;

    /* 12/24/05 In an attempt to mitigate weird linux goings on I have a dream that
       maybe the problem is that I'm doing two selects on a socket that's closed before
       reading the zero bytes returned, and maybe THAT's why I'm getting EINPROGRESS.
       So I've added a hack hack(hack) to skip the select first time in.
       Nope, that didn't help, always with the einprogress. */
    /* 8/17/03 waitatall, without breaking anything we want to be able to read
       partial buffers without waiting even a second, just poll. */
    /* 5/22/02 takeanything. This is for bufferedgetline. If this flag is set true
       then we will exit as soon as we get any data, if we get nothing in the timeout
       period we return false. successful return is bytes read */
    /* 3/19/02 All time stupid stu award. I was waiting 1millisecond instead of 1 second.
       Really bad things happen when it backs up */

    if (err != 0) // quick bailout if we're in a bad state 9/10/03
      return FALSE;
    if (fdin == 0)
      return FALSE; // has been closed

    offset = 0;
    starttime = unixtime();
    retdatasize = 0; // in case nothing comes back
    while (offset < len)
      {
        FD_ZERO(&fd);
        FD_SET(fdin, &fd);
        // Linux trashes timeval upon return, we have to reset it constantly.
        tme.tv_usec = 0; /* Don't hang the machine, give it some sleep time, if no data ready */
        if (waitatall == 0)
          tme.tv_sec = 0;  // don't wait at all.
        else
          if (waitatall == 1)
            tme.tv_sec = 1;
          else
            { // otherwise use as ms setting
              tme.tv_sec = 0;
              tme.tv_usec = waitatall;
            }
        int ret = select (fdin+1, &fd, 0, 0, &tme); // for read
        if (ret != 0) // something to read
          if (FD_ISSET(fdin, &fd) != 0) // and it's ours
            {
              /* Read up to the end of the packet */
              starttime = unixtime();
              ret = read (fdin, s + offset, len-offset);  // add to end
              if (ret == -1)
                {
                  // xxxz see if this is our EAGAIN case, if it is, read again.
                  // printf ("Pid: %d: Select says 1, but -1 from read socket: %s\n", (int)getpid(), strerror(errno));
                  // 12/12/07, fix the weird 386 solaris thing
                  /* So why exactly does select return just to tell me try again?
                     What is the point of select, and is this going to make the cpu spin? */
//printf ("socket read ret = -1: %s\n", strerror(errno));
                  if (errno == EAGAIN) // this isn't an error, everything's working, we shouldn't bail
                    continue;
                  err = errno;
                  return FALSE; // didn't get packet
                }
              if (ret == 0)
                { /* If select says yes but no data, it should be a clean socket close, but... */
                  // can't explain this, but it's an aix thing.
                  // okay, it's a unix thing, but linux returns EINPROGRESS when it's a close connection.
                  // I dunno why, but we'll leave it at that.
                  // 12/12/07, new new rule, see what the problem is.
//printf ("socket read ret = 0 \n");
//printf ("Pid: %d: Select says 1, 0 bytes from socket: %s\n", (int)getpid(), strerror(errno));
/* new rule, if select says yes and read says zero it's closed. don't ask questions. */
return FALSE;
/* We kinda hope this really works, because if we have no timeout, we can only exit
   on socket errors now. (for postfix) */

                  UINT32 dat = 0;
                  socklen_t datsize = sizeof(dat);
                  ret = getsockopt(fdin, SOL_SOCKET, SO_ERROR, (char *)&dat, &datsize);
//printf ("socket close, errno %d ret %d getsockopt %d\n", errno, ret, dat); // xxxz
                  if (ret != 0)
                    err = ECONNABORTED; // why no errno set?
                  else
                    if (dat == 0)          // linux and AIX
                      err = ECONNABORTED;
                    else
                      if (dat == EINPROGRESS) // linux wazza who?
                        err = 0;
                      else
                        err = dat;
                  return FALSE; // didn't get a packet
                }
              if (takeanything)
                {
                  retdatasize = ret;
                  return TRUE;
                }
              offset += ret; // keep track of how much we got.
            } // if there's something to read
        if (takeanything)
          {
            return TRUE; // never wait for timeout, if we got here, than nothing was read, zero returned, but not error
          }
        if (sockettimeout != 0) // 10/2/06, for postfix, a sockettimeout of 0 means never time out on read
          if ((unixtime() - starttime) >= sockettimeout)
            {
              //(*ls)(LSERR) << "Timed out waiting on socket read. " << sockettimeout <<
              //              " seconds have passed." << endl;
              err = ETIMEDOUT;
              return FALSE; /* took too long */
            }
      }
    retdatasize = len; // got the whole thing.
    return TRUE;
  }


int socketio::getalldata(Memory &out, int &retlen)
  {
    fd_set fd;
    struct timeval tme;
    int offset;
    ULONG starttime;

    // read all the data until the end of the socket closes

    retlen = 0;
    if (err != 0) // quick bailout if we're in a bad state 9/10/03
      return FALSE;
    if (fdin == 0)
      return FALSE; // has been closed

    offset = 0;
    starttime = unixtime();

    out = "";
    int curpos = 0;
    int done = FALSE;
    while (done == FALSE)
      {
        FD_ZERO(&fd);
        FD_SET(fdin, &fd);
        // Linux trashes timeval upon return, we have to reset it constantly.
        tme.tv_sec = 1;
        tme.tv_usec = 0; /* Don't hang the machine, give it some sleep time, if no data ready */
        int ret = select (fdin+1, &fd, 0, 0, &tme); // for read
        if (ret != 0) // something to read
          if (FD_ISSET(fdin, &fd) != 0) // and it's ours
            {
              /* Read up to the end of the packet */
              starttime = unixtime();
              char buf[65536];
              ret = read (fdin, buf, 65536);
              if (ret == -1)
                {
                  if (errno == EAGAIN) // this isn't an error, everything's working, we shouldn't bail
                    continue;
                  err = errno;
                  return FALSE; // didn't get packet
                }
              if (ret == 0)
                { /* If select says yes but no data, it should be a clean socket close,
                     we're done. */
                  return TRUE;
                }
              // copy it to outgoing buffer
              // this could be more efficient, we resize on every part read. Blah.
              out.resize(curpos + ret + 10);
              memcpy(out+curpos, buf, ret);
              curpos += ret;
              retlen = curpos;
            } // if there's something to read
        if (sockettimeout != 0)
          if ((unixtime() - starttime) >= sockettimeout)
            {
              err = ETIMEDOUT;
              return FALSE; /* took too long */
            }
      }
    // never gets here.
    return TRUE;
  }

int socketio::getline(Memory &line, char delimeter)
  {
    /* Cheap and lazy and slow. we read char by char.
       If we buffer, we may screw up the stream.
       Perhaps someday I'll make Memory a real string class and have it
       allocate more memory on the fly rather than have realloc do it
       for me. */
    /* 3/10/00 want/need more efficient version of this. Not using addchar anymore */
    line = ""; // default if we fail for some reason
    int linesize = 1000;
    int linepos = 0;
    int growsize = 1000; // start big, so we almost never have to do this
    char *block = getmem(linesize);
    int ret = TRUE;

    /* we can make this faster by not using getdata 1 char at a time,
       read and prebuffer everything until we get a crlf. the
       presumption is we stay within the buffered framework. */
    while (ret != FALSE)
      {
        int retlen;
        ret = getdata(block+linepos, 1, retlen);
        // 12/11/07, this is a blocking socket (I think) so we should never
        // get EAGAIN, but we are. So deal with that case. Handled in getdata.
        if (ret == FALSE)
          {
            freemem(block);
            return FALSE;
          }
        if (*(block+linepos) == '\r')
          continue;
        if (*(block+linepos) == delimeter)
          break;
        linepos++;
        if (linepos+1 >= linesize) // leave room for NT
          {
            linesize += growsize;
            growsize *= 2;
            /* Don't crash because of out of memory if somebody's
               trying to kill us with too big of a single line */
            if (linesize > (1024*1024*4))
              {
                freemem (block);
                return FALSE;
              }
            block = reallocmem(block, linesize);
          }
      }
    *(block+linepos) = '\0'; // nt it, it's a string
    line.steal(block);
    return TRUE;
  }

int socketio::sendline(char *line)
  {
    Memory e2(line);
    e2.addstr(CRLF);

    return senddata((char *)e2, strlen(e2));
  }

int socketio::bufferedgetline(Memory &line, char delimeter, int immediatereturn)
  {
    /* complex, animated and fast. We need to getline very quickly.
       The downside of this function is that once you use it, you
       have to keep using it, unless we fix all other socketio
       functions to honor the buffer. */
    /* for popclient. 8/17/03 If immediatereturn is true, then
       add to our buffer, don't wait for the timeout, if there's
       not a full line of data, then return false right away. */
    /* If one incoming line is bigger than GETLINEBUFFERSIZE then this will
       hang forever, fix that later, but now it will work if we get a line
       in multiple dribs and drabs */
    if (xbuf == NULL)
      {
        xbuf = getmem(GETLINEBUFFERSIZE);
        xbufpos = 0;
        xbufend = 0;
      }
    int origxbufpos = xbufpos; // in case we have to restore it on failed exit
    line = ""; // default if we fail for some reason
    int linesize = 1000;
    int linepos = 0;
    int growsize = 1000; // start big, so we almost never have to do this

    char *block = getmem(linesize);
    int more = TRUE;

    int left;
    while (more != FALSE)
      {
        left = xbufend-xbufpos;
        // copy it until we hit delim or run out of buffer
        while (left > 0)
          {
            char *here = block+linepos;
            *here = *(xbuf + xbufpos);
            left--;
            xbufpos++;

            if (*here == delimeter) // we're at end of line
              {
                more = FALSE;
                break;
              }
            if ((*here == '\r') && (delimeter == '\n')) // this is different from getline
              continue; // go to next char

            linepos++; // advance write buffer and check for overflow
            if (linepos+1 >= linesize) // leave room for NT
              {
                linesize += growsize;
                growsize *= 2;
                /* Don't crash because of out of memory if somebody's
                   trying to kill us with too big of a single line */
                if (linesize > (1024*1024*4))
                  {
                    freemem (block); // xbuf is in an okay, if out of sync state
                    return FALSE;
                  }
                block = reallocmem(block, linesize);
              }
          } // end of copying from buffered input

        if (more)
          {
            // we need more data, read from the stream
            // try, if there's nothing then keep the existing buffer for next call in.
            int newend;
            char *addtobuf = xbuf + xbufpos; // adding more to current part?
            int request = GETLINEBUFFERSIZE - xbufpos; // don't max out buffer

            /* Now I remember why I did this. We request a huge buffer and we ask getdata for it.
               If we waited the full timeout, we're waiting for the whole buffer, so we wait for anything
               for a second, and the timeout check should be here, in the buffered getline.
               There's still something wrong with the design of this. I guess I should add the
               timeout check here, and not expect getdata to do it.
               This immediate return just allows getdata to sleep or not and not spin the cpu,
               we should loop a full socketio->readtimeout in this function. */

            ULONG starttime = unixtime();
            int retlen;
            while (1) // loop read retrying for sockettimeout
              {
                if (immediatereturn)
                  newend = getdata(addtobuf, request, retlen, TRUE, FALSE); // come back NOW (poll) whether you have anything or not
                else
                  newend = getdata(addtobuf, request, retlen, TRUE, TRUE); // come back when you've got something but not just polled

                if (newend == FALSE)
                  {
                    int timedout = FALSE;
                    if (sockettimeout > 0) // 10/2/06, for postfix
                      {
                        if ((unixtime() - starttime) > sockettimeout) // do real timeout check
                          timedout = TRUE;
                        else
                          timedout = FALSE;
                      }

                    if ((immediatereturn || timedout) || (err == ECONNABORTED) || // 12/27/03 avoid spinning
                        (err == ECONNRESET) || (err == EPIPE)) // 6/02/04 also avoid spinning on hung up connection.
                      {
                        freemem (block);
                        // 8/17/03, this being missing may have been a bad bug.
                        xbufpos = origxbufpos;
                        return FALSE;
                      }
                  }
                else
                  break; // if we got something, get out and process it.
              } // while looping waiting for timeout read

            // xbufpos stays where it is, because we might be in the middle of a drib-drab
            xbufend += retlen; // move new end position of buffer

            // now slide down the buffer so we don't keep adding on forever.
            // origxbufpos is the beginning of the most recent line we read in this call.
            // bufpos is the start of the next line, move that to zero.
            // this used to be at the end of bufferedgetline, but this will happen possibly
            // less frequently than calls to this function, so it will be quicker here.
            // of course this should be a circular queue, not a sliding buffer,
            // remember to fix that someday.
            if (origxbufpos > 0)
              {
                left = xbufend - origxbufpos; // what's left of our valid unread cache
                char *dest = xbuf;
                char *src = xbuf + origxbufpos;
                while (left > 0)
                  {
                    *dest++ = *src++;
                    left--;
                  }
                // adjust everything for next time in
                xbufend -= origxbufpos;
                xbufpos -= origxbufpos;
                origxbufpos = 0;
              }
          }
      } // end while we're still looking for delim

    *(block+linepos) = '\0'; // nt it, it's a string
    line.steal(block);

    return TRUE;
  }

/* here's a neat one, external forces (newt) doing selects on my sockets will
   block because we've already buffered data. There's more to read, but it's not
   on the socket, it's in my buffer.
   A simple way to do this, would be to check if xbufpos < xbufend, but if the sender
   is holding up sending us data in the middle of a line we will return true,
   but we're still going to fail if we try and get a whole line, so what we do is check
   to see if there is an available line in the buffer, if not, return false, and
   then the caller can call select. but if this returns true, don't do the select,
   but call bufferedgetline anyway, and you're guaranteed to get something. */

int socketio::bufferedlineavailable(char delim)
  {
    int lp;
    char *p = xbuf+xbufpos;
    for (lp = xbufpos; lp < xbufend; lp++)
      if (*p++ == delim)
        return TRUE;
    return FALSE;
  }



