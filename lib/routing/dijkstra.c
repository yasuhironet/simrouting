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

struct dijkstra_path *
dijkstra_table_create (struct graph *graph)
{
  struct dijkstra_path *table;
  int i;
  table = (struct dijkstra_path *)
    calloc (graph->nodes->size, sizeof (struct dijkstra_path));
  for (i = 0; i < graph->nodes->size; i++)
    {
      table[i].pqueue_index = -1;
      table[i].nexthops = vector_create ();
    }
  return table;
}

void
dijkstra_table_delete (struct graph *graph, struct dijkstra_path *table)
{
  int i;
  for (i = 0; i < graph->nodes->size; i++)
    {
      if (table[i].nexthops)
        vector_delete (table[i].nexthops);
    }
  free (table);
}

void
dijkstra_table_clear (struct graph *graph, struct dijkstra_path *table)
{
  int i;
  for (i = 0; i < graph->nodes->size; i++)
    {
      table[i].node = NULL;
      table[i].metric = 0;
      vector_clear (table[i].nexthops);
      table[i].pqueue_index = -1;
    }
}

void *
dijkstra_data_create (struct graph *graph)
{
  struct dijkstra_path **data;
  data = (struct dijkstra_path **)
    calloc (graph->nodes->size, sizeof (struct dijkstra_path *));
  return data;
}

void
dijkstra_data_delete (struct graph *graph, void *data)
{
  struct dijkstra_path **dijkstra_data;
  int i;

  dijkstra_data = (struct dijkstra_path **) data;
  for (i = 0; i < graph->nodes->size; i++)
    dijkstra_table_delete (graph, dijkstra_data[i]);

  free (data);
}

int
dijkstra_candidate_cmp (void *a, void *b)
{
  struct dijkstra_path *ca = (struct dijkstra_path *) a;
  struct dijkstra_path *cb = (struct dijkstra_path *) b;
  return (ca->metric - cb->metric);
}

void
dijkstra_candidate_update (void *data, int index)
{
  struct dijkstra_path *p = (struct dijkstra_path *) data;
  p->pqueue_index = index;
}

void
routing_dijkstra (struct node *root, struct weight *weight,
                        struct routing *R)
{
  struct dijkstra_path *c, *v;
  struct pqueue *candidate_list;
  struct vector_node *vn;
  struct dijkstra_path **dijkstra_data;
  struct dijkstra_path *dijkstra_table;

  dijkstra_data = (struct dijkstra_path **) R->data;
  dijkstra_table = dijkstra_data[root->id];

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

      /* for each node that is reachable through "v" */
      for (vn = vector_head (v->node->olinks); vn;
           vn = vector_next (vn))
        {
          struct link *edge = (struct link *) vn->data;
          unsigned int edge_cost = 0;

          /* new candidate */
          c = &dijkstra_table[edge->to->id];
          c->node = edge->to;

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
          if (v->node == root)
            vector_add (c->node, c->nexthops);
          else
            vector_merge (c->nexthops, v->nexthops);

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
routing_dijkstra_route (struct node *root, struct routing *R)
{
  struct vector_node *vn, *vni;
  int s = root->id;
  struct dijkstra_path **dijkstra_data;
  struct dijkstra_path *dijkstra_table;

  dijkstra_data = (struct dijkstra_path **) R->data;
  dijkstra_table = dijkstra_data[root->id];

  /* set routing table from spf result table */
  for (vn = vector_head (root->g->nodes); vn; vn = vector_next (vn))
    {
      struct node *dst = (struct node *) vn->data;
      int t = dst->id;
      struct dijkstra_path *path = &dijkstra_table[dst->id];

      nexthop_delete_all (R->route[s][t].nexthops);

      if (! path->nexthops)
        continue;

      for (vni = vector_head (path->nexthops); vni; vni = vector_next (vni))
        {
          struct node *nexthop = (struct node *) vector_data (vni);
          route_add (root, dst, nexthop, R);
        }
    }
}

void
routing_dijkstra_node (struct node *root, struct routing *R)
{
  /* execute dijkstra */
  routing_dijkstra (root, R->W, R);

  /* set routing table from spf result table */
  routing_dijkstra_route (root, R);
}

void
routing_dijkstra_all_node (struct routing *routing)
{
  struct vector_node *vn;
  struct node *node;

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      if (node == NULL)
        continue;
      routing_dijkstra_node (node, routing);
    }
}

DEFINE_COMMAND (routing_algorithm_dijkstra_node,
                "routing-algorithm dijkstra node "NODE_SPEC,
                ROUTING_ALGORITHM_HELP_STR
                "Dijkstra's SPF calculation by labeling (faster).\n"
                "Specify node to calculate routes\n"
                NODE_SPEC_HELP_STR)
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct node *node;
  unsigned long node_id;
  struct dijkstra_path **dijkstra_data;
  timer_counter_t start, end, res;

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
  if (routing->G != routing->W->G)
    {
      fprintf (shell->terminal, "base graph does not match with weight's.\n");
      return;
    }

  node_id = strtoul (argv[3], NULL, 0);
  node = node_lookup (node_id, routing->G);
  if (! node)
    {
      fprintf (shell->terminal, "No such node: %lu\n", node_id);
      return;
    }

  /* prepare dijkstra table */
  if (! routing->data)
    routing->data = dijkstra_data_create (routing->G);
  dijkstra_data = (struct dijkstra_path **) routing->data;
  if (! dijkstra_data[node->id])
    dijkstra_data[node->id] = dijkstra_table_create (routing->G);
  dijkstra_table_clear (routing->G, dijkstra_data[node->id]);

  timer_count (start);

  /* execute Dijkstra's SPF */
  routing_dijkstra_node (node, routing);

  timer_count (end);

  timer_sub (start, end, res);
  fprintf (shell->terminal, "Dijkstra calculation time "
           "for node %u: %llu us\n",
           node->id, timer_to_usec (res));
}

DEFINE_COMMAND (routing_algorithm_dijkstra_node_all,
                "routing-algorithm dijkstra node all",
                ROUTING_ALGORITHM_HELP_STR
                "Dijkstra's SPF calculation.\n"
                "Specify node to calculate routes\n"
                "Calculate on all node individually\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct node *node;
  struct vector_node *vn;
  struct dijkstra_path **dijkstra_data;
  timer_counter_t start, end, res;

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
  if (routing->G != routing->W->G)
    {
      fprintf (shell->terminal, "base graph does not match with weight's.\n");
      return;
    }

  /* prepare dijkstra table */
  if (! routing->data)
    routing->data = dijkstra_data_create (routing->G);
  dijkstra_data = (struct dijkstra_path **) routing->data;

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;

      if (! dijkstra_data[node->id])
        dijkstra_data[node->id] = dijkstra_table_create (routing->G);
      dijkstra_table_clear (routing->G, dijkstra_data[node->id]);

      timer_count (start);

      /* execute Dijkstra's SPF */
      routing_dijkstra_node (node, routing);

      timer_count (end);

      timer_sub (start, end, res);
      fprintf (shell->terminal, "Dijkstra calculation time"
               " for node %u: %llu us\n",
               node->id, timer_to_usec (res));
    }
}


DEFINE_COMMAND (routing_algorithm_dijkstra,
                "routing-algorithm dijkstra",
                ROUTING_ALGORITHM_HELP_STR
                "Dijkstra's SPF calculation.\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct vector_node *vn;
  struct node *node;
  struct dijkstra_path **dijkstra_data;
  int i;
  timer_counter_t start, end, res;

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
  if (routing->G != routing->W->G)
    {
      fprintf (shell->terminal, "base graph does not match with weight's.\n");
      return;
    }

  /* prepare dijkstra table */
  if (! routing->data)
    routing->data = dijkstra_data_create (routing->G);
  dijkstra_data = (struct dijkstra_path **) routing->data;
  for (i = 0; i < routing->G->nodes->size; i++)
    {
      if (! dijkstra_data[i])
        dijkstra_data[i] = dijkstra_table_create (routing->G);
      dijkstra_table_clear (routing->G, dijkstra_data[i]);
    }

  timer_count (start);

  /* calculate dijkstra for each node */
  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      routing_dijkstra_node (node, routing);
    }

  timer_count (end);

  timer_sub (start, end, res);
  fprintf (shell->terminal,
           "Dijkstra overall calculation time: %llu us\n",
           timer_to_usec (res));
}


