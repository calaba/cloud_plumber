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
#ifndef INCL_ALLTYPES_H
#define INCL_ALLTYPES_H

/* Generic global defines */

#define FLAG char // small as possible

/* planning ahead for 64 bit problems. */

#ifdef _BIT64_  /* what xlC calls -q64 */
#else  /* -q32 */

#define INT8 char
#define INT16 short

#define UINT8 unsigned char
#define UINT16 unsigned short

#define MAKESIMPLETYPES 
#ifdef OS_WINDOWS
  #undef MAKESIMPLETYPES
#endif

#ifdef MAKESIMPLETYPES
#define INT32 int
#define UINT32 unsigned int

#define LONG32 long
#define ULONG32 unsigned long

#define ULONG ULONG32
#define UINT UINT32

#define INT64 long long
#define UINT64 unsigned long long

#define BOOL char 

/* for non-trits */
#define TRUE 1
#define FALSE 0

#endif /* windows */

#endif


/* For anybody who thinks that computers answering questions in binary, let me introduce you
   to the concept of 'failure'. 
   "Hey dude, can you tell me if there's any beer in the fridge?"
   Now here, a computer would be forced to reply with a yes or no, but in reality
   maybe somebody stole the fridge 6 months ago, and the computer can not query
   the fridge because it doesn't know where it is. For this we need to be able to
   respond, I don't know. Thus the trit. */
   
typedef enum {True, False, Unknown} TRIT; /* mixed case only because true and TRUE are taken */



#endif
