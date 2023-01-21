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
#include "network/graph.h"
#include "network/weight.h"
#include "network/path.h"

#include "command.h"
#include "shell.h"

#include "reliability.h"

struct cubebit *
cubebit_create ()
{
  struct cubebit *cubebit;

  cubebit = (struct cubebit *) malloc (sizeof (struct cubebit));
  cubebit->type = 'x';
  cubebit->label = 0;

  return cubebit;
}

void
cubebit_delete (struct cubebit *cubebit)
{
  free (cubebit);
}

struct cubebit *
cubebit_copy (struct cubebit *cubebit)
{
  struct cubebit *copy;

  copy = (struct cubebit *) malloc (sizeof (struct cubebit));
  copy->type = cubebit->type;
  copy->label = cubebit->label;

  return copy;
}

struct cube *
cube_create (struct path *path)
{
  struct cube *cube;
  struct node *node, *prev;
  struct link *link;
  struct vector_node *vn;
  int i;
  struct cubebit *cubebit;

  cube = (struct cube *) malloc (sizeof (struct cube));
  cube->v = vector_create ();

  node = vector_get (path->path, 0);
  for (i = 0; i < node->g->links->size; i++)
    {
      cubebit = cubebit_create ();
      vector_set (cube->v, i, cubebit);
    }

  prev = NULL;
  for (vn = vector_head (path->path); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;

      if (prev)
        {
          link = link_lookup (prev, node, node->g);
          cubebit = vector_get (cube->v, link->id);
          cubebit->type = '1';
        }

      prev = node;
    }

  return cube;
}

void
cube_delete (struct cube *cube)
{
  struct cubebit *cubebit;
  struct vector_node *vn;
  for (vn = vector_head (cube->v); vn; vn = vector_next (vn))
    {
      cubebit = (struct cubebit *) vn->data;
      cubebit_delete (cubebit);
    }
  vector_delete (cube->v);
  free (cube);
}

struct cube *
cube_copy (struct cube *cube)
{
  struct cube *copy;
  struct cubebit *cubebit;
  int i;

  copy = (struct cube *) malloc (sizeof (struct cube));
  copy->v = vector_create ();

  for (i = 0; i < cube->v->size; i++)
    {
      cubebit = cubebit_copy ((struct cubebit *) vector_get (cube->v, i));
      vector_set (copy->v, i, cubebit);
    }

  return copy;
}

void
cube_sprint (char *s, int size, struct cube *cube)
{
  char tmp[256];
  struct vector_node *vn;
  struct cubebit *cubebit;

  for (vn = vector_head (cube->v); vn; vn = vector_next (vn))
    {
      cubebit = (struct cubebit *) vn->data;
      tmp[0] = cubebit->type;
      tmp[1] = '\0';
      strncat (s, tmp, size - strlen (s) - 1);
      if (cubebit->label)
        {
          snprintf (tmp, sizeof (tmp), "_{%u}", cubebit->label);
          strncat (s, tmp, size - strlen (s) - 1);
        }
    }
}

void
cube_print (FILE *fp, struct cube *cube)
{
  char buf[1024];
  memset (buf, 0, sizeof (buf));
  cube_sprint (buf, sizeof (buf), cube);
  fprintf (fp, "%s", buf);
}

int
get_gamma (struct cube *C)
{
  int i;
  int gamma = 0;
  struct cubebit *c;
  for (i = 0; i < C->v->size; i++)
    {
      c = (struct cubebit *) vector_get (C->v, i);
      if (gamma < c->label)
        gamma = c->label;
    }
  return gamma;
}

struct cube *
C_create (struct cube *A, struct cube *B)
{
  struct cube *C;
  int i;
  struct cubebit *a, *b, *c;
  int gamma;

  C = cube_copy (A);
  gamma = get_gamma (C);
  for (i = 0; i < A->v->size; i++)
    {
      a = (struct cubebit *) vector_get (A->v, i);
      b = (struct cubebit *) vector_get (B->v, i);
      c = (struct cubebit *) vector_get (C->v, i);
      if (a->type == 'x' && b->type == '1')
        {
          c->type = '0';
          c->label = gamma + 1;
        }
    }

  return C;
}

int
have_01_condition (struct cube *A, struct cube *B)
{
  int i;
  struct cubebit *a, *b;

  for (i = 0; i < A->v->size; i++)
    {
      a = (struct cubebit *) vector_get (A->v, i);
      b = (struct cubebit *) vector_get (B->v, i);
      if (a->type == '0' && b->type == '1')
        return 1;
    }
  return 0;
}

int
split_condition (struct cube *A, struct cube *B, int v)
{
  int i;
  int condition_01 = 0;
  int condition_0x = 0;
  struct cubebit *a, *b;

  for (i = 0; i < A->v->size; i++)
    {
      a = (struct cubebit *) vector_get (A->v, i);
      b = (struct cubebit *) vector_get (B->v, i);
      if (a->type == '0' && a->label == v && b->type == '1')
        condition_01++;
      if (a->type == '0' && a->label == v && b->type == 'x')
        condition_0x++;
    }

  if (condition_01 && condition_0x)
    return 1;
  return 0;
}

int
have_x1_condition (struct cube *A, struct cube *B)
{
  int i;
  struct cubebit *a, *b;

  for (i = 0; i < A->v->size; i++)
    {
      a = (struct cubebit *) vector_get (A->v, i);
      b = (struct cubebit *) vector_get (B->v, i);
      if (a->type == 'x' && b->type == '1')
        return 1;
    }
  return 0;
}

#define SUBSET          1
#define DISJOINT        2
#define X1              3
#define SPLIT_RECURSIVE 4

int
relation (struct cube *A, struct cube *B)
{
  int gamma, v;

  if (have_01_condition (A, B))
    {
      gamma = get_gamma (A);
      for (v = 1; v <= gamma; v++)
        {
          if (split_condition (A, B, v))
            return SPLIT_RECURSIVE;
        }
      return DISJOINT;
    }

  if (have_x1_condition (A, B))
    return X1;

  return SUBSET;
}

struct cube *
A1_create (struct cube *A, struct cube *B, int u)
{
  struct cube *A1;
  int i;
  struct cubebit *a, *b, *c;

  A1 = cube_copy (A);
  for (i = 0; i < A->v->size; i++)
    {
      a = (struct cubebit *) vector_get (A->v, i);
      b = (struct cubebit *) vector_get (B->v, i);
      c = (struct cubebit *) vector_get (A1->v, i);
      if (a->type == '0' && a->label == u && b->type == '1')
        {
          c->type = '1';
          c->label = 0;
        }
    }

  return A1;
}

struct cube *
Ax_create (struct cube *A, struct cube *B, int u)
{
  struct cube *Ax;
  int i;
  struct cubebit *a, *b, *c;

  Ax = cube_copy (A);
  for (i = 0; i < A->v->size; i++)
    {
      a = (struct cubebit *) vector_get (A->v, i);
      b = (struct cubebit *) vector_get (B->v, i);
      c = (struct cubebit *) vector_get (Ax->v, i);
      if (a->type == '0' && a->label == u && b->type == 'x')
        {
          c->type = 'x';
          c->label = 0;
        }
    }

  return Ax;
}

int detail = 0;
int sdp_stat = 0;
int sdp_stat_detail = 0;

void
operator_little (struct vector *terms, struct cube *B, struct shell *shell,
                 struct sdp_stat *stat)
{
  struct vector *result;
  struct vector_node *vn;
  struct cube *A, *C, *A1, *Ax;
  int gamma, u, v;
  struct vector *newterms;
  int class = 0;

  if (detail)
    {
      fprintf (shell->terminal, "(");
      for (vn = vector_head (terms); vn; vn = vector_next (vn))
        {
          A = (struct cube *) vn->data;
          fprintf (shell->terminal, " + ");
          cube_print (shell->terminal, A);
        }
      fprintf (shell->terminal, ") # ");
      cube_print (shell->terminal, B);
      fprintf (shell->terminal, "\n");
    }

  if (stat)
    stat->intermediates = terms->size;

  result = vector_create ();
  for (vn = vector_head (terms); vn; vn = vector_next (vn))
    {
      A = (struct cube *) vn->data;
      class = relation (A, B);
      switch (class)
        {
        case SUBSET:
          /* Do nothing */
          if (detail)
            fprintf (shell->terminal, "= 0 (subset)\n");
          if (stat)
            stat->subset++;
          break;

        case DISJOINT:
          /* add A */
          vector_add (cube_copy (A), result);
          if (detail)
            {
              fprintf (shell->terminal, "= ");
              fprintf (shell->terminal, "+ ");
              cube_print (shell->terminal, A);
              fprintf (shell->terminal, " (disjoint)\n");
            }
          if (stat)
            stat->disjoint++;
          break;

        case X1:
          C = C_create (A, B);
          vector_add (C, result);
          if (detail)
            {
              fprintf (shell->terminal, "= ");
              fprintf (shell->terminal, "+ ");
              cube_print (shell->terminal, C);
              fprintf (shell->terminal, " (x1)\n");
            }
          if (stat)
            stat->x1++;
          break;

        case SPLIT_RECURSIVE:
          gamma = get_gamma (A);
          for (v = 1; v <= gamma; v++)
            {
              if (! split_condition (A, B, v))
                continue;
              u = v;
              break;
            }
          A1 = A1_create (A, B, u);
          Ax = Ax_create (A, B, u);

          if (detail)
            {
              fprintf (shell->terminal, "= ");
              fprintf (shell->terminal, "+ ");
              cube_print (shell->terminal, A1);
              fprintf (shell->terminal, " # ");
              cube_print (shell->terminal, B);
              fprintf (shell->terminal, " + ");
              cube_print (shell->terminal, Ax);
              fprintf (shell->terminal, " (split-recursive)\n");
            }

          /* A1 \natural B */
          newterms = vector_create ();
          vector_add (A1, newterms);
          operator_little (newterms, B, shell, NULL);
          vector_catenate (result, newterms);
          vector_delete (newterms);

          /* + Ax */
          vector_add (Ax, result);

          if (stat)
            stat->split_recursive++;
          break;

        default:
          assert (0);
          break;
        }
    }

  for (vn = vector_head (terms); vn; vn = vector_next (vn))
    {
      A = (struct cube *) vn->data;
      cube_delete (A);
    }
  vector_clear (terms);
  vector_catenate (terms, result);
  vector_delete (result);

  if (detail)
    {
      fprintf (shell->terminal, "=");
      for (vn = vector_head (terms); vn; vn = vector_next (vn))
        {
          A = (struct cube *) vn->data;
          fprintf (shell->terminal, " + ");
          cube_print (shell->terminal, A);
        }
      fprintf (shell->terminal, "\n");
    }
}

double
calculate_state_reliability (struct node *s, struct node *t,
                             struct vector *state, struct shell *shell)
{
  struct vector_node *vn;
  struct cube *cube;
  int i, u;
  int gamma;
  struct cubebit *a;
  struct link *link;
  double reliability = 0.0;

  if (detail)
    {
      fprintf (shell->terminal, "calculate reliability from %d to %d",
               s->id, t->id);

      for (vn = vector_head (state); vn; vn = vector_next (vn))
        {
          cube = (struct cube *) vn->data;
          gamma = get_gamma (cube);

          fprintf (shell->terminal, " + ");
          for (i = 0; i < cube->v->size; i++)
            {
              a = (struct cubebit *) vector_get (cube->v, i);
              if (a->type != '1')
                continue;
              fprintf (shell->terminal, "p%d", i);
            }

          for (u = 1; u <= gamma; u++)
            {
              fprintf (shell->terminal, "(1 - ");
              for (i = 0; i < cube->v->size; i++)
                {
                  a = (struct cubebit *) vector_get (cube->v, i);
                  if (a->type != '0' || a->label != u)
                    continue;
                  fprintf (shell->terminal, "p%d", i);
                }
              fprintf (shell->terminal, ")_{%d}", u);
            }
        }
      fprintf (shell->terminal, "\n");

      fprintf (shell->terminal, " = ");
      for (vn = vector_head (state); vn; vn = vector_next (vn))
        {
          cube = (struct cube *) vn->data;
          gamma = get_gamma (cube);
          fprintf (shell->terminal, " + ");

          for (i = 0; i < cube->v->size; i++)
            {
              a = (struct cubebit *) vector_get (cube->v, i);
              if (a->type != '1')
                continue;
              link = link_lookup_by_id (i, s->g);
              fprintf (shell->terminal, "(%f)", link->reliability);
            }

          for (u = 1; u <= gamma; u++)
            {
              fprintf (shell->terminal, "(1 - ");
              for (i = 0; i < cube->v->size; i++)
                {
                  a = (struct cubebit *) vector_get (cube->v, i);
                  if (a->type != '0' || a->label != u)
                    continue;
                  link = link_lookup_by_id (i, s->g);
                  fprintf (shell->terminal, "(%f)", link->reliability);
                }
              fprintf (shell->terminal, ")_{%d}", u);
            }
        }
      fprintf (shell->terminal, "\n");
    }

  if (detail)
    fprintf (shell->terminal, " = ");

  for (vn = vector_head (state); vn; vn = vector_next (vn))
    {
      double reliability_term = 0.0;

      cube = (struct cube *) vn->data;
      gamma = get_gamma (cube);

      for (i = 0; i < cube->v->size; i++)
        {
          a = (struct cubebit *) vector_get (cube->v, i);
          if (a->type != '1')
            continue;
          link = link_lookup_by_id (i, s->g);
          if (reliability_term == 0.0)
            reliability_term = link->reliability;
          else
            reliability_term *= link->reliability;
        }

      if (detail)
        fprintf (shell->terminal, " + %f", reliability_term);

      for (u = 1; u <= gamma; u++)
        {
          double subreliability = 0.0;
          for (i = 0; i < cube->v->size; i++)
            {
              a = (struct cubebit *) vector_get (cube->v, i);
              if (a->type != '0' || a->label != u)
                continue;
              link = link_lookup_by_id (i, s->g);
              if (subreliability == 0.0)
                subreliability = link->reliability;
              else
                subreliability *= link->reliability;
            }
          if (reliability_term == 0.0)
            reliability_term = (1 - subreliability);
          else
            reliability_term *= (1 - subreliability);

          if (detail)
            fprintf (shell->terminal, " * (1 - %f)_{%d}\n", subreliability, u);
        }

      if (reliability == 0.0)
        reliability = reliability_term;
      else
        reliability += reliability_term;
    }

  return reliability;
}

void
debug_break ()
{
}

void
incremental_path_reliability (struct node *s, struct node *t, struct path *path,
                              unsigned long path_index,
                              struct vector *previous, struct vector *state,
                              struct shell *shell)
{
  struct cube *MPi, *MPj;
  struct cube *cube;
  struct vector *result;
  struct vector_node *vn;
  struct sdp_stat stat;

  memset (&stat, 0, sizeof (stat));

  if (sdp_stat_detail)
    fprintf (shell->terminal, "path-%lu:\n", path_index);

  MPi = cube_create (path);

  if (detail)
    {
      fprintf (shell->terminal, "path = ");
      print_nodelinklist (shell->terminal, path->path);
      fprintf (shell->terminal, "\n");
      fprintf (shell->terminal, "cube = ");
      cube_print (shell->terminal, MPi);
      fprintf (shell->terminal, "\n");
    }

  result = vector_create ();
  vector_add (cube_copy (MPi), result);

  if (path_index == 244)
    debug_break ();

  for (vn = vector_head (previous); vn; vn = vector_next (vn))
    {
      MPj = (struct cube *) vn->data;

      if (detail)
        fprintf (shell->terminal, "path-%d: ", vn->index);

      operator_little (result, MPj, shell, &stat);

      if (sdp_stat_detail)
        fprintf (shell->terminal, "path-%d: intermediate terms: %u, "
                 "subset: %u, disjoint: %u, x1: %u, split-recursive: %u\n",
                 vn->index, result->size, stat.subset, stat.disjoint,
                 stat.x1, stat.split_recursive);
    }

  if (detail)
    {
      /* print result cubes */
      fprintf (shell->terminal, "result =");
      for (vn = vector_head (result); vn; vn = vector_next (vn))
        {
          cube = (struct cube *) vn->data;
          fprintf (shell->terminal, " + ");
          cube_print (shell->terminal, cube);
        }
      fprintf (shell->terminal, "\n");
    }

  vector_catenate (state, result);
  vector_delete (result);
  stat.states = state->size;

  vector_add (cube_copy (MPi), previous);
  cube_delete (MPi);

  if (sdp_stat)
    fprintf (shell->terminal, "s-t: %u-%u stat[path=%lu]: states: %u, "
             "intermediates: %u, subset: %u, disjoint: %u, x1: %u, "
             "split-recursive: %u\n",
             s->id, t->id, path_index, stat.states, stat.intermediates,
             stat.subset, stat.disjoint, stat.x1, stat.split_recursive);
}


void
st_reliability (struct node *s, struct node *t, struct shell *shell)
{
  struct path *path;
  struct cube *cube;
  struct vector *previous, *state;
  struct vector_node *vn;
  double reliability;
  unsigned long path_index = 0;
  struct timeval start, end;

  if (detail)
    fprintf (shell->terminal, "# <- operator 'little'\n");

  previous = vector_create ();
  state = vector_create ();

  gettimeofday (&start, NULL);

  for (path = path_enum_first (s); path; path = path_enum_next (path))
    if (path_end (path) == t)
      {
        incremental_path_reliability (s, t, path, path_index, previous, state, shell);
        path_index++;
      }

  /* delete path cube list */
  for (vn = vector_head (previous); vn; vn = vector_next (vn))
    {
      cube = (struct cube *) vn->data;
      cube_delete (cube);
    }
  vector_delete (previous);

  if (detail)
    {
      /* print state cubes */
      fprintf (shell->terminal, "state =");
      for (vn = vector_head (state); vn; vn = vector_next (vn))
        {
          cube = (struct cube *) vn->data;
          fprintf (shell->terminal, " + ");
          cube_print (shell->terminal, cube);
        }
      fprintf (shell->terminal, "\n");
    }

  /* calculate reliability */
  reliability = calculate_state_reliability (s, t, state, shell);

  gettimeofday (&end, NULL);

  fprintf (shell->terminal, "s-t: %u-%u reliability = %.10f (%d terms)\n",
           s->id, t->id, reliability, state->size);

  if (sdp_stat)
    {
      fprintf (shell->terminal, "s-t: %u-%u start time: %lu.%06lu\n", s->id, t->id,
               start.tv_sec, start.tv_usec);
      fprintf (shell->terminal, "s-t: %u-%u end time: %lu.%06lu\n", s->id, t->id,
               end.tv_sec, end.tv_usec);
      fprintf (shell->terminal, "s-t: %u-%u time taken: %lu.%06lu\n", s->id, t->id,
               end.tv_sec - start.tv_sec - (end.tv_usec < start.tv_usec ? 1 : 0),
               (end.tv_usec < start.tv_usec ? 1000000 : 0) +
                end.tv_usec - start.tv_usec);
    }

  /* delete state cubes */
  for (vn = vector_head (state); vn; vn = vector_next (vn))
    {
      cube = (struct cube *) vn->data;
      cube_delete (cube);
    }
  vector_delete (state);
}

DEFINE_COMMAND (calculate_reliability_source_destination,
                "calculate reliability source <0-4294967295> destination <0-4294967295>",
                "calculate\n"
                "calculate reliability\n"
                "specify source node\n"
                "specify source node\n"
                "specify destination node\n"
                "specify destination node\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  unsigned long sid, tid;
  struct node *s, *t;

  if (! strcmp (argv[argc - 1], "detail"))
    detail++;
  if (! strcmp (argv[argc - 1], "stat-detail"))
    {
      sdp_stat++;
      sdp_stat_detail++;
    }
  if (! strcmp (argv[argc - 2], "stat-detail"))
    {
      sdp_stat++;
      sdp_stat_detail++;
    }
  if (! strcmp (argv[argc - 1], "stat"))
    sdp_stat++;
  if (! strcmp (argv[argc - 2], "stat"))
    sdp_stat++;

  sid = strtoul (argv[3], NULL, 0);
  tid = strtoul (argv[5], NULL, 0);
  s = node_lookup (sid, G);
  if (! s)
    {
      fprintf (shell->terminal, "No such node: %lu\n", sid);
      return;
    }
  t = node_lookup (tid, G);
  if (! t)
    {
      fprintf (shell->terminal, "No such node: %lu\n", tid);
      return;
    }

  if (s == t)
    {
      fprintf (shell->terminal, "source == destination: %lu\n", tid);
      return;
    }

  st_reliability (s, t, shell);

  detail = 0;
  sdp_stat = 0;
}


ALIAS_COMMAND (calculate_reliability_source_destination_detail,
               calculate_reliability_source_destination,
                "calculate reliability source <0-4294967295> destination <0-4294967295> detail",
                "calculate\n"
                "calculate reliability\n"
                "specify source node\n"
                "specify source node\n"
                "specify destination node\n"
                "specify destination node\n"
                "show computation details\n")

ALIAS_COMMAND (calculate_reliability_source_destination_stat,
               calculate_reliability_source_destination,
                "calculate reliability source <0-4294967295> destination <0-4294967295> stat",
                "calculate\n"
                "calculate reliability\n"
                "specify source node\n"
                "specify source node\n"
                "specify destination node\n"
                "specify destination node\n"
                "show computation details\n")

ALIAS_COMMAND (calculate_reliability_source_destination_stat_detail,
               calculate_reliability_source_destination,
                "calculate reliability source <0-4294967295> destination <0-4294967295> stat detail",
                "calculate\n"
                "calculate reliability\n"
                "specify source node\n"
                "specify source node\n"
                "specify destination node\n"
                "specify destination node\n"
                "show statistics\n"
                "show computation details\n")

ALIAS_COMMAND (calculate_reliability_source_destination_sdp_stat_detail,
               calculate_reliability_source_destination,
                "calculate reliability source <0-4294967295> destination <0-4294967295> stat-detail",
                "calculate\n"
                "calculate reliability\n"
                "specify source node\n"
                "specify source node\n"
                "specify destination node\n"
                "specify destination node\n"
                "show detailed statistics\n")

ALIAS_COMMAND (calculate_reliability_source_destination_sdp_stat_detail_detail,
               calculate_reliability_source_destination,
                "calculate reliability source <0-4294967295> destination <0-4294967295> stat-detail detail",
                "calculate\n"
                "calculate reliability\n"
                "specify source node\n"
                "specify source node\n"
                "specify destination node\n"
                "specify destination node\n"
                "show detailed statistics\n"
                "show computation details\n")

DEFINE_COMMAND (calculate_reliability_all_to_all,
                "calculate reliability all-to-all",
                "calculate\n"
                "calculate reliability\n"
                "calculate all-to-all reliability\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  struct node *s, *t;
  struct vector_node *vns, *vnt;

  if (! strcmp (argv[argc - 1], "detail"))
    detail++;
  if (! strcmp (argv[argc - 1], "stat"))
    sdp_stat++;
  if (! strcmp (argv[argc - 2], "stat"))
    sdp_stat++;

  for (vns = vector_head (G->nodes); vns; vns = vector_next (vns))
    {
      s = (struct node *) vns->data;
      for (vnt = vector_head (G->nodes); vnt; vnt = vector_next (vnt))
        {
          t = (struct node *) vnt->data;
          if (s == t)
            continue;

          st_reliability (s, t, shell);

        }
    }

  detail = 0;
  sdp_stat = 0;
}

ALIAS_COMMAND (calculate_reliability_all_to_all_stat,
               calculate_reliability_all_to_all,
                "calculate reliability all-to-all stat",
                "calculate\n"
                "calculate reliability\n"
                "calculate all-to-all reliability\n"
                "show statistics\n")

DEFINE_COMMAND (calculate_reliability_all_to_half,
                "calculate reliability all-to-half",
                "calculate\n"
                "calculate reliability\n"
                "calculate all-to-half reliability\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  struct node *s, *t;
  struct vector_node *vns, *vnt;

  if (! strcmp (argv[argc - 1], "detail"))
    detail++;
  if (! strcmp (argv[argc - 1], "stat"))
    sdp_stat++;
  if (! strcmp (argv[argc - 2], "stat"))
    sdp_stat++;

  for (vns = vector_head (G->nodes); vns; vns = vector_next (vns))
    {
      s = (struct node *) vns->data;
      for (vnt = vector_head (G->nodes); vnt; vnt = vector_next (vnt))
        {
          t = (struct node *) vnt->data;

          if (vns->index < vnt->index)
            st_reliability (s, t, shell);
        }
    }

  detail = 0;
  sdp_stat = 0;
}

ALIAS_COMMAND (calculate_reliability_all_to_half_stat,
               calculate_reliability_all_to_half,
                "calculate reliability all-to-half stat",
                "calculate\n"
                "calculate reliability\n"
                "calculate all-to-half reliability\n"
                "show statistics\n")

DEFINE_COMMAND (link_all_reliability,
                "link all reliability <[-]ddd.ddd>",
                "link\n"
                "select all link\n"
                "specify all link's reliability\n"
                "specify all link's reliability\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  double reliability;
  struct vector_node *vn;
  struct link *link;

  reliability = strtod (argv[3], NULL);

  for (vn = vector_head (G->links); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;
      link->reliability = reliability;
    }
}


