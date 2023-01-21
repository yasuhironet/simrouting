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

#include "graph.h"
#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"

int
nbits (unsigned long bitstring)
{
  unsigned char *p;
  int i, j, nbit;

  p = (unsigned char *)&bitstring;
  nbit = 0;
  for (i = 0; i < sizeof (unsigned long); i++)
    {
      for (j = 0; j < 8; j++)
        if (*p >> j & 1)
          nbit++;
      p++;
    }
  return nbit;
}

#define FLOW_LABEL_MASK 0x000fffff

void
packet_forward_with_fail (struct node *s, struct node *t, struct routing *R,
                          struct node *fs, struct node *ft, int ntrial)
{
  time_t seed;
  unsigned long flowlabel;
  struct vector *router_mask;
  struct path *path;
  unsigned long mask;
  struct nexthop *nexthop;
  int i;
  struct node *node;
  struct vector *table;
  char buf[256];

  seed = time (NULL);
  fprintf (stderr, "seed: %lu\n", (unsigned long) seed);
  srandom ((unsigned int) seed);

  router_mask = vector_create ();
  for (i = 0; i < graph_nodes (s->g); i++)
    {
      mask = (unsigned long) random () & FLOW_LABEL_MASK;
      fprintf (stderr, "node %d: %#lx\n", i, mask);
      vector_set (router_mask, i, (void *) mask);
    }

  for (i = 0; i < ntrial; i++)
    {
      path = path_create ();

      flowlabel = (unsigned long) random () & FLOW_LABEL_MASK;
      node = s;

      while (node->id != t->id)
        {
          vector_add (node, path->path);
          table = R->route[node->id][t->id].nexthops;
          mask = (unsigned long) vector_get (router_mask, node->id);
          nexthop = vector_get (table, (nbits (flowlabel & mask) % table->size));
          fprintf (stderr, "node %d: flowlabel %#lx, mask %#lx,"
                           " value: %#lx, bits: %d"
                           " #nexthop %d, index %d, nexthop %d\n", 
                   node->id, flowlabel, mask,
                   flowlabel & mask, nbits (flowlabel & mask),
                   table->size, (nbits (flowlabel & mask) % table->size),
                   nexthop->node->id);
          node = nexthop->node;
        }

      sprint_nodelist (buf, sizeof (buf), path->path);
      fprintf (stderr, "packet forwarding path: %s, %s\n",
               buf, (path_is_include (path, fs, ft) ? "inval" : "valid"));
      path_delete (path);
    }
}


