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
#ifndef HASHBOARD_H
#define HASHBOARD_H

/* A template class to make a hash key value lookup table.
   This is exactly like hashtable except it owns the elements
   in the table, much like tree and zlist. Not sure why I didn't
   do it this way the first time.

   usage:

   hashboard<int> ht(10); // 10 items in list
   int ii = new int(1);
   ht.set("hi", &1);
   int *i;
   ht.get("hi" i);

   now there's an iterator too.
   resetiterator() and
   int ip* = iterator();

   ht.resetiterator();
   char *k; int *v;
   while (ht.iterator(k, v) != FALSE)
     {
       printf ("%s = %d\n", k, *v);
     }


   hashboard will resize from initial size passed to constructor if enough
   items are added to hash table.
   List will never return duplicate values if duplicate keys are added.
*/

/* the multiply factor to make room for possible hash results
   the bigger the number the faster the hash will function,
   the more memory it will take. The type passed to this template
   must have a functioning copy constructor. */

/* Today I learned that you if you don't want to include template method
   bodies in the template class declaration, you have to do this:
   template <class T> hashboard<class T>::function(params) {}
*/

#define HT_EXTRAROOM 2

template <class T> class hashboard
  {
    public:
      hashboard (int initialsize = 100)
        {
          tablelength = initialsize;
          if (tablelength < 10)
            tablelength = 10;

          tablelength *= HT_EXTRAROOM; // so we have room to thrash the hash about

          key = (Memory **)getmem(sizeof(Memory *) * tablelength); // array of Memory objects
          value = (T **)getmem(sizeof(T *) * tablelength); // array of pointers to T objects
          int lp; // must zero out tables
          for (lp = 0; lp < tablelength; lp++)
            {
              key[lp] = NULL;
              value[lp] = NULL;
            }
          resetiterator();
        }

      ~hashboard(void)
        {
          wipe();
          freemem(key); // free these now
          freemem(value);
        }

      void wipe(void)
        {
          // wipe only erases/deletes what we stored, it does not free the array holder memory
          int lp;
          for (lp = 0; lp < tablelength; lp++)
            {
              Memory *m = key[lp];
              if (m != NULL)
                delete m;
              T* v = value[lp];
              if (v != NULL)
                delete v;
              // such the moron am I. We delete, but don't null out, so the second time
              // you call wipe, you delete something we already deleted.
              key[lp] = NULL;
              value[lp] = NULL;
            }
        }

      void resetiterator(void)
        {
          iterpos = 0;
        }

      int iterator(char *&ikey, T *&ivalue)
        {
          /* return FALSE if there's no more in the list, otherwise it sets
             the pointers you pass in to the next key and value in the list */

          while (iterpos < tablelength)
            {
              Memory *ik = key[iterpos];
              if (ik != NULL)
                {
                  ikey = *(key[iterpos]);   // copy pointers
                  ivalue = value[iterpos];
                  iterpos++;
                  return TRUE;
                }
              iterpos++;
            }
          return FALSE;
        }

      int set(char *k, T *Value) // return FALSE if table is full (future will resize)
        {
          // we own the object passed to us
          int hv = hashstring(k);
          // see if hash value already exists, if so, delete the old one and use that space
          int pos = lookup(k);
          if (pos != -1)
            {
//printf("HASHMAP *** setting an existing value: %s %d\n", k, hv);
              delete (value[pos]);
              value[pos] = Value;
              return TRUE;
            }
          // find a new space
          pos = findspace(hv);
          if (pos == -1)
            {
              // we ran out of space, make the hashboard bigger and rehash
              resizehash();
              pos = findspace(hv);
              if (pos == -1) // if we STILL can't find space, there's a problem
                return FALSE;
            }
          key[pos] = new Memory(k);
          value[pos] = Value; // we own this we will delete it.
          return TRUE;
        }

      int get(char *k, T *&Value) // return TRUE and set val if there, FALSE if not found
        { // return TRUE and set val if there, FALSE if not found
          int pos = lookup(k);
          if (pos == -1)
            return FALSE;
          Value = value[pos];
          return TRUE;
        }

      int del(char *k) // return TRUE if found and, FALSE if not found
        {
//printf ("Deleting hash value: %s\n", k);
          int pos = lookup(k);
          if (pos == -1)
            return FALSE;
          delete key[pos];
          key[pos] = NULL;
          T *v = value[pos];
          delete v;
          value[pos] = NULL;
          return TRUE;
        }

    private:
      int hashstring(char *s)
        {
          /* Come up with a good hash for this string to a number within tablesize.
             for now we'll add the chars. */
          int ret = 0;
          int lp;
          int max = strlen(s);
          for (lp = 0; lp < max; lp++)
            ret += (tablelength/4+1) * (int)s[lp]; // always uppercase
          ret %= tablelength;
          return ret;
        }

      int findspace(int pos)
        {
          /* starting from this position, find an empty spot, return -1 if
             error, which of course should never happen. empty spaces
             have a null key */
          int lp = pos;
          if (key[lp] == NULL)
            return lp;
          lp++;
          while (lp != pos) // if we get to where we started, stop
            {
              if (lp == tablelength)
                {
                  lp = 0;
                  if (pos == 0) // would otherwise cause an endless loop if pos == 0
                    pos = 1;
                }
              if (key[lp] == NULL)
                return lp;
              lp++;
            }
          return -1;
        }

      int lookup(char *text)
        {
          /* Given this piece of text, find the position in the hash table and return it.
             -1 if not found */
          int pos = hashstring(text);

          /* now look for it. */
          int lp = pos;
          if (key[lp] != NULL)
            if (strcmp(text, *(key[lp])) == 0) // be opportunistic, see if it's right where we expect
              return lp;
          lp++;
          while (lp != pos) // if we get to where we started, stop
            {
              if (lp == tablelength)
                {
                  lp = 0;
                  if (pos == 0) // would otherwise cause an endless loop if pos == 0
                    pos = 1;
                }
              if (key[lp] != NULL)
                if (strcmp(text, *(key[lp])) == 0)
                  return lp;
              lp++;
            }
          return -1;
        }

      int resizehash(void)
        {
          Memory **oldkey = key;
          T **oldvalue = value;
          int oldtablelength = tablelength;

          tablelength *= 2; // double it
          key = (Memory **)getmem(sizeof(Memory *) * tablelength);
          value = (T **)getmem(sizeof(T *) * tablelength);
          int lp;
          for (lp = 0; lp < tablelength; lp++)
            {
              key[lp] = NULL;
              value[lp] = NULL;
            }
          if (rehash(oldkey, oldvalue, oldtablelength) == FALSE) // from old to new
            {
              printf ("hashboard is in invalid state\n"); // beyond saving here
              return FALSE; // and lots of memory leaked to boot!
            }
          freemem (oldkey);
          freemem (oldvalue);
          return TRUE;
        }

      int rehash(Memory **oldkey, T **oldvalue, int oldtablelength)
        {
          // copy all the live items POINTERS from the old table to the new
          int lp;
          for (lp = 0; lp < oldtablelength; lp++)
            {
              Memory *m = oldkey[lp];
              if (m != NULL) // something here
                {
                  int hv = hashstring(*m);
                  int pos = findspace(hv);
                  // this can't fail because we just doubled the size of the table
                  key[pos] = m;  // this is much faster than the hashtable anyway, what was I thinking?
                  value[pos] = oldvalue[lp]; // 10/20/06, what a sinister and nasty bug, this used to say pos
                }
            }
          return TRUE;
        }

      Memory **key; // array for hash item keys
      int iterpos;
      T **value; // array for hash item values
      int tablelength;
  };

#endif


