/*
 * Copyright (C) 2007  Yasuhiro Ohara
 * 
 * This file is part of SimRouting.
 * 
 * SimRouting is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * SimRouting is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <includes.h>

unsigned long tmp;
#define SWAP(a,b,table)      \
  do {                       \
    tmp = (table)[a];        \
    (table)[a] = (table)[b]; \
    (table)[b] = tmp;        \
  } while (0);

unsigned long *
tag_hash_table_create (unsigned long size)
{
  unsigned long i;
  unsigned long *table;

  if (size > 0xffffffff)
    return NULL;

  table = (unsigned long *) malloc (size * sizeof (unsigned long));

  for (i = 0; i < size; i++)
    table[i] = i;

  for (i = 0; i < size; i++)
    SWAP (i, random () % size, table);

  return table;
}

void
tag_hash_table_delete (unsigned long *table)
{
  free (table);
}


