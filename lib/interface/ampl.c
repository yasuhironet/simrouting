/*
 * Copyright (C) 2007,2008  Yasuhiro Ohara
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

#include "includes.h"

#include "file.h"
#include "vector.h"
#include "shell.h"
#include "command.h"

#include "network/graph.h"
#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"
#include "traffic-model/demand.h"

DEFINE_COMMAND(export_ampl_append_graph,
               "export ampl append <FILENAME>",
               "export information\n"
               "export AMPL data\n"
               "export AMPL data by appending to file\n"
               "specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *g = (struct graph *) shell->context;
  struct vector_node *vni, *vnj;
  struct node *i, *j;
  struct link *link;
  int c = 0;
  FILE *fp;

  fp = fopen_create (argv[3], "a");
  if (! fp)
    {
      fprintf (stderr, "Cannot open file %s: %s\n",
               argv[3], strerror (errno));
      return;
    }

  fprintf (fp, "param N := %d;\n", g->nodes->size);
  fprintf (fp, "\n");

  fprintf (fp, "set E :=\n");
  for (vni = vector_head (g->nodes); vni; vni = vector_next (vni))
    {
      i = (struct node *) vni->data;
      for (vnj = vector_head (i->olinks); vnj; vnj = vector_next (vnj))
        {
          link = (struct link *) vnj->data;
          if (link == NULL)
            continue;

          assert (i == link->from);
          j = link->to;

          fprintf (fp, " (%d,%d)", i->id, j->id);
          c++;
          if (c % 5 == 0)
            fprintf (fp, "\n");
        }
    }
  fprintf (fp, ";\n");
  fprintf (fp, "\n");

  fprintf (fp, "param cap :=\n");
  for (vni = vector_head (g->links); vni; vni = vector_next (vni))
    {
      link = (struct link *) vni->data;
      if (link == NULL)
        continue;
      fprintf (fp, "  [%d,%d] %lf,\n",
               link->from->id, link->to->id, link->bandwidth);
    }
  fprintf (fp, ";\n");
  fprintf (fp, "\n");
  fflush (fp);
  fclose (fp);
}


DEFINE_COMMAND (export_ampl_append_routing,
                "export ampl append <FILENAME>",
                "export information\n"
                "export AMPL data\n"
                "export AMPL data by appending to file\n"
                "specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  int s, t;
  struct vector_node *vn;
  FILE *fp;
  int c = 0;

  fp = fopen_create (argv[3], "a");
  if (! fp)
    {
      fprintf (stderr, "Cannot open file %s: %s\n",
               argv[3], strerror (errno));
      return;
    }

  fprintf (fp, "set R :=\n ");
  for (t = 0; t < routing->nnodes; t++)
    {
      for (s = 0; s < routing->nnodes; s++)
        {
          if (s == t)
            continue;
          for (vn = vector_head (routing->route[s][t].nexthops); vn;
               vn = vector_next (vn))
            {
              struct nexthop *nexthop = (struct nexthop *) vn->data;
              if (nexthop)
                {
                  fprintf (fp, " (%d,%d,%d)",
                           t, s, nexthop->node->id);
                  c++;
                  if (c % 5 == 0)
                    fprintf (fp, "\n");
                }
            }
        }
      fprintf (fp, "\n ");
    }
  fprintf (fp, ";\n");
  fprintf (fp, "\n");
  fflush (fp);
  fclose (fp);
}

DEFINE_COMMAND (export_ampl_append_traffic,
                "export ampl append <FILENAME>",
                "export information\n"
                "export AMPL data\n"
                "export AMPL data by appending to file\n"
                "specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct traffic *traffic = (struct traffic *) shell->context;
  int i, j;
  FILE *fp;

  fp = fopen_create (argv[3], "a");
  if (! fp)
    {
      fprintf (stderr, "Cannot open file %s: %s\n",
               argv[3], strerror (errno));
      return;
    }

  fprintf (fp, "%7s", "param D");
  fprintf (fp, " %3s", ":");
  for (j = 0; j < traffic->demands->nnodes; j++)
    fprintf (fp, " %5d", j);
  fprintf (fp, " :=\n");

  for (i = 0; i < traffic->demands->nnodes; i++)
    {
      fprintf (fp, "%7s", " ");
      fprintf (fp, " %3d", i);
      for (j = 0; j < traffic->demands->nnodes; j++)
        {
          if (i == j)
            fprintf (fp, " %5.2f", 0.0);
          else
            fprintf (fp, " %5.2f", traffic->demands->traffic[i][j]);
        }
      fprintf (fp, "\n");
    }
  fprintf (fp, ";\n");
  fprintf (fp, "\n");
  fflush (fp);
  fclose (fp);
}


DEFINE_COMMAND(import_ampl_routing_ratio,
               "import ampl routing-ratio <FILENAME>",
               "import information\n"
               "import AMPL data\n"
               "import AMPL routing-ratio from file\n"
               "specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct routing *routing = (struct routing *) shell->context;
  FILE *fp;
  char buf1[4098], buf2[4098], *buf;
  char *argv1[1024], *argv2[1024], **arg, **header;
  int narg;
  struct vector *nexthops;
  struct vector_node *vn;
  int in_rmatrix = 0, in_continue = 0;
  int i, t, x, y;
  char *word, *stringp;
  char *ret;
  double ratio;
  struct nexthop *match;

  fp = fopen (argv[3], "r");
  if (! fp)
    {
      fprintf (stderr, "Cannot open file %s: %s\n",
               argv[3], strerror (errno));
      return;
    }

  in_rmatrix = 0;
  t = -1;
  buf = buf1;
  arg = argv1;
  header = NULL;

  while ((ret = fgets (buf, sizeof (buf1), fp)) != NULL)
    {
      stringp = buf;
      narg = 0;
      while ((word = strsep (&stringp, " [],\n")) != NULL)
        {
          if (*word == '\0')
            continue;
          arg[narg++] = word;
        }
      arg[narg] = NULL;

      if (! narg)
        continue;

      if (! strcmp (arg[0], "r"))
        {
          in_rmatrix++;

          t = (int) strtoul (arg[1], NULL, 0);
          assert (t < routing->G->nodes->size);
          assert (! strcmp (arg[2], "*"));
          assert (! strcmp (arg[3], "*"));
          continue;
        }

      if (! in_rmatrix)
        continue;

      if (narg > 2 && ! strcmp (arg[1], "*") && ! strcmp (arg[2], "*"))
        {
          t = (int) strtoul (arg[0], NULL, 0);
          assert (t < routing->G->nodes->size);
          continue;
        }

      if (! strcmp (arg[0], ":"))
        {
          header = (arg == argv1 ? argv1 : argv2);
          buf = (buf == buf1 ? buf2 : buf1);
          arg = (arg == argv1 ? argv2 : argv1);
          continue;
        }

      if (isdigit (arg[0][0]))
        {
          x = (int) strtoul (arg[0], NULL, 0);
          assert (x < routing->G->nodes->size);

          if (narg == 1)
            {
              in_continue++;
              continue;
            }

          for (i = 1; i < narg; i++)
            {
              if (! strcmp (arg[i], "."))
                continue;

              y = (int) strtoul (header[i], NULL, 0);
              assert (y < routing->G->nodes->size);
              ratio = strtod (arg[i], NULL);

              fprintf (shell->terminal, "r[%d,%d,%d] = %f\n",
                       t, x, y, ratio);

              match = NULL;
              nexthops = routing->route[x][t].nexthops;
              for (vn = vector_head (nexthops); vn; vn = vector_next (vn))
                {
                  struct nexthop *n = (struct nexthop *) vector_data (vn);
                  if (n->node->id == y)
                    match = n;
                }

              assert (match);
              match->ratio = ratio;
            }
        }

      if (in_continue)
        {
           for (i = 0; i < narg; i++)
            {
              if (! strcmp (arg[i], "."))
                continue;

              y = (int) strtoul (header[i+1], NULL, 0);
              assert (y < routing->G->nodes->size);
              ratio = strtod (arg[i], NULL);

              fprintf (shell->terminal, "r[%d,%d,%d] = %f\n",
                       t, x, y, ratio);

              match = NULL;
              nexthops = routing->route[x][t].nexthops;
              for (vn = vector_head (nexthops); vn; vn = vector_next (vn))
                {
                  struct nexthop *n = (struct nexthop *) vector_data (vn);
                  if (n->node->id == y)
                    match = n;
                }

              assert (match);
              match->ratio = ratio;
            }
          in_continue = 0;
          continue;
        }

      if (! strcmp (arg[0], ";"))
        {
          in_rmatrix = 0;
          continue;
        }

      /* do nothing; read next line */
    }

  fclose (fp);
}


