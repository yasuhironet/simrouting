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

#include <stdlib.h>

#include "connectivity.h"

/* Weighted Quick Union with path compression by halving */
void
connectivity_init (int N, struct connectivity *c)
{
  int i;
  c->N = N;
  c->node_array = malloc (sizeof (int) * c->N);
  c->tree_size = malloc (sizeof (int) * c->N);
  for (i = 0; i < c->N; i++)
    {
      c->node_array[i] = i;
      c->tree_size[i] = 1;
    }
}

void
connectivity_connect (int p, int q, struct connectivity *c)
{
  int ptop, qtop;

  for (ptop = p; ptop != c->node_array[ptop]; ptop = c->node_array[ptop])
    {
      /* path compression by halving */
      c->node_array[ptop] = c->node_array[c->node_array[ptop]];
    }
  for (qtop = q; qtop != c->node_array[qtop]; qtop = c->node_array[qtop]);
    {
      /* path compression by halving */
      c->node_array[qtop] = c->node_array[c->node_array[qtop]];
    }

  if (ptop == qtop)
    return;

  if (c->tree_size[ptop] < c->tree_size[qtop])
    {
      /* merge p's tree to q's */
      c->node_array[ptop] = qtop;
      c->tree_size[qtop] += c->tree_size[ptop];
    }
  else
    {
      /* merge q's tree to p's */
      c->node_array[qtop] = ptop;
      c->tree_size[ptop] += c->tree_size[qtop];
    }
}

int
is_connectivity (struct connectivity *c)
{
  int zerotop, i, itop;

  for (zerotop = c->node_array[0]; zerotop != c->node_array[zerotop];
       zerotop = c->node_array[zerotop]);
  for (i = 1; i < c->N; i++)
    {
      for (itop = c->node_array[i]; itop != c->node_array[itop];
           itop = c->node_array[itop]);
      if (itop != zerotop)
        return 0;
    }
  return 1;
}

void
connectivity_finish (struct connectivity *c)
{
  free (c->node_array);
  free (c->tree_size);
}

