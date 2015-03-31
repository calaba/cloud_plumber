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
#ifndef CFG_H
#define CFG_H

#include "memm.h"

#define TXT_PROXYSERVER "proxyserver:"
#define TXT_PROXYPORT "proxyport:"
#define TXT_LISTENSERVER "listenserver:"
#define TXT_LISTENPORT "listenport:"
#define TXT_ENDCALLER "endcaller:"
#define TXT_USERNAME "username:"
#define TXT_PASSWORD "password:"
#define TXT_DESTSERVER "destserver:"
#define TXT_DESTPORT "destport:"

#define TXT_THROTTLEMAX "throttlemax:"
#define TXT_THROTTLEDROPOFF "throttledropoff:"
#define TXT_THROTTLEDROPOFFACCELERATION "throttledropoffacceleration:"
#define TXT_NETSILSERVER "netsilserver:"
#define TXT_NETSILPORT "netsilport:"

#define TXT_TSEDSERVER "tsedserver:"
#define TXT_TSEDPORT "tsedport:"

#define TXT_SERVERTIMEOUT "servertimeout:"

#define TXT_PASSPHRASE "passphrase:"

#define TXT_SECRET "secret:"

class config
  {
    public:
      config(const char *fname)
        {
          proxyserver = NULL;
          listenserver = NULL;
          listenport = 0;
          proxyport = 0;
          endcaller = NULL;
          username = NULL;
          password = NULL;
          destserver = NULL;
          destport = 0;
          throttlemax = 0;
          throttledropoff = 0;
          throttledropoffacceleration = 0;
          netsilserver = NULL;
          netsilport = 0;
          tsedserver = NULL;
          tsedport = 0;
          servertimeout = 0;
          passphrase = NULL;
          secret = NULL;

          FILE *f = fopen(fname, "r");
          if (f == NULL)
            {
              printf ("Can't open config file: %s\n", fname);
              proxyserver = (char *)malloc(2);
              *proxyserver = '\0';
              listenserver = (char *)malloc(2);
              *listenserver = '\0';
              endcaller = (char *)malloc(2);
              *endcaller = '\0';
              proxyport = 0;
              listenport = 0;
              username = (char *)malloc(2);
              *username = '\0';
              password = (char *)malloc(2);
              *password = '\0';
              destserver = (char *)malloc(2);
              *destserver = '\0';
              destport = 0;
              throttlemax = 0;
              throttledropoff = 0;
              throttledropoffacceleration = 0;
              netsilserver = (char *)malloc(2);
              *netsilserver = '\0';
              tsedserver = (char *)malloc(2);
              *tsedserver = '\0';
              tsedport = 0;
              servertimeout = 0;
              passphrase = (char *)malloc(2);
              *passphrase = '\0';
              secret = (char *)malloc(2);
              *secret = '\0';
              return;
            }
          while (1)
            {
              char buf[1000];
              if (readline(buf, f) == 0)
                break;
              loadentry(buf, TXT_PROXYSERVER, proxyserver);
              if (strncasecmp(buf, TXT_PROXYPORT, strlen(TXT_PROXYPORT)) == 0)
                proxyport = atoi(buf+strlen(TXT_PROXYPORT));

              loadentry(buf, TXT_LISTENSERVER, listenserver);
              if (strncasecmp(buf, TXT_LISTENPORT, strlen(TXT_LISTENPORT)) == 0)
                listenport = atoi(buf+strlen(TXT_LISTENPORT));

              loadentry(buf, TXT_ENDCALLER, endcaller);
              loadentry(buf, TXT_USERNAME, username);
              loadentry(buf, TXT_PASSWORD, password);
              loadentry(buf, TXT_DESTSERVER, destserver);
              if (strncasecmp(buf, TXT_DESTPORT, strlen(TXT_DESTPORT)) == 0)
                destport = atoi(buf+strlen(TXT_DESTPORT));
              if (strncasecmp(buf, TXT_THROTTLEMAX, strlen(TXT_THROTTLEMAX)) == 0)
                throttlemax = atoi(buf+strlen(TXT_THROTTLEMAX));
              if (strncasecmp(buf, TXT_THROTTLEDROPOFF, strlen(TXT_THROTTLEDROPOFF)) == 0)
                throttledropoff = atoi(buf+strlen(TXT_THROTTLEDROPOFF));
              if (strncasecmp(buf, TXT_THROTTLEDROPOFFACCELERATION, strlen(TXT_THROTTLEDROPOFFACCELERATION)) == 0)
                throttledropoffacceleration = atoi(buf+strlen(TXT_THROTTLEDROPOFFACCELERATION));

              loadentry(buf, TXT_NETSILSERVER, netsilserver);
              if (strncasecmp(buf, TXT_NETSILPORT, strlen(TXT_NETSILPORT)) == 0)
                netsilport = atoi(buf+strlen(TXT_NETSILPORT));

              loadentry(buf, TXT_TSEDSERVER, tsedserver);
              if (strncasecmp(buf, TXT_TSEDPORT, strlen(TXT_TSEDPORT)) == 0)
                tsedport = atoi(buf+strlen(TXT_TSEDPORT));

              if (strncasecmp(buf, TXT_SERVERTIMEOUT, strlen(TXT_SERVERTIMEOUT)) == 0)
                servertimeout = atol(buf+strlen(TXT_SERVERTIMEOUT));

              loadentry(buf, TXT_PASSPHRASE, (char *&)passphrase);
              loadentry(buf, TXT_SECRET, (char *&)secret);
            }
          fclose (f);
        }
      ~config (void)
        {
          if (listenserver != NULL)
            free(listenserver);
          if (proxyserver != NULL)
            free(proxyserver);
          if (endcaller != NULL)
            free(endcaller);
          if (username != NULL)
            free(username);
          if (password != NULL)
            free(password);
          if (destserver != NULL)
            free(destserver);
          if (netsilserver != NULL)
            free(netsilserver);
          if (tsedserver != NULL)
            free(tsedserver);
          if (passphrase != NULL)
            free(passphrase);
          if (secret != NULL)
            free(secret);

        }
      char *getproxyserver(void) { return proxyserver; }
      int getproxyport(void) { return proxyport; }
      char *getlistenserver(void) { return listenserver; }
      int getlistenport(void) { return listenport; }
      char *getendcaller(void) { return endcaller; }
      char *getusername(void) { return username; }
      char *getpassword(void) { return password; }
      char *getdestserver(void) { return destserver; }
      int getdestport(void) { return destport; }
      int getthrottlemax(void) { return throttlemax; }
      int getthrottledropoff(void) { return throttledropoff; }
      int getthrottledropoffacceleration(void) { return throttledropoffacceleration; }
      char *getnetsilserver(void) { return netsilserver; }
      int getnetsilport(void) { return netsilport; }
      char *gettsedserver(void) { return tsedserver; }
      int gettsedport(void) { return tsedport; }
      time_t getservertimeout(void) { return servertimeout; }
      char *getpassphrase(void) { return passphrase; }
      char *getsecret(void) { return secret; }

      void loadentry(char *buf, const char *fieldname, char *&data)
        {
          if (strncasecmp(buf, fieldname, strlen(fieldname)) == 0)
            {
              data = (char *)malloc(strlen(buf+strlen(fieldname)) + 5);
              strcpy (data, buf+strlen(fieldname));
              trim(data);
            }
        }

      int readline(char *line, FILE *f)
        {
          int c;
          int count;
          count = 0;
          while (1)
            {
              c = fgetc (f);
              if (c == EOF)
                break;

              count++;
              if (c == '\n')
                break;
              if (c == '\r')
                {
                  c = fgetc (f); /* get the assumed \n */
                  break;
                }
              *(line++) = (char)c;
            }
          *line = '\0'; /* n.t. */
          return count;
        }

    private:
      char *proxyserver;
      int proxyport;
      char *listenserver;
      int listenport;
      char *endcaller;
      char *username;
      char *password;
      char *destserver;
      int destport;
      int throttlemax;
      int throttledropoff;
      int throttledropoffacceleration;
      char *netsilserver;
      int netsilport;
      char *tsedserver;
      int tsedport;
      time_t servertimeout;
      char *passphrase;
      char *secret;
  };


#endif
