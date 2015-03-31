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
#include <stdlib.h>
#include <time.h>
#include <strings.h>
#include <sys/time.h>
#include <netdb.h>
#include <ctype.h>

#include "memm.h"

int memleak;

#ifdef FASTMEMM

/* See below for description. */
#define FASTMEMMPILESIZE 200 /* What's the delta size between sibling piles */
#define FASTMEMMMAXTOCACHE 20480 /* Any single allocation over this won't be cached. */
#define FASTMEMMPILES (FASTMEMMMAXTOCACHE / FASTMEMMPILESIZE) // this will constify
#define FASTMEMMCACHEDEPTH 100  // setting this to 1 is least efficient

char **FMpiles[FASTMEMMPILES];
int FMpileheight[FASTMEMMPILES];
int FMpilealloc[FASTMEMMPILES];
/* 12/27/02 it would seem that some memory objects don't get freed until after the
   fastmemcleaner has cleaned, so we have a flag to say nobody home anymore. */
int fastmemmexists = 0;

class FASTMEMMCLEANER
  {
    public:

    FASTMEMMCLEANER(void)
      {
        // init the arrays. Guarentee there's always something there.
        int lp;
        for (lp = 0; lp < FASTMEMMPILES; lp++)
          {
            FMpiles[lp] = (char **)slowgetmem(2*sizeof(char *));
            FMpileheight[lp] = 0;
            FMpilealloc[lp] = 2; // how much space there is allocated in this pile
          }
        fastmemmexists = 1;
      }
    ~FASTMEMMCLEANER(void)
      {
        int lp;
        for (lp = 0; lp < FASTMEMMPILES; lp++)
          {
            int lz;
            /* 12/29/02 There was a bug here, the +lz was supposed to move
               by a pointer, but in linux anyway, it was only moving
               by a byte. I guess once again AIX is very forgiving */
            for (lz = 0; lz < FMpileheight[lp]; lz++)
              {
                char **row = FMpiles[lp]; // get the base of this pile
                char *tofree = row[lz]; // get the memory to delete
                slowfreemem(tofree);
              }
            slowfreemem ((char *)FMpiles[lp]); // clean the pile storage area
          }
        fastmemmexists = 0;
      }
  };

FASTMEMMCLEANER FastMemmCleanerInstance; // this will cause clean on exit


char *getmem(long m)
  {
    if (fastmemmexists)
      return getmemfromcache(m);
    return slowgetmem(m);
  }

void freemem (char *m)
  {
    if (fastmemmexists)
      putmemincache(m);
    else
      slowfreemem(m);
  }

void freemem (void *m)
  {
    if (fastmemmexists)
      putmemincache((char *)m);
    else
      slowfreemem(m);
  }

#define getmem slowgetmem
#define freemem slowfreemem
#endif // FASTMEMM

/* Not that it mattered too much, but whoopsie.
   I was mixing new/delete with realloc. What was I thinking... */

char *getmem (long m)
  {
    /* must zero out data */
    char *z;
    char *zf;

    z = (char *)malloc(m+8);  /* make room for size and "STU." */
    zf = z + m+6;
    memleak++;

#ifdef OLDWAY
    while ((--zf) != z)
      *zf = 0;
    *z = 0;
#else
    /* Stupid me. This is where we waste a lot of time. Probably we
       can get away with just setting the zero byte to zero. */
    *(z+4) = '\0';
#endif

    /* store check info */
    memcpy (z, &m, 4); /* copy the long in */
    memcpy (z+m+4, "Stu.", 4); /* my over run check bytes */

    return z + 4;
  }

void freemem (void *m)
  {
    freemem ((char *)m);
  }

void freemem (char *m)
  {
    /* check to see if we overran */
    long sz;

    memcpy (&sz, m-4, 4); /* get long back */

#ifndef FASTMEMM // might as well speed everything up.
    if (memcmp (m + sz, "Stu.", 4) != 0)
      {
        printf("[Memory Overrun Error.]\n");
      }
#endif
    free (m - 4); // nobody tells me these things
    memleak--;
  }


void checkmem(char *m)
  {
    /* looks for overrun errors */
    long sz;

    memcpy (&sz, m-4, 4); /* get long back */

    if (memcmp (m + sz, "Stu.", 4) != 0)
      printf("[Memory Overrun Error.]\n");
  }

char *reallocmem(char *m, ULONG newsize)
  {
    /* check to see if we overran */
    ULONG sz;
    char *ret;

    memcpy (&sz, m-4, 4); /* get long back */

#ifndef FASTMEMM
    if (memcmp (m + sz, "Stu.", 4) != 0)
      printf ("[Memory Overrun Error.]\n");
#endif
    /* just like malloc */
    #ifdef FASTMEMM
    /* The problem here, is we can not allow any memory object
       to be smaller than the max size of the first pile size, because
       if we pull something out of it, it has to be at least 40-80
       not 0-40 */
    if (newsize < FASTMEMMPILESIZE)
      newsize = FASTMEMMPILESIZE;
    #endif
    if (newsize <= sz) // save me some time and effort
      return m;
    ret = (char *)realloc (m-4, newsize+8);
    memcpy (ret, &newsize, 4); /* copy the long in */
    memcpy (ret + newsize + 4, "Stu.", 4); /* my over run check bytes */

    return (ret + 4);
  }

// I think this is fastmemsafe 1/19/05
char *strdupmem(char *m)
  {
    ULONG sz;

    memcpy (&sz, m-4, 4); /* get long back */

    if (memcmp (m + sz, "Stu.", 4) != 0)
      printf ("[Memory Overrun Error.]\n");

    char *nn = getmem(sz);
    memcpy(nn, m, sz);
    return nn;
  }

ULONG unixtime(void)
  {
    return time(0);
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

int readline(Memory &mline, FILE *f)
  {
    /* I really should have done this years ago, but now there's a maild
       core dump to justify it.
       This readline will load line with a line of text from f.
       The difference is that it will size the buffer as neccesary. */

    int c;
    UINT count = 0;

    // check the buffer size
    ULONG lsize;
    memcpy (&lsize, ((char *)mline)-4, 4); /* get long back. We're cheating, we know how Memory works. */
    // we coulse use Memory.addchar, but it would go too slow, we know to jump the buffer quickly
    // and not do a zillion small incremental memory reallocs
    char *line = mline; // movable positional pointer
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
        // check the buffer size
        if (count >= lsize)
          {
            UINT pos = line - mline; // take note of where we are in case the ptr changes
            ULONG need = lsize * 2; // double each time.
            mline.resize(need);
            memcpy (&lsize, ((char *)mline)-4, 4); /* get new size back. */
            line = mline + pos; // put the ptr back where it was in (possibly) new buffer
          }
        *(line++) = (char)c;

      }
    *line = '\0'; /* n.t. */
    return count;
  }

int readline(char *line, char *&source)
  {
    // we move the source pointer for next time
    char c;
    int count;
    count = 0;
    *line = '\0'; // start them off right.
    if (source == NULL)
      return 0; // I don't think so

    while (*source != '\0')
      {
        c = *source;
        source++; // it's a reference, get it?
        if (c == '\0')
          break;

        count++;
        if (c == '\n')
          break;
        if (c == '\r')
          {
            source++; /* skip the assumed \n */
            break;
          }
        *(line++) = c;
      }
    *line = '\0'; /* n.t. */
    return count;
  }

int readlinemax(char *line, char *&source, int max)
  {
    // we move the source pointer for next time
    char c;
    int count;
    count = 0;
    *line = '\0'; // start them off right.
    if (source == NULL)
      return 0; // I don't think so
    int didsomething = FALSE;

    while (*source != '\0')
      {
        didsomething = TRUE;
        c = *source;
        source++; // it's a reference, get it?
        if (c == '\0')
          break;

        count++;
        if (count > max)
          {
            *(line-1) = '\0';
            return 0;
          }
        if (c == '\n')
          break;
        if (c == '\r')
          {
            source++; /* skip the assumed \n */
            break;
          }
        *(line++) = c;
      }
    if (didsomething != FALSE)
      *line = '\0'; /* n.t. */
    return count;
  }


void quotestrip(char *s)
  {
    /* Remove quotes from string */
    char *st;
    char *en;

    st = s;
    en = s;
    while (*en != '\0')
      {
        *st = *en;
        if (*st != '\"')
          st++;
        en++;
      }
    *st = '\0';
  }

int strcasecmpx(char *line, char *match)
  {
    // compare line to match size of match
    Memory l(line);
    if (strlen(l) > strlen(match))
      *(l + strlen(match)) = '\0'; // make 'em same size
    return strcasecmp (l, match);
  }

void nonumstrip(char *n1)
  {
    char *from = n1;
    char *to = from;
    while(*from != '\0')
      {
        if ((*from >= '0') && (*from <= '9'))
          *to++ = *from;
        from++;
      }
    *to = '\0';
  }

void charstrip(char *n1, char x)
  {
    char *from = n1;
    char *to = from;
    while(*from != '\0')
      {
        if (*from != x)
          *to++ = *from;
        from++;
      }
    *to = '\0';
  }

/* for stripping out things that could screw up a text line (like <> in html), we don't take
   <> or " for example */
/* Do not allow / because it is the foldername hierarchy delimeter (userfolder.hpp) */
/* Removed & and ; so they can't type &lt; */
void nicetext(char *n1, int allowcrlf)
  {
    char *from = n1;
    char *to = from;
    char *checker = NICETEXT;
    if (allowcrlf)
      checker = NICETEXTCRLF;

    while(*from != '\0')
      {
        if (strchr (checker, *from) != NULL)
          *to++ = *from;
        from++;
      }
    *to = '\0';
  }

char *trim (char *s)
  {
    /* 1/27/03, amazing stupid bug, don't do rtrim first or " hello" will kill it */
    /* Turns out I was being dumb, the version of rtrim I thought I was using
       was wrong, this one works, so rtrim first is quicker. */
    rtrim (s);
    ltrim (s);
    return s;
  }

void ltrim(char *s)
  {
    char *p;

    p = s;
    while (((*p == '\t') || (*p == ' ')) && (*p != '\0'))
      p++;
    if (p == s) return; /* nothing to do */
    if (*p == '\0') /* string is all empty */
      {
        *s = '\0'; /* kill it */
        return;
      }
    /* copy the rest of the string from p to s */
    while (*p != '\0')
      *(s++) = *(p++);
    *s = '\0';
  }

int rtrim (char *s)
  {
    /* given a null terminated string, we will lose all the spaces
       at the end */
    char *p;

    p = s + strlen(s) - 1 ; /* go to end before \0 */
    while (((*p == '\t') || (*p == ' ')) && (p > s))
      p--;
    *(++p) = '\0'; /* kill it */
    if ((--p == s) && ((*p == '\t') || (*p == ' '))) /* whole string is 1 space */
      {
        *p = '\0'; /* null the whole string */
        return 0;
      }
    return (p-s+1); /* difference is lstrlen */
  }

/* Other sometimes handy routines. */

#ifndef SLASH // from portability
  #define SLASH '/' // unix default
  #define SZSLASH "/"
#endif

void addslash (char *s)
  {
    if (*(s + strlen(s) - 1) != SLASH)
      strcat (s, SZSLASH);
  }

void addslash (Memory &s)
  {
    if (*(s + strlen(s) - 1) != SLASH)
      s.addchar(SLASH);
  }

void pad(Memory &s, int size, char padchar)
  {
    int add = size - strlen(s);
    if (add < 1)
      return;
    Memory addstr(add+1); // room for nt
    memset (addstr, padchar, add);
    *(addstr+add) = '\0';
    s.addstr(addstr); // pad with spaces
  }

char *getdelim(char *st, Memory &buf, char delim)
  {
    st = skipdelim(st, delim);
    return getdelimpart(st, buf, delim);
  }

char *getdelimpart(char *st, Memory &buf, char delim)
  {
    /* I'm guessing this is like strtok */
    Memory bufn(100);
    buf = bufn; // this will make the addchars go quicker (up to 100 anyway)
    if (st == NULL)
      return NULL;
    while (*st != delim)
      {
        if (*st == '\0')
          return NULL; // no more
        buf.addchar(*(st++));
      }
    st++; // skip delim
    return st;
  }

char *skipdelim(char *st, char delim)
  {
    /* skip till we're not on this delimeter */
    if (st == NULL)
      return NULL;
    while (*st == delim)
      st++;
    return st;
  }

void getdelimnumber(char *st, Memory &buf, char delim, int pos)
  {
    int lp;
    Memory junk;
    for (lp = 0; lp < pos; lp++)
      st = getdelimpart(st, junk, delim);
    getdelimpart(st, buf, delim); // get the one they want
  }

void szmax(char *s, UINT sz)
  {
    if (strlen(s) > sz)
      *(s+sz) = '\0';
  }

char *stristr (char *s1, char *s2, int n)
  {
    // why isn't this in stdlib?
    // case insensitive strstr
    int lp;
    int nd;
    if (n == 0)
      nd = strlen(s1) - strlen(s2) + 1;
    else
      nd = n - strlen(s2) + 1; // only search the length specified

    int rd = strlen(s2);
    for (lp = 0; lp < nd; lp++)
      {
        int lr;
        int good = TRUE;
        for (lr = 0; lr < rd; lr++)
          {
            if (toupper(*(s1+lp+lr)) != toupper(*(s2+lr)))
              {
                good = FALSE;
                break;
              }
          }
        if (good)
          return s1+lp;
      }
    return NULL;
  }

int val(const char *s)
  {
    return atoi(s);
  }

UINT endflip(UINT s)
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

char toupper (char &c)
  {
    if ((c >= 'a') && (c <= 'z'))
      return c - ('a' - 'A');
    return c;
  }

void fixsize(Memory &var, int sz)
  {
    if (strlen(var) > (UINT)sz)
      *(var+sz) = '\0';
  }

int isipaddress(char *address)
  {
    while (*address != '\0')
      {
        if (! ((*address == '.') || ((*address >= '0') && (*address <= '9'))))
          return FALSE;
        address++;
      }
    return TRUE;
  }

void stringfromip(ULONG addr, Memory &addrout)
  {
    /* Take a long, and make it a decimal notation 4 point address */
    int p1, p2, p3, p4;

    p1 = addr % 256;
    addr /= 256;
    p2 = addr % 256;
    addr /= 256;
    p3 = addr % 256;
    addr /= 256;
    p4 = addr;
    Memory ret(50);
    sprintf (ret, "%d.%d.%d.%d", p4, p3, p2, p1);
    addrout = ret;
  }


long sizereset (FILE *f)
  {
    // return the size of open file handle and reset to beginning of file
    fseek(f, 0, SEEK_END);
    long ret = ftell(f);
    fseek(f, 0, SEEK_SET);
    return ret;
  }

void htmlunescapebinary(char *dest, int &destsize, char *src, int srcsize)
  {
    destsize = 0;
    char *end = src+srcsize;
    while (src < end)
      {
        if (*src == '+')
          *dest = ' ';
        else if (*src == '%')
          {
            int code;
            unsigned char first = tolower(*(src+1));
            if (first >= 'a')
              code = 16 * (10 + (first - 'a'));
            else
              code = 16 * (first - '0');
            unsigned char second = tolower(*(src+2));
            if (second >= 'a')
              code += (10 + (second - 'a'));
            else
              code += (second - '0');

            *dest = code;
            src += 2;
          }
        else
          *dest = *src;
        src++;
        dest++;
        destsize++;
      }
  }

void htmlescapebinary (Memory &dest, int &destsize, char *source, int sourcesize)
  {
    /* Same thing below but as in binary. */
    // and really slow
    // okay, now it's much faster
    dest = "";
    destsize = 0;
    int curpos = 0;
    dest.resize((sourcesize * 3) + 16);
    char *dpos = dest;
    while (curpos < sourcesize)
      {
        int fix = TRUE;
        unsigned char c = *(source + curpos);
        if (c == ' ')
          {
            c = '+';
            fix = FALSE;
          }
        else
          if (((c >= 'A') && (c <= 'Z')) ||
              ((c >= 'a') && (c <= 'z')) ||
              ((c >= '0') && (c <= '9')))
            fix = FALSE;
        if (fix)
          {
            *dpos = '%'; dpos++;
            int first = c / 16;
            if (first >= 10)
              *dpos = 'a' + (first-10);
            else
              *dpos = '0' + first;
            dpos++;
            int second = c % 16;
            if (second >= 10)
              *dpos = 'a' + (second-10);
            else
              *dpos = '0' + second;
            dpos++;

            destsize += 3;
          }
        else
          {
            *dpos = c;
            dpos++;
            destsize++;
          }
        curpos++;
      }
  }

void htmlescape (Memory &dest, Memory &source)
  {
    /* You know the drill,
       1. The form field names and values are escaped: space
          characters are replaced by `+', and then reserved characters
          are escaped as per [URL]; that is, non-alphanumeric
          characters are replaced by `%HH', a percent sign and two
          hexadecimal digits representing the ASCII code of the
          character. Line breaks, as in multi-line text field values,
          are represented as CR LF pairs, i.e. `%0D%0A'.  */

    dest = "";
    char *s = source;
    while (*s != '\0')
      {
        /* while there are much more efficient ways of doing this,
           I expect this will suffice. */
        int fix = TRUE;
        char c = *s;
        if (c == ' ')
          c = '+';
        else
          if (((*s >= 'A') && (*s <= 'Z')) ||
              ((*s >= 'a') && (*s <= 'z')) ||
              ((*s >= '0') && (*s <= '9')))
            fix = FALSE;
        if (fix)
          {
            char buf[10];
            sprintf (buf, "%%%02X", c);
            dest.addstr(buf);
          }
        else
          dest.addchar(c);
        s++;
      }
  }

void losehtml(Memory &inout)
  {
    // remove all <> pairs, and do minor translation
    // remove all &<---*--->;
    Memory to;
    to = "";
    char *s = inout;
    if (s == NULL)
      s = "";
    int copying = TRUE;

    while (*s != '\0')
      {
        char c = *s;
        // parse html here, then strip out all others, then parse
        // inline conversions like &
        if (strlen (s) >= 4)
          if (strncasecmp (s, "<br>", 4) == 0)
            {
              to.addchar(' ');
              s += 4;
              continue;
            }
        if (c == '<')
          {
            copying = FALSE;
            s++;
            continue;
          }
        if (c == '>')
          {
            copying = TRUE;
            s++;
            continue;
          }
        if (copying == FALSE)
          { // shortcut
            s++;
            continue;
          }
        if (strlen (s) >= 6)
          if (strncasecmp (s, "&quot;", 6) == 0)
            {
              c = '\"';
              s += 5;
            }
        if (strlen (s) >= 4)
          if (strncasecmp (s, "&lt;", 4) == 0)
            {
              c = '<';
              s += 3;
            }
        if (strlen (s) >= 4)
          if (strncasecmp (s, "&gt;", 4) == 0)
            {
              c = '>';
              s += 3;
            }
        int isamp = FALSE;
        if (strlen (s) >= 5)
          if (strncasecmp (s, "&amp;", 5) == 0)
            {
              c = '&';
              s += 4;
              isamp = TRUE;
            }
        if (isamp == FALSE)
          if (c == '&')
            { /* Take all & though ; and wipe it */
              char *p = s;
              while (*p != '\0')
                {
                  if (*p == ';')
                    break;
                  p++;
                }
              if (*p == ';')
                {
                  s = p+1; // go to char past ;
                  c = ' '; // replace whole thing with ' '
                }
            }

        to.addchar(c);
        s++;
      }
    inout = to;
  }

void temppathname(Memory &name)
  {
    // I'm tired of getting that stupid tmpnam error
    static int tumper = 0;
    name = P_tmpdir;
    char b[20];
    sprintf (b, "%d%d", (unsigned)getpid(), tumper++);
    name.addstr(b);
  }

void tempfilename(Memory &name)
  {
    ULONG x = unixtime();
    pid_t y = getpid();
    srand(x);
    int n = rand();

    Memory fname(100);
    sprintf (fname, "%lu%u%u", x, (unsigned)y, n);
    name = fname;
  }

char *gettempfilename(void)
  { // replacing tmpnam 11/7/02
    static Memory nlocal;

    nlocal = P_tmpdir;
    addslash(nlocal);
    Memory n;
    tempfilename(n);
    nlocal.addstr(n);
    return nlocal;
  }

/* my reinvented Yokohama style address class */

emailaddr::emailaddr(char *strin)
  {
    /* Rip this apart into nice pieces. */
    // 1/8/03 this really needs to be rewritten
    /* 6/22/06 I think we should force everything to be case insensitive
       but for now, we'll just force lowercase everything. */
    char *brpos;
    Memory strlower(strin);
    lowdo(strlower);
    char *s = strlower;

    brpos = strchr (s, '<');
    if (brpos == NULL)
      {
        /* There is no friendly name, it's all email */
        friendly = "";
        email = s;
        name = s;
        trim(name);
        brpos = strchr(name, '@');
        if (brpos != NULL) // strip off domain
          *brpos = '\0';
        trim(email);
        pretty = email;
        Memory ts(strlen(email) + 3);
        sprintf (ts, "<%s>", (char *)email);
        fullpretty = ts;
        proper = ts;
        return;
      }
    /* Take out the friendly part */
    Memory fr(s, brpos-s); /* take just the first bit */
    trim(fr);
    friendly = fr;
    quotestrip(friendly);

    Memory em0(brpos);
    trim(em0);
    Memory em(brpos+1); // skip the <
    trim (em);
    proper = em0;
    if (strrchr(proper, '>') == NULL)
      proper.addchar('>');
    // 10/5/01 neato bug, we assumed, there would be a matching >, maybe not...
    if (strchr (em, '>') != NULL)
      *((char *)em + strlen(em) - 1) = '\0'; // chop off the >
    email = em;
    pretty = em;
    name = email;
    brpos = strchr(name, '@');
    if (brpos != NULL)
      *brpos = '\0';

    Memory ts(strlen(email) + 3);
    sprintf (ts, "<%s>", (char *)email);
    fullpretty = ts;
    if (strlen(friendly) > 0)
      {
        pretty = friendly;
        Memory ts(strlen(friendly) + strlen(email) + 7);
        sprintf (ts, "\"%s\" <%s>", (char *)friendly, (char *)email);
        fullpretty = ts;
      }
  }

emailaddr::~emailaddr (void)
  {
  }

/* swiped this from mailUtil */
void getDateString(Memory &dt)
  {
#ifndef OS_WINDOWS
    char dateline[80];
    char ctimestr[80];
    time_t timen;
    char daystr[4];
    char datestr[3];
    char monstr[4];
    char timestr[9];
    char yearstr[5];
    struct timeval tv;
    struct timezone tz;
    struct tm *broken_down_time_structure;
    int dst; /* daylight savings time: 1->yes, 0->no, -1->no data */
    int mw;
    int atz;

    /*init substrings to null strings*/
    memset(daystr, '\0', 4);
    memset(datestr, '\0', 3);
    memset(monstr, '\0', 4);
    memset(timestr, '\0', 9);
    memset(yearstr, '\0', 5);

    /* get time */
    timen = time(0);
    gettimeofday(&tv, &tz);
    /* get minutes west of Greewich */
    mw = tz.tz_minuteswest;
    /* get ctime string */
    strcpy(ctimestr, (char *)ctime(&timen));
    /* get substrings for ctimestring */
    strncpy(daystr, ctimestr, 3);    /* get day string */
    strncpy(monstr, ctimestr+4, 3);  /* get month string */
    strncpy(datestr, ctimestr+8, 2); /* get date string */
    strncpy(timestr, ctimestr+11, 8);/* get time string */
    strncpy(yearstr, ctimestr+20, 4);/* get year string */

    /* Check to see if it is daylight savings time */
    broken_down_time_structure = localtime(&timen);
    dst = broken_down_time_structure->tm_isdst;

    /* calculate arpa timezone info */
    atz=-(((mw/60) - ((dst > 0) ? 1 : 0)) * 100);

    /*if west of Greenwich, use this*/
    if (atz < 0)
      sprintf(dateline, "%s, %s %s %s %s %05d", daystr, datestr,
              monstr, yearstr, timestr, atz);
    else /* else east of Greenwich */
      sprintf(dateline, "%s, %s %s %s %s %04d", daystr, datestr,
              monstr, yearstr, timestr, atz);
    dt = dateline;
    return;
#endif // windows
  }

unsigned long lookuphost(char *addr)
  {
    struct hostent *nam;
    nam = gethostbyname (addr);
    if (nam == NULL)
      return 0;
    unsigned long *nentry = (unsigned long *)nam->h_addr_list[0];
    return *nentry;
  }

void sleep10 (int t)
  {
    /* Sleep in tenths of seconds. */
    struct timeval tme;
    fd_set fd;
    int sec;

    sec = t / 10; // seconds
    t = t % 10; // 10ths of seconds
    t = t * 100000;
    if (t > 999999)
      t = 999999;

    FD_ZERO(&fd);
    tme.tv_sec = sec;
    tme.tv_usec = t;
    select (1, &fd, 0, 0, &tme);
  }

/********************************************************************/
/*                 2/2001 my neat memory cacher thingi              */
/********************************************************************/

#ifdef FASTMEMM


/* Here's how it works folks...
   There's 2 parrelle arrays of PILES size. 1 array is pointer to the pile
   of pointers (for that size) and the other is the size of the pile.
   The size of the pile is determined by dividing the max up by the number
   of piles. Change these numbers to optomize for size and speed and memory
   wasting. But the basic idea is simple. When you want memory of X size,
   figure out which pile to go after, take a pointer from there.
   Erm, 3 arrays, the third is the size of the memory allocated for the
   pile of pointers.
   Anyway, take one off the bottom and dec the count. If pile is empty,
   do a slowgetmem and return.
   To put in the pile, figure out which pile, add it to the bottom, if the
   count is bigger than the allocated memory for the pile make the pile
   bigger. might want to put a max on this so it doesn't get out of
   hand, without it, the program will never actually free up any memory. */

// oh, and these should be silly fast.

char *getmemfromcache(long m)
  {
    if (m >= FASTMEMMMAXTOCACHE) // out of cache range
      return slowgetmem(m);
    int pile = m / FASTMEMMPILESIZE;
    /* Everything in pile 0 has to be alloced at least PILESIZE.
       Really we do all the allocating, so the only way to mess
       this up is with reallocmem. */
    if (FMpileheight[pile] == 0)
      {
        return slowgetmem(FASTMEMMPILESIZE * (1+pile)); // max size for this pile
      }
    // go get one off this pile
    int item = --FMpileheight[pile];
    char *ret = FMpiles[pile][item];
    /* 3/2/00 it occures to me the getmem guarentees zeroeod out
       memory, and we don't.  I guess we have to do that here. */
    /* we're going to try and save time by just setting the first
       byte. The assumtion is if they assume its zero its because
       it's a string. */

    *ret = 0;
    return ret;
  }

void putmemincache(char *m)
  {
    /* This is the complicated one, have to incremement piles. */

    long sz;

    memcpy (&sz, m-4, 4); /* get long back */
    /* We save some time by NOT checking validity. */

    if (sz >= FASTMEMMMAXTOCACHE) // 99 gets saved, 100 doesn't
      {
        slowfreemem(m);
        return;
      }
    /* piles going in are different than piles going out.
       something going in a pile has to completely support that range.
       A request for 59 rounds to 60 and comes out of pile 3 (for 80)
       When it comes back as 80, it goes in 3 again. Hmm.... */
    /* Ahhh, this is the problem. from 0-19 are useless they're
       too small to be guarenteed to fill any pile's request and therefore
       must be freed. But you have to try to make one.
       20 and 21 and 25 and 38 all go in pile zero because
       they can't completely support pile 1 (20-39) */
    int pile = (sz / FASTMEMMPILESIZE) - 1;
    if (pile == -1)
      { // this is too small to be useful, lose it
        slowfreemem(m);
        return;
      }
    // we have to figure out what's the fastest way to do this...
    int item = FMpileheight[pile]++; // pointer is where it should go
    if (item >= FASTMEMMCACHEDEPTH) // 1 means nothing is stored
      {
        // pile's full, fix the pile size and free it
        FMpileheight[pile]--;
        slowfreemem(m);
        return;
      }
    if (FMpilealloc[pile] <= item)
      {
        // make the pile size bigger
        FMpiles[pile] = (char **)reallocmem((char *)FMpiles[pile], sizeof(char *) * ((item+1) * 2));
        FMpilealloc[pile] = (item+1) * 2;
      }
    FMpiles[pile][item] = m; // puttin' on the ritz
  }

#endif // fastmemm


void printfastmemmcachestate(FILE *f)
  { // so I can optimize it
#ifdef DEBUG
#ifdef FASTMEMM

    int lp;
    for (lp = 0; lp < FASTMEMMPILES; lp++)
      fprintf (f, "%5d - %5d: %d items.\n", lp*FASTMEMMPILESIZE, (lp+1)*FASTMEMMPILESIZE-1, FMpileheight[lp]);
#endif
#endif
  }


void logstringandtime(int debug, char *s)
  {
    struct timeval tp;
    gettimeofday(&tp, NULL);

    int sz = strlen(s) + 16 + 10 + 5 + 5 + 5 + 1;
    Memory ts(sz);
    Memory ct(35);
    time_t t = tp.tv_sec;
    ct = ctime(&t);
    memcpy (ts, ct+4, 15);

    sprintf (ts+15, ",%03ld %s", tp.tv_usec/1000, (char *)s);

    printf ("%s\n", (char *)ts);
  }


void crappycrypt(unsigned char *data, int datasize, unsigned char *passphrase, int seed)
  {
    /* This is just to keep the people watching the proxy from getting curious */
    /* It also serves to tie a client to a server so not just anybody can connect to your
     * sshhd server via your sshhcgi setup. If your cgi passphrase doesn't match, you
     * won't be able to connect. */

    int ppmax = strlen((char *)passphrase);
    int stpos = seed % ppmax;
    int lp = 0;
    int pplp = stpos;
    unsigned char *cp = data;
    unsigned char *ppp = passphrase + pplp;
    while (lp < datasize)
      {
        *cp ^= *ppp;
        cp++;
        lp++;
        ppp++;
        pplp++;
        if (pplp >= ppmax)
          {
            ppp = passphrase;
            pplp = 0;
          }
      }


  }


