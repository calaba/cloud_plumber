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
/*

   sshhd server.

   We make one listensocket, get a request, handle CONN DROP or SWAP.
   Make, delete or transfer data for the session id that it sent.
   Return quickly and go back to listening, for there is only one of us. */



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <netinet/tcp.h>

#include <Q/sshhcfg.h>
#include "sshhdefs.h"
#include "Q/memm.h"
#include "Q/connector.h"
#include "Q/socketio.h"
#include "Q/hashboard.h"

config *globalcfg = NULL;

// I don't really think we can handle 1000 sessions in one process, nor will I ever need to.
hashboard<socketio> sockets(1000);
hashboard<time_t> lastseen(1000);
Memory controlqueue = (char *)"";

int processopen(socketio &scoclient, char *sessionid);
int processdrop(socketio &scoclient, char *sessionid);
int nothingback(void);
int cleanup(char *sessionid);
void fixsigpipe(void);
int getaddr(char *address, struct sockaddr_in *serv_addr);
int droperror(socketio &sco, char *sessionid, char *msg, int docleanup = TRUE);
int handlerequest (int acceptedsocket);
int handleincoming (int acceptedsocket);
void dumpdata(int connid, char *buf, int size);
void dumpdatanew(int connid, char *buf, int size);
void cleandeadsockets(void);


void nolog (int debug, char *s)
  {
    if (debug == 0)
      logstringandtime(debug, s);
  }

#define logstringandtime nolog

int main (int num, char *opts[])
  {
    srand(time(NULL));
    fixsigpipe();
    char *fname = (char *)"sshhd.cf";
    if (num > 1)
      if (memcmp(opts[1], (char *)"-c", 2) == 0)
        fname = opts[1]+2;

    config cfg(fname);
    globalcfg = &cfg;

    // make socket to listen on
    SOCKET local = socket (AF_INET, SOCK_STREAM, 0);
    if (local < 0)
      {
        printf ("Error creating listen socket, error = %d, %s\n", errno, strerror(errno));
        return 1;
      }
    struct sockaddr_in localaddr;
    memset (&localaddr, 0, sizeof(localaddr));

    if (getaddr(cfg.getlistenserver(), &localaddr) != 0)
      return 1;
    localaddr.sin_port = htons(cfg.getlistenport());

    int dat = 1;
    setsockopt(local, SOL_SOCKET, SO_REUSEADDR, (char *)&dat, sizeof(dat));
    dat = 1;
    setsockopt(local, IPPROTO_TCP, TCP_NODELAY, (char *)&dat, sizeof(dat));
    printf ("Binding to %s:%d\n", cfg.getlistenserver(), cfg.getlistenport());
    if (bind(local, (struct sockaddr *) &localaddr, sizeof(localaddr)) < 0)
      {
        printf ("Can not bind to port %d, error = %d, %s\n", cfg.getlistenport(), errno, strerror(errno));
        close (local);
        return 1;
      }
    if (listen(local, 5) != 0)
      {
        printf ("Can not listen on second socket, error = %d, %s\n", errno, strerror(errno));
        close (local);
        return 1;
      }


    // make socket to netsil on
    SOCKET lacol = socket (AF_INET, SOCK_STREAM, 0);
    if (lacol < 0)
      {
        printf ("Error creating listen socket, error = %d, %s\n", errno, strerror(errno));
        return 1;
      }
    struct sockaddr_in lacoladdr;
    memset (&lacoladdr, 0, sizeof(lacoladdr));

    if (getaddr(cfg.getnetsilserver(), &lacoladdr) != 0)
      return 1;
    lacoladdr.sin_port = htons(cfg.getnetsilport());

    printf ("Listening on %s:%d\n", cfg.getnetsilserver(), cfg.getnetsilport());
    setsockopt(lacol, SOL_SOCKET, SO_REUSEADDR, (char *)&dat, sizeof(dat));
    dat = 1;
    setsockopt(lacol, IPPROTO_TCP, TCP_NODELAY, (char *)&dat, sizeof(dat));
    if (bind(lacol, (struct sockaddr *) &lacoladdr, sizeof(lacoladdr)) < 0)
      {
        printf ("Can not bind to port %d, error = %d, %s\n", cfg.getnetsilport(), errno, strerror(errno));
        close (lacol);
        close (local);
        return 1;
      }

    if (listen(lacol, 5) != 0)
      {
        printf ("Can not listen on second socket, error = %d, %s\n", errno, strerror(errno));
        close (lacol);
        close (local);
        return 1;
      }

    int highestsocket = local;
    if (lacol > highestsocket)
      highestsocket = lacol;

    int errorcount = 0;
    while (1)
      {
        size_t namelen; // so the caller can get peer address info
        struct sockaddr_in addr;

        // we have to make select on both listen sockets since we can get a connection from either
        fd_set ler;
        FD_ZERO(&ler);
        FD_SET(local, &ler);
        FD_SET(lacol, &ler);
        struct timeval t;
        t.tv_sec = globalcfg->getservertimeout() / 1000;
        t.tv_usec = 0;
        int bret = select (highestsocket+1, &ler, 0, 0, &t); // block for max timeout period
        if (bret == -1)
          {
            sleep(1); // I'm still open to ideas.
            continue;
          }
        if (FD_ISSET(local, &ler) != FALSE)
          {
            namelen = sizeof(addr);
            int ret = accept(local, (struct sockaddr *)&addr, (socklen_t *)&namelen);
            if (ret < 0)
              {
                printf ("Error accepting socket: %d - %s\n", errno, strerror(errno));
                errorcount++;
                if (errorcount > 1000) // consecutive
                  return 1; // bail
                sleep(1);
                continue;
              }

            errorcount = 0;
            handlerequest(ret);
          }

        if (FD_ISSET(lacol, &ler) != FALSE)
          {
            namelen = sizeof(addr);
            int ret = accept(lacol, (struct sockaddr *)&addr, (socklen_t *)&namelen);
            if (ret < 0)
              {
                printf ("Error accepting socket: %d - %s\n", errno, strerror(errno));
                errorcount++;
                if (errorcount > 1000) // consecutive
                  return 1; // bail
                sleep(1);
                continue;
              }

            errorcount = 0;
            handleincoming(ret);
          }
        logstringandtime(1, (char *)"cleandeadsocket check.");
        cleandeadsockets();
      } // while
  } // main

void cleandeadsockets(void)
  {
    // iterate through hashmap looking for sessions that haven't been polled in a while
    char *sessionid;
    time_t *ltime;
    lastseen.resetiterator();
    time_t now = time(NULL);
    time_t timeout = globalcfg->getservertimeout() / 1000; // we only need seconds
    while (lastseen.iterator(sessionid, ltime) != FALSE)
      {
      char bb[2000];
      sprintf (bb, (char *)"Checking %s lastping %ld,  now %ld,  delta %ld, timeout %ld", sessionid, *ltime, now, (now-timeout) - *ltime, timeout);
      logstringandtime(2, bb);

        if (*ltime < (now - timeout))
          {
            char bb[2000];
            sprintf(bb, (char *)"Closing session %s due to request poll timeout.", sessionid);
            logstringandtime(0, bb);
            cleanup(sessionid);
            lastseen.resetiterator();
            return;
          }
      }
  }

int handleincoming (int acceptedsocket)
  {
    /* Make a session for this socket end, and post something in the control queue
     * so the sshh on the other end knows to connect to it. */

    logstringandtime(0, (char *)"Handling netsil connection.");
    int sessionnumber = rand() + 1; // not zero
    Memory sessionid;
    sessionid.addnum(sessionnumber);
    char bb[2000];
    sprintf (bb, (char *)"New sessionid = %d\n", sessionnumber);
    logstringandtime(0, bb);

    socketio *sco = new socketio(acceptedsocket, acceptedsocket, 0);
    sockets.set(sessionid, sco); // owned by hashtable

    time_t *now = new time_t(time(NULL));
    lastseen.set(sessionid, now); // owned by hashtable

    // now tell other end we're here.
    controlqueue.addstr((char *)"NNOC ");
    controlqueue.addstr(sessionid);
    controlqueue.addstr("-");
    controlqueue.addstr(globalcfg->getsecret());
    controlqueue.addstr((char *)"\r\n");
    return 0;
  }

int handlerequest (int acceptedsocket)
  {
    // get all the request data and see what they want to do.
    socketio sco(acceptedsocket, acceptedsocket, 10); // takes ownership of the socket/FD
    Memory datain;
    int datainlen;
    int ret = sco.getalldata(datain, datainlen);
    if (ret == FALSE)
      {
        printf ("Unable to get data from cgi: %s\n", strerror(sco.errorstate()));
        return FALSE; // go back to listening
      }

    char *s = datain; // data from above
    // now read the command from the first line
    Memory line(101);
    ret = readlinemax(line, s, 100);
    if (ret == 0)
      return droperror(sco, 0, (char *)"Invalid command.");
    char *p = strchr(line, ' ');
    if (p == NULL)
      return droperror(sco, 0, (char *)"Invalid protocol");
    *p = '\0';
    p++;
    char *cmd = line;

    char *nl = p;
    p = strchr(nl, ' ');
    if (p == NULL)
      return droperror(sco, 0, (char *)"Invalid sessionid");
    *p = '\0';
    p++;
    char *sessionid = nl;

    nl = p;

    int clientdatasize = val(nl);
    // and s should point to the start of client data
    if (strcmp(cmd, (char *)"CONN") == 0)
      return processopen(sco, sessionid);
    if (strcmp(cmd, (char *)"DROP") == 0)
      return processdrop(sco, sessionid);
    if (strcmp(cmd, (char *)"SWAP") != 0)
      return droperror(sco, sessionid, (char *)"Command not recognized.");

    // do the swap

    // get the session's socket
    socketio *server;

    // special case for control queue
    if (strcmp(sessionid, (char *)"0") == 0)
      {
        logstringandtime(1, (char *)"Handling control queue reqeust.");
        logstringandtime(1, controlqueue);
        Memory smbuf(strlen(controlqueue) + 100);
        sprintf(smbuf, (char *)"BACK %d\r\n", strlen(controlqueue));
        sco.senddata(smbuf, strlen(smbuf));
        ret = sco.senddata(controlqueue, strlen(controlqueue));
        if (ret == FALSE)
          {
            printf ("Unable to write data back to client.\n");
            // not a tragety, it's a short lived connection. it will die on the other end
            // but we also can't say DROP to them. But no worries, they'll find out on the next request
            return FALSE;
          }
        controlqueue = (char *)""; // reset for next time.
        return 0;
      }

    ret = sockets.get(sessionid, server);
    if (ret == FALSE)
      {
        // send back drop command, if server socket doesn't exist
        printf ("Can't find sessionid %s to swap.\n", sessionid);
        return droperror(sco, sessionid, (char *)"Session does not exist.");
      }
    // mark that we've seen this socket requested lately
    lastseen.del(sessionid); // this will delete the int
    time_t *now = new time_t(time(NULL)); // make a new one with now time
    lastseen.set(sessionid, now); // owned by hashtable

    // see if there's anything from client to send to server
    if (clientdatasize > 0)
      {
        //printf ("Data to send to server from client...\n");
        //dumpdatanew(val(sessionid), s, clientdatasize);
        // s still has location of data
        ret = server->senddata(s, clientdatasize);
        if (ret == FALSE)
          {
            printf("Error writing to socket in session id %s in swap. %s\n", sessionid, strerror(server->errorstate()));
          }
      }
    // see if there's anything from the server to send back.
    char inbuf[MAXBUF];
    int inlen = 0;
    // get as much data as you can, don't wait.
    // Ben suggests waiting 500 ms or so for the daemon to respond.
    ret = server->getdata(inbuf, MAXBUF, inlen, TRUE, 200);
    if (ret != FALSE) // got data
      {
        char smbuf[100];
        sprintf(smbuf, (char *)"BACK %d\r\n", inlen);
        sco.senddata(smbuf, strlen(smbuf));
        ret = sco.senddata(inbuf, inlen);
        if (ret == FALSE)
          {
            printf ("Unable to write data back to client.\n");
            // not a tragety, it's a short lived connection. it will die on the other end
            // but we also can't say DROP to them. But no worries, they'll find out on the next request
            return FALSE;
          }
      }
    else // got error, return drop command
      {
        /* If server disconnected send back a drop command to the client */
        printf ("Server closed connection while trying to read, sending DROP to client.\n");
        return droperror(sco, sessionid, (char *)"Server closed connection.");
      }
    // sco goes out of scope, socket closes, blah blah blah.
    return 0;
  }


int processopen(socketio &scoclient, char *sessionid)
  {
    // really simple just make the connection
    // since we have no error reporting yet, it's gotta work
    char *dest = globalcfg->getdestserver();
    int destport = globalcfg->getdestport();

    // break session-id up into session-id and secret, and validate secret.
    char *secret = strchr(sessionid, '-');
    if (secret == NULL)
      {
        printf("invalid protocol\n");
        return droperror(scoclient, sessionid, (char *)"Unable to connect to server.", FALSE);
      }
    *secret = '\0';
    secret++;
    if (strcmp(secret, globalcfg->getsecret()) != 0)
      {
        printf("invalid protocol\n");
        return droperror(scoclient, sessionid, (char *)"Unable to connect to server.", FALSE);
      }

    printf ("Open request for sessionid %s.\n", sessionid);
    Connector con(dest, destport, 0);
    if (con.errorstate() != 0)
      {
        printf ("Unable to connect to server: %s:%d %s\n", dest, destport, strerror(con.errorstate()));
        // tell the client to drop
        // don't need to call cleanup because we haven't added anything to the
        // hashmaps nor have we opened a socket.
        return droperror(scoclient, sessionid, (char *)"Unable to connect to server.", FALSE);
      }
    socketio *sco = new socketio(con.victim(), con.victim(), 0);
    sockets.set(sessionid, sco); // owned by hashtable

    time_t *now = new time_t(time(NULL));
    lastseen.set(sessionid, now); // owned by hashtable

    // return a success code
    char smbuf[100];
    sprintf(smbuf, (char *)"OKAY %s\r\n", sessionid);
    int ret = scoclient.senddata(smbuf, strlen(smbuf));
    if (ret == FALSE)
      printf ("Unable to write OKAY command data back to client.\n");
    // again we can't respond, but they'll find out next time something is wrong
    return 0;
  }

int droperror(socketio &sco, char *sessionid, char *msg, int docleanup)
  {
    printf ("Sending DROP to client.\n");
    char smbuf[100];
    sprintf(smbuf, (char *)"DROP %s %s\r\n", sessionid, msg);
    int ret = sco.senddata(smbuf, strlen(smbuf));
    if (ret == FALSE)
      printf ("Unable to write drop command data back to client.\n");

    if (docleanup)
      return cleanup(sessionid);
    return TRUE;
  }

int processdrop(socketio &scoclient, char *sessionid)
  {
    printf("Drop session id %s\n", sessionid);

    // return a success code
    char smbuf[100];
    sprintf(smbuf, (char *)"OKAY %s\r\n", sessionid);
    int ret = scoclient.senddata(smbuf, strlen(smbuf));
    if (ret == FALSE)
      printf ("Unable to write OKAY command data back to client.\n");
    return cleanup(sessionid);
  }

int cleanup(char *sessionid)
  {
    if (sessionid == NULL)
      return TRUE;
    socketio *sco;
    int ret = sockets.get(sessionid, sco);
    if (ret == FALSE)
      {
        printf ("Can't find sessionid %s to cleanup.\n", sessionid);
        return FALSE;
      }
    sockets.del(sessionid); // this will delete the sco
    lastseen.del(sessionid); // this will delete the int
    return TRUE;
  }



void fixsigpipe(void)
  {
    // Turn SIGPIPE off
    struct sigaction Action;
    Action.sa_handler = SIG_IGN; // let signal fall through to errno

    sigset_t s_t;
    sigemptyset(&s_t);

    Action.sa_mask = s_t; // no other signals needed
    Action.sa_flags = 0; // no special flags needed

    int ret = sigaction(SIGPIPE, &Action, NULL); /* Don't need to save old handler */
    if (ret != 0)
      {
        printf("Unable to set signal handler for sigpipe: %s\n", strerror(errno));
      }
  }

int getaddr(char *address, struct sockaddr_in *serv_addr)
  {
    /* return 1 if fail 0 if okay */
    struct hostent *nam;
    memset (serv_addr, 0, sizeof(*serv_addr));
    serv_addr->sin_family = AF_INET;
    if ((*address >= '0') && (*address <= '9')) // cheezy but it works
      serv_addr->sin_addr.s_addr = inet_addr(address);
    else
      {
        nam = gethostbyname (address);
        if (nam == NULL)
          {
            printf ("Error resolving name: %s, error = %d, %s\n", address, h_errno, strerror(h_errno));
            return 1;
          }
        unsigned long *nentry = (unsigned long *)nam->h_addr_list[0];
        serv_addr->sin_addr.s_addr = *nentry;
      }
    return 0;
  }

void dumpdata(int connid, char *buf, int size)
  {
    /* print out in a nice format so it will be readable.
       Good old hex-n-ascii would be nice */

    printf ("Data from socket %d. %d bytes.\n", connid, size);
    char asciibuf[85];
    int lpos = 0;
    int cpos = 0;
    int pcpos = 0;
    printf ("  "); // indent new line
    while (cpos < size)
      {
        unsigned char c = *(buf+cpos);
        printf ("%02x ", c);
        c = c & 0x7f;
        if ((c < 32) || (c > 126))
          c = '.';
        asciibuf[pcpos] = c;
        lpos++;
        pcpos++;
        if ((lpos % 8) == 0)
          {
            printf (" ");
            asciibuf[pcpos++] = ' ';
          }

        if (lpos >= 32)
          {
            asciibuf[pcpos] = '\0';
            printf (" %s\n  ", asciibuf);
            lpos = 0;
            pcpos = 0;
          }
        cpos++;
      }
    // flush last line
    if (lpos > 0)
      {
        int diff = 31-lpos; // what's missing to indent
        while (diff >= 0)
          {
            printf("   "); // for each hex spot display
            if ((diff % 8) == 0)
              printf (" ");
            diff--;
          }
        asciibuf[pcpos] = '\0';
        printf (" %s\n", asciibuf);
      }
    printf("\n");
  }




void dumpdatanew(int connid, char *buf, int size)
  {
    /* print out in a nice format so it will be readable.
       Good old hex-n-ascii would be nice */

    printf ("Data from socket %d. %d bytes.\n", connid, size);
    char asciibuf[35];
    int lpos = 0;
    int cpos = 0;
    int pcpos = 0;
    printf ("  "); // indent new line
    while (cpos < size)
      {
        unsigned char c = *(buf+cpos);
        printf ("%02x ", c);
        c = c & 0x7f;
        if ((c < 32) || (c > 126))
          c = '.';
        asciibuf[pcpos] = c;
        lpos++;
        pcpos++;
        if ((lpos % 16) == 0) // was 8
          {
            printf (" ");
            asciibuf[pcpos++] = ' ';
          }

        if (lpos >= 16)
          {
            asciibuf[pcpos] = '\0';
            printf (" %s\n  ", asciibuf);
            lpos = 0;
            pcpos = 0;
          }
        cpos++;
      }
    // flush last line
    if (lpos > 0)
      {
        int diff = 15-lpos; // what's missing to indent
        while (diff >= 0)
          {
            printf("   "); // for each hex spot display
            if ((diff % 8) == 0)
              printf (" ");
            diff--;
          }
        asciibuf[pcpos] = '\0';
        printf (" %s\n", asciibuf);
      }
    printf("\n");
  }




