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

#ifndef SSHH_H
#define SSHH_H

#define SOCKET int

int getaddr(char *address, struct sockaddr_in *serv_addr);
void clean(SOCKET incoming, fd_set *active, long sessionid[]);
void readwrite(SOCKET incoming, fd_set *active, long sessionid[]);
void getcredentials(Memory &c);

int makesshhdconnection(SOCKET client, fd_set *active, long sessionid[], char *proxy, int proxyport, int &highestsocket);
int main (int num, char *opts[]);
void fixsigpipe(void);
void dumpdata(int connid, char *buf, int size);
void pollthissocket(char *proxy, int proxyport, SOCKET client, fd_set *active, long sessionid[], time_t nextpolltime[], int delaymodifier[], int &highestsocket);
void pollallsockets(char *proxy, int proxyport, int &highestsocket, fd_set *active, long sessionid[], time_t nextpolltime[], int delaymodifier[]);
void pollcontrolsocket(char *proxy, int proxyport, long sessionid[], fd_set *active, int &highestsocket, time_t nextpolltime[], int delaymodifier[]);
void makedhhssconnection(char *sessionidstr, long sessionid[], fd_set *active, int &highestsocket, time_t nextpolltime[], int delaymodifier[]);
void primetime(void);
time_t mytime(void);



#endif




