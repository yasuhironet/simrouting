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

#ifndef _DEMAND_H_
#define _DEMAND_H_

typedef double demand_t;

struct demand_matrix
{
  u_int nnodes;
  demand_t **traffic;
  demand_t total; /* Total amount of traffic flows */
};

struct traffic
{
  unsigned long id;
  char *name;
  struct graph *G;
  unsigned int seed;
  struct demand_matrix *demands;
  struct vector *config;
};

struct demand_matrix *demand_matrix_create (u_int nnodes);
void demand_matrix_delete (struct demand_matrix *demands);
struct demand_matrix *demand_matrix_copy (struct demand_matrix *demands);

struct traffic *traffic_lookup (unsigned long id);

EXTERN_COMMAND (traffic_enter);
EXTERN_COMMAND (show_traffic_summary);
EXTERN_COMMAND (show_traffic_id);

void traffic_init ();
void traffic_finish ();

#endif /*_DEMAND_H_*/
