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
#include "timer.h"

#include "network/graph.h"
#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"

#include "routing/dijkstra.h"

#define ROUTING_ALGORITHM_LFI_HELP_STR \
  "Loop-Free-Invariant (LFI) based multipath algorithm.\n"

void
routing_lfi_node_calculation (struct node *node, struct node *neighbor,
                              struct routing *routing)
{
  struct dijkstra_path **dijkstra_data;
  struct dijkstra_path *dijkstra_table, *neighbor_table;
  struct vector_node *vn;

  dijkstra_data = (struct dijkstra_path **) routing->data;
  dijkstra_table = dijkstra_data[node->id];
  neighbor_table = dijkstra_data[neighbor->id];

  /* for each destination */
  for (vn = vector_head (node->g->nodes); vn; vn = vector_next (vn))
    {
      struct node *dst = (struct node *) vector_data (vn);

      /* ignore if the dest is the node itself */
      if (dst == node)
        continue;

      /* ignore for the routes the nodes failed to calculate */
      if (! dijkstra_table[dst->id].nexthops->size)
        continue;
      if (! neighbor_table[dst->id].nexthops->size)
        continue;

      /* add routes that satisfy loop-free-invariant condition */
      if (neighbor_table[dst->id].metric <
          dijkstra_table[dst->id].metric)
        route_add (node, dst, neighbor, routing);
    }
}

DEFINE_COMMAND (routing_algorithm_lfi_node,
                "routing-algorithm loop-free-invariant node " NODE_SPEC,
                ROUTING_ALGORITHM_HELP_STR
                ROUTING_ALGORITHM_LFI_HELP_STR
                "Specify node to calculate routes\n"
                NODE_SPEC_HELP_STR)
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct vector_node *vnn;
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
  dijkstra_data_clear (routing->G, dijkstra_data);

  timer_count (start);

  /* execute Dijkstra's SPF */
  routing_dijkstra (node, routing->W, routing);

  /* for each neighbor */
  for (vnn = vector_head (node->olinks); vnn; vnn = vector_next (vnn))
    {
      struct link *link = (struct link *) vnn->data;
      struct node *neighbor = link->to;

      /* execute Dijkstra's SPF */
      routing_dijkstra (neighbor, routing->W, routing);
    }

  timer_count (end);

  /* set routing table from spf result table */
  routing_dijkstra_route (node, routing);

  /* for each neighbor */
  for (vnn = vector_head (node->olinks); vnn; vnn = vector_next (vnn))
    {
      struct link *link = (struct link *) vnn->data;
      struct node *neighbor = link->to;

      /* calculate LFI routes */
      routing_lfi_node_calculation (node, neighbor, routing);
    }

  timer_sub (start, end, res);

  fprintf (shell->terminal, "Loop-Free-Invariant calculation time "
           "for node %u: %llu us\n",
           node->id, timer_to_usec (res));
}

DEFINE_COMMAND (routing_algorithm_lfi_node_all,
                "routing-algorithm loop-free-invariant node all",
                ROUTING_ALGORITHM_HELP_STR
                ROUTING_ALGORITHM_LFI_HELP_STR
                "Specify node to calculate routes\n"
                "Calculate on all node individually\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct vector_node *vn, *vnn;
  struct node *node;
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
  dijkstra_data_clear (routing->G, dijkstra_data);

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;

      /* clear all dijkstra table */
      dijkstra_data_clear (routing->G, dijkstra_data);

      timer_count (start);

      /* execute Dijkstra's SPF */
      routing_dijkstra (node, routing->W, routing);

      /* for each neighbor */
      for (vnn = vector_head (node->olinks); vnn; vnn = vector_next (vnn))
        {
          struct link *link = (struct link *) vnn->data;
          struct node *neighbor = link->to;

          /* execute Dijkstra's SPF */
          routing_dijkstra (neighbor, routing->W, routing);
        }

      timer_count (end);

      /* set routing table from spf result table */
      routing_dijkstra_route (node, routing);

      /* for each neighbor */
      for (vnn = vector_head (node->olinks); vnn; vnn = vector_next (vnn))
        {
          struct link *link = (struct link *) vnn->data;
          struct node *neighbor = link->to;

          /* calculate LFI routes */
          routing_lfi_node_calculation (node, neighbor, routing);
        }

      timer_sub (start, end, res);
      fprintf (shell->terminal, "Loop-Free-Invariant calculation time "
               "for node %u: %llu us\n",
               node->id, timer_to_usec (res));
    }
}


DEFINE_COMMAND (routing_algorithm_lfi,
                "routing-algorithm loop-free-invariant",
                ROUTING_ALGORITHM_HELP_STR
                ROUTING_ALGORITHM_LFI_HELP_STR)
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct vector_node *vn, *vnn;
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
  dijkstra_data_clear (routing->G, dijkstra_data);

  timer_count (start);

  /* calculate dijkstra for each node */
  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *node = (struct node *) vn->data;

      /* execute Dijkstra's SPF */
      routing_dijkstra (node, routing->W, routing);

      /* set routing table from spf result table */
      routing_dijkstra_route (node, routing);
    }

  timer_count (end);

  /* for each node */
  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      struct node *node = (struct node *) vn->data;

      /* for each neighbor */
      for (vnn = vector_head (node->olinks); vnn; vnn = vector_next (vnn))
        {
          struct link *link = (struct link *) vector_data (vnn);
          struct node *neighbor = link->to;

          /* calculate LFI routes */
          routing_lfi_node_calculation (node, neighbor, routing);
        }
    }

  timer_sub (start, end, res);
  fprintf (shell->terminal, "LFI overall calculation "
           "time: %llu us\n", timer_to_usec (res));
}


