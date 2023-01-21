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

#ifndef _GRAPH_H_
#define _GRAPH_H_

#include "vector.h"
#include "command.h"

struct node
{
  struct graph *g;
  unsigned int id;

  struct vector *olinks; /* outgoing links */
  struct vector *ilinks; /* incoming links */

  struct in_addr addr;
  unsigned int plen;
  unsigned char type;
#define NODE_TYPE_ROUTER  1
#define NODE_TYPE_NETWORK 2

  char *name;
  char *domain_name;
  char *descr;

  double xpos;
  double ypos;

  /* for MA Ordering */
  unsigned int label;      /* represents node order */
  unsigned int adjacency;  /* number of adjacency */
  double bandwidth;        /* bandwidth to labeled nodes */

  /* for Dijkstra Labeling */
  unsigned int metric;
  struct node *nexthop;
};

struct link
{
  struct graph *g;
  unsigned int id;

  struct node *from;
  struct node *to;
  struct link *inverse;

  char *name;
  char *descr;
  double bandwidth;
  double delay;
  double length;
  unsigned long weight;
  double reliability;  /* link reliability */
  double probability;  /* routing probability */
};

struct graph
{
  unsigned long id;
  char *name;
  struct vector *nodes;
  struct vector *links;

  struct vector *config;
};

struct node *node_create (unsigned int id, struct graph *g);
void node_delete (struct node *v);
void node_add (struct node *v, struct graph *g);
void node_remove (struct node *v, struct graph *g);
struct node *node_lookup (unsigned int id, struct graph *g);
struct node *node_lookup_by_name (char *name, struct graph *g);

struct link *link_create (unsigned int id, struct graph *g);
void link_delete (struct link *e);
void link_add (struct link *e, struct graph *g);
void link_remove (struct link *e, struct graph *g);

struct graph *graph_create ();
void graph_clear (struct graph *G);
void graph_delete (struct graph *g);

struct node *node_get (unsigned int id, struct graph *g);
struct node *node_get_by_name (char *name, struct graph *g);
void node_set (unsigned int id, struct graph *g);

struct link *link_lookup_by_id (unsigned int id, struct graph *g);
struct link *link_lookup (struct node *s, struct node *t, struct graph *g);
struct link *link_lookup_by_node_id (unsigned int source, unsigned int sink, struct graph *g);
struct link *link_get_by_id (unsigned int id, struct graph *g);
void link_connect (struct link *e, struct node *s, struct node *t, struct graph *g);
struct link *link_get (struct node *s, struct node *t, struct graph *g);
struct link *link_get_by_node_id (unsigned int source, unsigned int sink, struct graph *g);
void link_set (struct node *s, struct node *t, struct graph *g);
void link_set_by_node_id (unsigned int source, unsigned int sink, struct graph *g);

int graph_nodes (struct graph *G);
int graph_edges (struct graph *G);

struct node *node_copy_create (struct node *node, struct graph *g);
struct link *link_copy_create (struct link *link, struct graph *g);

struct graph *graph_copy_create (struct graph *G);
void graph_copy (struct graph *dst, struct graph *src);
void graph_pack_id (struct graph *G);

void graph_print (struct graph *g);

#endif /*_GRAPH_H_*/


