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
#include "file.h"
#include "timer.h"
#include "pqueue.h"

#include "network/graph.h"
#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"

#include "routing/mara-mc-mmmf.h"
#include "routing/dijkstra.h"
#include "routing/reverse-dijkstra.h"

typedef int (*cmp_t) (void *, void *);

void
routing_mara_spe (struct node *t, cmp_t cmp_func, struct weight *weight,
                  struct dijkstra_path *dijkstra_table,
                  struct mara_node **mara_data)
{
  struct vector_node *vn;
  struct pqueue *pqueue;
  unsigned int label = 1;
  struct mara_node *c, *v;

  struct dijkstra_path *w;
  int notyet;
  struct vector_node *vnn;

  /* compute SPT */
  routing_reverse_dijkstra (t, weight, dijkstra_table);

  /* Priority queue (Heap sort) */
  pqueue = pqueue_create ();
  pqueue->cmp = cmp_func;
  pqueue->update = mara_pqueue_index_update;

  /* The first candidate is the destination itself */
#define MARA_ADJ_INFINITY UINT_MAX
#define MARA_BW_INFINITY UINT_MAX
  c = &mara_data[t->id][t->id];
  c->node = t;
  c->adjacency = MARA_ADJ_INFINITY;
  c->bandwidth = MARA_BW_INFINITY;

  /* Install the destination itself in the priority queue */
  pqueue_enqueue (c, pqueue);

  /* Continue until the priority queue becomes empty */
  while (pqueue->size)
    {
      /* retrieve the most preferred node candidate */
      c = pqueue_dequeue (pqueue);

      /* skip if it is already labeled */
      if (c->label != 0)
        continue;

      /* label the node newly. */
      c->label = label++;

      /* Call the just decided vertex "v" */
      v = c;

#ifdef DEBUG
      fprintf (stderr, "MARA-SPE: dst: %d node[%d]: "
               "label: %d adjacency: %d\n",
               t->id, v->node->id, v->label, v->adjacency);
#endif /*DEBUG*/

      /* for each neighbor "w" of v */
      for (vn = vector_head (v->node->ilinks); vn; vn = vector_next (vn))
        {
          struct link *link = (struct link *) vector_data (vn);
          struct node *candidate = link->from;

          w = &dijkstra_table[candidate->id];
          c = &mara_data[t->id][candidate->id];

          /* skip if the neighbor candidate w is already labeled */
          if (c->label > 0)
            continue;

          /* update the candidate */
          c->node = candidate;
          c->adjacency++;
          c->bandwidth += (link->bandwidth ? link->bandwidth : 0) ;

          /* check whether all successor nodes in the SPT are
             already labeled. */
          notyet = 0;
          for (vnn = vector_head (w->nexthops); vnn;
               vnn = vector_next (vnn))
            {
              struct node *n = (struct node *) vector_data (vnn);
              struct mara_node *u = &mara_data[t->id][n->id];

              if (! u->label)
                notyet++;
            }

          /* skip if not all successors in the SPT are labeled */
          if (notyet)
            continue;

          /* Newly enqueue, or update in the priority queue */
          if (c->pqueue_index < 0)
            pqueue_enqueue (c, pqueue);
          else
            pqueue_update (c->pqueue_index, pqueue);

#ifdef DEBUG
          fprintf (stderr, "MARA-SPE: dst: %d candidate node[%d]: "
                   "adjacency: %d bandwidth: %f\n", t->id, candidate->id,
                   c->adjacency, c->bandwidth);
#endif /*DEBUG*/
        }
    }

  pqueue_delete (pqueue);
}

DEFINE_COMMAND (routing_algorithm_mara_spe,
                "routing-algorithm mara-spe",
                ROUTING_ALGORITHM_HELP_STR
                "MARA-SPE (Shortest Path Extension).\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  timer_counter_t start, end, res;
  struct dijkstra_path **dijkstra_data;
  struct vector_node *vn;

  if (routing->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified for routing.\n");
      return;
    }
  if (routing->W == NULL)
    {
      fprintf (shell->terminal, "no weight specified for routing.\n");
      return;
    }

  if (routing->data == NULL)
    routing->data = mara_data_create (routing->G);
  mara_data_clear (routing->G, routing->data);

  /* prepare dijkstra table */
  dijkstra_data = dijkstra_data_create (routing->G);
  dijkstra_data_clear (routing->G, dijkstra_data);

  timer_count (start);
  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *t = (struct node *) vector_data (vn);
      routing_mara_spe (t, mara_mc_cmp, routing->W,
                        dijkstra_data[t->id], routing->data);
    }
  timer_count (end);

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *s = (struct node *) vector_data (vn);
      routing_mara_route_node (s, routing);
    }

  timer_sub (start, end, res);
  fprintf (shell->terminal, "MARA-SPE overall calculation "
           "time: %llu us\n", timer_to_usec (res));

  dijkstra_data_delete (routing->G, dijkstra_data);
}

DEFINE_COMMAND (routing_algorithm_mara_spe_node,
                "routing-algorithm mara-spe node " NODE_SPEC,
                ROUTING_ALGORITHM_HELP_STR
                "MARA-SPE (Shortest Path Extension).\n"
                "Specify node to calculate routes\n"
                NODE_SPEC_HELP_STR)
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct node *node;
  unsigned long node_id;
  timer_counter_t start, end, res;
  struct dijkstra_path *dijkstra_table;
  struct vector_node *vn;

  if (routing->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified for routing.\n");
      return;
    }
  if (routing->W == NULL)
    {
      fprintf (shell->terminal, "no weight specified for routing.\n");
      return;
    }

  node_id = strtoul (argv[3], NULL, 0);
  node = node_lookup (node_id, routing->G);
  if (! node)
    {
      fprintf (shell->terminal, "No such node: %lu\n", node_id);
      return;
    }

  if (routing->data == NULL)
    routing->data = mara_data_create (routing->G);
  mara_data_clear (routing->G, routing->data);

  dijkstra_table = dijkstra_table_create (routing->G);
  dijkstra_table_clear (routing->G, dijkstra_table);

  timer_count (start);
  for (vn = vector_head (node->g->nodes); vn; vn = vector_next (vn))
    {
      struct node *t = (struct node *) vector_data (vn);
      routing_mara_spe (t, mara_mc_cmp, routing->W,
                        dijkstra_table, routing->data);
    }
  timer_count (end);

  routing_mara_route_node (node, routing);

  timer_sub (start, end, res);
  fprintf (shell->terminal, "MARA-SPE calculation time "
           "for node %u: %llu us\n", node->id, timer_to_usec (res));

  dijkstra_table_delete (routing->G, dijkstra_table);
}

DEFINE_COMMAND (routing_algorithm_mara_spe_node_all,
                "routing-algorithm mara-spe node all",
                ROUTING_ALGORITHM_HELP_STR
                "MARA-SPE (Shortest Path Extension).\n"
                "Specify node to calculate routes\n"
                "Calculate on all node individually\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct vector_node *vn, *vni;
  timer_counter_t start, end, res;
  struct dijkstra_path **dijkstra_data;

  if (routing->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified for routing.\n");
      return;
    }
  if (routing->W == NULL)
    {
      fprintf (shell->terminal, "no weight specified for routing.\n");
      return;
    }

  if (routing->data == NULL)
    routing->data = mara_data_create (routing->G);

  /* prepare dijkstra table */
  dijkstra_data = dijkstra_data_create (routing->G);
  dijkstra_data_clear (routing->G, dijkstra_data);

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *node = (struct node *) vector_data (vn);

      mara_data_clear (routing->G, routing->data);
      dijkstra_data_clear (routing->G, dijkstra_data);

      timer_count (start);
      for (vni = vector_head (node->g->nodes); vni; vni = vector_next (vni))
        {
          struct node *t = (struct node *) vector_data (vni);
          routing_mara_spe (t, mara_mc_cmp, routing->W,
                            dijkstra_data[t->id], routing->data);
        }
      timer_count (end);

      routing_mara_route_node (node, routing);

      timer_sub (start, end, res);
      fprintf (shell->terminal, "MARA-SPE calculation time "
               "for node %u: %llu us\n", node->id, timer_to_usec (res));
    }

  dijkstra_data_delete (routing->G, dijkstra_data);
}


