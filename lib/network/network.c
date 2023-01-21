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
#include "module.h"

#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"
#include "network/graph.h"
#include "network/graph_cmd.h"
#include "traffic-model/demand.h"
#include "network/network.h"

#if 0
#include "routing/deflection.h"
#include "routing/dijkstra.h"
#endif /*0*/

struct network *
network_create ()
{
  struct network *network;
  network = (struct network *) malloc (sizeof (struct network));
  memset (network, 0, sizeof (struct network));
  network->config = vector_create ();
  return network;
}

void
network_delete (struct network *network)
{
  int i;
  struct vector_node *vn;
  struct flow *flow;

  if (network->name)
    free (network->name);

  if (network->flows)
    {
      for (vn = vector_head (network->flows); vn; vn = vector_next (vn))
        {
          flow = (struct flow *) vn->data;
          vector_delete (flow->path);
          free (flow);
        }
      vector_delete (network->flows);
    }

  if (network->flows_on_node)
    {
      for (i = 0; i < network->nnodes; i++)
        vector_delete (network->flows_on_node[i]);
      free (network->flows_on_node);
    }

  if (network->flows_on_node)
    {
      for (i = 0; i < network->nedges; i++)
        vector_delete (network->flows_on_edge[i]);
      free (network->flows_on_edge);
    }

  command_config_clear (network->config);
  vector_delete (network->config);
  free (network);
}

struct command_set *cmdset_network;
struct vector *networks;

DEFINE_COMMAND (network_graph,
                "network-graph NAME",
                "network's base graph\n"
                "specify graph ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct network *N = (struct network *) shell->context;
  struct graph *G = NULL;
  int i;

  G = (struct graph *) instance_lookup ("graph", argv[1]);
  if (G == NULL)
    {
      fprintf (shell->terminal, "no such graph: graph-%s\n", argv[1]);
      return;
    }
  N->G = G;

  N->nnodes = graph_nodes (N->G);
  N->nedges = graph_edges (N->G);

  if (N->flows_on_node)
    {
      for (i = 0; i < N->nnodes; i++)
        vector_delete (N->flows_on_node[i]);
      free (N->flows_on_node);
    }
  N->flows_on_node = (struct vector **)
    malloc (sizeof (struct vector *) * N->nnodes);
  for (i = 0; i < N->nnodes; i++)
    N->flows_on_node[i] = vector_create ();

  if (N->flows_on_edge)
    {
      for (i = 0; i < N->nedges; i++)
        vector_delete (N->flows_on_edge[i]);
      free (N->flows_on_edge);
    }
  N->flows_on_edge = (struct vector **)
    malloc (sizeof (struct vector *) * N->nedges);
  for (i = 0; i < N->nedges; i++)
    N->flows_on_edge[i] = vector_create ();

  if (N->flows_drop_on_node)
    {
      for (i = 0; i < N->nnodes; i++)
        vector_delete (N->flows_drop_on_node[i]);
      free (N->flows_drop_on_node);
    }
  N->flows_drop_on_node = (struct vector **)
    malloc (sizeof (struct vector *) * N->nnodes);
  for (i = 0; i < N->nnodes; i++)
    N->flows_drop_on_node[i] = vector_create ();

  command_config_add (N->config, argc, argv);
}

DEFINE_COMMAND (network_traffic,
                "network-traffic <0-4294967295>",
                "network's traffic\n"
                "specify traffic ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct network *N = (struct network *) shell->context;
  struct traffic *T = NULL;
  if (N->G == NULL)
    {
      fprintf (shell->terminal,
               "no graph specified: do network-graph first.\n");
      return;
    }
  T = (struct traffic *) instance_lookup ("traffic", argv[1]);
  if (T == NULL)
    {
      fprintf (shell->terminal, "no such traffic: traffic-%s\n", argv[1]);
      return;
    }
  if (T->G != N->G)
    {
      fprintf (shell->terminal, "error: base graph does not match.\n");
      return;
    }
  N->T = T;
  command_config_add (N->config, argc, argv);
}

DEFINE_COMMAND (network_routing,
                "network-routing NAME",
                "network's routing\n"
                "specify routing ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct network *N = (struct network *) shell->context;
  struct routing *R = NULL;
  if (N->G == NULL)
    {
      fprintf (shell->terminal,
               "no graph specified: do network-graph first.\n");
      return;
    }
  R = (struct routing *) instance_lookup ("routing", argv[1]);
  if (R == NULL)
    {
      fprintf (shell->terminal, "no such routing: routing-%s\n", argv[1]);
      return;
    }
  if (R->G != N->G)
    {
      fprintf (shell->terminal, "error: base graph does not match.\n");
      return;
    }
  N->R = R;
  command_config_add (R->config, argc, argv);
}

void
load_flow (u_int source, u_int sink, demand_t demand, struct network *N)
{
  struct flow *flow;

  if (demand == 0.0)
    return;

  flow = (struct flow *) malloc (sizeof (struct flow));
  memset (flow, 0, sizeof (struct flow));
  flow->source = source;
  flow->sink = sink;
  flow->bandwidth = demand;
  flow->path = vector_create ();
  vector_add (flow, N->flows);

  vector_add ((void *)source, flow->path);
  vector_add (flow, N->flows_on_node[source]);

  fprintf (stderr, "load_flow: %2u->%2u: %f\n", source, sink, demand);
}

struct flow *
flow_divide (struct flow *flow, double ratio, struct network *N)
{
  int p, q;
  struct flow *newflow;
  struct vector_node *vn;

  newflow = (struct flow *) malloc (sizeof (struct flow));
  memset (newflow, 0, sizeof (struct flow));
  newflow->source = flow->source;
  newflow->sink = flow->sink;
  newflow->bandwidth = flow->bandwidth * ratio;
  newflow->path = vector_create ();
  vector_add (newflow, N->flows);

  p = -1;
  for (vn = vector_head (flow->path); vn; vn = vector_next (vn))
    {
      q = (int) vn->data;
      vector_add ((void *)q, newflow->path);
      if (p >= 0)
        {
          struct link *link = link_get_by_node_id (p, q, N->G);
          vector_add (newflow, N->flows_on_edge[link->id]);
        }
      p = q;
    }
  return newflow;
}

int
is_loading_complete (struct network *N)
{
  int i;
  for (i = 0; i < N->nnodes; i++)
    if (N->flows_on_node[i]->size)
      return 0;
  return 1;
}

void
load_traffic_flows (struct network *N)
{
  int i, j;
  struct demand_matrix *demands;
  struct flow *flow;
  struct vector_node *vni, *vnj, *vnk;
  double drop_ratio;

  demands = demand_matrix_copy (N->T->demands);
  for (i = 0; i < N->nnodes; i++)
    for (j = 0; j < N->nnodes; j++)
      if (i != j)
        load_flow (i, j, demands->traffic[i][j], N);

  while (! is_loading_complete (N))
  for (i = 0; i < N->nnodes; i++)
    for (vni = vector_head (N->flows_on_node[i]); vni; vni = vector_next (vni))
      {
        fprintf (stderr, "vni->index: %d, vector->size: %d\n",
                 vni->index, N->flows_on_node[i]->size);
        fprintf (stderr, "vni->data: %p, vector->array[%d]: %p\n",
                 vni->data, vni->index, N->flows_on_node[i]->array[vni->index]);
        flow = (struct flow *) vni->data;
        vector_remove (flow, N->flows_on_node[i]);
        fprintf (stderr, "flow(%p): %u->%u(%f): @node[%d]\n",
                 flow, flow->source, flow->sink, flow->bandwidth, i);

        drop_ratio = 1.0;
        for (vnj = vector_head (N->R->route[i][flow->sink].nexthops); vnj;
             vnj = vector_next (vnj))
          {
            struct nexthop *nexthop = (struct nexthop *) vnj->data;
            struct link *link = link_get_by_node_id (i, nexthop->node->id, N->G);
            struct flow *newflow = flow_divide (flow, nexthop->ratio, N);
            drop_ratio -= nexthop->ratio;

            vector_add ((void *)nexthop->node->id, newflow->path);
            vector_add (newflow, N->flows_on_edge[link->id]);
            if (newflow->sink != nexthop->node->id)
              vector_add (newflow, N->flows_on_node[nexthop->node->id]);

            fprintf (stderr, "    ->%u(%p): %u->%u(%f/%f) @edge[%u](%u-%u)\n",
                     nexthop->node->id, newflow, 
                     newflow->source, newflow->sink,
                     newflow->bandwidth, flow->bandwidth, link->id,
                     i, nexthop->node->id);

            fprintf (stderr, "      newflow(%u->%u)[%p]:",
                     newflow->source, newflow->sink, newflow);
            for (vnk = vector_head (newflow->path); vnk; vnk = vector_next (vnk))
              fprintf (stderr, " %d", (int) vnk->data);
            fprintf (stderr, "\n");
          }

        fprintf (stderr, "    path flow(%u->%u)[%p]:",
                 flow->source, flow->sink, flow);
        for (vnj = vector_head (flow->path); vnj; vnj = vector_next (vnj))
          fprintf (stderr, " %d", (int) vnj->data);
        fprintf (stderr, "\n");

#if 1
        flow->bandwidth *= drop_ratio;
        vector_add (flow, N->flows_drop_on_node[i]);
#else
        p = -1;
        for (vnj = vector_head (flow->path); vnj; vnj = vector_next (vnj))
          {
            q = (int) vnj->data;
            if (p >= 0)
              {
                struct link *link = link_lookup_index (p, q, N->G);
                fprintf (stderr, "  remove flow(%u->%u)[%p] from edge[%u](%u-%u)\n",
                         flow->source, flow->sink, flow, link->id, p, q);
                vector_remove (flow, N->flows_on_edge[link->id]);
              }
            p = q;
          }

        vector_remove (flow, N->flows);
        vector_delete (flow->path);
        free (flow);
#endif
      }

  demand_matrix_delete (demands);
}

DEFINE_COMMAND (network_load_traffic_flows,
                "network-load traffic-flows",
                "load traffic flows on network\n"
                "load traffic flows on network\n")
{
  struct shell *shell = (struct shell *) context;
  struct network *N = (struct network *) shell->context;
  struct vector_node *vn;
  if (N->G == NULL)
    {
      fprintf (shell->terminal, "no graph specified: do network-graph first.\n");
      return;
    }
  if (N->T == NULL)
    {
      fprintf (shell->terminal, "no traffic specified: do network-traffic first.\n");
      return;
    }
  if (N->R == NULL)
    {
      fprintf (shell->terminal, "no routing specified: do network-routing first.\n");
      return;
    }

  if (N->flows)
    {
      for (vn = vector_head (N->flows); vn; vn = vector_next (vn))
        {
          struct flow *flow = (struct flow *) vn->data;
          vector_remove (flow, N->flows);
          vector_delete (flow->path);
          free (flow);
        }
      vector_delete (N->flows);
    }
  N->flows = vector_create ();

  load_traffic_flows (N);
  command_config_add (N->config, argc, argv);
}

void
show_network_summary_header (FILE *terminal)
{
  fprintf (terminal, "%-8s %-12s %-12s\n",
           "Network", "Name", "Network-Graph");
}

void
show_network_summary (FILE *terminal, void *instance)
{
  struct network *N = (struct network *) instance;
  if (N->G)
    fprintf (terminal, "%-8lu %-12s Graph-%lu\n",
             N->id, N->name, N->G->id);
  else
    fprintf (terminal, "%-8lu %-12s Graph-%s\n",
             N->id, N->name, "None");
}

void
show_network_instance (FILE *terminal, void *instance)
{
  struct network *N = (struct network *) instance;
  int i;
  struct vector_node *vni, *vnj, *vnk;
  struct flow *flow;
  double load, util;
  struct node *node;

  double avg, med, max, min;
  double vari;
  double usum, usumsq;
  double n;

  fprintf (terminal, "Network %lu:\n", N->id);

  fprintf (terminal, "#Flows: %d\n", N->flows->size);
  for (vni = vector_head (N->flows); vni; vni = vector_next (vni))
    {
      flow = (struct flow *) vni->data;
      fprintf (terminal, "Flow: %2d->%2d: %f:",
               flow->source, flow->sink, flow->bandwidth);
      for (vnj = vector_head (flow->path); vnj; vnj = vector_next (vnj))
        fprintf (terminal, " %d", (int) vnj->data);
      fprintf (terminal, "\n");
    }

  for (i = 0; i < N->nnodes; i++)
    fprintf (terminal, "Node[%d]: %d flows left\n", i, N->flows_on_node[i]->size);

  for (i = 0; i < N->nnodes; i++)
    {
      double total_dropped = 0.0;
      for (vni = vector_head (N->flows_drop_on_node[i]); vni; vni = vector_next (vni))
        {
          flow = (struct flow *) vni->data;
          total_dropped += flow->bandwidth;
#if 0
          fprintf (terminal, "dropped: on %d: %d->%d, bandwidth %f\n",
                   i, flow->source, flow->sink, flow->bandwidth);
#endif
        }
      fprintf (terminal, "Node[%d]: total bandwidth of flows dropped: %f\n",
               i, total_dropped);
    }

  med = max = min = 0.0;
  usum = usumsq = 0.0;
  vari = 0.0;

  fprintf (terminal, "Edge[%2s]: %3s %3s %9s %9s %5s %6s\n",
           "##", "src", "dst", "Load", "Bandwidth", "Util", "#Flows");

  for (vni = vector_head (N->G->nodes); vni; vni = vector_next (vni))
    {
      node = (struct node *) vni->data;
      for (vnj = vector_head (node->olinks); vnj; vnj = vector_next (vnj))
        {
          struct link *link = (struct link *) vnj->data;
          i = link->id;

          load = 0.0;
          for (vnk = vector_head (N->flows_on_edge[i]); vnk;
               vnk = vector_next (vnk))
            {
              flow = (struct flow *) vnk->data;
              load += flow->bandwidth;
            }

          util = load / link->bandwidth;
          fprintf (terminal, "Edge[%2d]: %3u %3u %9.3f %9.3f %5.3f %6d\n",
                   i, link->from->id, link->to->id, load, link->bandwidth, util,
                   N->flows_on_edge[i]->size);

          if (max < util)
            max = util;
          if (util != 0.0 && (min == 0.0 || min > util))
            min = util;
          usum += util;
          usumsq += util * util;
        }
    }

  n = graph_edges (N->G);
  avg = usum / n;
  vari = usumsq / n - avg * avg;
  med = (max - min) / 2 + min;

  fprintf (terminal, "Util: routing-%lu: max: %f min: %f med: %f avg: %f std: %f\n",
           N->R->id, max, min, med, avg, sqrt (vari));
}

DEFINE_COMMAND (show_network,
                "show network",
                "display information\n"
                "display network information\n"
                "specify network ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct network *network = (struct network *) shell->context;
  show_network_instance (shell->terminal, network);
}

DEFINE_COMMAND (show_flows_on_link,
                "show flows on link <0-4294967295>",
                "display information\n"
                "display network-flow information\n"
                "display network-flow information\n"
                "display network-flow information\n"
                "specify link ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct network *network = (struct network *) shell->context;
  unsigned long id = 0;
  struct vector_node *vni, *vnj;
  struct flow *flow;

  id = strtoul (argv[4], NULL, 0);

  fprintf (shell->terminal, "#Flows on link[%lu]: %d\n",
           id, network->flows_on_edge[id]->size);
  for (vni = vector_head (network->flows_on_edge[id]); vni; vni = vector_next (vni))
    {
      flow = (struct flow *) vni->data;

      if (flow->bandwidth == 0.0)
        continue;

      fprintf (shell->terminal, "Flow: %2d->%2d: %f:",
               flow->source, flow->sink, flow->bandwidth);
      for (vnj = vector_head (flow->path); vnj; vnj = vector_next (vnj))
        fprintf (shell->terminal, " %d", (int) vnj->data);
      fprintf (shell->terminal, "\n");
    }
}



/* Dependability simulation */

/* Sieve of Eratosthenes */
struct vector *
prime_vector_create (int limit)
{
  int i, p;
  int *array;
  struct vector *primvec = vector_create ();
  int maxprim;

  array = (int *) malloc (sizeof (int) * (limit + 1));
  assert (array);

  for (i = 0; i <= limit; i++)
    array[i] = i;

  array[1] = 0;

  while (1)
    {
      if (primvec->size)
        {
          maxprim = (int) vector_get (primvec, primvec->size - 1);
          fprintf (stderr, "limit: %d, maxprim: %d\n", limit, maxprim);
          if (limit < maxprim * maxprim)
            break;
        }

      /* first value in the array is a prime */
      p = 0;
      while (! array[p])
        p++;

      /* move the prime to the prime vector */
      vector_add ((void *) array[p], primvec);
      array[p] = 0;

      /* delete all multiples of the prime */
      for (i = 0; p * i <= limit; i++)
        array[p * i] = 0;
    }

  for (i = 0; i <= limit; i++)
    if (array[i])
      vector_add ((void *) i, primvec);

  return primvec;
}

int
chash_selection (int router_id, unsigned int tag,
                 void *hash_tables)
{
  unsigned long *table;
  unsigned long seed;
  table = (unsigned long *) vector_get (hash_tables, router_id);
  seed = table[tag];
  return (tag + seed);
}

int
deflection_selection (int router_id, unsigned int tag,
                      void *router_primes)
{
  int prime;
  prime = (int) vector_get (router_primes, router_id);
#if 0
  fprintf (stderr, "    router[%d]: prime: %d tag: %d nhindex: %d\n",
           router_id, prime, tag, (tag % prime));
#endif
  return (tag % prime);
}

#if 0
void
deflect_1_tag_forward (struct node *dst, unsigned int tag,
                       struct deflection_info **dinfo_table,
                       int (*nhselection) (int, unsigned int, void *),
                       void *router_seeds, struct path *path)
{
  struct node *prev = NULL, *current = NULL, *next = NULL;
  struct vector *deflection_set;
  int index;

  /* forward until the destination is reached */
  while (path_end (path) != dst)
    {
      prev = current;
      current = path_end (path);

      deflection_set = dinfo_table[current->id]->deflection_set_1[dst->id];
      assert (deflection_set);

      if (deflection_set->size)
        {
          /* select the next-hop */
          index = (*nhselection) (current->id, tag, router_seeds);
          next = vector_get (deflection_set, index % deflection_set->size);
#if 0
          fprintf (stderr, "  deflect-1 forward: node: %d from: %d dst: %d "
                   "#nexthops: %d tag: %d index: %d %d-th nexthop: %d\n",
                   current->id, (prev ? prev->id : -1), dst->id,
                   deflection_set->size, tag, index,
                   index % deflection_set->size, next->id);
          fflush (stderr);
#endif
        }
      else
        {
          /* select the default next-hop */
          struct candidate *c = dinfo_table[current->id]->table[dst->id];
          assert (c->destination == dst);
          next = vector_get (c->nexthops, 0);
          fprintf (stderr, "node %d, dst %d, rule-%d, "
                   "no deflection set, use default\n",
                   current->id, dst->id, 1);
          fflush (stderr);
        }

      vector_add_allow_dup (next, path->path);
    }

  fflush (stderr);
}

void
deflect_2_tag_forward (struct node *dst, unsigned int tag,
                       struct deflection_info **dinfo_table,
                       int (*nhselection) (int, unsigned int, void *),
                       void *router_seeds, struct path *path)
{
  struct node *prev = NULL, *current = NULL, *next = NULL;
  struct vector *deflection_set;
  struct prev_hop_entry *e, *entry;
  struct vector_node *vn;
  int index;

  /* forward until the destination is reached */
  while (path_end (path) != dst)
    {
      prev = current;
      current = path_end (path);

      if (! prev)
        deflection_set = dinfo_table[current->id]->deflection_set_1[dst->id];
      else
        {
          entry = NULL;
          for (vn = vector_head (dinfo_table[current->id]->prev_hop_table[dst->id]);
               vn; vn = vector_next (vn))
            {
              e = (struct prev_hop_entry *) vn->data;
              if (e->prev_hop == prev)
                entry = e;
            }
          assert (entry);

          deflection_set = entry->deflection_set_2;
        }
      assert (deflection_set);

      if (deflection_set->size)
        {
          /* select the next-hop */
          index = (*nhselection) (current->id, tag, router_seeds);
          next = vector_get (deflection_set, index % deflection_set->size);
#if 0
          fprintf (stderr, "  deflect-2 forward: node: %d from: %d dst: %d "
                   "#nexthops: %d tag: %d index: %d %d-th nexthop: %d\n",
                   current->id, (prev ? prev->id : -1), dst->id,
                   deflection_set->size, tag, index,
                   index % deflection_set->size, next->id);
          fflush (stderr);
#endif
        }
      else
        {
          /* select the default next-hop */
          struct candidate *c = dinfo_table[current->id]->table[dst->id];
          assert (c->destination == dst);
          next = vector_get (c->nexthops, 0);
          fprintf (stderr, "node %d, dst %d, rule-%d, "
                   "no deflection set, use default\n",
                   current->id, dst->id, 2);
        }

      vector_add_allow_dup (next, path->path);
    }

  fflush (stderr);
}

void
deflect_3_tag_forward (struct node *dst, unsigned int tag,
                       struct deflection_info **dinfo_table,
                       int (*nhselection) (int, unsigned int, void *),
                       void *router_seeds, struct path *path)
{
  struct node *prev = NULL, *current = NULL, *next = NULL;
  struct vector *deflection_set;
  struct prev_hop_entry *e, *entry;
  struct vector_node *vn;
  int index;

  /* forward until the destination is reached */
  while (path_end (path) != dst)
    {
      prev = current;
      current = path_end (path);

      if (! prev)
        deflection_set = dinfo_table[current->id]->deflection_set_1[dst->id];
      else
        {
          entry = NULL;
          for (vn = vector_head (dinfo_table[current->id]->prev_hop_table[dst->id]);
               vn; vn = vector_next (vn))
            {
              e = (struct prev_hop_entry *) vn->data;
              if (e->prev_hop == prev)
                entry = e;
            }
          assert (entry);

          deflection_set = entry->deflection_set_3;
        }
      assert (deflection_set);

      if (deflection_set->size)
        {
          /* select the next-hop */
          index = (*nhselection) (current->id, tag, router_seeds);
          next = vector_get (deflection_set, index % deflection_set->size);
#if 0
          fprintf (stderr, "  deflect-3 forward: node: %d from: %d dst: %d "
                   "#nexthops: %d tag: %d index: %d %d-th nexthop: %d\n",
                   current->id, (prev ? prev->id : -1), dst->id,
                   deflection_set->size, tag, index,
                   index % deflection_set->size, next->id);
          fflush (stderr);
#endif
        }
      else
        {
          /* select the default next-hop */
          struct candidate *c = dinfo_table[current->id]->table[dst->id];
          assert (c->destination == dst);
          next = vector_get (c->nexthops, 0);
          fprintf (stderr, "node %d, dst %d, rule-%d, "
                   "no deflection set, use default\n",
                   current->id, dst->id, 3);
        }

      vector_add_allow_dup (next, path->path);
    }

  fflush (stderr);
}

void
drouting_tag_forward (struct node *dst, unsigned int tag,
                      struct routing *drouting,
                      int (*nhselection) (int, unsigned int, void *),
                      void *router_seeds, struct path *path)
{
  struct node *prev = NULL, *current = NULL, *next = NULL;
  struct nexthop *nexthop;
  struct vector *nexthop_set;
  int index;

  while (path_end (path) != dst)
    {
      prev = current;
      current = path_end (path);

      nexthop_set = drouting->route[current->id][dst->id].nexthops;
      if (nexthop_set->size == 0)
        {
          fprintf (stderr, "no route from %d to %d\n", current->id, dst->id);
          return;
        }

      /* select the next-hop */
      index = (*nhselection) (current->id, tag, router_seeds);
      nexthop = vector_get (nexthop_set, index % nexthop_set->size);
      next = nexthop->node;

#if 0
      fprintf (stderr, "  drouting forward: node: %d from: %d dst: %d "
               "#nexthops: %d tag: %d index: %#x %d-th nexthop: %d\n",
               current->id, (prev ? prev->id : -1), dst->id,
               nexthop_set->size, tag, index,
               index % nexthop_set->size, next->id);
      fflush (stderr);
#endif

      vector_add_allow_dup (next, path->path);
    }

  fflush (stderr);
}


#define FLOW_LABEL_MASK 0x000fffff

unsigned long *tag_hash_table_create (unsigned long size);
void tag_hash_table_delete (unsigned long *table);

int
count_path_failure (struct path *path, struct vector *failure_nodes)
{
  int fcount = 0;
  struct vector_node *vn;

  /* does the path contain failures ? */
  for (vn = vector_head (failure_nodes); vn;
       vn = vector_next (vn))
    {
      struct node *node = (struct node *) vn->data;
      if (vector_lookup (node, path->path))
        fcount++;
    }

  return fcount;
}

void
deflection_count_retry (struct shell *shell,
                        struct node *src, struct node *dst,
                        struct routing *deflection,
                        struct vector *router_primes,
                        struct vector *failure_nodes)
{
  int count, fcount;
  int deflect_1_recovered = 0, deflect_1_count = 0;
  int deflect_2_recovered = 0, deflect_2_count = 0;
  int deflect_3_recovered = 0, deflect_3_count = 0;

  struct deflection_info **dinfo_table = deflection->data;
  struct path *path;
  unsigned long tag;
  int nfailures = failure_nodes->size;

  for (count = 1; count <= 10; count++)
    {
      if (count <= 5)
        tag = count;
      else
        tag = random () % (1024 - 6) + 6;

      if (! deflect_1_recovered)
        {
          deflect_1_count = count;

          path = path_create ();
          vector_add (src, path->path);
          deflect_1_tag_forward (dst, tag, dinfo_table,
                                 deflection_selection, router_primes,
                                 path);
          fcount = count_path_failure (path, failure_nodes);
          if (fcount == 0)
            deflect_1_recovered++;

          fprintf (shell->terminal, "deflect-1 %s with tag %d, failure (",
                   (deflect_1_recovered ? "recovered" : "failed"),
                   (int) tag);
          print_nodelist (shell->terminal, failure_nodes);
          fprintf (shell->terminal, "): ");
          print_nodelist (shell->terminal, path->path);
          fprintf (shell->terminal, "\n");
          path_delete (path);
        }

      if (! deflect_2_recovered)
        {
          deflect_2_count = count;

          path = path_create ();
          vector_add (src, path->path);
          deflect_2_tag_forward (dst, tag, dinfo_table,
                                 deflection_selection, router_primes,
                                 path);
          fcount = count_path_failure (path, failure_nodes);
          if (fcount == 0)
            deflect_2_recovered++;

          fprintf (shell->terminal, "deflect-2 %s with tag %d failure (",
                   (deflect_2_recovered ? "recovered" : "failed"),
                   (int) tag);
          print_nodelist (shell->terminal, failure_nodes);
          fprintf (shell->terminal, "): ");
          print_nodelist (shell->terminal, path->path);
          fprintf (shell->terminal, "\n");
          path_delete (path);
        }

      if (! deflect_3_recovered)
        {
          deflect_3_count = count;

          path = path_create ();
          vector_add (src, path->path);
          deflect_3_tag_forward (dst, tag, dinfo_table,
                                 deflection_selection, router_primes,
                                 path);
          fcount = count_path_failure (path, failure_nodes);
          if (fcount == 0)
            deflect_3_recovered++;

          fprintf (shell->terminal, "deflect-3 %s with tag %d, failure (",
                   (deflect_3_recovered ? "recovered" : "failed"),
                   (int) tag);
          print_nodelist (shell->terminal, failure_nodes);
          fprintf (shell->terminal, "): ");
          print_nodelist (shell->terminal, path->path);
          fprintf (shell->terminal, "\n");
          path_delete (path);
        }

      if (deflect_1_recovered && deflect_2_recovered &&
          deflect_3_recovered)
        break;
    }

  if (! deflect_1_recovered)
    deflect_1_count = count;
  if (! deflect_2_recovered)
    deflect_2_count = count;
  if (! deflect_3_recovered)
    deflect_3_count = count;

  fprintf (shell->terminal,
           "EVAL: %02d-%02d #fails:%d deflection-1 (original): "
           "retry count: %d recover: %s\n",
           src->id, dst->id, nfailures, deflect_1_count,
           (deflect_1_recovered ? "yes" : "no"));

  fprintf (shell->terminal,
           "EVAL: %02d-%02d #fails:%d deflection-2 (original): "
           "retry count: %d recover: %s\n",
           src->id, dst->id, nfailures, deflect_2_count,
           (deflect_2_recovered ? "yes" : "no"));

  fprintf (shell->terminal,
           "EVAL: %02d-%02d #fails:%d deflection-3 (original): "
           "retry count: %d recover: %s\n",
           src->id, dst->id, nfailures, deflect_3_count,
           (deflect_3_recovered ? "yes" : "no"));

  fflush (shell->terminal);
}

void
deflection_default_ok (struct shell *shell,
                       struct node *src, struct node *dst,
                       struct routing *deflection,
                       struct vector *router_primes,
                       struct vector *failure_nodes)
{
  int deflect_1_recovered = 1, deflect_1_count = 0;
  int deflect_2_recovered = 1, deflect_2_count = 0;
  int deflect_3_recovered = 1, deflect_3_count = 0;
  int nfailures = failure_nodes->size;

  fprintf (shell->terminal,
           "EVAL: %02d-%02d #fails:%d deflection-1 (original): "
           "retry count: %d recover: %s\n",
           src->id, dst->id, nfailures, deflect_1_count,
           (deflect_1_recovered ? "yes" : "no"));

  fprintf (shell->terminal,
           "EVAL: %02d-%02d #fails:%d deflection-2 (original): "
           "retry count: %d recover: %s\n",
           src->id, dst->id, nfailures, deflect_2_count,
           (deflect_2_recovered ? "yes" : "no"));

  fprintf (shell->terminal,
           "EVAL: %02d-%02d #fails:%d deflection-3 (original): "
           "retry count: %d recover: %s\n",
           src->id, dst->id, nfailures, deflect_3_count,
           (deflect_3_recovered ? "yes" : "no"));

  fflush (shell->terminal);
}

void
drouting_count_retry (struct shell *shell,
                      struct node *src, struct node *dst,
                      struct routing *drouting,
                      struct vector *hash_tables,
                      struct vector *failure_nodes)
{
  int count, fcount;
  int recovered = 0;
  struct path *path;
  unsigned long tag;
  int nfailures = failure_nodes->size;

  for (count = 1; count <= 10; count++)
    {
      path = path_create ();
      vector_add (src, path->path);
      tag = (unsigned long) random () & FLOW_LABEL_MASK;
      drouting_tag_forward (dst, tag, drouting,
                            chash_selection, hash_tables,
                            path);

      fcount = count_path_failure (path, failure_nodes);
      if (fcount == 0)
        recovered++;

      fprintf (shell->terminal,
               "drouting %s with tag %#x, failure (",
               (recovered ? "recovered" : "failed"),
               (unsigned int) tag);
      print_nodelist (shell->terminal, failure_nodes);
      fprintf (shell->terminal, "): ");
      print_nodelist (shell->terminal, path->path);
      fprintf (shell->terminal, "\n");
      fflush (shell->terminal);

      path_delete (path);

      if (recovered)
        break;
    }

  fprintf (shell->terminal,
           "EVAL: %02d-%02d #fails:%d drouting: "
           "retry count: %d recover: %s\n",
           src->id, dst->id, nfailures, count,
           (recovered ? "yes" : "no"));
  fflush (shell->terminal);
}

void
drouting_default_ok (struct shell *shell,
                     struct node *src, struct node *dst,
                     struct routing *drouting,
                     struct vector *hash_tables,
                     struct vector *failure_nodes)
{
  int count = 0;
  int recovered = 1;
  int nfailures = failure_nodes->size;
  fprintf (shell->terminal,
           "EVAL: %02d-%02d #fails:%d drouting: "
           "retry count: %d recover: %s\n",
           src->id, dst->id, nfailures, count,
           (recovered ? "yes" : "no"));
  fflush (shell->terminal);
}


DEFINE_COMMAND (simulate_deflection,
                "simulate deflection NAME ntrials <0-10000> failure nodes <1-100>",
                "simulate\n"
                "specify deflection routing\n"
                "specify deflection routing\n"
                "specify number of trials\n"
                "specify number of trials\n"
                "specify number of failure nodes\n"
                "specify number of failure nodes\n"
                "specify number of failure nodes\n")
{
  struct shell *shell = (struct shell *) context;
  int ntrials, nfailures;
  struct routing *deflection;

  time_t seed;
  int prime;
  struct vector *prime_vector;
  struct vector *router_primes;

  struct vector_node *vn, *vns, *vnt;
  struct node *node, *src, *dst;

  int i, n;
  int failure_id;
  struct vector *failure_nodes;
  struct path *path;
  int stdown, isfail;

  ntrials = strtoul (argv[4], NULL, 0);
  nfailures = strtoul (argv[7], NULL, 0);

  deflection = (struct routing *) instance_lookup ("routing", argv[2]);
  if (! deflection)
    {
      fprintf (shell->terminal, "No such routing instance: %s\n", argv[2]);
      return;
    }

  seed = time (NULL);
  fprintf (shell->terminal, "seed: %lu\n", (unsigned long) seed);
  srandom ((unsigned int) seed);

  /* create router seeds (prime value) */
  prime_vector = prime_vector_create (120);
#if 1
  fprintf (stderr, "prime vector: ");
  for (i = 0; i < prime_vector->size; i++)
    fprintf (stderr, " %d", (int) vector_get (prime_vector, i));
  fprintf (stderr, "\n");
#endif /*1*/

  router_primes = vector_create ();
  for (vns = vector_head (deflection->G->nodes); vns; vns = vector_next (vns))
    {
      node = (struct node *) vns->data;
      prime = (int) vector_get (prime_vector, random () % 10 + 10);
      vector_set (router_primes, node->id, (void *) prime);
    }
  vector_delete (prime_vector);

  /* execute "ntrials" times */
  for (n = 0; n < ntrials; n++)
    {
      /* prepare failure nodes */
      fprintf (shell->terminal, "%d-th trial: failure node: ", n);
      failure_nodes = vector_create ();
      for (i = 0; i < nfailures; i++)
        {
          failure_id = random () % deflection->G->nodes->size;
          fprintf (shell->terminal, " %d", failure_id);
          node = node_lookup (failure_id, deflection->G);
          vector_set (failure_nodes, i, node);
        }
      fprintf (shell->terminal, "\n");

      /* for each source */
      for (vns = vector_head (deflection->G->nodes); vns;
           vns = vector_next (vns))
        {
          src = (struct node *) vns->data;

          /* for each destination */
          for (vnt = vector_head (deflection->G->nodes); vnt;
               vnt = vector_next (vnt))
            {
              dst = (struct node *) vnt->data;

              if (src == dst)
                continue;

              stdown = 0;
              for (vn = vector_head (failure_nodes); vn;
                   vn = vector_next (vn))
                {
                  node = (struct node *) vn->data;
                  if (node == src || node == dst)
                    stdown++;
                }
              if (stdown)
                continue;

              /* get the default path */
              path = route_path_first (src, dst, deflection);
              route_path_probability (path, dst, deflection);
              fprintf (shell->terminal, "    deflection primary path: ");
              print_nodelist (shell->terminal, path->path);
              fprintf (shell->terminal, " (%f)\n", path->probability);
              fflush (shell->terminal);

              /* if the path does not contain failure, skip */
              isfail = 0;
              for (vn = vector_head (failure_nodes); vn;
                   vn = vector_next (vn))
                {
                  node = (struct node *) vn->data;
                  if (vector_lookup (node, path->path))
                    isfail++;
                }

              if (path_end (path) == dst)
                {
                  if (isfail)
                    deflection_count_retry (shell, src, dst,
                                            deflection, router_primes,
                                            failure_nodes);
                  else
                    deflection_default_ok (shell, src, dst,
                                            deflection, router_primes,
                                            failure_nodes);
                }

              path_delete (path);

            }
        }
    }

  vector_delete (failure_nodes);
}

DEFINE_COMMAND (simulate_drouting,
                "simulate drouting <0-4294967295> ntrials <0-10000> failure nodes <1-100>",
                "simulate\n"
                "specify drouting routing\n"
                "specify drouting routing\n"
                "specify number of trials\n"
                "specify number of trials\n"
                "specify number of failure nodes\n"
                "specify number of failure nodes\n"
                "specify number of failure nodes\n")
{
  struct shell *shell = (struct shell *) context;
  int ntrials, nfailures;
  struct routing *drouting;

  time_t seed;
  struct vector *hash_tables;

  struct vector_node *vn, *vns, *vnt;
  struct node *node, *src, *dst;

  int i, n;
  int failure_id;
  struct vector *failure_nodes;
  struct path *path;
  unsigned long tag;
  unsigned long *table;
  int stdown, isfail;

  ntrials = strtoul (argv[4], NULL, 0);
  nfailures = strtoul (argv[7], NULL, 0);

  drouting = (struct routing *) instance_lookup ("routing", argv[2]);
  if (! drouting)
    {
      fprintf (shell->terminal, "No such routing instance: %s\n", argv[2]);
      return;
    }

  seed = time (NULL);
  fprintf (shell->terminal, "seed: %lu\n", (unsigned long) seed);
  srandom ((unsigned int) seed);

  /* create router seeds (complete hash table) */
  hash_tables = vector_create ();
  for (vns = vector_head (drouting->G->nodes); vns; vns = vector_next (vns))
    {
      node = (struct node *) vns->data;
      table = (unsigned long *) tag_hash_table_create (FLOW_LABEL_MASK);
#if 0
      for (i = 0; i < FLOW_LABEL_MASK; i++)
        fprintf (stderr, "node %d: table[%#x]: %#x\n",
                 node->id, i, table[i]);
#endif /*0*/
      vector_set (hash_tables, node->id, (void *) table);
    }

  /* execute "ntrials" times */
  for (n = 0; n < ntrials; n++)
    {
      /* prepare failure nodes */
      fprintf (shell->terminal, "%d-th trial: failure node: ", n);
      failure_nodes = vector_create ();
      for (i = 0; i < nfailures; i++)
        {
          failure_id = random () % drouting->G->nodes->size;
          fprintf (shell->terminal, " %d", failure_id);
          node = node_lookup (failure_id, drouting->G);
          vector_set (failure_nodes, i, node);
        }
      fprintf (shell->terminal, "\n");

      /* for each source */
      for (vns = vector_head (drouting->G->nodes); vns;
           vns = vector_next (vns))
        {
          src = (struct node *) vns->data;

          /* for each destination */
          for (vnt = vector_head (drouting->G->nodes); vnt;
               vnt = vector_next (vnt))
            {
              dst = (struct node *) vnt->data;

              if (src == dst)
                continue;

              stdown = 0;
              for (vn = vector_head (failure_nodes); vn;
                   vn = vector_next (vn))
                {
                  node = (struct node *) vn->data;
                  if (node == src || node == dst)
                    stdown++;
                }
              if (stdown)
                continue;

              /* get the default path */
              path = path_create ();
              vector_add (src, path->path);
              tag = (unsigned long) random () & FLOW_LABEL_MASK;
              drouting_tag_forward (dst, tag, drouting,
                                    chash_selection, hash_tables, path);
              route_path_probability (path, dst, drouting);
              fprintf (shell->terminal, "    drouting primary path: ");
              print_nodelist (shell->terminal, path->path);
              fprintf (shell->terminal, " (%f)\n", path->probability);
              fflush (shell->terminal);

              /* if the path does not contain failure, skip */
              isfail = 0;
              for (vn = vector_head (failure_nodes); vn;
                   vn = vector_next (vn))
                {
                  node = (struct node *) vn->data;
                  if (vector_lookup (node, path->path))
                    isfail++;
                }

              if (path_end (path) == dst)
                {
                  if (isfail)
                    drouting_count_retry (shell, src, dst, drouting, hash_tables,
                                          failure_nodes);
                  else
                    drouting_default_ok (shell, src, dst, drouting, hash_tables,
                                          failure_nodes);
                }

              path_delete (path);

            }
        }
    }

  vector_delete (failure_nodes);
}


DEFINE_COMMAND (simulate_deflection_drouting,
                "simulate deflection <0-4294967295> drouting <0-4294967295> ntrials <0-10000> failure nodes <1-100>",
                "simulate\n"
                "specify deflection routing\n"
                "specify deflection routing\n"
                "specify number of trials\n"
                "specify number of trials\n"
                "specify number of failure nodes\n"
                "specify number of failure nodes\n"
                "specify number of failure nodes\n")
{
  struct shell *shell = (struct shell *) context;
  int ntrials, nfailures;
  struct routing *deflection, *drouting;

  time_t seed;
  int prime;
  struct vector *prime_vector;
  struct vector *router_primes;
  struct vector *hash_tables;

  struct vector_node *vn, *vns, *vnt;
  struct node *node, *src, *dst;

  int i, n;
  int failure_id;
  struct vector *failure_nodes;
  struct path *path;
  unsigned long *table;
  int stdown, isfail;

  ntrials = strtoul (argv[6], NULL, 0);
  nfailures = strtoul (argv[9], NULL, 0);

  deflection = (struct routing *) instance_lookup ("routing", argv[2]);
  if (! deflection)
    {
      fprintf (shell->terminal, "No such routing instance: %s\n", argv[2]);
      return;
    }

  drouting = (struct routing *) instance_lookup ("routing", argv[2]);
  if (! drouting)
    {
      fprintf (shell->terminal, "No such routing instance: %s\n", argv[2]);
      return;
    }

  seed = time (NULL);
  fprintf (shell->terminal, "seed: %lu\n", (unsigned long) seed);
  srandom ((unsigned int) seed);

  /* create router seeds (prime value) */
  prime_vector = prime_vector_create (120);
#if 1
  fprintf (stderr, "prime vector: ");
  for (i = 0; i < prime_vector->size; i++)
    fprintf (stderr, " %d", (int) vector_get (prime_vector, i));
  fprintf (stderr, "\n");
#endif /*1*/

  router_primes = vector_create ();
  for (vns = vector_head (deflection->G->nodes); vns; vns = vector_next (vns))
    {
      node = (struct node *) vns->data;
      prime = (int) vector_get (prime_vector, random () % 10 + 10);
      vector_set (router_primes, node->id, (void *) prime);
    }
  vector_delete (prime_vector);

  /* create router seeds (complete hash table) */
  hash_tables = vector_create ();
  for (vns = vector_head (drouting->G->nodes); vns; vns = vector_next (vns))
    {
      node = (struct node *) vns->data;
      table = (unsigned long *) tag_hash_table_create (FLOW_LABEL_MASK);
#if 0
      for (i = 0; i < FLOW_LABEL_MASK; i++)
        fprintf (stderr, "node %d: table[%#x]: %#x\n",
                 node->id, i, table[i]);
#endif /*0*/
      vector_set (hash_tables, node->id, (void *) table);
    }


  /* execute "ntrials" times */
  for (n = 0; n < ntrials; n++)
    {
      /* prepare failure nodes */
      fprintf (shell->terminal, "%d-th trial: failure node: ", n);
      failure_nodes = vector_create ();
      for (i = 0; i < nfailures; i++)
        {
          failure_id = random () % deflection->G->nodes->size;
          fprintf (shell->terminal, " %d", failure_id);
          node = node_lookup (failure_id, deflection->G);
          vector_set (failure_nodes, i, node);
        }
      fprintf (shell->terminal, "\n");

      /* for each source */
      for (vns = vector_head (deflection->G->nodes); vns;
           vns = vector_next (vns))
        {
          src = (struct node *) vns->data;

          /* for each destination */
          for (vnt = vector_head (deflection->G->nodes); vnt;
               vnt = vector_next (vnt))
            {
              dst = (struct node *) vnt->data;

              if (src == dst)
                continue;

              stdown = 0;
              for (vn = vector_head (failure_nodes); vn;
                   vn = vector_next (vn))
                {
                  node = (struct node *) vn->data;
                  if (node == src || node == dst)
                    stdown++;
                }
              if (stdown)
                continue;

              /* get the default path */
              path = route_path_first (src, dst, deflection);
              route_path_probability (path, dst, deflection);
              fprintf (shell->terminal, "    deflection primary path: ");
              print_nodelist (shell->terminal, path->path);
              fprintf (shell->terminal, " (%f)\n", path->probability);
              fflush (shell->terminal);

              /* if the path does not contain failure, skip */
              isfail = 0;
              for (vn = vector_head (failure_nodes); vn;
                   vn = vector_next (vn))
                {
                  node = (struct node *) vn->data;
                  if (vector_lookup (node, path->path))
                    isfail++;
                }

              if (! isfail)
                {
                  path_delete (path);
                  continue;
                }

              /* ignore disconnected case */
              if (path_end (path) != dst)
                {
                  fprintf (shell->terminal, "      no primary path, disconnected ?");
                  path_delete (path);
                  continue;
                }

              path_delete (path);

              deflection_count_retry (shell, src, dst,
                                      deflection, router_primes,
                                      failure_nodes);
              drouting_count_retry (shell, src, dst,
                                    drouting, hash_tables,
                                    failure_nodes);
            }
        }
      vector_delete (failure_nodes);
    }
}

#endif /*0*/

DEFINE_COMMAND (simulate_failure_recovery,
                "simulate failure-recovery routing <0-4294967295> ntrials <0-10000> failure-nodes <1-100> st-trial <1-1000>",
                "simulate\n"
                "simulate failure-recovery simulation\n"
                "specify routing\n"
                "specify routing ID\n"
                "specify number of trials\n"
                "specify number of trials\n"
                "specify number of failure nodes\n"
                "specify number of failure nodes\n"
                "specify number of trials between src and dst\n"
                "specify number of trials between src and dst\n")
{
  struct shell *shell = (struct shell *) context;
  int ntrials, nfailures, mtrials;
  struct routing *routing;
  time_t seed;
  struct vector_node *vn, *vns, *vnt;
  struct node *node, *src, *dst;
  int i, n, m;
  struct vector *failure_nodes;
  struct path *path;
  int stdown, isfail;
  int success;

  routing = (struct routing *) instance_lookup ("routing", argv[3]);
  if (! routing)
    {
      fprintf (shell->terminal, "No such routing instance: %s\n", argv[3]);
      return;
    }

  ntrials = strtoul (argv[5], NULL, 0);
  nfailures = strtoul (argv[7], NULL, 0);
  mtrials = strtoul (argv[9], NULL, 0);

  seed = time (NULL);
  fprintf (shell->terminal, "seed: %lu\n", (unsigned long) seed);
  srandom ((unsigned int) seed);

  /* execute "ntrials" times */
  for (n = 0; n < ntrials; n++)
    {
      /* prepare failure nodes */
      fprintf (shell->terminal, "%d-th trial: failure node: ", n);
      failure_nodes = vector_create ();
      for (i = 0; i < nfailures; i++)
        {
          do {
            int failure_id = random () % routing->G->nodes->size;
            node = node_lookup (failure_id, routing->G);
          } while (vector_lookup (node, failure_nodes));
          vector_set (failure_nodes, i, node);
          fprintf (shell->terminal, " %d", node->id);
        }
      fprintf (shell->terminal, "\n");

      /* for each source */
      for (vns = vector_head (routing->G->nodes); vns;
           vns = vector_next (vns))
        {
          src = (struct node *) vns->data;

          /* for each destination */
          for (vnt = vector_head (routing->G->nodes); vnt;
               vnt = vector_next (vnt))
            {
              dst = (struct node *) vnt->data;

              if (src == dst)
                continue;

              stdown = 0;
              for (vn = vector_head (failure_nodes); vn;
                   vn = vector_next (vn))
                {
                  node = (struct node *) vn->data;
                  if (node == src || node == dst)
                    stdown++;
                }
              if (stdown)
                {
                  fprintf (shell->terminal,
                           "routing-%s %d-th trial %d-%d stdown\n",
                           routing->name, n, src->id, dst->id);
                  continue;
                }

              success = 0;
              for (m = 0; m < mtrials; m++)
                {
                  path = route_path_random_forward (src, dst, routing);

                  /* if the path does not contain failure, skip */
                  isfail = 0;
                  for (vn = vector_head (failure_nodes); vn;
                       vn = vector_next (vn))
                    {
                      node = (struct node *) vn->data;
                      if (vector_lookup (node, path->path))
                        isfail++;
                    }

                  fprintf (shell->terminal, "  m: %d path: ", m);
                  print_nodelist (shell->terminal, path->path);
                  fprintf (shell->terminal, " %s\n",
                           (isfail ? "(fail)" : ""));

                  if (! isfail)
                    success++;

                  path_delete (path);
                }
              fprintf (shell->terminal,
                       "routing-%s %d-th trial %d-%d success %d/%d %f\n",
                       routing->name, n, src->id, dst->id, success, mtrials,
                       (double) success / mtrials);
            }
        }
      vector_delete (failure_nodes);
    }
}



void
network_init ()
{
  networks = vector_create ();
  cmdset_network = command_set_create ();

  INSTALL_COMMAND (cmdset_network, network_graph);
  INSTALL_COMMAND (cmdset_network, network_traffic);
  INSTALL_COMMAND (cmdset_network, network_routing);
  INSTALL_COMMAND (cmdset_network, show_network);
  INSTALL_COMMAND (cmdset_network, show_flows_on_link);
  INSTALL_COMMAND (cmdset_network, network_load_traffic_flows);
#if 0
  INSTALL_COMMAND (cmdset_network, simulate_deflection);
  INSTALL_COMMAND (cmdset_network, simulate_drouting);
  INSTALL_COMMAND (cmdset_network, simulate_deflection_drouting);
#endif /*0*/

  INSTALL_COMMAND (cmdset_network, simulate_failure_recovery);
}

void
network_finish ()
{
  struct vector_node *vn;
  struct network *network;
  for (vn = vector_head (networks); vn; vn = vector_next (vn))
    {
      network = (struct network *) vn->data;
      if (network)
        network_delete (network);
    }
  vector_delete (networks);
}

void *
create_network (char *id)
{
  struct network *w;
  w = network_create ();
  w->name = strdup (id);
  return (void *) w;
}

char *
network_name (void *instance)
{
  struct network *network = (struct network *) instance;
  return network->name;
}

void
network_config_write (void *instance, FILE *out)
{
  struct network *network = (struct network *) instance;
  command_config_write (network->config, out);
}



struct module mod_network =
{
  "network",
  network_init,
  network_finish,
  &networks,
  create_network,
  network_name,
  show_network_summary_header,
  show_network_summary,
  show_network_instance,
  &cmdset_network,
  network_config_write
};


