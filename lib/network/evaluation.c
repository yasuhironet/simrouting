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

#include <includes.h>

#include "vector.h"
#include "graph.h"
#include "demand.h"

u_int
phi (struct link *e)
{
  double u = e->utilization;
  assert (u >= 0.0);
  if (0.0 <= u && u < 1.0/3.0)
    return 1;
  if (1.0/3.0 <= u && u < 2.0/3.0)
    return 3;
  if (2.0/3.0 <= u && u < 9.0/10.0)
    return 10;
  if (9.0/10.0 <= u && u < 1.0)
    return 70;
  if (1.0 <= u && u < 11.0/10.0)
    return 500;
  return 5000;
}


u_int
total_cost (struct graph *g)
{
  u_int Phi = 0;
  struct vector_node *vn;
  struct link *e;

  for (vn = vector_head (g->links); vn; vn = vector_next (vn))
    {
      e = (struct link *) vn->data;
      if (e == NULL)
        continue;
      Phi += phi (e);
    }

  return Phi;
}

double
max_utilization (struct graph *g)
{
  struct vector_node *vn;
  struct link *link;
  double max = 0.0;

  for (vn = vector_head (g->links); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;
      if (! link)
        continue;

      if (max < link->utilization)
        max = link->utilization;
    }

  return max;
}

void
evaluation (char *desc, struct graph *g, u_int Psi, struct demand_matrix *D)
{
  u_int Phi;
  double maxutil;

  Phi = total_cost (g);
  maxutil = max_utilization (g);

  printf ("\n");
  printf ("%s: Phi: %u Psi: %u nPhi: %1.3f MaxUtil: %1.3f #demand: %u\n",
          desc, Phi, Psi, (double) Phi / Psi, maxutil, D->total);
  printf ("\n");
}


