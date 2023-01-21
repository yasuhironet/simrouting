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

#include "pqueue.h"
#include "graph.h"
#include "routing.h"
#include "demand.h"
#include "load.h"
#include "path-vector.h"

extern int improve;

void
routing_path_vector_init (struct graph *g)
{
  struct vector_node *vn, *vm;
  struct node *x, *destination;
  struct path_table *table;

  for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
    {
      x = (struct node *) vn->data;
      if (! x)
        continue;

      for (vm = vector_head (g->nodes); vm; vm = vector_next (vm))
        {
          destination = (struct node *) vm->data;
          if (! destination)
            continue;

          table = (struct path_table *) malloc (sizeof (struct path_table));
          memset (table, 0, sizeof (struct path_table));
          table->size = routing_multipath_size;
          table->entry = (struct path_route **)
            malloc (sizeof (struct path_route *) * routing_multipath_size);
          memset (table->entry, 0,
                  sizeof (struct path_route *) * routing_multipath_size);

          vector_set (x->path_tables, destination->index, table);
        }
    }
}

void
routing_path_vector_clear (struct graph *g)
{
  struct vector_node *vn, *vm;
  struct node *x, *destination;
  struct path_table *table;

  for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
    {
      x = (struct node *) vn->data;
      if (! x)
        continue;

      for (vm = vector_head (g->nodes); vm; vm = vector_next (vm))
        {
          destination = (struct node *) vm->data;
          if (! destination)
            continue;
          table = vector_get (x->path_tables, destination->index);
          free (table->entry);
          vector_set (x->path_tables, destination->index, NULL);
        }
    }
}

void
path_route_print (struct path_route *route)
{
  struct vector_node *vn;
  struct node *x;
  printf ("route: dest: %d bw: %u(max:%u) nexthop: %d path: ",
          route->destination->index,
          route->bandwidth, route->bandwidth_wo_load,
          (route->nexthop == NULL ? 0 : route->nexthop->index));
  for (vn = vector_head (route->path_list); vn; vn = vector_next (vn))
    {
      x = (struct node *) vn->data;
      printf (" %d", x->index);
    }
  printf ("\n");
}

void
path_table_print_node_dest (struct node *v, struct node *destination)
{
  struct path_table *table;
  int i;
  struct path_route *route;

  table = (struct path_table *)
    vector_get (v->path_tables, destination->index);
  if (! table)
    return;

  for (i = 0; i < routing_multipath_size && table->entry[i]; i++)
    {
      route = table->entry[i];
      printf ("node: %d ", v->index);
      path_route_print (route);
    }
}

void
path_table_print_node (struct node *v)
{
  struct vector_node *wn;
  struct node *w;

  for (wn = vector_head (v->g->nodes); wn; wn = vector_next (wn))
    {
      w = (struct node *) wn->data;
      if (! w)
        continue;
      path_table_print_node_dest (v, w);
    }
}

void
path_table_print (struct graph *g)
{
  struct vector_node *vn;
  struct node *v;

  for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
    {
      v = (struct node *) vn->data;
      if (! v)
        continue;
      printf ("node[%d] path table:\n", v->index);
      path_table_print_node (v);
    }
}

int
event_cmp (void *a, void *b)
{
  struct message_event *ma = (struct message_event *) a;
  struct message_event *mb = (struct message_event *) b;
  if (ma == NULL)
    return 1;
  if (mb == NULL)
    return -1;
  return (ma->event_id - mb->event_id);
}

struct path_route *
path_route_create ()
{
  struct path_route *route;
  route = (struct path_route *) malloc (sizeof (struct path_route));
  memset (route, 0, sizeof (struct path_route));
  route->path_list = vector_create ();
  route->path_list->cmp = NULL;
  return route;
}

int
path_route_cmp (const void *a, const void *b)
{
  struct path_route *pa = *(struct path_route **) a;
  struct path_route *pb = *(struct path_route **) b;
  int i;
  struct node *xa, *xb;

  if (pa == NULL)
    return 1;
  if (pb == NULL)
    return -1;

  if (pa->bandwidth != pb->bandwidth)
    return (pa->bandwidth > pb->bandwidth ? -1 : 1);

  if (pa->path_list->size != pb->path_list->size)
    return (int) pa->path_list->size - pb->path_list->size;

  for (i = 0; i < pa->path_list->size; i++)
    {
      xa = (struct node *) pa->path_list->array[i];
      xb = (struct node *) pb->path_list->array[i];
      if (xa->index != xb->index)
        return (int) xa->index - xb->index;
    }

  /* should not reached */
  assert (0);
  return 0;
}

void
path_route_delete (struct path_route *route)
{
  vector_delete (route->path_list);
  free (route);
}

void
path_route_update (struct path_route *route, struct link *link)
{
  u_int bandwidth = 0;

#if 0
  printf ("update before: ");
  path_route_print (route);
  printf ("update: link: %d->%d capa %u load %u\n",
          link->from->index, link->to->index, link->capacity, link->load);
#endif

  route->bandwidth_wo_load = MIN (route->bandwidth_wo_load, link->capacity);
  if (route->bandwidth > route->bandwidth_wo_load)
    route->bandwidth = route->bandwidth_wo_load;

  if (link->capacity < link->load)
    bandwidth = 0;

#ifdef ADAPTIVE
  else if (route->bandwidth_wo_load > link->capacity - link->load)
    bandwidth = link->capacity - link->load;
#endif

  else
    bandwidth = route->bandwidth_wo_load;

#if 0
  if (route->bandwidth != bandwidth)
    {
      printf ("path route update: link %d->%d capa %u load %u\n",
              link->from->index, link->to->index, link->capacity, link->load);
      printf ("path route update: route bw %u(max:%u) -> %u\n",
              route->bandwidth, route->bandwidth_wo_load, bandwidth);
    }
#endif

  route->bandwidth = bandwidth;

#if 0
  printf ("update after : ");
  path_route_print (route);
#endif
}

int
path_route_is_loop (struct path_route *route)
{
  struct node *x, *check;
  int i, loop = 0;
  u_int index = route->path_list->size - 1;

  check = route->path_list->array[index];
  for (i = 0; i < route->path_list->size - 1; i++)
    {
      x = (struct node *) route->path_list->array[i];
      if (x->index == check->index)
        loop++;
    }
  return loop;
}

void
routing_path_route_install (struct node *s, struct node *t, struct graph *g)
{
  struct path_table *table;
  struct routing_entry *rtentry;

  table = vector_get (s->path_tables, t->index);
  assert (table);

  if (table->entry[0])
    {
      rtentry = vector_get (s->routes,
                            table->entry[0]->destination->index);
      if (rtentry == NULL)
        {
          rtentry = routing_entry_create ();
          rtentry->destination = table->entry[0]->destination;
          printf ("node[%d] route to [%d] newly installed: nexthop %d\n",
                  s->index, t->index, table->entry[0]->nexthop->index);
        }
      else
        {
          printf ("node[%d] route to [%d] nexthop change %d -> %d\n",
                  s->index, t->index, rtentry->nexthop[0]->index,
                  table->entry[0]->nexthop->index);
        }
      rtentry->nexthop[0] = table->entry[0]->nexthop;
      rtentry->balance[0] = 1;
      vector_set (s->routes, rtentry->destination->index, rtentry);
    }
  else
    {
      rtentry = vector_get (s->routes, t->index);
      if (rtentry != NULL)
        {
          routing_entry_delete (rtentry);
          vector_set (s->routes, t->index, NULL);
        }
    }
}

int
path_route_same (struct path_route *p1, struct path_route *p2)
{
  int i;
  assert (p1 || p2);
  if (! p1 || ! p2)
    return 0;
  if (p1->destination != p2->destination)
    return 0;
  if (p1->nexthop != p2->nexthop)
    return 0;
  if (p1->bandwidth != p2->bandwidth)
    return 0;
  if (p1->bandwidth_wo_load != p2->bandwidth_wo_load)
    return 0;
  if (p1->path_list->size != p2->path_list->size)
    return 0;
  for (i = 0; i < p1->path_list->size; i++)
    if (p1->path_list->array[i] != p2->path_list->array[i])
      return 0;
  return 1;
}

struct path_route *
path_route_propagate (struct node *v, struct node *w,
                      struct path_route *route)
{
  struct path_route **array;
  struct path_table *table;
  struct path_route *prev_best;
  int i;
  struct link *link;
  struct node *destination = route->destination;
  int update = 0;
  int update_best = 0;
  int change = 0;

  if (v != w)
    {
      link = link_lookup_index (w->index, v->index, v->g);
      assert (link);
      path_route_update (route, link);
    }

  vector_add (w, route->path_list);
  route->nexthop = v;

  printf ("propagate: %2d->%2d: ", v->index, w->index);
  path_route_print (route);

  table = vector_get (w->path_tables, destination->index);
  prev_best = table->entry[0];

  array = (struct path_route **) malloc (sizeof (struct path_route *) *
                                         routing_multipath_size * 2);
  memset (array, 0, sizeof (struct path_route *) * routing_multipath_size * 2);
  memcpy (array, table->entry, sizeof (struct path_route *) *
                               routing_multipath_size);

  /* update previous or add tail */
  for (i = 0; i < routing_multipath_size; i++)
    {
      if (array[i] && array[i]->nexthop == route->nexthop)
        {

          if (! path_route_same (array[i], route))
            {
              printf ("prev: node[%d]: ", w->index);
              path_route_print (array[i]);
              printf ("next: node[%d]: ", w->index);
              path_route_print (route);

              change ++;
            }

          update ++;
          if (i == 0)
            update_best++;

          path_route_delete (array[i]);
          array[i] = route;

          break;
        }
    }
  if (i == routing_multipath_size &&
      ! path_route_is_loop (route))
    {
      array[routing_multipath_size] = route;
      printf ("add : node[%d]: ", w->index);
      path_route_print (array[routing_multipath_size]);
      update++;
      change++;
    }

  for (i = 0; i < routing_multipath_size + 1; i++)
    {
      if (array[i] && path_route_is_loop (array[i]))
        {
          assert (route == array[i]);
          path_route_delete (array[i]);
          route = NULL;
          array[i] = NULL;
        }
    }

  qsort (array, routing_multipath_size + 1,
         sizeof (struct path_route *), path_route_cmp);

  if (array[routing_multipath_size])
    path_route_delete (array[routing_multipath_size]);
  memcpy (table->entry, array,
          sizeof (struct path_route *) * routing_multipath_size);
  free (array);

  if (change)
    {
      path_table_print_node_dest (w, destination);
      routing_path_route_install (w, destination, w->g);
    }

  if (! improve && ! change)
    return NULL;
  if (improve && ! change && ! update_best)
    return NULL;

  return table->entry[0];
}

struct path_route *
path_route_copy (struct path_route *route)
{
  struct path_route *copy;
  int i;

  copy = path_route_create ();
  copy->destination = route->destination;
  for (i = 0; i < route->path_list->size; i++)
    vector_set (copy->path_list, i, vector_get (route->path_list, i));
  copy->bandwidth = route->bandwidth;
  copy->bandwidth_wo_load = route->bandwidth_wo_load;
  copy->nexthop = route->nexthop;
  return copy;
}

struct message_event *
message_event (struct node *s, struct node *t, int event_id,
               struct path_route *route)
{
  struct message_event *event;

  event = (struct message_event *) malloc (sizeof (struct message_event));
  memset (event, 0, sizeof (struct message_event));
  event->s = s;
  event->t = t;
  event->event_id = event_id;
  event->destination = route->destination;
  return event;
}

void
routing_path_vector_route_to (struct node *root, struct graph *g)
{
  struct path_route *route;
  struct node *x;
  int event_id = 0;
  struct vector_node *vn;
  struct message_event *event;
  struct link *link;
  struct path_table *table;
  struct pqueue *process_queue;

  process_queue = pqueue_create ();
  process_queue->cmp = event_cmp;

  route = path_route_create ();
  route->destination = root;
  route->bandwidth = UINT_MAX;
  route->bandwidth_wo_load = UINT_MAX;
  vector_add (root, route->path_list);
  route->nexthop = root;
  table = vector_get (root->path_tables, root->index);
  table->entry[0] = route;
  routing_path_route_install (root, root, g);

  for (vn = vector_head (root->olinks); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;
      x = link->to;
      event = message_event (root, x, event_id++, route);
      pqueue_enqueue (event, process_queue);
    }

  while (process_queue->size)
    {
      event = pqueue_dequeue (process_queue);
      table = vector_get (event->s->path_tables, event->destination->index);
      route = table->entry[0];
      route = path_route_propagate (event->s, event->t,
                                    path_route_copy (route));
      x = event->t;
      free (event);
      if (! route)
        continue;

      for (vn = vector_head (x->olinks); vn; vn = vector_next (vn))
        {
          link = (struct link *) vn->data;
          event = message_event (link->from, link->to, event_id++, route);
          pqueue_enqueue (event, process_queue);
        }
    }

  pqueue_delete (process_queue);
}

void
routing_path_vector (struct graph *g)
{
  struct vector_node *vnt;
  struct node *t;

  routing_path_vector_init (g);

  for (vnt = vector_head (g->nodes); vnt; vnt = vector_next (vnt))
    {
      t = (struct node *) vnt->data;
      if (t == NULL)
        continue;
      routing_path_vector_route_to (t, g);
    }
}

void
routing_path_vector_update_link (struct graph *g, struct link *e)
{
  struct node *v, *w, *dest, *x;
  struct path_route *route;
  struct path_table *table;
  struct vector_node *vn;
  struct pqueue *process_queue;
  struct link *link;
  int event_id = 0;
  struct message_event *event;

  process_queue = pqueue_create ();
  process_queue->cmp = event_cmp;

  printf ("update routing for change on link %d->%d\n",
          e->from->index, e->to->index);
  v = e->to;
  w = e->from;

  for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
    {
      dest = (struct node *) vn->data;
      if (! dest)
        continue;

      table = vector_get (v->path_tables, dest->index);
      assert (table);

      if (! table->entry[0])
        continue;

      event = message_event (v, w, event_id++, table->entry[0]);
      pqueue_enqueue (event, process_queue);
    }

  while (process_queue->size)
    {
      event = pqueue_dequeue (process_queue);
      table = vector_get (event->s->path_tables, event->destination->index);
      route = table->entry[0];
      route = path_route_propagate (event->s, event->t,
                                    path_route_copy (route));
      x = event->t;
      free (event);
      if (! route)
        continue;

      for (vn = vector_head (x->olinks); vn; vn = vector_next (vn))
        {
          link = (struct link *) vn->data;
          event = message_event (link->from, link->to, event_id++,
                                 path_route_copy (route));
          pqueue_enqueue (event, process_queue);
        }
    }

  pqueue_delete (process_queue);
}


