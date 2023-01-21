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

#include "network/graph.h"
#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"

#include "routing/dijkstra.h"


DEFINE_COMMAND (routing_algorithm_2_dijkstra,
                "routing-algorithm 2-dijkstra source <0-4294967295> sink <0-4294967295> <FILENAME>",
                "calculate routing using algorithm\n"
                "twice execution of dijkstra's SPF calculation\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  unsigned long s, t;
  FILE *fp;
  struct candidate **table1, **table2;
  struct path *path1, *path2;
  struct node *source, *a, *b;
  struct graph *G2;
  struct vector_node *vn;
  struct link *link;

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

  s = strtoul (argv[3], NULL, 0);
  t = strtoul (argv[5], NULL, 0);
  fp = fopen_create (argv[6], "w+");

  /* prepare spf result table */
  table1 = (struct candidate **) malloc (sizeof (struct candidate *) * routing->nnodes);
  memset (table1, 0, sizeof (struct candidate *) * routing->nnodes);

  /* execute first dijkstra */
  source = node_lookup (s, routing->G);
  routing_dijkstra (source, routing->W, table1, NULL);

  path1 = table1[t]->paths->array[0];
  fprintf (fp, "first path: cost: %lu path: ", path1->cost);
  print_nodelist (fp, path1->path);
  fprintf (fp, "\n");

  /* new graph with first path removed */
  G2 = graph_copy_create (routing->G);
  a = b = NULL;
  for (vn = vector_head (path1->path); vn; vn = vector_next (vn))
    {
      a = b;
      b = (struct node *) vn->data;
      if (a == NULL)
        continue;

      link = link_lookup_by_node_id (a->id, b->id, G2);
      link_delete (link);
      link = link_lookup_by_node_id (b->id, a->id, G2);
      link_delete (link);
    }

  /* prepare spf result table */
  table2 = (struct candidate **) malloc (sizeof (struct candidate *) * routing->nnodes);
  memset (table2, 0, sizeof (struct candidate *) * routing->nnodes);

  /* execute first dijkstra */
  source = node_lookup (s, G2);
  routing_dijkstra (source, routing->W, table2, NULL);

  path2 = table2[t]->paths->array[0];
  fprintf (fp, "second path: cost: %lu path: ", path2->cost);
  print_nodelist (fp, path2->path);
  fprintf (fp, "\n");
}



