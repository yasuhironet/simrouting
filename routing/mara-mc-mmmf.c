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

typedef int (*cmp_t) (void *, void *);

void
mara_pqueue_index_update (void *data, int index)
{
  struct mara_node *m = (struct mara_node *) data;
  m->pqueue_index = index;
}

struct mara_node *
mara_table_create (struct graph *graph)
{
  struct mara_node *table;
  table = (struct mara_node *)
    calloc (graph->nodes->size, sizeof (struct mara_node));
  return table;
}

void
mara_table_delete (struct graph *graph, struct mara_node *table)
{
  free (table);
}

void
mara_table_clear (struct graph *graph, struct mara_node *table)
{
  int i;
  for (i = 0; i < graph->nodes->size; i++)
    {
      table[i].node = NULL;
      table[i].label = 0;
      table[i].adjacency = 0;
      table[i].bandwidth = 0.0;
      table[i].pqueue_index = -1;
    }
}

struct mara_node **
mara_data_create (struct graph *graph)
{
  struct mara_node **data;
  int i;
  data = (struct mara_node **)
    calloc (graph->nodes->size, sizeof (struct mara_node *));
  for (i = 0; i < graph->nodes->size; i++)
    data[i] = mara_table_create (graph);
  return data;
}

void
mara_data_delete (struct graph *graph, void *data)
{
  struct mara_node **mara_data;
  int i;

  mara_data = (struct mara_node **) data;
  for (i = 0; i < graph->nodes->size; i++)
    mara_table_delete (graph, mara_data[i]);

  free (data);
}

void
mara_data_clear (struct graph *graph, struct mara_node **data)
{
  int i;
  for (i = 0; i < graph->nodes->size; i++)
    mara_table_clear (graph, data[i]);
}

int
mara_mc_cmp (void *va, void *vb)
{
  struct mara_node *ca = (struct mara_node *) va;
  struct mara_node *cb = (struct mara_node *) vb;
  if (ca->adjacency != cb->adjacency)
    return (cb->adjacency - ca->adjacency);
  return (ca->node->id - cb->node->id);
}

int
mara_mmmf_cmp (void *va, void *vb)
{
  struct mara_node *ca = (struct mara_node *) va;
  struct mara_node *cb = (struct mara_node *) vb;
  if (ca->bandwidth != cb->bandwidth)
    return ((int)cb->bandwidth - (int)ca->bandwidth);
  return (ca->node->id - cb->node->id);
}

#ifdef DEBUG
void
routing_mara_pqueue_debug (struct pqueue *pqueue)
{
  int i;
  for (i = 0; i < pqueue->size; i++)
    {
      struct mara_node *c = pqueue->array[i];
      fprintf (stderr, " [pi=%d:id=%d:#adj=%d]",
               c->pqueue_index, c->node->id, c->adjacency);
    }
  fprintf (stderr, "\n");
}
#endif /*DEBUG*/

void
routing_ma_ordering (struct node *t, cmp_t cmp_func,
                     struct mara_node **mara_data)
{
  struct vector_node *vn;
  struct pqueue *pqueue;
  unsigned int label = 1;
  struct mara_node *c, *v;

  pqueue = pqueue_create ();
  pqueue->cmp = cmp_func;
  pqueue->update = mara_pqueue_index_update;

  c = &mara_data[t->id][t->id];
  c->node = t;
#define MARA_ADJ_INFINITY UINT_MAX
#define MARA_BW_INFINITY UINT_MAX
  c->adjacency = MARA_ADJ_INFINITY;
  c->bandwidth = MARA_BW_INFINITY;

  pqueue_enqueue (c, pqueue);

  while (pqueue->size)
    {
      c = pqueue_dequeue (pqueue);

      if (c->label != 0)
        continue;

      c->label = label++;
      v = c;

#ifdef DEBUG
      fprintf (stderr, "MA Ordering: dst: %d node[%d]: "
               "label: %d adjacency: %d\n",
               t->id, v->node->id, v->label, v->adjacency);
#endif /*DEBUG*/

      for (vn = vector_head (v->node->ilinks); vn; vn = vector_next (vn))
        {
          struct link *link = (struct link *) vector_data (vn);
          struct node *candidate = link->from;

          c = &mara_data[t->id][candidate->id];
          if (c->label > 0)
            continue;

          c->node = candidate;
          c->adjacency++;
          c->bandwidth += (link->bandwidth ? link->bandwidth : 0) ;

          if (c->pqueue_index < 0)
            pqueue_enqueue (c, pqueue);
          else
            pqueue_update (c->pqueue_index, pqueue);

#ifdef DEBUG
          fprintf (stderr, "MA Ordering: dst: %d candidate node[%d]: "
                   "adjacency: %d bandwidth: %f\n", t->id, candidate->id,
                   c->adjacency, c->bandwidth);
          routing_mara_pqueue_debug (pqueue);
#endif /*DEBUG*/
        }
    }

  pqueue_delete (pqueue);
}

void
routing_mara_route_node (struct node *s, struct routing *routing)
{
  struct vector_node *vn, *vnn;
  struct mara_node **mara_data = (struct mara_node **) routing->data;

  /* for each destination */
  for (vn = vector_head (s->g->nodes); vn; vn = vector_next (vn))
    {
      struct node *t = (struct node *) vector_data (vn);
      struct mara_node *sn = &mara_data[t->id][s->id];

      /* no label; the node s is disconnected from the node t */
      if (sn->label == 0)
        continue;

      /* set nexthop */
      nexthop_delete_all (routing->route[s->id][t->id].nexthops);
      for (vnn = vector_head (s->olinks); vnn; vnn = vector_next (vnn))
        {
          struct link *link = (struct link *) vector_data (vnn);
          struct node *nei = link->to;
          struct mara_node *nn = &mara_data[t->id][nei->id];

          /* direct edge from the higher-labeled node to the lower */
          if (sn->label > nn->label)
            route_add (s, t, nei, routing);
        }

      /* set nexthop to itself if the node is the destination */
      if (s == t)
        {
          assert (sn->label == 1);
          route_add (s, t, t, routing);
        }
    }
}

void
routing_mara_mc_node (struct node *node, struct routing *R)
{
  struct vector_node *vn;
  mara_data_clear (R->G, R->data);
  for (vn = vector_head (node->g->nodes); vn; vn = vector_next (vn))
    {
      struct node *t = (struct node *) vector_data (vn);
      routing_ma_ordering (t, mara_mc_cmp, R->data);
    }
  routing_mara_route_node (node, R);
}

void
routing_mara_mmmf_node (struct node *node, struct routing *R)
{
  struct vector_node *vn;
  mara_data_clear (R->G, R->data);
  for (vn = vector_head (node->g->nodes); vn; vn = vector_next (vn))
    {
      struct node *t = (struct node *) vector_data (vn);
      routing_ma_ordering (t, mara_mmmf_cmp, R->data);
    }
  routing_mara_route_node (node, R);
}

void
routing_mara_mc_all (struct routing *R)
{
  struct vector_node *vn;
  mara_data_clear (R->G, R->data);
  for (vn = vector_head (R->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *t = (struct node *) vector_data (vn);
      routing_ma_ordering (t, mara_mc_cmp, R->data);
    }
  for (vn = vector_head (R->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *s = (struct node *) vector_data (vn);
      routing_mara_route_node (s, R);
    }
}

void
routing_mara_mmmf_all (struct routing *R)
{
  struct vector_node *vn;
  mara_data_clear (R->G, R->data);
  for (vn = vector_head (R->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *t = (struct node *) vector_data (vn);
      routing_ma_ordering (t, mara_mmmf_cmp, R->data);
    }
  for (vn = vector_head (R->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *s = (struct node *) vector_data (vn);
      routing_mara_route_node (s, R);
    }
}

DEFINE_COMMAND (routing_algorithm_mara_mc,
                "routing-algorithm mara-mc",
                ROUTING_ALGORITHM_HELP_STR
                "MARA-MC (Maximizing Connectivity).\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  timer_counter_t start, end, res;
  struct vector_node *vn;

  if (routing->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified for routing.\n");
      return;
    }

  if (routing->data == NULL)
    routing->data = mara_data_create (routing->G);
  mara_data_clear (routing->G, routing->data);

  timer_count (start);
  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *t = (struct node *) vector_data (vn);
      routing_ma_ordering (t, mara_mc_cmp, routing->data);
    }
  timer_count (end);

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *s = (struct node *) vector_data (vn);
      routing_mara_route_node (s, routing);
    }

  timer_sub (start, end, res);
  fprintf (shell->terminal, "MARA-MC overall calculation "
           "time: %llu us\n", timer_to_usec (res));
}


DEFINE_COMMAND (routing_algorithm_mara_mmmf,
                "routing-algorithm mara-mmmf",
                ROUTING_ALGORITHM_HELP_STR
                "MARA-MMMF (Maximizing Minimum Max-Flow).\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  timer_counter_t start, end, res;
  struct vector_node *vn;

  if (routing->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified for routing.\n");
      return;
    }

  if (routing->data == NULL)
    routing->data = mara_data_create (routing->G);
  mara_data_clear (routing->G, routing->data);

  timer_count (start);
  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *t = (struct node *) vector_data (vn);
      routing_ma_ordering (t, mara_mmmf_cmp, routing->data);
    }
  timer_count (end);

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *s = (struct node *) vector_data (vn);
      routing_mara_route_node (s, routing);
    }

  timer_sub (start, end, res);
  fprintf (shell->terminal, "MARA-MMMF overall calculation "
           "time: %llu us\n", timer_to_usec (res));
}

DEFINE_COMMAND (routing_algorithm_mara_mc_node,
                "routing-algorithm mara-mc node " NODE_SPEC,
                ROUTING_ALGORITHM_HELP_STR
                "MARA-MC (Maximizing Connectivity).\n"
                "Specify node to calculate routes\n"
                NODE_SPEC_HELP_STR)
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct node *node;
  unsigned long node_id;
  timer_counter_t start, end, res;
  struct vector_node *vn;

  if (routing->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified for routing.\n");
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

  timer_count (start);
  for (vn = vector_head (node->g->nodes); vn; vn = vector_next (vn))
    {
      struct node *t = (struct node *) vector_data (vn);
      routing_ma_ordering (t, mara_mc_cmp, routing->data);
    }
  timer_count (end);

  routing_mara_route_node (node, routing);

  timer_sub (start, end, res);
  fprintf (shell->terminal, "MARA-MC calculation time "
           "for node %u: %llu us\n", node->id, timer_to_usec (res));
}

DEFINE_COMMAND (routing_algorithm_mara_mmmf_node,
                "routing-algorithm mara-mmmf node " NODE_SPEC,
                ROUTING_ALGORITHM_HELP_STR
                "MARA-MMMF (Maximizing Minimum Max-Flow).\n"
                "Specify node to calculate routes\n"
                NODE_SPEC_HELP_STR)
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct node *node;
  unsigned long node_id;
  timer_counter_t start, end, res;
  struct vector_node *vn;

  if (routing->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified for routing.\n");
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

  timer_count (start);
  for (vn = vector_head (node->g->nodes); vn; vn = vector_next (vn))
    {
      struct node *t = (struct node *) vector_data (vn);
      routing_ma_ordering (t, mara_mmmf_cmp, routing->data);
    }
  timer_count (end);

  routing_mara_route_node (node, routing);

  timer_sub (start, end, res);
  fprintf (shell->terminal, "MARA-MMMF calculation time "
           "for node %u: %llu us\n", node->id, timer_to_usec (res));
}

DEFINE_COMMAND (routing_algorithm_mara_mc_node_all,
                "routing-algorithm mara-mc node all",
                ROUTING_ALGORITHM_HELP_STR
                "MARA-MC (Maximizing Connectivity).\n"
                "Specify node to calculate routes\n"
                "Calculate on all node individually\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct node *node;
  struct vector_node *vn, *vni;
  timer_counter_t start, end, res;

  if (routing->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified for routing.\n");
      return;
    }

  if (routing->data == NULL)
    routing->data = mara_data_create (routing->G);

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;

      mara_data_clear (routing->G, routing->data);

      timer_count (start);
      for (vni = vector_head (node->g->nodes); vni; vni = vector_next (vni))
        {
          struct node *t = (struct node *) vector_data (vni);
          routing_ma_ordering (t, mara_mc_cmp, routing->data);
        }
      timer_count (end);

      routing_mara_route_node (node, routing);

      timer_sub (start, end, res);
      fprintf (shell->terminal, "MARA-MC calculation time "
               "for node %u: %llu us\n", node->id, timer_to_usec (res));
    }
}

DEFINE_COMMAND (routing_algorithm_mara_mmmf_node_all,
                "routing-algorithm mara-mmmf node all",
                ROUTING_ALGORITHM_HELP_STR
                "MARA-MMMF (Maximizing Minimum Max-Flow).\n"
                "Specify node to calculate routes\n"
                "Calculate on all node individually\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct node *node;
  struct vector_node *vn, *vni;
  timer_counter_t start, end, res;

  if (routing->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified for routing.\n");
      return;
    }

  if (routing->data == NULL)
    routing->data = mara_data_create (routing->G);

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      mara_data_clear (routing->G, routing->data);

      timer_count (start);
      for (vni = vector_head (node->g->nodes); vni; vni = vector_next (vni))
        {
          struct node *t = (struct node *) vector_data (vni);
          routing_ma_ordering (t, mara_mmmf_cmp, routing->data);
        }
      timer_count (end);

      routing_mara_route_node (node, routing);

      timer_sub (start, end, res);
      fprintf (shell->terminal, "MARA-MMMF calculation time "
               "for node %u: %llu us\n", node->id, timer_to_usec (res));
    }
}


