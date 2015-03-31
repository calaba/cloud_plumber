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


/* This is the little cgi, that takes the request and connects to the sshhd
 * to send it data and get response back.
 */

#include <stdio.h>
#include "Q/memm.h"
#include "Q/socketio.h"
#include "Q/connector.h"
#include "sshhdefs.h"

int droperror(char *msg);
void dumpdata(int connid, char *buf, int size);
void logline(const char *s);

unsigned char *passphrase = (unsigned char *)"";

int main(int num, char *opts[])
  {
    /* read in all the data, connect to sshd send the command get the response
     * and spoo it back out */

    // get the two params of where to connect to
    // ./sshhcgi 127.0.0.1 7776 "this is my passphrase"
    char *host = opts[1];
    int port = val(opts[2]);
    passphrase = (unsigned char *)opts[3];

    int blocksize = 65536;
    char *cl = getenv("CONTENT_LENGTH");
    if (cl == NULL)
      cl = (char *)"-1"; // read to EOF
    int clen = val(cl);
    if (clen != -1)
      blocksize = clen;

    // now read all the data we can/need to.
    int pos = 0;
    int bufsize = 0;
    int capacity = blocksize;
    Memory bufdata(blocksize);
    while (1)
      {
        int ret = fread(bufdata+pos, 1, blocksize, stdin);
        if (ret == EOF)
          break;
        if (ret == 0)
          break;
        bufsize += ret;

        if (clen != -1)
          if (bufsize >= clen)
            break;

        capacity += blocksize;
        bufdata.resize(capacity);
        pos += blocksize;
      }
    Memory data(capacity);
    int datasize = 0;
    htmlunescapebinary(data, datasize, bufdata, bufsize);
    char *dataarea = data;
    crappycrypt((unsigned char *)dataarea, datasize, (unsigned char *)passphrase, datasize);

    Memory datain;
    int datainlen = 0;

    { // scope connection
    // send all the raw data to the sshhd daemon
    Connector con(host, port, 0); // does not own socket., 10 sec timeout
    if (con.errorstate() != 0)
      {
        char msg[1000];
        sprintf (msg, "Unable to connect to server: %s:%d %s\n", host, port, strerror(con.errorstate()));
        return droperror(msg);
      }
    socketio sco(con.victim(), con.victim(), 10); // this scope owns the connection
    int ret = sco.senddata(data, datasize);
    if (ret == FALSE)
      {
        char msg[1000];
        sprintf (msg, "Unable to send data to server: %s:%d %s\n", host, port, strerror(sco.errorstate()));
        return droperror(msg);
      }
    /* In order for the daemon to be able to read until end, we must close our outgoing
       half of the socket. */
    shutdown(sco.getfd(), SHUT_WR);

    // get response. We must take the entire response from sshhd,
    ret = sco.getalldata(datain, datainlen);
    if (ret == FALSE)
      {
        char msg[1000];
        sprintf (msg, "Unable to get data from server: %s:%d %s\n", host, port, strerror(sco.errorstate()));
        return droperror(msg);
      }
    } // end sshd connection

    dataarea = datain;
    crappycrypt((unsigned char *)dataarea, datainlen, passphrase, datainlen);

    // send the cgi response:
    fprintf(stdout, "Content-Type: application/octet-stream\r\n");
    fprintf(stdout, "Content-Length: %d\r\n\r\n", datainlen);
    fwrite(datain, 1, datainlen, stdout);
    fflush(stdout);
    return 0;
  }

int droperror(char *msg)
  {
    fprintf(stdout, "Content-Type: application/octet-stream\r\n\r\n");
    char buf[25+strlen(msg)];
    sprintf(buf, "DROP 0 %s\r\n", msg);
    int sz = strlen(buf);
    crappycrypt((unsigned char *)buf, sz, passphrase, sz);
    fwrite(buf, 1, sz, stdout);
    fflush(stdout);
    return 1;
  }

void logline(const char *s)
  {
    FILE *f = fopen("/tmp/sshhcgi.log", "a");
    if (f == NULL)
      {
        printf ("Unable to open log file: %s\n", strerror(errno));
        return;
      }
    fprintf (f, "%s\n", s);
    fclose (f);
  }

void dumpdata(int connid, char *buf, int size)
  {
    /* print out in a nice format so it will be readable.
       Good old hex-n-ascii would be nice */

    FILE *f = fopen("/tmp/sshhcgi.log", "a");
    if (f == NULL)
      {
        printf ("Unable to open log file: %s\n", strerror(errno));
        return;
      }
    fprintf (f, "Data from socket %d. %d bytes.\n", connid, size);
    char asciibuf[85];
    int lpos = 0;
    int cpos = 0;
    int pcpos = 0;
    fprintf (f, "  "); // indent new line
    while (cpos < size)
      {
        unsigned char c = *(buf+cpos);
        fprintf(f, "%02x ", c);
        c = c & 0x7f;
        if ((c < 32) || (c > 126))
          c = '.';
        asciibuf[pcpos] = c;
        lpos++;
        pcpos++;
        if ((lpos % 8) == 0)
          {
            fprintf(f, " ");
            asciibuf[pcpos++] = ' ';
          }

        if (lpos >= 16)
          {
            asciibuf[pcpos] = '\0';
            fprintf(f, " %s\n  ", asciibuf);
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
            fprintf(f, "   "); // for each hex spot display
            if ((diff % 8) == 0)
              fprintf(f, " ");
            diff--;
          }
        asciibuf[pcpos] = '\0';
        fprintf(f, " %s\n", asciibuf);
      }
    fprintf(f, "\n");
    fclose (f);
  }






