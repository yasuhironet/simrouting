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

#ifndef _WEIGHT_H_
#define _WEIGHT_H_

typedef unsigned long weight_t;

struct weight
{
  unsigned long id;
  char *name;
  struct graph *G;
  u_int nedges;
  weight_t *weight;
  struct vector *config;
};

void weight_save_config (struct weight *w);

EXTERN_COMMAND (weight_enter);
EXTERN_COMMAND (show_weight_summary);
EXTERN_COMMAND (show_weight_id);

void weight_init ();
void weight_finish ();

struct weight *weight_create ();

#endif /*_WEIGHT_H_*/

