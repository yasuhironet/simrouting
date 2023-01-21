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

#ifndef _CONNECTIVITY_H_
#define _CONNECTIVITY_H_

struct connectivity
{
  int N;
  int *node_array;
  int *tree_size;
};

void connectivity_init (int N, struct connectivity *c);
void connectivity_connect (int p, int q, struct connectivity *c);
int is_connectivity (struct connectivity *c);
void connectivity_finish (struct connectivity *c);

#endif /*_CONNECTIVITY_H_*/

