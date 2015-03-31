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

   sshh client.

   Based on packetbouncer, we accept connections, and send the incoming data over a http proxy
   request, we send along our random session number so that the other end knows which session
   to connect the connection to.
   We will ping every N seconds to see if there's any data waiting to come back to us.
   This should fill up the proxy logs nicely.

*/

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
#include <math.h>

#include <Q/sshhcfg.h>
#include "sshh.h"
#include "sshhdefs.h"
#include <Q/memm.h>
#include <Q/connector.h>
#include <Q/socketio.h>
#include <Q/base64.h>

config *globalcfg = NULL;

void nolog (int debug, char *s)
  {
    int x0 = 0;
    int x1 = -1;
    int x2 = -1;
    int x3 = -1;
    if (debug == x0)
      logstringandtime(debug, s);
    if (debug == x1)
      logstringandtime(debug, s);
    if (debug == x2)
      logstringandtime(debug, s);
    if (debug == x3)
      logstringandtime(debug, s);
  }

#define logstringandtime nolog

int main (int num, char *opts[])
  {
    primetime(); // init the time system
    srand(time(NULL));
    fixsigpipe();
    const char *fname = (char *)"sshh.cf";
    if (num > 1)
      if (memcmp(opts[1], (char *)"-c", 2) == 0)
        fname = opts[1]+2;

    config cfg(fname);
    globalcfg = &cfg;

    fd_set active;
    /* There's a list of sockets from the client, and a list of session ID's with
     * which we hit sshhd. We need to at least poll the sshhds every second for every socket
     * so we have to keep a list of sockets to search through.
     * Don't sshhd the listensocket.  */

    int sessionidsize = FD_SETSIZE * 8 * sizeof(long); // 8 bits per byte per long, matching array element to socket number.
    long sessionid[sessionidsize];
    time_t nextpolltime[sessionidsize];
    int delaymodifier[sessionidsize];
    time_t now = mytime();
    int lp;
    for (lp = 0; lp < sessionidsize; lp++)
      {
        sessionid[lp] = -1; // just a flag for debugging.
        nextpolltime[lp] = now;
        delaymodifier[lp] = 0; // how much to step up the nextpolltime
      }

    FD_ZERO(&active); // the array of sockets to sit on

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

    printf ("Listening on %s:%d\n", cfg.getlistenserver(), cfg.getlistenport());
    int dat = 1;
    setsockopt(local, SOL_SOCKET, SO_REUSEADDR, (char *)&dat, sizeof(dat));
    dat = 1;
    setsockopt(local, IPPROTO_TCP, TCP_NODELAY, (char *)&dat, sizeof(dat));
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

    char *proxy = cfg.getproxyserver();
    int proxyport = cfg.getproxyport();

    SOCKET highestsocket = local; // what to select on...

    /* Now loop in select for our interval */
    while (1)
      {
        /* Now we just select on everything, set the timer to our connect interval */

        fd_set taken; // set everything we wanna listen on
        memcpy (&taken, &active, sizeof(fd_set));
        FD_SET(local, &taken); // so we can listen on accept too

        // set timeout for how often we should poll sshhd
        struct timeval t;
        t.tv_sec = 0;

        // work out the smallest poll time we need

        /* 7/11/09 Malcolm is 5 weeks and 1 day old.
         * For outgoing connections this works great. For incoming
         * connections, the one second dropoff is too quick, because it's either
         * zero and it polls right away (which you miss most of the time if you're
         * typing) or it's one second and there's a really bad lag.
         * So for incoming connections, which we have not marked in any way,
         * we will poll in 10ths of seconds.
         */

        t.tv_usec = globalcfg->getthrottlemax(); // max out to start
        now = mytime();
        for (lp = 1; lp < highestsocket+1; lp++)
          {
            if (FD_ISSET(lp, &active)) // only check things that are live
              {
                long diff = nextpolltime[lp] - now;
                if (diff < 0)
                  diff = 0;
                if (diff < t.tv_usec)
                  t.tv_usec = diff;
              }
          }
        t.tv_sec = t.tv_usec / 1000;
        t.tv_usec %= 1000;
        t.tv_usec *= 1000;

        char bb[2000];
        sprintf(bb, (char *)"Idle, polling delay = tvsec = %ld, tv_usec = %ld sleeping...", t.tv_sec, t.tv_usec);
        logstringandtime(1, bb);
        int ret = select (highestsocket+1, &taken, 0, 0, &t);
        if (ret == 0)
          { // we timed out, hit the daemon
            pollallsockets(proxy, proxyport, highestsocket, &active, sessionid, nextpolltime, delaymodifier);
            continue;
          }
        if (ret == -1)
          {
            /* chances are it's the kind of error we're going to get
               repeatedly, and we'll just tie up CPU spinning out
               wheels doing nothing, so bail out. */
            printf ("Error on select, error = %d, %s\n", errno, strerror(errno));
            break; // continue;
          }
        // see if its our listen socket
        if (FD_ISSET(local, &taken))
          { // get it and make a new connection to the mail server
            #ifdef __CYGWIN__
                int namelen;
            #else
                size_t namelen;
            #endif
            namelen = sizeof(localaddr);
            ret = accept(local, (struct sockaddr *)&localaddr, &namelen);
            if (ret < 0)
              {
                printf ("Error from accept, error = %d, %s\n", errno, strerror(errno));
                continue; // ignore this socket
              }
            int sessionnumber = makesshhdconnection (ret, &active, sessionid, proxy, proxyport, highestsocket);
            if (sessionnumber == FALSE)
              {
                printf ("Error creating session.\n");
                continue; // ignore this socket
              }
          }

        // run through active list, see if there's anything to do
        logstringandtime(1, (char *)"Processing select...\n");
        int lp;
        for (lp = 1; lp < highestsocket+1; lp++)
          {
            if (FD_ISSET(lp, &active)) // only check things we're looking for
              if (FD_ISSET(lp, &taken)) // is this ours?
                {
                  // do the mush
                  pollthissocket(proxy, proxyport, lp, &active, sessionid, nextpolltime, delaymodifier, highestsocket);
                }
          }
      } // while trying to bounce and listen

    return 0;
  }


int hitproxy(char *proxy, int proxyport, Memory &dataout, int dataoutlen, Memory &datain, int &datainlen)
  {
    logstringandtime(2, (char *)"Before connect");

    Connector con(proxy, proxyport, 10); // does not own socket., 10 sec timeout
    if (con.errorstate() != 0)
      {
        printf ("Unable to hit proxy: %s:%d %s\n", proxy, proxyport, strerror(con.errorstate()));
        return FALSE;
      }
    logstringandtime(2, (char *)"After connect");
    socketio sco(con.victim(), con.victim(), 20); // this scope owns the connection

    logstringandtime(2, (char *)"before senddata");
    int ret = sco.senddata(dataout, dataoutlen);
    logstringandtime(2, (char *)"after senddata");
    if (ret == FALSE)
      {
        printf ("Unable to send data to proxy: %s:%d %s\n", proxy, proxyport, strerror(sco.errorstate()));
        return FALSE;
      }
    // get response. We must take the entire http response, first we take in the headers
    // then stuff the byte data back to client
    // we can't use sco.bufferedgetline because we can't get the binary data out afterwards
    // so we raw read the entire response and parse later.
    logstringandtime(2, (char *)"before getdata");
    ret = sco.getalldata(datain, datainlen);
    logstringandtime(2, (char *)"after getdata");

    if (ret == FALSE)
      {
        printf ("Unable to get data from proxy: %s:%d %s\n", proxy, proxyport, strerror(sco.errorstate()));
        return FALSE;
      }
    return TRUE; // good things come to those who complete correctly closes the connection
  }


int sendproxyrequest(char *proxy, int proxyport, char *cmd, char *data, int datasize, Memory &response, int &responsesize)
  {
    // make a post request

    int cmdlen = strlen(cmd);
    int rdatasize = cmdlen + 2 + datasize;
    Memory rdata(rdatasize);
    char *p = rdata;
    memcpy(p, cmd, cmdlen);           p += cmdlen;
    memcpy(p, (char *)"\r\n", 2);     p += 2;
    memcpy(p, data, datasize);        p += datasize;

    // encrypty typty
    logstringandtime(2, (char *)"Before crypt");
    p = rdata;
    crappycrypt((unsigned char *)p, rdatasize, (unsigned char *)globalcfg->getpassphrase(), rdatasize);
    logstringandtime(2, (char *)"After crypt");
    Memory encoded(rdatasize * 3 + 10); // make sure encoded data will fit
    int encodedsize;
    logstringandtime(2, (char *)"Before esc");
    htmlescapebinary(encoded, encodedsize, rdata, rdatasize);
    logstringandtime(2, (char *)"After esc");

    Memory headers = (char *)"POST ";
    headers.addstr(globalcfg->getendcaller());
    headers.addstr((char *)" HTTP/1.0\r\n");
    headers.addstr((char *)"Content-Length: ");
    headers.addnum(encodedsize);
    headers.addstr((char *)"\r\n");
    headers.addstr((char *)"Content-Type: application/x-www-form-urlencoded\r\n");

    Memory credentials;
    getcredentials(credentials);
    headers.addstr(credentials); // comes with trailing \r\n

    // end of headers
    headers.addstr((char *)"\r\n");

    logstringandtime(3, (char *)"Built headers.");
    int hreqsize = strlen(headers) + encodedsize;
    headers.resize(hreqsize);
    int headerssize = strlen(headers);
    memcpy(headers + headerssize, encoded, encodedsize);
    logstringandtime(2, (char *)"Before hitproxy");
    int ret = hitproxy(proxy, proxyport, headers, hreqsize, response, responsesize);
    logstringandtime(2, (char *)"after hitproxy");
    if (ret == FALSE)
      return FALSE;
    return TRUE;
  }

void getcredentials(Memory &c)
  {
    // make credentials
    Memory user = globalcfg->getusername();
    Memory pass = globalcfg->getpassword();
    Memory credentials = user;
    credentials.addstr((char *)":");
    credentials.addstr(pass);

    char *hdr = (char *)"Proxy-Authorization: Basic ";
    int ppsize = 2 * (strlen(hdr) + strlen(credentials) + 2);
    Memory pp(ppsize);
    memset(pp, 0, ppsize); // so we can pp.addstr later
    strcpy(pp, hdr);
    char *bdest = pp+strlen(hdr);
    unsigned char *bsrc = (unsigned char *)(char *)credentials;
    encode_base64(bdest, bsrc, strlen(credentials));
    pp.addstr((char *)"\r\n");
    c = pp;
  }

int makesshhdconnection(SOCKET client, fd_set *active, long sessionid[], char *proxy, int proxyport, int &highestsocket)
  {
    /* Here's the deal. We got a connection from the client, now send a request for a new
     * session at the other end. If it fails,
       close the incoming socket and return. */

    int sessionnumber = rand() + 1; // not zero
    Memory cmd = (char *)"CONN ";
    cmd.addnum(sessionnumber);
    cmd.addstr((char *)"-"); // add secret
    cmd.addstr(globalcfg->getsecret());
    cmd.addstr((char *)" ");
    cmd.addnum(0);

    char bb[2000];
    sprintf(bb, (char *)"Creating connection session id: %d\n", sessionnumber);
    logstringandtime(0, bb);

    Memory response;
    int responsesize;
    int ret = sendproxyrequest(proxy, proxyport, cmd, (char *)"", 0, response, responsesize);
    if (ret == FALSE)
      {
        printf ((char *)"Unable to make proxy request.\n");
        close (client);
        return FALSE;
      }
    // check for 200 response
    Memory in(4096);
    char *s = response;
    readlinemax(in, s, 4000);
    // HTTP/1.1 200 OK
    char *np = in;
    np = strchr(np, ' ');
    if ((np == NULL) || (memcmp(++np, (char *)"200", 3) != 0)) // how unlike me
      {
        printf((char *)"Bad response from proxy hit: %s\n", (char *)in);
        close(client);
        return FALSE;
      }
    // check for DROP command, in case something went wrong on the other end
    while (strlen(in) > 0)
      {
        int err = readlinemax(in, s, 4000);
        if (err == 0)
          {
            printf((char *)"Can't read all headers from response from proxy hit: %s\n", s);
            close (client);
            return FALSE;
          }
      }
    int lsz = s - response;
    int cdatasz = responsesize - lsz;
    crappycrypt((unsigned char *)s, cdatasz, (unsigned char *)globalcfg->getpassphrase(), cdatasz);
    // s now points to the raw response data from sshhd
    Memory line(1001);
    ret = readlinemax(line, s, 1000);
    if (ret == 0)
      {
        printf ((char *)"Bad protocol in response %s.\n", (char *)line);
        close (client);
        return FALSE;
      }
    char *p = strchr(line, ' ');
    if (p == NULL)
      {
        printf ((char *)"Bad protocol in response %s.\n", (char *)line);
        close (client);
        return FALSE;
      }
    *p = '\0';
    p++;
    char *cmdin = line;
    if (strcmp(cmdin, (char *)"OKAY") != 0)
      {
        printf((char *)"Bad response back from connect request: %s\n", (char *)p);
        close (client);
        return FALSE;
      }

     // add client socket to active list
    FD_SET(client, active);
    sessionid[client] = sessionnumber;

    if (client > highestsocket)
      highestsocket = client;

    return sessionnumber;
  }


void makedhhssconnection(char *sessionidstr, long sessionid[], fd_set *active, int &highestsocket, time_t nextpolltime[], int delaymodifier[])
  {
    // try and make a new connection out to the configured server, if it works add it to the live list
    char bb[2000];
    sprintf(bb, (char *)"Making new reverse connection for sessionid %s", sessionidstr);
    logstringandtime(0, bb);

    /* I stole this from sshhd.
     * This isn't as pretty as makenewsshd connection, since they can batch them
     * together, we can't respond to each one individually, so they'll find out the hard
     * way, and their end will just timeout.
     * I suppose we can make a dead socket that's closed so we'll trigger a DROP
     */

    char *tsed = globalcfg->gettsedserver();
    int tsedport = globalcfg->gettsedport();
    Connector con(tsed, tsedport, 0);
    if (con.errorstate() != 0)
      {
        printf ((char *)"Unable to connect to server: %s:%d %s\n", tsed, tsedport, strerror(con.errorstate()));
        // tell the client to drop
        // for now we'll just let them time out
        return;
      }

    int server = con.victim();
    char *spos = strchr(sessionidstr, '-');
    if (spos == NULL)
      {
        printf ((char *)"invalid protocol\n");
        return;
      }
    *spos = '\0';
    spos++;
    if (strcmp(spos, globalcfg->getsecret()) != 0)
      {
        printf ((char *)"invalid protocol\n");
        return;
      }
    int sessionnumber = val(sessionidstr);

    FD_SET(server, active);
    sessionid[server] = sessionnumber;

    time_t now = mytime(); // force this connection to poll right away
    nextpolltime[server] = now;
    delaymodifier[server] = 0;

    if (server > highestsocket)
      highestsocket = server;
  }


void pollallsockets(char *proxy, int proxyport, int &highestsocket, fd_set *active, long sessionid[], time_t nextpolltime[], int delaymodifier[])
  {
    int lp;
    time_t now = mytime();
    for (lp = 1; lp < highestsocket+1; lp++)
      {
        if (FD_ISSET(lp, active))
          {
            // see if our nextpolltime has passed
            if (now >= nextpolltime[lp]) // if it's now or later do it.
              pollthissocket(proxy, proxyport, lp, active, sessionid, nextpolltime, delaymodifier, highestsocket);
            else
              {
                char bb[2000];
                sprintf (bb, (char *)"Skipping socket %d, time to hit: %ld", lp, nextpolltime[lp]-now);
                logstringandtime(2, bb);
              }
          }
      }
    // just in case there's nothing going on, be sure to poll anyway.
    pollcontrolsocket(proxy, proxyport, sessionid, active, highestsocket, nextpolltime, delaymodifier);
  }

void pollthissocket(char *proxy, int proxyport, SOCKET client, fd_set *active, long sessionid[], time_t nextpolltime[], int delaymodifier[], int &highestsocket)
  {
    // hit this proxy, take the client data and send it to the matching session id
    // take the response and send it out the client socket.

    // see if there's something from the client socket
    char bb[2000];
    sprintf(bb, (char *)"Pollthissocket start, client socket: %d", client);
    logstringandtime(2, bb);

    fd_set taken; // set everything we wanna listen on
    FD_ZERO(&taken);
    FD_SET(client, &taken);

    struct timeval t;
    t.tv_sec = 0; // come back immediately
    t.tv_usec = 0;

    int isdataout = 0;
    int isdatain = 0;

    logstringandtime(2, (char *)"before select");
    int ret = select (client+1, &taken, 0, 0, &t);
    logstringandtime(2, (char *)"after select");
    if (ret == -1)
      {
        printf((char *)"Unable to check client socket: %d\n", client);
        return;
      }
    char buf[MAXBUF];
    int clientdatasize = 0;
    if (ret > 0)
      {
        logstringandtime(3, (char *)"Got something back from select");

        if (FD_ISSET(client, &taken)) // is this ours?
          {
            logstringandtime(3, (char *)"Got our client socket from select");

            // batch read here. Goes quicker if we batch reads going out
#define working
#ifdef working
            logstringandtime(2, (char *)"before read from client");
            clientdatasize = read (client, buf, MAXBUF); // if we come up short, that's okay, we'll catch it next time
            logstringandtime(2, (char *)"after read from client");

#else
            /* This causes some select stalling problems */
            int bpos = 0;
            while (1)
              {
                int left = MAXBUF - bpos;
                if (left == 0) // bail if we run out of buffer
                  {
                    clientdatasize = bpos;
                    break;
                  }
                int batchsize = read (client, buf+bpos, left);
                if (batchsize == -1) // bail on error
                  {
                    clientdatasize = batchsize;
                    break;
                  }
                bpos += batchsize;
                // bail if there's no more data on the socket.
                fd_set batcher;
                FD_ZERO(&batcher);
                FD_SET(client, &batcher);
                struct timeval tb;
                tb.tv_sec = 0; // come back immediately
                tb.tv_usec = 0;

                int bret = select (client+1, &batcher, 0, 0, &t);
                if (bret == -1)
                  { // bail on error, let next cycle deal with the error
                    clientdatasize = bpos;
                    break;
                  }
                if (FD_ISSET(client, &batcher) == FALSE)
                  { // no more data on socket.
                    clientdatasize = bpos;
                    break;
                  }
              }
#endif

            int sessionnumber = sessionid[client];
            if (clientdatasize == -1)
              {
                printf ((char *)"Error from read, socket = %d,  error = %d, %s\n", client, errno, strerror(errno));
                printf ((char *)"TCP session ending. %d Closing connection.\n", client);
                // we didn't get data, so we don't have session number
                Memory cmd = (char *)"DROP ";
                cmd.addnum(sessionnumber);
                cmd.addstr((char *)" ");
                cmd.addnum(0);

                Memory junk;
                int junkback;
                sendproxyrequest(proxy, proxyport, cmd, (char *)"", 0, junk, junkback);

                // and remove it out of the active list
                clean(client, active, sessionid);
                return;
              }
            if (clientdatasize == 0)
              {
                // we get a zero then the client hung up, tell the sshhd to drop this too
                char bb[2000];
                sprintf(bb, (char *)"Session ending %d Closing connection.", sessionnumber);
                logstringandtime (0, bb);
                Memory cmd = (char *)"DROP ";
                cmd.addnum(sessionnumber);
                cmd.addstr((char *)" ");
                cmd.addnum(0);

                Memory junk;
                int junkback;
                sendproxyrequest(proxy, proxyport, cmd, (char *)"", 0, junk, junkback);

                // and remove it out of the active list
                clean(client, active, sessionid);
                return;
              }
            if (clientdatasize > 0)
              isdataout = clientdatasize;
          }
        else
          logstringandtime(3, (char *)"Return from select wasn't our client socket.");

      }
    else
      logstringandtime(3, (char *)"ret !> 0");

    // now send it via the proxy and get what's in the queue back.

    int sessionnumber = sessionid[client];

    Memory cmd = (char *)"SWAP ";
    cmd.addnum(sessionnumber);
    cmd.addstr((char *)" ");
    cmd.addnum(clientdatasize);

    logstringandtime(4, (char *)"SWAP command");
    logstringandtime(4, cmd);

    Memory resp;
    int respsize;
    logstringandtime(2, (char *)"Calling sendproxyrequest");
    ret = sendproxyrequest(proxy, proxyport, cmd, buf, clientdatasize, resp, respsize);
    logstringandtime(2, (char *)"Returned from sendproxyrequest");

    if (ret == FALSE)
      return;

    if (respsize > 0)
      {
        // read in 200 response and headers, then get data
        Memory in(4096);
        char *s = resp;
        logstringandtime(3, (char *)"Readline proxy response");
        readlinemax(in, s, 4000);
        // this is where we'd handle the proxy renegotiate request
        // HTTP/1.1 200 OK
        char *np = in;
        np = strchr(np, ' ');
        if ((np == NULL) || (memcmp(++np, (char *)"200", 3) != 0)) // how unlike me
          {
            printf((char *)"Bad response from proxy hit: %s\n", (char *)in);
            return;
          }
        // keep reading in headers
        while (strlen(in) > 0)
          {
            logstringandtime(3, (char *)"readline header proxy response");
            int err = readlinemax(in, s, 4000);
            if (err == 0)
              {
                printf((char *)"Can't read all headers from response from proxy hit: %s\n", s);
                return;
              }
          }
        // s now points to the raw response data from sshhd
        int lsz = s - resp;
        int cdatasz = respsize - lsz;
        crappycrypt((unsigned char *)s, cdatasz, (unsigned char *)globalcfg->getpassphrase(), cdatasz);
        // get BACK response (can have a message on the end now)
        Memory line(1001);
        logstringandtime(3, (char *)"read command back");
        ret = readlinemax(line, s, 1000);
        if (ret == 0)
          return;
        char *p = strchr(line, ' ');
        if (p == NULL)
          return;
        *p = '\0';
        p++;
        char *cmd = line;
        if (strcmp(cmd, (char *)"DROP") == 0)
          {
            // the server closed their side
            printf ((char *)"Server closed connection for sessionid %s\n", p);
            // remove it out of the active list
            ret = close(client);
            if (ret != 0)
              printf((char *)"Unable to close client socket: %s\n", strerror(errno));
            // remove it out of our lists of sockets to poll.
            FD_CLR(client, active);
            return;
          }
        if (strcmp(cmd, (char *)"BACK") != 0)
          {
            printf ((char *)"Didn't get BACK response from server via web proxy. Cmd = %s\n", cmd);
            return;
          }
        int backsize = val(p);

        if (backsize > 0)
          isdatain = backsize; // mark that some data came back

        // write it all out to client socket, s is now the beginning of the raw data buffer
        int left = backsize;

        logstringandtime(3, (char *)"writing proxy response to client");

        while (left > 0)
          {
            logstringandtime(2, (char *)"before write");
            int ret2 = write (client, s+(backsize-left), left);
            logstringandtime(2, (char *)"after write");

            if (ret2 == -1) // writing 0 is okay
              {
                printf ((char *)"Error from write, error = %d, %s\n", errno, strerror(errno));
                printf ((char *)"TCP session ending. %d Closing connection.\n", client);
                Memory cmd = (char *)"DROP ";
                cmd.addnum(sessionnumber);
                cmd.addstr((char *)" ");
                cmd.addnum(0);

                Memory junk;
                int junkback;
                sendproxyrequest(proxy, proxyport, cmd, (char *)"", 0, junk, junkback);

                // and remove it out of the active list
                clean(client, active, sessionid);
                return;
              }
            left -= ret2;
          }
      }
    else
      {
        logstringandtime(3, (char *)"no response from proxy");
      }
    /* check to see if there was anything in the conversation.
     * If there was, reset the poll information to zero, otherwise bump it up */
    sprintf(bb, (char *)"Done session %d.  Data out: %d   Data in: %d", sessionnumber, isdataout, isdatain);
    logstringandtime(1, bb);

    time_t now = mytime();
    if ((isdatain > 0) || (isdataout > 0))
      {
        sprintf(bb, (char *)"Setting socket %d to zero timeout.", client);
        logstringandtime(1, bb);
        nextpolltime[client] = now-1; // don't delay before hitting this guy again
        delaymodifier[client] = 0;
      }
    else
      {
        // if there was no data either way, bump up the
        nextpolltime[client] = now + delaymodifier[client];
        if (nextpolltime[client] > (now + globalcfg->getthrottlemax()))
          {
            nextpolltime[client] = now + globalcfg->getthrottlemax();
          }
        else // make the next delay farther out
          {
            delaymodifier[client] += globalcfg->getthrottledropoff();
          }
      }
    pollcontrolsocket(proxy, proxyport, sessionid, active, highestsocket, nextpolltime, delaymodifier);
  }

time_t lastcontrolpolltime = 0;
void pollcontrolsocket(char *proxy, int proxyport, long sessionid[], fd_set *active, int &highestsocket, time_t nextpolltime[], int delaymodifier[])
  {
    /* we keep our own timer to make sure we don't call too often
     * Get the data from the control socket on the other end and if there's
     * any session ID's there, add it to the list of stuff on our end, make
     * outgoing connections for it, and voila */

    time_t now = mytime();
    if (now - lastcontrolpolltime < 15000)
      return;

    lastcontrolpolltime = now; // so when we poll we don't recursivly call pollcontrolsocket()

    // we don't have to connect to control socket, it's always there.
    Memory ocmd = (char *)"SWAP ";
    ocmd.addnum(0);
    ocmd.addstr((char *)" ");
    ocmd.addnum(0); // no client data

    Memory resp;
    int respsize;
    logstringandtime(1, (char *)"Polling control socket");
    int ret = sendproxyrequest(proxy, proxyport, ocmd, (char *)"", 0, resp, respsize); // no outbound data.
    logstringandtime(1, (char *)"Returned Polling control socket");

    if (ret == FALSE)
      return;

    if (respsize < 0)
      return; // nothing to do.

    // read in 200 response and headers, then get data
    Memory in(4096);
    char *s = resp;
    logstringandtime(3, (char *)"Readline proxy response");
    readlinemax(in, s, 4000);
    // HTTP/1.1 200 OK
    char *np = in;
    np = strchr(np, ' ');
    if ((np == NULL) || (memcmp(++np, (char *)"200", 3) != 0)) // how unlike me
      {
        printf((char *)"Bad response from proxy hit: %s\n", (char *)in);
        return;
      }
    // keep reading in headers
    while (strlen(in) > 0)
      {
        logstringandtime(3, (char *)"readline header proxy response");
        int err = readlinemax(in, s, 4000);
        if (err == 0)
          {
            printf((char *)"Can't read all headers from response from proxy hit: %s\n", s);
            return;
          }
      }
    // s now points to the raw response data from sshhd
    int lsz = s - resp;
    int cdatasz = respsize - lsz;
    crappycrypt((unsigned char *)s, cdatasz, (unsigned char *)globalcfg->getpassphrase(), cdatasz);
    // get SWAP command with NNOC data.
    Memory line(1001);
    logstringandtime(3, (char *)"read command from controlqueue");
    ret = readlinemax(line, s, 1000);
    logstringandtime(3, (char *)"line");
    logstringandtime(3, line);
    if (ret == 0)
      return;
    char *p = strchr(line, ' ');
    if (p == NULL)
      return;
    *p = '\0';
    p++;
    char *cmd = line;
    if (strcmp(cmd, (char *)"DROP") == 0)
      {
        printf ((char *)"Got DROP response from control queue request. server isn't running.\n");
        return;
      }
    if (strcmp(cmd, (char *)"BACK") != 0)
      {
        printf ((char *)"Didn't get BACK response from control queue via web proxy. Cmd = %s\n", cmd);
        return;
      }
    int backsize = val(p);
    if (backsize == 0)
      return; // nothing to do
    Memory indata(backsize + 1);
    memset(indata, 0, backsize+1);
    memcpy(indata, s, backsize); // make an NT string out of it.


    // now just read NNOC commands...
    s = indata;
    Memory entry(1001);
    ret = readlinemax(entry, s, 1000);
    while (ret > 0)
      {
        char bb[1000];
        sprintf (bb, (char *)"Line of data %s", (char *)entry);
        logstringandtime(3, bb);
        char *p = strchr(entry, ' ');
        if (p == NULL)
          return;
        if (memcmp(entry, (char *)"NNOC", 4) == 0)
          makedhhssconnection(p+1, sessionid, active, highestsocket, nextpolltime, delaymodifier); // p+1 = sessionid
        ret = readlinemax(indata, s, 1000);
      }
    return;
  }

void clean(SOCKET client, fd_set *active, long sessionid[])
  {
    FD_CLR(client, active);
    close (client);
    sessionid[client] = -1; // set flag
    return;
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
            printf ((char *)"Error resolving name: %s, error = %d, %s\n", address, h_errno, strerror(h_errno));
            return 1;
          }
        unsigned long *nentry = (unsigned long *)nam->h_addr_list[0];
        serv_addr->sin_addr.s_addr = *nentry;
      }
    return 0;
  }

struct timeval systemstartup;
void primetime(void)
  {
    // take note of the time now so we can return deltas based on program startup
    gettimeofday(&systemstartup, NULL);
    systemstartup.tv_usec = 0;
  }

time_t mytime(void)
  {
    // return the number of ms since we started
    struct timeval ernow;
    gettimeofday(&ernow, NULL);
    // this is cheating, but we happen to know that susections is a long also.
    time_t ret = ernow.tv_sec-systemstartup.tv_sec;
    ret *= 1000;
    ret += (ernow.tv_usec / 1000);
    return ret;
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
        printf((char *)"Unable to set signal handler for sigpipe: %s\n", strerror(errno));
      }
  }


void dumpdata(int connid, char *buf, int size)
  {
    /* print out in a nice format so it will be readable.
       Good old hex-n-ascii would be nice */
    int width = 16;

    printf ((char *)"Data from socket %d. %d bytes.\n", connid, size);
    char asciibuf[85];
    int lpos = 0;
    int cpos = 0;
    int pcpos = 0;
    printf ((char *)"  "); // indent new line
    while (cpos < size)
      {
        unsigned char c = *(buf+cpos);
        printf ((char *)"%02x ", c);
        c = c & 0x7f;
        if (c < width)
          c = '.';
        asciibuf[pcpos] = c;
        lpos++;
        pcpos++;
        if ((lpos % 8) == 0)
          {
            printf ((char *)" ");
            asciibuf[pcpos++] = ' ';
          }

        if (lpos >= width)
          {
            asciibuf[pcpos] = '\0';
            printf ((char *)" %s\n  ", asciibuf);
            lpos = 0;
            pcpos = 0;
          }
        cpos++;
      }
    // flush last line
    if (lpos > 0)
      {
        int diff = (width - 1) - lpos; // what's missing to indent
        while (diff >= 0)
          {
            printf((char *)"   "); // for each hex spot display
            if ((diff % 8) == 0)
              printf ((char *)" ");
            diff--;
          }
        asciibuf[pcpos] = '\0';
        printf ((char *)" %s\n", asciibuf);
      }
    printf((char *)"\n");
  }






