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
#ifndef INCL_CONNECTOR_H
#define INCL_CONNECTOR_H

/* Instructions:

   Connector (char *host, int port, int timeout);
     Given a host and port (host can be string or dotted ip address)
     this object will create a blocking TCP connection to it. 
     If errorstate() is not 0 after construction, then it is the 
     errno of the error.
     The object will close the socket upon descruction, unless it's been
     a victim during its lifetime.
     The timeout value is number of seconds to wait for the connect.
     If you set it to zero, connector will make a blocking socket and 
     wait forever, or for the OS to give up. If you specify > 0, the
     socket will be made nonblocking and errorstate will be ETIMEDOUT
     if the connect took longer than your timeout value.
   
   int victim(void);
     Give up ownership of socket. This object owns the connection
     and will close the socket upon destruction unless this function
     is called. It can be called repeatedly, but after the first time
     it will no longer claim ownership of the socket and will not close
     it upon destruction. See example below. (For use with socketio)
     
   int errorstate(void);
     Return state (errno) of the connection attempt.

   Example:
     int main (int num, char *opts[])
       {
         Connector con("sturgeon.fishy.net", 4090, 0);
         // calling con.victim() the first time is what makes con give up ownership
         socketio sco (con.victim(), con.victim()); // takes ownership of the socket/FD
         Memory str;
         sco.getstr(str);
                    
         cout << str << endl;
         return 0; // this will make socketio's destructor close the connection
       }                             
*/


class Connector
  {
    public:
      Connector (char *host, int port, int timeout);
      ~Connector ();
      int victim(void); // give up ownership of socket
      int errorstate(void) { return err; }
        
    private:
      int fd;
      int err;
      int saver;
  };

#endif

