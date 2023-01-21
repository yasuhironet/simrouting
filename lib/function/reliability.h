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

#ifndef _RELIABILITY_H_
#define _RELIABILITY_H_

struct cubebit
{
  char type; /* '1', 'x', or '0' */
  unsigned int label; /* label for '0' */
};

struct cube
{
  struct vector *v;
};

struct sdp_stat
{
  unsigned int states;
  unsigned int intermediates;
  unsigned int subset;
  unsigned int disjoint;
  unsigned int x1;
  unsigned int split_recursive;
};

struct cubebit *cubebit_create ();
void cubebit_delete (struct cubebit *cubebit);
struct cube *cube_create (struct path *path);
void cube_delete (struct cube *cube);
void cube_sprint (char *s, int size, struct cube *cube);
void cube_print (FILE *fp, struct cube *cube);

EXTERN_COMMAND (calculate_reliability_source_destination);
EXTERN_COMMAND (calculate_reliability_source_destination_detail);
EXTERN_COMMAND (calculate_reliability_source_destination_stat);
EXTERN_COMMAND (calculate_reliability_source_destination_stat_detail);
EXTERN_COMMAND (calculate_reliability_source_destination_sdp_stat_detail);
EXTERN_COMMAND (calculate_reliability_source_destination_sdp_stat_detail_detail);
EXTERN_COMMAND (calculate_reliability_all_to_all);
EXTERN_COMMAND (calculate_reliability_all_to_all_stat);
EXTERN_COMMAND (calculate_reliability_all_to_half);
EXTERN_COMMAND (calculate_reliability_all_to_half_stat);

EXTERN_COMMAND (link_all_reliability);

#endif /*_RELIABILITY_H_*/


