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

#ifndef _PATH_VECTOR_H_
#define _PATH_VECTOR_H_

struct path_route
{
  struct node *destination;
  struct vector *path_list;
  u_int bandwidth;
  u_int bandwidth_wo_load;
  struct node *nexthop;
};

struct message_event
{
  struct node *s;
  struct node *t;
  int event_id;
  struct node *destination;
};

struct path_table
{
  unsigned int size;
  struct path_route **entry;
};

void path_table_print (struct graph *g);

void routing_path_vector_clear (struct graph *g);
void routing_path_vector (struct graph *g);
void routing_path_vector_update_link (struct graph *g, struct link *e);

#endif /*_PATH_VECTOR_H_*/

