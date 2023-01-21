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

#ifndef _ROUTING_H_
#define _ROUTING_H_

struct nexthop
{
  struct node *node;
  double ratio;
};

struct route
{
  struct vector *nexthops;
};

struct routing
{
  unsigned long id;
  char *name;
  struct graph *G;
  struct weight *W;
  u_int nnodes;
  struct route **route;
  struct vector *config;

  /* algorithm specific data */
  void *data;
  void (*data_free) (void *data);
};

EXTERN_COMMAND (routing_graph);
//EXTERN_COMMAND (routing_dijkstra);
//EXTERN_COMMAND (routing_lbra);
EXTERN_COMMAND (show_route);
EXTERN_COMMAND (show_route_source);
EXTERN_COMMAND (show_route_destination);
EXTERN_COMMAND (clear_route);

void nexthop_delete_all (struct vector *nexthops);
struct nexthop *nexthop_create ();
void nexthop_delete (struct nexthop *nexthop);

void route_add (struct node *s, struct node *t, struct node *nexthop,
                struct routing *routing);

void route_path_probability (struct path *path, struct node *dst,
                        struct routing *routing);
struct path *
route_path_random_forward (struct node *s, struct node *t,
                           struct routing *routing);

struct path * route_path_first (struct node *src, struct node *dst,
                  struct routing *routing);
struct path * route_path_next (struct path *path, struct node* dst,
                 struct routing *routing);

struct routing *routing_create ();
struct route **route_table_create (int nnodes);

struct graph *
graph_import_routing (struct routing *routing, struct node *destnode);

void routing_init ();
void routing_finish ();

#endif /*_ROUTING_H_*/

