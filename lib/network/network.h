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

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include "network/weight.h"
#include "network/path.h"

#if 0
#include "routing/deflection.h"
#endif /*0*/
#include "traffic-model/demand.h"

struct network
{
  unsigned long id;
  char *name;
  struct graph *G;
  struct traffic *T;
  struct routing *R;
  u_int nnodes;
  u_int nedges;
  struct vector *flows;
  struct vector **flows_on_node;
  struct vector **flows_on_edge;
  struct vector **flows_drop_on_node;
  struct vector *config;
};

struct flow
{
  int source;
  int sink;
  demand_t bandwidth;
  struct vector *path;
};

void network_init ();
void network_finish ();

struct vector *prime_vector_create (int limit);

int chash_selection (int router_id, unsigned int tag,
                     void *hash_tables);
int deflection_selection (int router_id, unsigned int tag,
                          void *router_primes);

#if 0
void
deflect_1_tag_forward (struct node *dst, unsigned int tag,
                       struct deflection_info **dinfo_table,
                       int (*nhselection) (int, unsigned int, void *),
                       void *router_seeds, struct path *path);
void
deflect_2_tag_forward (struct node *dst, unsigned int tag,
                       struct deflection_info **dinfo_table,
                       int (*nhselection) (int, unsigned int, void *),
                       void *router_seeds, struct path *path);
void
deflect_3_tag_forward (struct node *dst, unsigned int tag,
                       struct deflection_info **dinfo_table,
                       int (*nhselection) (int, unsigned int, void *),
                       void *router_seeds, struct path *path);
#endif /*0*/


#endif /*_NETWORK_H_*/

