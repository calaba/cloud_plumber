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
#ifndef MEMORY_H
#define MEMORY_H

/* The Memory class is halfway between a simple string class and a
   block of Memory. In most cases, it will automatically be casted to
   (char *) so it acts a lot like a C string most of the time. */

/* Take care when using it. Some functions act on memory length, some
   act on string length. Complain all you want, but it's a simpler class than
   RWCString, and can also be used for simple temporary mem blocks. */

/* The rest of memm.cpp is filled with routines I've found useful over the
   years. There's a htmlescape function that doesn't belong here, but its
   here anyway. */

#ifndef __AIX__
#ifndef OS_WINDOWS
using namespace std;
#endif
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "alltypes.h"

#ifndef SOCKET
#define SOCKET int
#endif

#define CR "\r"
#define LF "\n"
#define CRLF CR LF

#define NICETEXT         " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-_':,.?|{}!@#$%^*()[]~`"
#define NICETEXTCRLF "\r\n abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-_':,.?|{}!@#$%^*()[]~`"
extern int memleak;

/* The trace thing. */
#ifdef DEBUGTRACEON

const int tracenamemaxsize = 60;
class TRACERM
  {
    // it would be nice to put a hit counter in here
    public:
      TRACERM(char *s, int count)
        {
          if (strlen(s) < tracenamemaxsize)
            strcpy(trfname, s);
          else
            { memcpy (trfname, s, tracenamemaxsize-1);
              trfname[tracenamemaxsize-1] = '\0';}
          FILE *f = fopen("/tmp/trace.out", "a");
          if (f)
            {
              fprintf (f, "enter %5d-%s\n", count, trfname);
              fclose (f);
            }
        }
      ~TRACERM(void)
        {
          FILE *f = fopen("/tmp/trace.out", "a");
          if (f)
            {
              fprintf (f, "exit  %s\n", trfname);
              fclose (f);
            }
        }

    private:
      char trfname[tracenamemaxsize];
  };

  #define TRACEM(a) TRACERM(a, 0)

#else
  #define TRACEM(a)  ;
#endif



/* handy functions. */
class Memory;

char toupper (char &c);
char *getmem (long m);
void freemem (char *m);
void freemem (void *m);
void checkmem(char *m);
char *reallocmem(char *m, ULONG newsize);
char *strdupmem(char *m);

#ifdef FASTMEMM
char *slowgetmem (long m);
void slowfreemem (char *m);
void slowfreemem (void *m);
char *getmemfromcache(long m);
void putmemincache(char *m);
#endif

int readline(char *line, FILE *f);
int readline(Memory &line, FILE *f);
int readline(char *line, char *&source);
int readlinemax(char *line, char *&source, int max);
char *stristr (char *s1, char *s2, int n = 0);
char *trim (char *s);
void ltrim(char *s);
int rtrim (char *s);
void addslash (char *s);
void addslash (Memory &s);
int strcasecmpx(char *line, char *match);
long sizereset (FILE *f);
int isipaddress(char *address);
void stringfromip(ULONG addr, Memory &addrout);
void pad(Memory &s, int sz, char padchar = ' ');
ULONG unixtime(void);
int val(const char *s);
UINT endflip(UINT s);
void losehtml(Memory &inout);
void htmlescape (Memory &dest, Memory &source);
void htmlescapebinary (Memory &dest, int &destsize, char *source, int sourcesize);
void htmlunescapebinary(char *dest, int &destsize, char *src, int srcsize);
void quotestrip(char *s);
void nicetext(char *n1, int allowcrlf = FALSE);
char *getdelim(char *st, Memory &buf, char delim);
char *skipdelim(char *st, char delim);
char *getdelimpart(char *st, Memory &buf, char delim);
void getdelimnumber(char *st, Memory &buf, char delim, int pos);
void szmax(char *s, int sz);
void charstrip(char *n1, char x);
void nonumstrip(char *n1);
void getDateString(Memory &dt);
void temppathname(Memory &name);
void tempfilename(Memory &name);
void fixsize(Memory &var, int sz);
unsigned long lookuphost(char *addr);
void sleep10 (int t);

// Thank god for Barry Lieba.
inline void tolow (char &c) { if ((c >= 'A') && (c <= 'Z')) c += ('a' - 'A'); }
inline void toupp (char &c) { if ((c >= 'a') && (c <= 'z')) c -= ('a' - 'A'); }
inline void lowdo(char *s) { while (*s) { tolow(*s); s++; } }
inline void updo(char *s)  { while (*s) { toupp(*s); s++; } }
inline int isletter(char c) { if (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'))) return TRUE; else return FALSE; }

#ifdef DEBUG
/* AIX is forgiving about strcmp (NULL, x) but linux is not, check for that here .*/

#include <string.h>
#include <strings.h>

inline int debugstrcmp(const char *x1, const char *x2)
  {
    if (x1 == NULL)
      printf ("X1 is NULL in strcmp\n");
    if (x2 == NULL)
      printf ("X2 is NULL in strcmp\n");
    return strcmp(x1, x2);
  }
inline int debugstrcasecmp(const char *x1, const char *x2)
  {
    if (x1 == NULL)
      printf ("X1 is NULL in strcasecmp\n");
    if (x2 == NULL)
      printf ("X2 is NULL in strcasecmp\n");
    return strcasecmp(x1, x2);
  }
inline int debugstrncasecmp(const char *x1, const char *x2, int num)
  {
    if (x1 == NULL)
      printf ("X1 is NULL in strncasecmp\n");
    if (x2 == NULL)
      printf ("X2 is NULL in strncasecmp\n");
    return strncasecmp(x1, x2, num);
  }

#define strcmp debugstrcmp
#define strcasecmp debugstrcasecmp
#define strncasecmp debugstrncasecmp

#endif

void printfastmemmcachestate(FILE *f);

// 12/27/02 I found a case where memory being null is still a problem
// so now it will ALWAYS be valid
/* This really should not be all inline */

/* 3/13/05 I embarked on fixing the Memory class to keep track of the
   strlen to make addchar and addstr go faster, but then I realized
   that so many people cheat (like trim) that it would never work. */


class Memory
  {
    public:

      Memory (ULONG size)
        {
          ptr = getmem(size);
        }
      Memory (const Memory &meme)
        {
          // decided to make this smart. this will make a copy of real
          // memory block, not string based.
          Memory &mem = (Memory &)meme; // deconstify
          ULONG sz;
          memcpy (&sz, ((char *)mem)-4, 4); /* get long back */
          ptr = getmem(sz);
          memcpy (ptr, (char *)mem, sz);
        }
      Memory()
        {
          ptr = getmem(1); // getmem makes byte 0 = 0
        }
      Memory(char *s, int len)
        { /* Cheap shortcuts. argh, it's starting to look like a string class */
          if (s != NULL)
            {
              ptr = getmem(len+1);
              memcpy (ptr, s, len);
              *(ptr+len) = '\0';
              return;
            }
          ptr = getmem(1);
        }
      Memory(char *s)
        { /* Cheap shortcuts. argh, it's starting to look like a string class */
          if (s != NULL)
            {
              ptr = getmem(strlen(s)+1);
              strcpy (ptr, s);
              return;
            }
          ptr = getmem(1);
        }
      Memory(char *s, char *s2)
        {
          if (s != NULL)
            {
              ptr = getmem(strlen(s)+strlen(s2)+1);
              strcpy (ptr, s);
              strcat (ptr, s2);
              return;
            }
          ptr = getmem(strlen(s2)+1); // 12/27/02 oops, missed this one too
          strcpy (ptr, s2);
        }
      ~Memory()
        {
          if (ptr != NULL)
            freemem(ptr);
          ptr = NULL;
        }
      /* This is used when you want to take ownership but have to pass
         a pointer to a string to a function */
      char **consume(void) /* as in it now owns the memory */
        { // don't use this if you don't have to. It's not pretty.
          if (ptr != NULL)
            freemem (ptr);
          ptr = getmem(1); // wipe it clean but keep it valid
          return &ptr; /* So it can be set externally */
        }

      /* This is just flat out stealing the memory, must have been getmemed */
      void steal(char *s) /* as in it now owns the memory */
        {
          if (ptr != NULL)
            freemem (ptr);
          ptr = s;
        }
      char *victim(void)
        { // when you steal from a Memory object, you must denote the victim.
          char *ret = ptr;
          ptr = getmem(1);
          return ret;
        }
      /* I've got a big bad conundrum here.
         We shouldn't allow returning NULL. AIX doesn't care, you can do
         strlen(NULL) and it's okay. Linux is not. Hooray for linux.
         So we generate a "" for NULLs. Let's hope nobody tried to add to it. */
      /* as of 12/27/02 we guarantee that ptr can never be null, so let's save
         some time here and not do the check anymore
         operator char*() { if (ptr == NULL) ptr = getmem(1); return ptr; }  */
      operator char*() { return ptr; }


      Memory &operator=(char *s)
        {
          char *old = ptr;
          ptr = NULL;
          if (s != NULL)
            {
              ptr = getmem(strlen(s)+1);
              strcpy (ptr, s);
            }
          else
            ptr = getmem(1); // this can't happen, but just in case ptr ever comes in null
          if (old != NULL)
            freemem (old);
          return *this;
        }
      Memory &operator=(const Memory &se)
        {
          Memory &s = (Memory &)se; // deconstify
          char *olds = (char *)s; // force in case we're copying from ourself
          // now we know s (and possibly our ptr) is not null (oldway: because of the deref, newway 12/27/02 because we assure it)
          char *old = ptr;
          ULONG sz;
          memcpy (&sz, olds-4, 4); /* get long back */
          char *ptr2 = getmem(sz);
          memcpy (ptr2, olds, sz);
          ptr = ptr2; // do this after copy, so we don't write on ourselves
          if (old != NULL) // well, just in case, check it again.
            freemem (old);
          return *this;
        }

      void copy(const Memory &se)
        {
          Memory &s = (Memory &)se; // deconstify
          char *olds = (char *)s; // force in case we're copying from ourself
          char *old = ptr;
          ULONG sz;
          memcpy (&sz, olds-4, 4); /* get long back */
          char *ptr2 = getmem(sz);
          memcpy (ptr2, olds, sz);
          ptr = ptr2; // do this after copy, so we don't write on ourselves
          if (old != NULL) // well, just in case, check it again.
            freemem (old);
        }

      void addchar(char c)
        { // someday we'll buffer this
          if (ptr == NULL)
            ptr = getmem(1);
          ULONG len = strlen(ptr); // it would be really nice if we could get rid of this...
          // efficient expert 3/17/01
          ULONG sz = 0;
          memcpy (&sz, ((char *)ptr)-4, 4); /* get long back */
          if (len+2 > sz)
            ptr = reallocmem(ptr, (len+2)*2);
          *(ptr+len) = c;
          *(ptr+len+1) = '\0';
        }
      void addstr(char *s)
        {
          if (s == NULL)
            return;
          //if (ptr == NULL)  can't happen 12/27/01
          //  ptr = getmem(1);
          // efficient expert 3/17/01
          ULONG sz = 0;
          memcpy (&sz, ((char *)ptr)-4, 4); /* get long back */
          int slen = strlen(ptr);
          ULONG need = slen + strlen(s) + 1; // if we get rid of strlen in addchar we can do it here too...
          if (need > sz)
            ptr = reallocmem(ptr, need*2); // if we're doing one, we're probably going to do more
          strcat (ptr+slen, s);
        }
      void addnum(UINT i)
        {
          char s[40];
          sprintf(s, "%d", i);
          // efficient expert 3/17/01
          ULONG sz = 0;
          memcpy (&sz, ((char *)ptr)-4, 4); /* get long back */
          int slen = strlen(ptr);
          ULONG need = slen + strlen(s) + 1; // remove strlen
          if (need > sz)
            ptr = reallocmem(ptr, need*2);
          strcat (ptr+slen, s);
        }

      ULONG getsize(void)
        {
          ULONG sz;
          memcpy (&sz, ((char *)ptr)-4, 4); /* get long back */
          return sz;
        }

      /* some versions of this did a NULL
         check but realloc (null, sz) is valid */
      void resize(UINT sz)
        { // so external users can force resize the buffer.
          ptr = reallocmem(ptr, sz);
        }

    private:
      char *ptr;
  };

#define T_FROM     "From: " // 822 header field
#define T_SUBJECT "Subject: "

// 1/8/03. Don't know why I called it addr, but I HAVE to change it
class emailaddr
  {
    /* Here's a perfect example of lack of reuse. We have a very good
       address class, but oop! it uses roguewave and we can't have that
       so we have to reinvent the wheel to be efficient */

    public:
      emailaddr (void) {} // we can assign to this
      emailaddr (char *s);
      ~emailaddr (void);

      emailaddr &operator=(emailaddr &s)
        {
          friendly = s.getfriendly();
          email = s.getemail();
          proper = s.getproperemail();
          pretty = s.getpretty();
          fullpretty = s.getfullpretty();
          name = s.getname();
          return *this;
        }
      char *getfriendly(void)    { return friendly; } // Stu
      char *getemail(void)       { return email; } // nixo@exprodigy.net
      char *getproperemail(void) { return proper; } // <nixo@exprodigy.net>
      char *getpretty(void)      { return pretty; } // best of above two
      char *getfullpretty(void)  { return fullpretty; } // best of pretty and email
      char *getname(void)        { return name; } // nixo
      char *getdomain(void)
        {
          char *ps = strchr(email, '@');
          if (ps == NULL)
            return email;
          return ps+1;
        }
    private:
      Memory friendly;
      Memory email;
      Memory proper;
      Memory pretty;
      Memory fullpretty;
      Memory name;
  };

class filedeleter
  {
    public:
      filedeleter(char *fname)
        {
          name = fname;
        }
      ~filedeleter(void)
        { // I LOVE destructors
          unlink(name);
        }
    private:
      Memory name;
  };

class filecloser
  {
    public:
      filecloser(FILE *f)
        {
          file = f;
        }
      int close(void)
        {
          int ret;
          if (file != NULL)
            ret = fclose(file);
          file = NULL;
          return ret;
        }
      ~filecloser(void)
        { // I LOVE destructors
          if (file != NULL)
            fclose(file);
        }
    private:
      FILE *file;
  };

char *gettempfilename(void);
void logstringandtime(int debug, char *s);
void crappycrypt(unsigned char *data, int datasize, unsigned char *passphrase, int seed);


#endif

