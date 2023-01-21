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

#include "log.h"
#include "graph.h"
#include "demand.h"
#include "routing.h"
#include "load.h"
#include "evaluation.h"

#include "shell.h"
#include "command.h"
#include "graph.h"

void
load_clear (struct graph *g)
{
  struct vector_node *vn;
  struct flow *flow;
  struct link *e;

  for (vn = vector_head (g->links); vn; vn = vector_next (vn))
    {
      e = (struct link *) vn->data;
      if (e == NULL)
        continue;
      vector_delete (e->flows);
      e->flows = vector_create ();
      e->load = 0;
      e->utilization = 0;
      e->phi = 0;
    }

  for (vn = vector_head (g->flows); vn; vn = vector_next (vn))
    {
      flow = (struct flow *) vn->data;
      if (flow == NULL)
        continue;
      free (flow);
    }
  vector_delete (g->flows);
  g->flows = vector_create ();
}

DEFINE_COMMAND (clear_load,
                "clear load",
                "clear information\n"
                "clear traffic load\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  load_clear (graph);
}

void
load_update (struct link *e)
{
  struct vector_node *vn;
  struct flow *flow;

  e->load = 0;
  for (vn = vector_head (e->flows); vn; vn = vector_next (vn))
    {
      flow = (struct flow *) vn->data;
      e->load += flow->demand;
    }
  e->utilization = (double) e->load / e->capacity;
  e->phi = phi (e);

  e->changed++;
}

void
load_traffic_flow (struct graph *g, struct node *s, struct node *t,
                   struct demand_matrix *D)
{
  struct flow *flow;
  u_int demand;

  demand = D->matrix[s->index - 1][t->index - 1];

  flow = (struct flow *) malloc (sizeof (struct flow));
  memset (flow, 0, sizeof (struct flow));
  flow->s = s;
  flow->t = t;
  flow->demand = demand;
  flow->path = vector_create ();
  flow->path->cmp = NULL;

  if (flow->demand == 0)
    {
      printf ("load flow (%2d,%2d): demand: %d (changed to 1)\n",
              s->index, t->index, flow->demand);
      flow->demand = 1;
    }
  else
    {
      printf ("load flow (%2d,%2d): demand: %4d\n",
              s->index, t->index, flow->demand);
    }

  vector_add (flow, g->flows);
  vector_add (flow, s->remain_flows);
  vector_add (s, flow->path);

  load_traffic_forward_flow (s, flow, g);
}

void
load_traffic_forward_flow (struct node *v, struct flow *flow, struct graph *g)
{
  int i, j;
  struct node *w, *x;
  struct flow *new;
  int forwarding_loop;
  struct link *vw;
  u_int total_demand;
  int total_balance;
  int path_count;
  struct routing_entry *route;

  assert (vector_lookup (flow, v->remain_flows));
  vector_remove (flow, v->remain_flows);

  route = vector_get (v->routes, flow->t->index);
  assert (route);

  total_demand = flow->demand;

  total_balance = 0;
  path_count = 0;
  for (i = 0; i < route->size; i++)
    if (route->nexthop[i])
      {
        path_count++;
        total_balance += route->balance[i];
      }
  assert (path_count);

  for (i = 0; i < route->size && route->nexthop[i]; i++)
    {
      /* create new flow */
      new = (struct flow *) malloc (sizeof (struct flow));
      memset (new, 0, sizeof (struct flow));
      new->s = flow->s;
      new->t = flow->t;
      if (total_balance == 0 && i == 0)
        new->demand = total_demand;
      else
        new->demand = total_demand * route->balance[i] / total_balance;

      /* copy path */
      new->path = vector_create ();
      new->path->cmp = NULL;
      for (j = 0; j < flow->path->size; j++)
        vector_set (new->path, j, vector_get (flow->path, j));

      /* find nexthop */
      w = route->nexthop[i];

      /* update intermediate link info */
      vw = link_lookup_index (v->index, w->index, g);
      vector_add (new, vw->flows);
      load_update (vw);

      /* check the flow path and add new hop */
      forwarding_loop = (int) vector_lookup (w, new->path);
      vector_add (w, new->path);

      /* If the nexthop is not destination, add to remain_flows list */
      if (w != new->t)
        vector_add (new, w->remain_flows);

      assert (! forwarding_loop);

      if (w == new->t)
        {
          /* The flow reached to its sink. print path and exit */

          /* print forwarding */
          printf ("flow (%2d,%2d) forward %2d->%2d: demand: %d path:",
                  new->s->index, new->t->index,
                  v->index, w->index, new->demand);
          for (j = 0; j < new->path->size; j++)
            {
              x = (struct node *) vector_get (new->path, j);
              printf (" %d", x->index);
            }
          printf ("\n");

          return;
        }

      /* recursively forward */
      load_traffic_forward_flow (w, new, g);
    }
}

void
load_traffic_demand (struct graph *g, struct demand_matrix *D)
{
  struct vector_node *vns, *vnt;
  struct node *s, *t;

  for (vns = vector_head (g->nodes); vns; vns = vector_next (vns))
    for (vnt = vector_head (g->nodes); vnt; vnt = vector_next (vnt))
      {
        s = (struct node *) vns->data;
        t = (struct node *) vnt->data;
        if (s == t)
          continue;
        if (!s || !t)
          continue;

        load_traffic_flow (g, s, t, D);
      }
}

DEFINE_COMMAND (load_traffic,
                "load traffic",
                "load traffic from demand-matrix\n"
                "load traffic from demand-matrix\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  if (shell->demand_matrix)
    load_traffic_demand (graph, shell->demand_matrix);
}

#if 0
void
load_traffic_forward (struct graph *g)
{
  int i, j;
  struct node *v, *w, *x;
  struct flow *flow, *new;
  struct vector_node *vn, *vo;
  int forwarding_loop;
  struct link *vw;
  int remain;
  u_int total_demand;
  int total_balance;
  int path_count;
  struct routing_entry *route;

  do
    {
      remain = 0;
      for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
        {
          v = (struct node *) vn->data;
          if (! v)
            continue;
          for (vo = vector_head (v->remain_flows); vo; vo = vector_next (vo))
            {
              flow = (struct flow *) vo->data;
              total_demand = flow->demand;

              route = vector_get (v->routes, flow->t->index);
              assert (route);

              total_balance = 0;
              path_count = 0;
              for (i = 0; i < route->size; i++)
                if (route->nexthop[i])
                  {
                    path_count++;
                    total_balance += route->balance[i];
                  }
              assert (path_count);

              for (i = 0; i < route->size && route->nexthop[i]; i++)
                {
                  new = (struct flow *) malloc (sizeof (struct flow));
                  memset (new, 0, sizeof (struct flow));
                  new->s = flow->s;
                  new->t = flow->t;
                  if (total_balance == 0 && i == 0)
                    new->demand = total_demand;
                  else
                    new->demand = total_demand * route->balance[i] /
                                  total_balance;
                  new->path = vector_create ();
                  new->path->cmp = NULL;
                  for (j = 0; j < flow->path->size; j++)
                    vector_set (new->path, j, vector_get (flow->path, j));

                  w = route->nexthop[i];
                  vw = link_lookup_index (v->index, w->index, g);
                  vector_add (new, vw->flows);

                  forwarding_loop = (int) vector_lookup (w, new->path);
                  vector_add (w, new->path);
                  if (w != new->t)
                    {
                      vector_add (new, w->remain_flows);
                      remain++;
                    }

                  printf ("flow (%2d,%2d) forward %2d->%2d: demand: %d path:",
                          new->s->index, new->t->index,
                          v->index, w->index, new->demand);
                  for (j = 0; j < new->path->size; j++)
                    {
                      x = (struct node *) vector_get (new->path, j);
                      printf (" %d", x->index);
                    }
                  printf ("\n");

                  assert (! forwarding_loop);
                }

              vector_remove (flow, v->remain_flows);
            }
        }
    }
  while (remain);

  for (vn = vector_head (g->links); vn; vn = vector_next (vn))
    {
      vw = (struct link *) vn->data;
      if (vw == NULL)
        continue;
      load_update (vw);
    }
}
#endif /*0*/

