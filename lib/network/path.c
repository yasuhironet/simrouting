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
#include "module.h"

#include "shell.h"
#include "command.h"
#include "command_shell.h"

#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"

struct path *
path_create ()
{
  struct path *p;
  p = (struct path *) malloc (sizeof (struct path));
  memset (p, 0, sizeof (struct path));
  p->path = vector_create ();
  p->cost = 0;
  return p;
}

struct path *
path_copy (struct path *path)
{
  struct path *p;
  p = (struct path *) malloc (sizeof (struct path));
  memset (p, 0, sizeof (struct path));
  p->path = vector_copy (path->path);
  p->cost = path->cost;
  return p;
}

void
path_delete (struct path *p)
{
  vector_delete (p->path);
  free (p);
}

char *
sprint_nodelist (char *buf, int size, struct vector *path)
{
  char *p = buf;
  int left = size;
  int ret;
  struct vector_node *vn;
  struct node *x;

  /* print head node of the path */
  memset (buf, 0, size);
  vn = vector_head (path);
  if (! vn)
    return NULL;
  x = (struct node *) vn->data;
  ret = snprintf (p, left, "%d", x->id);
  p += ret;
  left -= ret;

  /* print the rest nodes */
  while (left > 0 && (vn = vector_next (vn)) != NULL)
    {
      x = (struct node *) vn->data;
      ret = snprintf (p, left, " %d", x->id);
      p += ret;
      left -= ret;
    }

  return buf;
}

void
print_nodelist (FILE *fp, struct vector *path)
{
  char buf[256];
  sprint_nodelist (buf, sizeof (buf), path);
  fprintf (fp, "%s", buf);
  fflush (fp);
}

char *
sprint_nodelinklist (char *buf, int size, struct vector *path)
{
  struct vector_node *vn;
  struct node *node, *prev;
  struct link *link;
  char tmp[128];

  memset (tmp, 0, sizeof (tmp));
  prev = NULL;
  for (vn = vector_head (path); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;

      if (prev)
        {
          link = link_lookup (prev, node, node->g);
          snprintf (tmp, sizeof (tmp), "-(%u)-", link->id);
          strncat (buf, tmp, size - strlen (buf) - 1);
        }

      snprintf (tmp, sizeof (tmp), "%u", node->id);
      strncat (buf, tmp, size - strlen (buf) - 1);

      prev = node;
    }

  return buf;
}

void
print_nodelinklist (FILE *fp, struct vector *path)
{
  char buf[256];
  memset (buf, 0, sizeof (buf));
  sprint_nodelinklist (buf, sizeof (buf), path);
  fprintf (fp, "%s", buf);
  fflush (fp);
}

void
sprint_path (char *buf, int size, struct path *path)
{
  char nodelist[256];
  sprint_nodelist (nodelist, sizeof (nodelist), path->path);
  snprintf (buf, size, "path: %s (cost %lu)",
            nodelist, path->cost);
}

void
print_path (FILE *fp, struct path *path)
{
  char buf[256];
  sprint_path (buf, sizeof (buf), path);
  fprintf (fp, "%s", buf);
  fflush (fp);
}

struct node *
path_end (struct path *p)
{
  struct node *end;

  if (p->path->size <= 0)
    return NULL;

  end = vector_get (p->path, p->path->size - 1);
  return end;
}

int
path_is_dest (struct node *dest, struct path *path)
{
  if (path_end (path) == dest)
    return 1;
  return 0;
}

int
path_is_include (struct path *path, struct node *ts, struct node *tt)
{
  int index;
  struct node *s, *t;
  for (index = 0; index < path->path->size; index++)
    {
      s = (struct node *) path->path->array[index];
      t = NULL;
      if (index + 1 < path->path->size)
        t = (struct node *) path->path->array[index + 1];

      if (s == ts && t == tt)
        return 1;
      if (s == tt && t == ts)
        return 1;
    }
  return 0;
}

int
path_compare (struct path *p1, struct path *p2)
{
  int i;
  if (p1->cost != p2->cost)
    return p1->cost - p2->cost;
  if (p1->path->size != p2->path->size)
    return p1->path->size - p2->path->size;
  for (i = 0; i < p1->path->size; i++)
    if (p1->path->array[i] != p2->path->array[i])
      return p1->path->array[i] - p2->path->array[i];
  return 0;
}

/* enumeration of all possible paths from src node
   by depth first search */

struct path *
path_enum_first (struct node *src)
{
  struct path *path = path_create ();
  vector_add (src, path->path);
  return path;
}

struct path *
path_enum_next (struct path *path)
{
  struct node *current, *parent;
  struct link *link, *currentlink;
  int index;
  struct vector_node *vn;

  /* first try to lengthen the path by adding the node with least ID */
  current = path_end (path);
  for (vn = vector_head (current->olinks); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;

      /* avoid reversion of a node in the path */
      if (vector_lookup (link->to, path->path))
        continue;

      /* a new longer path found. return it. */
      vector_add (link->to, path->path);
      vector_break (vn);
      return path;
    }

  /* next try to increase the rightmost node-id in the path */
  for (index = path->path->size - 1; index > 0; index--)
    {
      int currentlink_found = 0;

      /* remove stale part (path is going to be shorten here) */
      if ((vector_get (path->path, index + 1)) != NULL)
        vector_remove (vector_get (path->path, index + 1), path->path);

      /* locate current link in this search */
      current = vector_get (path->path, index);
      parent = vector_get (path->path, index - 1);
      currentlink = link_get (parent, current, parent->g);

      for (vn = vector_head (parent->olinks); vn; vn = vector_next (vn))
        {
          link = (struct link *) vn->data;
          if (link == currentlink)
            {
              currentlink_found++;
              continue;
            }

          /* skip until currentlink is reached */
          if (! currentlink_found)
            continue;

          /* avoid reversion of a node in the path */
          if (vector_lookup (link->to, path->path))
            continue;

          /* new link found. return the path
             after replacing the current node */
          vector_set (path->path, index, link->to);
          vector_break (vn);
          return path;
        }
    }

  /* if neither method fails to produce new path,
     delete memory and return NULL */
  assert (index == 0);
  path_delete (path);
  return NULL;
}

unsigned long
path_cost (struct path *path, struct weight *W)
{
  struct node *s = NULL, *t = NULL;
  struct vector_node *vn;
  struct link *link;

  path->cost = 0;

  for (vn = vector_head (path->path); vn; vn = vector_next (vn))
    {
      s = t;
      t = (struct node *) vn->data;

      if (s == NULL)
        continue;

      link = link_get (s, t, s->g);
      assert (link);

      path->cost += W->weight[link->id];
    }

  return path->cost;
}


double
path_probability (struct path *path)
{
  struct node *s = NULL, *t = NULL;
  struct vector_node *vn;
  struct link *link;

  path->probability = 1.0;

  for (vn = vector_head (path->path); vn; vn = vector_next (vn))
    {
      s = t;
      t = (struct node *) vn->data;

      if (s == NULL)
        continue;

      link = link_get (s, t, s->g);
      assert (link);

      if (link->probability == 0.0)
        {
          fprintf (stderr, "link %u-%u: probability is not set\n",
                   link->from->id, link->to->id);
          vector_break (vn);
          path->probability = 1.0;
          return path->probability;
        }

      path->probability *= link->probability;
    }

  return path->probability;
}

void
path_print (FILE *fp, struct node *s, struct node *t)
{
  struct path *path;
  char buf[256];
  unsigned long max = 0;
  unsigned long min = 0;
  unsigned long num = 0;
  unsigned long sum = 0;
  double avg = 0.0;

  if (s == t)
    {
      num = max = min = sum = 1;
      avg = (double)sum / num;
      fprintf (fp, "num: %lu, min: %lu, max: %lu, avg: %f\n", num, min, max, avg);
      return;
    }

  for (path = path_enum_first (s); path; path = path_enum_next (path))
    if (path_end (path) == t)
      {
        num++;
        if (max < path->path->size)
          max = path->path->size;
        if (min == 0 || min > path->path->size)
          min = path->path->size;
        sum += path->path->size;

        sprint_nodelist (buf, sizeof (buf), path->path);
        fprintf (stderr, "path: %s\n", buf);
      }
  avg = (double)sum / num;
  fprintf (fp, "num: %lu, min: %lu, max: %lu, avg: %f\n", num, min, max, avg);
}

void
path_print_source_to_all (FILE *fp, struct node *s)
{
  struct path *path;
  char buf[256];
  unsigned long max = 0;
  unsigned long num = 0;
  unsigned long sum = 0;
  double avg = 0.0;

  for (path = path_enum_first (s); path; path = path_enum_next (path))
    {
      num++;
      if (max < path->path->size)
        max = path->path->size;
      sum += path->path->size;

      sprint_nodelist (buf, sizeof (buf), path->path);
      fprintf (fp, "path: %s\n", buf);
    }
  avg = (double)sum / num;
  fprintf (fp, "num: %lu, max: %lu, avg: %f\n", num, max, avg);
}

void
path_print_all_to_dest (FILE *fp, struct node *t)
{
  struct path *path;
  char buf[256];
  struct vector_node *vn;
  struct node *s;
  unsigned long max = 0;
  unsigned long min = 0;
  unsigned long num = 0;
  unsigned long sum = 0;
  double avg = 0.0;

  for (vn = vector_head (t->g->nodes); vn; vn = vector_next (vn))
    {
      s = (struct node *) vn->data;
      for (path = path_enum_first (s); path; path = path_enum_next (path))
        if (path_end (path) == t)
          {
            num++;
            if (max < path->path->size)
              max = path->path->size;
            if (min == 0 || min > path->path->size)
              min = path->path->size;
            sum += path->path->size;

            sprint_nodelist (buf, sizeof (buf), path->path);
            fprintf (fp, "path: %s\n", buf);
          }
    }
  avg = (double)sum / num;
  fprintf (fp, "num: %lu, min: %lu, max: %lu, avg: %f\n", num, min, max, avg);
}

void
path_print_cost (FILE *fp, struct node *s, struct node *t, struct weight *weight)
{
  struct path *path;

  for (path = path_enum_first (s); path; path = path_enum_next (path))
    if (path_end (path) == t)
      {
        path_cost (path, weight);
        print_path (fp, path);
        fprintf (fp, "\n");
      }
}

void
path_print_probability (FILE *fp, struct node *s, struct node *t)
{
  struct path *path;
  char buf[256];
  double p;
  double sum = 0.0;

  for (path = path_enum_first (s); path; path = path_enum_next (path))
    if (path_end (path) == t)
      {
        p = path_probability (path);
        sprint_nodelist (buf, sizeof (buf), path->path);
        fprintf (fp, "path: %s: %f\n", buf, p);
        sum += p;
      }

  fprintf (fp, "sum of all path's probability: %f\n", sum);
}

void
path_print_probability_with_fail (FILE *fp, struct node *s, struct node *t,
                                  struct node *fs, struct node *ft)
{
  struct path *path;
  char buf[256];
  double ret;
  double p_valid = 0.0;
  double p_inval = 0.0;

  for (path = path_enum_first (s); path; path = path_enum_next (path))
    if (path_is_dest (t, path))
      {
        ret = path_probability (path);
        sprint_nodelist (buf, sizeof (buf), path->path);
        if (path_is_include (path, fs, ft))
          {
            fprintf (fp, "path: %s, invalid: p=%.3f\n", buf, ret);
            fprintf (fp, "p_inval: %.3f = %.3f + %.3f\n",
                     p_inval + ret, p_inval, ret);
            p_inval += ret;
          }
        else
          {
            fprintf (fp, "path: %s, valid: p=%.3f\n", buf, ret);
            fprintf (fp, "p_valid: %.3f = %.3f + %.3f\n",
                     p_valid + ret, p_valid, ret);
            p_valid += ret;
          }
      }

  fprintf (fp, "failed link: %d-%d, p_valid=%.3f, p_inval=%.3f",
           fs->id, ft->id, p_valid, p_inval);
}

DEFINE_COMMAND (show_path_source_to_all,
                "show path source <0-4294967295>",
                "display information\n"
                "display path information\n"
                "specify source node\n"
                "specify source node\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  unsigned long id;
  struct node *s;

  id = strtoul (argv[3], NULL, 0);
  s = node_lookup (id, G);
  if (! s)
    {
      fprintf (shell->terminal, "No such node: %lu\n", id);
      return;
    }

  path_print_source_to_all (shell->terminal, s);
  fprintf (shell->terminal, "\n");
}

DEFINE_COMMAND (show_path_all_to_dest,
                "show path destination <0-4294967295>",
                "display information\n"
                "display path information\n"
                "specify destination node\n"
                "specify destination node\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  unsigned long id;
  struct node *t;

  id = strtoul (argv[3], NULL, 0);
  t = node_lookup (id, G);
  if (! t)
    {
      fprintf (shell->terminal, "No such node: %lu\n", id);
      return;
    }

  path_print_all_to_dest (shell->terminal, t);
  fprintf (shell->terminal, "\n");
}

DEFINE_COMMAND (show_path,
                "show path source <0-4294967295> destination <0-4294967295>",
                "display information\n"
                "display path information\n"
                "specify source node\n"
                "specify source node\n"
                "specify destination node\n"
                "specify destination node\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  unsigned long source_id, dest_id;
  struct node *start, *dest;

  source_id = strtoul (argv[3], NULL, 0);
  dest_id = strtoul (argv[5], NULL, 0);

  start = node_lookup (source_id, G);
  if (! start)
    {
      fprintf (shell->terminal, "No such node: %lu\n", source_id);
      return;
    }

  dest = node_lookup (dest_id, G);
  if (! dest)
    {
      fprintf (shell->terminal, "No such node: %lu\n", dest_id);
      return;
    }

  path_print (shell->terminal, start, dest);

  fprintf (shell->terminal, "\n");
}

DEFINE_COMMAND (show_path_all,
                "show path all-to-all",
                "display information\n"
                "display path information\n"
                "display all path information\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  struct node *s, *t;
  struct vector_node *vns, *vnt;

  for (vns = vector_head (G->nodes); vns; vns = vector_next (vns))
    {
      s = (struct node *) vns->data;
      for (vnt = vector_head (G->nodes); vnt; vnt = vector_next (vnt))
         {
           t = (struct node *) vnt->data;

           if (s != t)
             path_print (shell->terminal, s, t);
         }
    }

  fprintf (shell->terminal, "\n");
}


DEFINE_COMMAND (show_path_cost,
                "show path source <0-4294967295> destination <0-4294967295> cost weight <0-4294967295>",
                "display information\n"
                "display path information\n"
                "specify source node\n"
                "specify source node\n"
                "specify destination node\n"
                "specify destination node\n"
                "show cost of the path\n"
                "specify weight\n"
                "specify weight\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  unsigned long source_id, dest_id;
  struct node *start, *dest;
  struct weight *W = NULL;

  source_id = strtoul (argv[3], NULL, 0);
  dest_id = strtoul (argv[5], NULL, 0);

  W = (struct weight *) instance_lookup ("weight", argv[8]);
  if (W == NULL)
    {
      fprintf (shell->terminal, "no such weight: weight-%s\n", argv[8]);
      return;
    }

  start = node_lookup (source_id, G);
  if (! start)
    {
      fprintf (shell->terminal, "No such node: %lu\n", source_id);
      return;
    }

  dest = node_lookup (dest_id, G);
  if (! dest)
    {
      fprintf (shell->terminal, "No such node: %lu\n", dest_id);
      return;
    }

  path_print_cost (shell->terminal, start, dest, W);

  fprintf (shell->terminal, "\n");
}

DEFINE_COMMAND (show_path_probability,
                "show path source <0-4294967295> destination <0-4294967295> probability routing <0-4294967295>",
                "display information\n"
                "display path information\n"
                "specify source node\n"
                "specify source node\n"
                "specify destination node\n"
                "specify destination node\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  unsigned long source_id, dest_id;
  struct node *start, *dest;
  struct routing *R = NULL;

  source_id = strtoul (argv[3], NULL, 0);
  dest_id = strtoul (argv[5], NULL, 0);

  R = (struct routing *) instance_lookup ("routing", argv[8]);
  if (R == NULL)
    {
      fprintf (shell->terminal, "no such routing: routing-%s\n", argv[8]);
      return;
    }

  start = node_lookup (source_id, G);
  if (! start)
    {
      fprintf (shell->terminal, "No such node: %lu\n", source_id);
      return;
    }

  dest = node_lookup (dest_id, G);
  if (! dest)
    {
      fprintf (shell->terminal, "No such node: %lu\n", dest_id);
      return;
    }

  path_print_probability (shell->terminal, start, dest);

  fprintf (shell->terminal, "\n");
}

DEFINE_COMMAND (show_path_probability_failure,
                "show path source <0-4294967295> destination <0-4294967295> probability routing <0-4294967295> failure-link <0-4294967295> <0-4294967295>",
                "display information\n"
                "display path information\n"
                "specify source node\n"
                "specify source node\n"
                "specify destination node\n"
                "specify destination node\n"
                "specify failure link\n"
                "specify failure link's source node-id\n"
                "specify failure link's destination node-id\n"
                )
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  unsigned long source_id, dest_id;
  unsigned long f_source_id, f_dest_id;
  struct node *start, *dest;
  struct node *fs, *ft;
  struct routing *R = NULL;

  source_id = strtoul (argv[3], NULL, 0);
  dest_id = strtoul (argv[5], NULL, 0);
  f_source_id = strtoul (argv[10], NULL, 0);
  f_dest_id = strtoul (argv[11], NULL, 0);

  R = (struct routing *) instance_lookup ("routing", argv[8]);
  if (R == NULL)
    {
      fprintf (shell->terminal, "no such routing: routing-%s\n", argv[8]);
      return;
    }

  start = node_lookup (source_id, G);
  if (! start)
    {
      fprintf (shell->terminal, "No such node: %lu\n", source_id);
      return;
    }

  dest = node_lookup (dest_id, G);
  if (! dest)
    {
      fprintf (shell->terminal, "No such node: %lu\n", dest_id);
      return;
    }

  fs = node_lookup (f_source_id, G);
  if (! fs)
    {
      fprintf (shell->terminal, "No such node: %lu\n", f_source_id);
      return;
    }

  ft = node_lookup (f_dest_id, G);
  if (! ft)
    {
      fprintf (shell->terminal, "No such node: %lu\n", f_dest_id);
      return;
    }

  path_print_probability_with_fail (shell->terminal, start, dest, fs, ft);

  fprintf (shell->terminal, "\n");
}





