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

#ifndef _LOAD_H_
#define _LOAD_H_

struct flow
{
  struct node *s;
  struct node *t;
  u_int demand;
  struct vector *path;
};

void load_clear (struct graph *g);
void load_update (struct link *e);
void load_traffic_flow (struct graph *g, struct node *s, struct node *t,
                        struct demand_matrix *D);
void load_traffic_forward_flow (struct node *v, struct flow *flow,
                        struct graph *g);
void load_traffic_demand (struct graph *g, struct demand_matrix *D);

EXTERN_COMMAND (clear_load);
EXTERN_COMMAND (load_traffic);

#endif /*_LOAD_H_*/


