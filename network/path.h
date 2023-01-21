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

#ifndef _PATH_H_
#define _PATH_H_

#include "vector.h"

struct path
{
  struct vector *path;
  unsigned long cost;
  double probability;
};

struct path *path_create ();
struct path *path_copy (struct path *path);
void path_delete (struct path *p);

char *sprint_nodelist (char *buf, int size, struct vector *path);
void print_nodelist (FILE *fp, struct vector *path);

char *sprint_nodelinklist (char *buf, int size, struct vector *path);
void print_nodelinklist (FILE *fp, struct vector *path);

struct node *path_end (struct path *p);
int path_is_dest (struct node *dest, struct path *path);
int path_is_include (struct path *path, struct node *ts, struct node *tt);
int path_compare (struct path *p1, struct path *p2);

struct path *path_enum_first (struct node *src);
struct path *path_enum_next (struct path *path);

double path_probability (struct path *path);

void path_print (FILE *fp, struct node *s, struct node *t);
void path_print_source_to_all (FILE *fp, struct node *s);
void path_print_cost (FILE *fp, struct node *s, struct node *t, struct weight *W);
void path_print_probability (FILE *fp, struct node *s, struct node *t);
void path_print_probability_with_fail (FILE *fp, struct node *s, struct node *t,
                                  struct node *fs, struct node *ft);

EXTERN_COMMAND (show_path);
EXTERN_COMMAND (show_path_source_to_all);
EXTERN_COMMAND (show_path_all_to_dest);
EXTERN_COMMAND (show_path_all);
EXTERN_COMMAND (show_path_cost);
EXTERN_COMMAND (show_path_probability);
EXTERN_COMMAND (show_path_probability_failure);

#endif /*_PATH_H_*/

