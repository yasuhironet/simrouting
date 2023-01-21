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

#include "file.h"
#include "vector.h"
#include "shell.h"
#include "command.h"
#include "command_shell.h"
#include "module.h"

#include "network/graph.h"
#include "network/graph_cmd.h"

#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"

#include "routing/algorithms.h"

#include "interface/ampl.h"

struct command_set *cmdset_routing;
struct vector *routings;

unsigned long *tag_hash_table_create (unsigned long size);
void tag_hash_table_delete (unsigned long *table);

struct nexthop *
nexthop_create ()
{
  struct nexthop *nexthop;
  nexthop = (struct nexthop *) malloc (sizeof (struct nexthop));
  memset (nexthop, 0, sizeof (struct nexthop));
  return nexthop;
}

void
nexthop_delete (struct nexthop *nexthop)
{
  free (nexthop);
}

void
nexthop_delete_all (struct vector *nexthops)
{
  struct vector_node *vn;
  struct nexthop *nexthop;
  for (vn = vector_head (nexthops); vn; vn = vector_next (vn))
    {
      nexthop = (struct nexthop *) vn->data;
      vector_remove (nexthop, nexthops);
      nexthop_delete (nexthop);
    }
}

int
nexthop_cmp (void *a, void *b)
{
  struct nexthop *na = *(struct nexthop **) a;
  struct nexthop *nb = *(struct nexthop **) b;
  if (na == NULL && nb == NULL)
    return 0;
  if (na == NULL)
    return 1;
  if (nb == NULL)
    return -1;
  assert (na->node && nb->node);
  return (na->node->id - nb->node->id);
}

struct route **
route_table_create (int nnodes)
{
  int i, j;
  struct route **route;
  route = (struct route **) malloc (sizeof (struct route *) * nnodes);
  memset (route, 0, sizeof (struct route *) * nnodes);
  for (i = 0; i < nnodes; i++)
    {
      route[i] = (struct route *) malloc (sizeof (struct route) * nnodes);
      memset (route[i], 0, sizeof (struct route) * nnodes);
      for (j = 0; j < nnodes; j++)
        route[i][j].nexthops = vector_create ();
    }
  return route;
}

void
route_table_delete (int nnodes, struct route **route)
{
  int i, j;
  struct vector_node *vn;
  for (i = 0; i < nnodes; i++)
    for (j = 0; j < nnodes; j++)
      {
        for (vn = vector_head (route[i][j].nexthops); vn;
             vn = vector_next (vn))
          {
            struct nexthop *nexthop = (struct nexthop *) vn->data;
            if (nexthop)
              nexthop_delete (nexthop);
          }
        vector_delete (route[i][j].nexthops);
      }
  for (i = 0; i < nnodes; i++)
    free (route[i]);
  free (route);
}

struct nexthop *
nexthop_lookup (struct node *nexthop_node, struct vector *nexthops)
{
  struct vector_node *vn;
  struct nexthop *match = NULL;
  for (vn = vector_head (nexthops); vn; vn = vector_next (vn))
    {
      struct nexthop *nexthop = (struct nexthop *) vn->data;
      if (nexthop_node == nexthop->node)
        match = nexthop;
    }
  return match;
}

void
route_add (struct node *s, struct node *t, struct node *next,
           struct routing *routing)
{
  struct nexthop *nexthop;

  if (nexthop_lookup (next, routing->route[s->id][t->id].nexthops))
    return;

  nexthop = nexthop_create ();
  nexthop->node = next;
  nexthop->ratio = 0.0;
  vector_add (nexthop, routing->route[s->id][t->id].nexthops);
  vector_sort ((vector_cmp_t) nexthop_cmp, routing->route[s->id][t->id].nexthops);
}

struct routing *
routing_create ()
{
  struct routing *routing;
  routing = (struct routing *) malloc (sizeof (struct routing));
  memset (routing, 0, sizeof (struct routing));
  routing->config = vector_create ();
  return routing;
}

void
routing_delete (struct routing *routing)
{
  if (routing->name)
    free (routing->name);
  if (routing->route)
    route_table_delete (routing->nnodes, routing->route);
  command_config_clear (routing->config);
  vector_delete (routing->config);
  free (routing);
}

void
route_path_probability (struct path *path, struct node *dst,
                        struct routing *routing)
{
  struct node *current, *next;
  struct vector_node *vn;
  struct nexthop *n, *nexthop;
  int i;

  path->probability = 1.0;

  for (i = 0; i < path->path->size; i++)
    {
      current = vector_get (path->path, i);
      if (i + 1 < path->path->size)
        next = vector_get (path->path, i + 1);
      else
        next = NULL;

      if (! next)
        break;

      nexthop = NULL;
      for (vn = vector_head (routing->route[current->id][dst->id].nexthops); vn;
           vn = vector_next (vn))
        {
          n = (struct nexthop *) vn->data;
          if (n->node == next)
            nexthop = n;
        }
      assert (nexthop);
      path->probability *= nexthop->ratio;
    }
}

struct path *
route_path_random_forward (struct node *s, struct node *t,
                           struct routing *routing)
{
  struct path *path;
  struct node *current = NULL;
  struct vector *nexthops;
  struct nexthop *nexthop;
  double needle;
  double min, max;
  struct nexthop *selected;
  struct vector_node *vn;

  current = s;
  path = path_create ();
  vector_add_allow_dup (current, path->path);

  while (current != t)
    {
      nexthops = routing->route[current->id][t->id].nexthops;
      needle = (random () % 100000) * 0.00001;

      min = 0.0;
      max = 0.0;
      selected = NULL;
      for (vn = vector_head (nexthops); vn; vn = vector_next (vn))
        {
          nexthop = (struct nexthop *) vector_data (vn);
          min = max;
          max += nexthop->ratio;
          if (min <= needle && needle < max)
            selected = nexthop;
        }
      if (! selected)
        {
          fprintf (stderr, "s: %d t: %d current: %d path: ",
                   s->id, t->id, current->id);
          print_nodelist (stderr, path->path);
          fprintf (stderr, "\n");
          assert (selected);
        }

      current = selected->node;
      vector_add_allow_dup (current, path->path);
    }

  return path;
}

void
route_forward (struct path *path, struct node *dst,
               struct routing *routing)
{
  struct node *prev = NULL, *current = NULL, *next = NULL;
  struct nexthop *nexthop;

  while (path_end (path) != dst)
    {
      prev = current;
      current = path_end (path);

      nexthop = vector_get (routing->route[current->id][dst->id].nexthops, 0);
      next = nexthop->node;
      vector_add_allow_dup (next, path->path);
    }
}

struct path *
route_branch (struct path *path, struct node *dst,
              struct routing *routing)
{
  struct node *prev, *current, *next;
  int index;
  struct vector_node *vn;

  /* for each branch level */
  for (index = path->path->size - 1; index > 0; index--)
    {
      prev = vector_get (path->path, index - 1);
      current = vector_get (path->path, index);

      /* find current branch and next branch of the path */
      next = NULL;
      for (vn = vector_head (routing->route[prev->id][dst->id].nexthops); vn;
           vn = vector_next (vn))
        {
          struct nexthop *nexthop = (struct nexthop *) vn->data;
          struct node *node = nexthop->node;

          if (node != current)
            continue;

          vn = vector_next (vn);
          if (! vn)
            break;

          nexthop = (struct nexthop *) vn->data;
          next = nexthop->node;
          vector_break (vn);
          break;
        }

      /* change to the next branch */
      if (next)
        {
          vector_set (path->path, index, next);
          break;
        }

      /* remove already traversed part
         (and go to previous level of branching) */
      vector_remove_index (index, path->path);
    }

  /* no more branch, search end */
  if (index == 0)
    {
      path_delete (path);
      return NULL;
    }

  return path;
}

struct path *
route_path_first (struct node *src, struct node *dst,
                  struct routing *routing)
{
  struct path *path = path_create ();

  vector_add (src, path->path);

  if (routing->route[src->id][dst->id].nexthops->size == 0)
    {
      fprintf (stderr, "no route from %d to %d\n", src->id, dst->id);
      return path;
    }

  route_forward (path, dst, routing);
  assert (path_end (path) == dst);

  return path;
}

struct path *
route_path_next (struct path *path, struct node* dst,
                 struct routing *routing)
{
  /* go to the next branch of the path */
  path = route_branch (path, dst, routing);

  if (path)
    {
      /* lengthen path to the destination */
      route_forward (path, dst, routing);
      assert (path_end (path) == dst);
    }

  return path;
}

void
route_path_show (struct node *src, struct node *dst, struct routing *routing,
                 int detail, FILE *terminal)
{
  unsigned int s, t;
  unsigned int path_count;
  unsigned int path_size;
  unsigned int path_size_max;
  unsigned int path_size_min;
  struct path *path;

  /* do not count from dst to dst */
  if (src == dst)
    return;

  s = src->id;
  t = dst->id;

  /* ignore disconnected nodes */
  if (routing->route[s][t].nexthops->size == 0)
    {
      fprintf (terminal, "no route %d-%d, maybe disconnected\n", s, t);
      return;
    }

  path_count = 0;
  path_size = 0;
  path_size_max = 0;
  path_size_min = UINT_MAX;
  for (path = route_path_first (src, dst, routing); path;
       path = route_path_next (path, dst, routing))
    {
      route_path_probability (path, dst, routing);

      if (detail)
        {
          fprintf (terminal, "%d-%d route path: ", s, t);
          print_nodelist (terminal, path->path);
          fprintf (terminal, " (%f)\n", path->probability);
          fflush (terminal);
        }

      assert (path->path->size > 0);

      path_count++;
      path_size += path->path->size;
      if (path_size_max < path->path->size)
        path_size_max = path->path->size;
      if (path_size_min > path->path->size)
        path_size_min = path->path->size;
    }

  fprintf (terminal, "EVAL: %02d-%02d route #paths: %u "
           "avglen: %f maxlen: %u minlen: %u\n",
           s, t, path_count, (double) path_size / path_count,
           path_size_max, path_size_min);
  fflush (terminal);
}

DEFINE_COMMAND (show_route_path_source_destination,
                "show route path source <0-4294967295> destination <0-4294967295>",
                "display information\n"
                "display route\n"
                "display path along the routes\n"
                "specify source node\n"
                "node ID of the source\n"
                "specify destination node\n"
                "node ID of the destination\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct node *src, *dst;
  unsigned int s, t;
  int detail = 0;

  s = strtoul (argv[4], NULL, 0);
  t = strtoul (argv[6], NULL, 0);

  if (argc > 7 && ! strcmp ("detail", argv[7]))
    detail++;

  src = node_lookup (s, routing->G);
  dst = node_lookup (t, routing->G);

  route_path_show (src, dst, routing, detail, shell->terminal);
}

ALIAS_COMMAND (show_route_path_source_destination_detail,
                show_route_path_source_destination,
                "show route path source <0-4294967295> destination <0-4294967295> detail",
                "display information\n"
                "display route\n"
                "display path along the routes\n"
                "specify source node\n"
                "node ID of the source\n"
                "specify destination node\n"
                "node ID of the destination\n"
                "display detail\n");

DEFINE_COMMAND (show_route_path,
                "show route path",
                "display information\n"
                "display route\n"
                "display path along the routes\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct node *src, *dst;
  struct vector_node *vn, *vnn;
  int detail = 0;

  if (argc > 3 && ! strcmp ("detail", argv[3]))
    detail++;

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      src = (struct node *) vn->data;

      for (vnn = vector_head (routing->G->nodes); vnn; vnn = vector_next (vnn))
        {
          dst = (struct node *) vnn->data;

          route_path_show (src, dst, routing, detail, shell->terminal);
       }
    }
}

ALIAS_COMMAND (show_route_path_detail,
                show_route_path,
                "show route path detail",
                "display information\n"
                "display route\n"
                "display path along the routes\n"
                "display detail\n");

int nbits (unsigned long bitstring);


DEFINE_COMMAND (show_packet_forward,
                "show packet forward",
                "display information\n"
                "display packet\n"
                "display packet forwarding along the routes\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  struct vector_node *vn, *vnn;
  struct node *src, *dst, *node, *next;
  struct path *path;

  time_t seed;
  struct vector *router_value;
  struct vector *table;
  unsigned long rvalue, tag;
  unsigned long *rtable;
  int s, t, i;
  struct nexthop *nexthop;

  seed = time (NULL);
  fprintf (stderr, "seed: %lu\n", (unsigned long) seed);
  srandom ((unsigned int) seed);

  /* create router values */
  router_value = vector_create ();
  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
#define FLOW_LABEL_MASK 0x000fffff
      rtable = tag_hash_table_create (FLOW_LABEL_MASK);
#if 0
      for (i = 0; i < FLOW_LABEL_MASK; i++)
        fprintf (stderr, "node %d: tag hash table[%#lx]: %#lx\n", node->id, i, rtable[i]);
#endif
      vector_set (router_value, node->id, (void *) rtable);
    }

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      src = (struct node *) vn->data;
      s = src->id;

      for (vnn = vector_head (routing->G->nodes); vnn; vnn = vector_next (vnn))
        {
          dst = (struct node *) vnn->data;
          t = dst->id;

          /* do not count from dst to dst */
          if (src == dst)
            continue;

          for (i = 0; i < 1000; i++)
            {

              path = path_create ();
              vector_add (src, path->path);
              tag = (unsigned long) random () & FLOW_LABEL_MASK;

              while (path_end (path) != dst)
                {
                  node = path_end (path);
                  rtable = (unsigned long *) vector_get (router_value, node->id);
                  rvalue = rtable[tag];
                  table = routing->route[node->id][dst->id].nexthops;

                  fprintf (shell->terminal, "  forward[%d]: tag: %lx rvalue: %lx table-size=%d nth=%lu\n",
                           node->id, tag, rvalue, table->size, (tag + rvalue) % table->size);
                  nexthop = vector_get (table, (tag + rvalue) % table->size);
                  next = nexthop->node;
                  vector_add_allow_dup (next, path->path);
                }

              route_path_probability (path, dst, routing);
              fprintf (shell->terminal, "EVAL: %d-%d forward path: ", s, t);
              print_nodelist (shell->terminal, path->path);
              fprintf (shell->terminal, " (%f)\n", path->probability);
              fflush (shell->terminal);

              path_delete (path);
            }

        }
    }

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      rtable = vector_get (router_value, node->id);
      tag_hash_table_delete (rtable);
    }
  vector_delete (router_value);
}

DEFINE_COMMAND (routing_graph,
                "routing-graph NAME",
                "routing's base graph\n"
                "specify graph ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *R = (struct routing *) shell->context;
  struct graph *G = NULL;
  G = (struct graph *) instance_lookup ("graph", argv[1]);
  if (G == NULL)
    {
      fprintf (shell->terminal, "no such graph: graph-%s\n", argv[1]);
      return;
    }
  R->G = G;
  R->nnodes = graph_nodes (G);
  if (R->route)
    route_table_delete (R->nnodes, R->route);
  R->route = route_table_create (R->nnodes);
  command_config_add (R->config, argc, argv);
}

DEFINE_COMMAND (routing_weight,
                "routing-weight NAME",
                "routing's base weight\n"
                "specify weight ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *R = (struct routing *) shell->context;
  struct weight *W = NULL;
  if (R->G == NULL)
    {
      fprintf (shell->terminal,
               "no graph specified: do routing-graph first.\n");
      return;
    }
  W = (struct weight *) instance_lookup ("weight", argv[1]);
  if (W == NULL)
    {
      fprintf (shell->terminal, "no such weight: weight-%s\n", argv[1]);
      return;
    }
  if (W->G != R->G)
    {
      fprintf (shell->terminal, "error: base graph does not match.\n");
      return;
    }
  R->W = W;
  command_config_add (R->config, argc, argv);
}

DEFINE_COMMAND (show_route_nexthop_number,
                "show route nexthop-number",
                "display information\n"
                "display route information\n"
                "display the number of nexhops in the route\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;

  struct vector_node *vn, *vnn;
  struct node *node, *dst;
  int s, t;
  unsigned int nnodes = routing->G->nodes->size;

  unsigned int nexthops = 0;
  unsigned int st_pairs = 0;

  fprintf (shell->terminal, "EVAL: %2s-%2s %4s\n",
           "ss", "tt", "#nexthops");

  for (vn = vector_head (routing->G->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;
      s = node->id;

      for (vnn = vector_head (routing->G->nodes); vnn; vnn = vector_next (vnn))
        {
          dst = (struct node *) vnn->data;
          t = dst->id;

          /* do not count from dst to dst */
          if (node == dst)
            continue;

          st_pairs++;
          nexthops += routing->route[s][t].nexthops->size;

          fprintf (shell->terminal, "EVAL: %02d-%02d %4d\n",
                   s, t, routing->route[s][t].nexthops->size);

        }
    }

  fprintf (shell->terminal, "%6s %8s %8s\n",
           "#Nodes", "#s-t", "#nexthops");
  fprintf (shell->terminal, "%6d %8u %8u\n",
           nnodes, st_pairs, nexthops);
  fprintf (shell->terminal, "%6s %8s %8.2f\n",
           "Average", "", (double) nexthops / st_pairs);
}


void
show_routing_summary_header (FILE *terminal)
{
  fprintf (terminal, "%-8s %-12s %-12s\n",
           "Routing", "Name", "Routing-Graph");
}

void
show_routing_summary (FILE *terminal, void *instance)
{
  struct routing *R = (struct routing *) instance;
  if (R->G)
    fprintf (terminal, "%-8lu %-12s Graph-%lu\n",
             R->id, R->name, R->G->id);
  else
    fprintf (terminal, "%-8lu %-12s Graph-%s\n",
             R->id, R->name, "None");
}

void
routing_show_destination (FILE *terminal, struct routing *R,
                          u_int destination)
{
  int i;
  struct vector_node *vn;
  fprintf (terminal, "Destination: %d\n", destination);
  for (i = 0; i < R->nnodes; i++)
    {
      fprintf (terminal, "Node[%3d]: ", i);
      for (vn = vector_head (R->route[i][destination].nexthops); vn;
           vn = vector_next (vn))
        {
          struct nexthop *nexthop = (struct nexthop *) vn->data;
          if (nexthop)
            fprintf (terminal, " %3d", nexthop->node->id);
        }
      fprintf (terminal, "\n");
    }
}

void
routing_show_source (FILE *terminal, struct routing *R,
                     u_int source)
{
  int j;
  struct vector_node *vn;
  fprintf (terminal, "Node %d: Routing table:\n", source);
  for (j = 0; j < R->nnodes; j++)
    {
      fprintf (terminal, "[%2d]: ", j);
      for (vn = vector_head (R->route[source][j].nexthops); vn;
           vn = vector_next (vn))
        {
          struct nexthop *nexthop = (struct nexthop *) vn->data;
          if (nexthop)
            {
              fprintf (terminal, " %2d(%f)",
                       nexthop->node->id, nexthop->ratio);
            }
        }
      fprintf (terminal, "\n");
    }
}

void
show_routing_instance (FILE *terminal, void *instance)
{
  struct routing *R = (struct routing *) instance;
  int i;
  for (i = 0; i < R->nnodes; i++)
    routing_show_source (terminal, R, i);
}

DEFINE_COMMAND (show_routing,
                "show routing",
                "display information\n"
                "display routing information\n"
                "specify routing ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  show_routing_instance (shell->terminal, routing);
}

DEFINE_COMMAND (save_route_file,
                "save route file <FILENAME>",
                "save routes\n"
                "save routes to file\n"
                "specify filename to save\n")
{
  int i, j;
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  FILE *fp;
  struct vector_node *vn;
  struct nexthop *nexthop;

  fp = fopen (argv[3], "w+");
  if (! fp)
    {
      fprintf (shell->terminal, "cant open file: %s\n", argv[3]);
      return;
    }

  for (i = 0; i < routing->nnodes; i++)
    for (j = 0; j < routing->nnodes; j++)
      for (vn = vector_head (routing->route[i][j].nexthops); vn;
           vn = vector_next (vn))
        {
          nexthop = (struct nexthop *) vn->data;
          if (nexthop)
            fprintf (fp, "set route node %d destination %d "
                     "nexthop %d ratio %f\n",
                     i, j, nexthop->node->id, nexthop->ratio);
        }
  fclose (fp);
}

DEFINE_COMMAND (load_route_file,
                "load route file <FILENAME>",
                "load routes\n"
                "load routes from file\n"
                "specify filename to load\n")
{
  int i, j;
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  char buf[1024];
  FILE *fp;
  struct nexthop *nexthop;
  int nexthop_id;
  double nexthop_ratio;
  int ret;

  for (i = 0; i < routing->nnodes; i++)
    for (j = 0; j < routing->nnodes; j++)
      nexthop_delete_all (routing->route[i][j].nexthops);

  fp = fopen (argv[3], "r");
  if (! fp)
    {
      fprintf (shell->terminal, "cant open file: %s\n", argv[3]);
      return;
    }

  while (fgets (buf, sizeof (buf), fp))
    {
      if (strlen (buf) == sizeof (buf) - 1)
        fprintf (shell->terminal, "*** buffer too short ***\n");
      ret = sscanf (buf, "set route node %d destination %d "
                    "nexthop %d ratio %lf\n",
                    &i, &j, &nexthop_id, &nexthop_ratio);
      assert (ret == 4);
      nexthop = nexthop_create ();
      nexthop->node = node_get (nexthop_id, routing->G);
      nexthop->ratio = nexthop_ratio;
      vector_add (nexthop, routing->route[i][j].nexthops);
    }
  fclose (fp);
}

DEFINE_COMMAND (export_gnuplot_path_count,
                "export gnuplot path-count cumulative <FILENAME>",
                "export\n"
                "export to gnuplot data file\n"
                "export path count\n"
                "export path count by cumulative\n"
                "specify filename to export\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  FILE *fp;
  struct vector *x;
  struct vector_node *vn, *vnn, *vns, *vnt;
  int val, rest, total, path_count;
  struct path *path;

  fp = fopen (argv[4], "w+");
  if (! fp)
    {
      fprintf (shell->terminal, "cant open file: %s\n", argv[4]);
      return;
    }

  x = vector_create ();

  total = 0;
  for (vns = vector_head (routing->G->nodes); vns; vns = vector_next (vns))
    {
      struct node *src = (struct node *) vns->data;
      for (vnt = vector_head (routing->G->nodes); vnt; vnt = vector_next (vnt))
        {
          struct node *dst = (struct node *) vnt->data;

          if (src == dst)
            continue;

          path_count = 0;
          for (path = route_path_first (src, dst, routing); path;
               path = route_path_next (path, dst, routing))
            {
               if (path_end (path) == dst)
                 path_count++;
            }

          if (path_count == 0)
            continue;

          total++;

          val = (int) vector_get (x, path_count);
          if (val == 0)
            vector_set (x, path_count, (void *) 1);
          else
            vector_set (x, path_count, (void *) (val + 1));
        }
    }

  fprintf (fp, "# total = %d #nodes = %d \n", total, routing->G->nodes->size);
  for (vn = vector_head (x); vn; vn = vector_next (vn))
    {
      val = (int) vn->data;

      if (val == 0)
        continue;

      rest = 0;
      for (vnn = vector_head (x); vnn; vnn = vector_next (vnn))
        {
          if (vnn->index > vn->index)
            rest += (int) vnn->data;
        }

      fprintf (fp, "%d \t %f \t # #paths: %d #pairs: %d fraction: %f \n",
               vn->index, (double)(val + rest) / total * 100,
               vn->index, val, (double)val / total);
    }
  fclose (fp);
  vector_delete (x);
}


void
routing_init ()
{
  routings = vector_create ();
  cmdset_routing = command_set_create ();

  INSTALL_COMMAND (cmdset_routing, routing_graph);
  INSTALL_COMMAND (cmdset_routing, routing_weight);

  INSTALL_COMMAND (cmdset_routing, redirect_stderr_file);
  INSTALL_COMMAND (cmdset_routing, restore_stderr);
  INSTALL_COMMAND (cmdset_routing, redirect_terminal_file);
  INSTALL_COMMAND (cmdset_routing, restore_terminal);

  INSTALL_COMMAND (cmdset_routing, show_routing);
  INSTALL_COMMAND (cmdset_routing, show_route_nexthop_number);
  INSTALL_COMMAND (cmdset_routing, show_route_path);
  INSTALL_COMMAND (cmdset_routing, show_route_path_detail);
  INSTALL_COMMAND (cmdset_routing, show_route_path_source_destination);
  INSTALL_COMMAND (cmdset_routing, show_route_path_source_destination_detail);
  INSTALL_COMMAND (cmdset_routing, show_packet_forward);

  INSTALL_COMMAND (cmdset_routing, export_ampl_append_routing);
  INSTALL_COMMAND (cmdset_routing, export_gnuplot_path_count);

  INSTALL_COMMAND (cmdset_routing, save_route_file);
  INSTALL_COMMAND (cmdset_routing, load_route_file);

  INSTALL_COMMAND (cmdset_routing, import_ampl_routing_ratio);

  /* install commands for routing algorithms */
  routing_algorithms_commands (cmdset_routing);
}

void
routing_finish ()
{
  struct vector_node *vn;
  struct routing *routing;
  for (vn = vector_head (routings); vn; vn = vector_next (vn))
    {
      routing = (struct routing *) vn->data;
      if (routing)
        routing_delete (routing);
    }
  vector_delete (routings);
}

void *
create_routing (char *id)
{
  struct routing *w;
  w = routing_create ();
  w->name = strdup (id);
  return (void *) w;
}

char *
routing_name (void *instance)
{
  struct routing *routing = (struct routing *) instance;
  return routing->name;
}

void
routing_config_write (void *instance, FILE *out)
{
  struct routing *routing = (struct routing *) instance;
  command_config_write (routing->config, out);
}

struct module mod_routing =
{
  "routing",
  routing_init,
  routing_finish,
  &routings,
  create_routing,
  routing_name,
  show_routing_summary_header,
  show_routing_summary,
  show_routing_instance,
  &cmdset_routing,
  routing_config_write
};


