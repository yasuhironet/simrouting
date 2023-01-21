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

#ifndef _MARA_MC_MMMF_H_
#define _MARA_MC_MMMF_H_

struct mara_node
{
  struct node *node;
  unsigned int label;
  unsigned int adjacency;
  double bandwidth;
  int pqueue_index;
};

void mara_pqueue_index_update (void *data, int index);

struct mara_node *mara_table_create (struct graph *graph);
void mara_table_delete (struct graph *graph, struct mara_node *table);
void mara_table_clear (struct graph *graph, struct mara_node *table);
struct mara_node **mara_data_create (struct graph *graph);
void mara_data_delete (struct graph *graph, void *data);
void mara_data_clear (struct graph *graph, struct mara_node **data);

int mara_mc_cmp (void *va, void *vb);
int mara_mmmf_cmp (void *va, void *vb);

#ifdef DEBUG
void routing_mara_pqueue_debug (struct pqueue *pqueue);
#endif /*DEBUG*/

void routing_mara_route_node (struct node *s, struct routing *routing);

EXTERN_COMMAND (routing_algorithm_mara_mc_node);
EXTERN_COMMAND (routing_algorithm_mara_mc_node_all);
EXTERN_COMMAND (routing_algorithm_mara_mc);

EXTERN_COMMAND (routing_algorithm_mara_mmmf_node_all);
EXTERN_COMMAND (routing_algorithm_mara_mmmf_node);
EXTERN_COMMAND (routing_algorithm_mara_mmmf);

#endif /*_MARA_MC_MMMF_H_*/

