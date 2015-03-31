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
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h> // go figure this one out, FD_ZERO needs this
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
              
#include "memm.h"
#include "connector.h"



/* Oh the things I do to make it compile everywhere... */
/* connector has to use these two function so the windows library
   will work, but if we're not building a USING_OS program
   then we have to supply them.
*/

int inprogress(int *errval)
  {
    /* return true if errno is EINPROGRESS or EAGAIN */
    int err = errno;
    *errval = err;
    if ((err == EINPROGRESS) || (err == EAGAIN))
      return TRUE;
    return FALSE;
  }

// I work at harris now...
#define __SUN__ 

int setnonblocking (SOCKET s)
  {
#ifdef __SUN__
    
    int flags = fcntl(s, F_GETFL, 0);
    if (flags == -1)
      return -1;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
#else
    UINT32 dat = 1;
    return ioctl(s, FIONBIO, (void *)&dat, sizeof(dat));
#endif    
  }



Connector::Connector (char *address, int port, int timeout)
  {
    int scin;
    struct sockaddr_in serv_addr;
    struct hostent *nam;

    fd_set w; /* write */ 
    struct timeval tm; 
    int ret;
    int dat;
    socklen_t datsize;
    
    err = 0;
    saver = 0;
    fd = 0;  // ~ only closes if > 0

    // go make the socket
    memset (&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    if (isipaddress(address))
      serv_addr.sin_addr.s_addr = inet_addr(address);
    else
      {
#ifdef _THREAD_SAFE
        struct hostent hp;
        struct hostent_data hd;
        memset (&hp, 0, sizeof(hp));
        memset (&hd, 0, sizeof(hd));
        if (gethostbyname_r (address, &hp, &hd) == 0)
          nam = &hp;
        else
          nam = NULL;        
#else
        nam = gethostbyname (address);
#endif
        if (nam == NULL)
          {
            err = h_errno;
            return;
          }
        unsigned long *nentry = (unsigned long *)nam->h_addr_list[0];
        serv_addr.sin_addr.s_addr = *nentry;
      }
    serv_addr.sin_port = htons(port);
    
    scin = socket (AF_INET, SOCK_STREAM, 0);
    if (scin < 0)
      {
        err = errno;
        return;
      }
      
    /* Connectum serverum */
    /* If there's no timeout, wait forever, make it block connect. 
       You could argue that this is a lousy way to decide if the socket
       should be blocking or non blocking, but all the other 
       library routines use select, so it doesn't matter. */
    if (timeout == 0)
      {
        ret = connect (scin, (struct sockaddr *)&serv_addr, sizeof (serv_addr));
        /* The connect has finished */ 
        if (ret < 0)
          err = errno;
        else
          fd = scin;
        return;
      } 

    // other wise, go by the timeout 
    dat = 1;
    ret = setnonblocking (scin);
    if (ret != 0)
      {
        err = errno;
        return; // fd is 0
      }    

    ret = connect(scin, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret != 0) 
      { 
        err = errno;
        int errval;
        if (inprogress(&errval) == FALSE) // all non-valid-inprogress get error msg
          return; /* failed connect, err is set */ 
      } 
 
    err = 0;
    fd = scin; // we have a real socket, needs to be closed by destructor
 
    FD_ZERO (&w); 
    FD_SET (scin, &w); /* check the donness of the connect by write, not read */

    tm.tv_sec = timeout; 
    tm.tv_usec = 0; 

    ret = select(scin+1, NULL, &w, NULL, &tm); 
    if (ret < 0) /* select returns number of descriptors */ 
      { 
        err = errno; // fd is set and will get closed upon destruction
        return; 
      } 
    if (FD_ISSET(scin, &w) == 0) 
      {
        // then we timed out
        err = ETIMEDOUT;
        return;
      }

    /* check here to see if socket connect was successful. */
    datsize = sizeof(dat);
    dat = 0;
    ret = getsockopt(scin, SOL_SOCKET, SO_ERROR, (char *)&dat, &datsize);

    if (ret != 0) 
      {
        err = errno;
        return;
      }
    if (dat != 0)
      {
        err = dat;
        return; /* connect really failed */
      }    
    err = 0; // connect worked, fd is set
  }

Connector::~Connector(void)
  {
    if (fd > 0)
      close (fd);
  }

int Connector::victim(void)
  {
    /* give up ownership so I can go away freely */
    int ret = fd;
    if (ret == 0)
      ret = saver;
    else
      saver = ret;
    fd = 0;
    return ret;
  }

