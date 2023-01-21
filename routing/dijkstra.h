
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

#ifndef _DIJKSTRA_H_
#define _DIJKSTRA_H_

struct dijkstra_path
{
  struct node *node;
  unsigned int metric;
  struct vector *nexthops;
  int pqueue_index;
};

struct dijkstra_path *dijkstra_table_create (struct graph *graph);
void dijkstra_table_delete (struct graph *graph, struct dijkstra_path *table);
void dijkstra_table_clear (struct graph *graph, struct dijkstra_path *table);
void *dijkstra_data_create (struct graph *graph);
void dijkstra_data_delete (struct graph *graph, void *data);
void dijkstra_data_clear (struct graph *graph, void *data);

int dijkstra_candidate_cmp (void *a, void *b);
void dijkstra_candidate_update (void *data, int index);

void routing_dijkstra (struct node *root, struct weight *weight,
                             struct routing *R);
void routing_dijkstra_route (struct node *root, struct routing *R);

EXTERN_COMMAND (routing_algorithm_dijkstra_node);
EXTERN_COMMAND (routing_algorithm_dijkstra_node_all);
EXTERN_COMMAND (routing_algorithm_dijkstra);

#endif /*_DIJKSTRA_H_*/

