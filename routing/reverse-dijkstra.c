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

#include "vector.h"
#include "shell.h"
#include "command.h"
#include "command_shell.h"
#include "pqueue.h"
#include "timer.h"

#include "network/graph.h"
#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"

#include "routing/dijkstra.h"

void
routing_reverse_dijkstra (struct node *root, struct weight *weight,
                          struct dijkstra_path *dijkstra_table)
{
  struct dijkstra_path *c, *v;
  struct pqueue *candidate_list;
  struct vector_node *vn;

  /* candidate list is a priority queue */
  candidate_list = pqueue_create ();
  candidate_list->cmp = dijkstra_candidate_cmp;
  candidate_list->update = dijkstra_candidate_update;

  /* consider the calculating node itself as a starting candidate */
  c = &dijkstra_table[root->id];
  c->node = root;
  c->metric = 0;
  c->nexthops = vector_create ();
  vector_add (root, c->nexthops);

  /* install the calculating node in the candidate list */
  pqueue_enqueue (c, candidate_list);

  /* continue while the candidate list is not empty */
  while (candidate_list->size)
    {
      /* get the shortest candidate path among all candidates */
      c = pqueue_dequeue (candidate_list);

      /* Call the just added vertex "v" */
      v = c;

      /* for each node that is reachable *to* "v" (reverse) */
      for (vn = vector_head (v->node->ilinks); vn;
           vn = vector_next (vn))
        {
          struct link *edge = (struct link *) vn->data;
          unsigned int edge_cost = 0;

          /* new candidate */
          c = &dijkstra_table[edge->from->id];
          c->node = edge->from;

          /* calculating node (root) has always metric 0, so treat */
          if (c->node == root)
            continue;

          /* edge cost */
          if (weight)
            edge_cost = weight->weight[edge->id];
          else
            edge_cost = 1;

          /* update candidate path metric */
          /* ignore longer path */
          if (c->metric && c->metric < v->metric + edge_cost)
            continue;

          /* update nexthop for ECMP */
          if (c->metric && c->metric == v->metric + edge_cost)
            {
              /* do nothing, fall through */
            }

          /* new or shorter path */
          if (! c->metric || c->metric > v->metric + edge_cost)
            {
              /* update cost */
              c->metric = v->metric + edge_cost;

              /* calculate nexthops from scratch (below) */
              if (c->nexthops)
                vector_clear (c->nexthops);
            }

          if (! c->nexthops)
            c->nexthops = vector_create ();

          /* calculate nexthop for the candidate */
          vector_add (v->node, c->nexthops);

          /* install in the candidate list */
          if (c->pqueue_index < 0)
            pqueue_enqueue (c, candidate_list);
          else
            pqueue_update (c->pqueue_index, candidate_list);
        }
    }

  /* free candidate list */
  pqueue_delete (candidate_list);
}

void
routing_reverse_dijkstra_route (struct node *root,
                                struct dijkstra_path *dijkstra_table,
                                struct routing *R)
{
  struct vector_node *vn, *vni;
  int t = root->id;

  /* set routing table from spf result table */
  for (vn = vector_head (root->g->nodes); vn; vn = vector_next (vn))
    {
      struct node *src = (struct node *) vn->data;
      int s = src->id;
      struct dijkstra_path *path = &dijkstra_table[src->id];

      nexthop_delete_all (R->route[s][t].nexthops);

      if (! path->nexthops)
        continue;

      for (vni = vector_head (path->nexthops); vni; vni = vector_next (vni))
        {
          struct node *nexthop = (struct node *) vector_data (vni);
          route_add (src, root, nexthop, R);
        }
    }
}

