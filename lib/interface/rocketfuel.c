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

#include "shell.h"
#include "command.h"
#include "command_shell.h"
#include "table.h"

#include "network/graph.h"
#include "network/weight.h"

/* uid @loc [+] [bb] (num_neigh) [&ext] -> <nuid-1> <nuid-2> ... {-euid} ... =name[!] rn */

#define REGMATCH_MAX 16

#define START "^"
#define END "$"
#define SPACE "[ \t]+"
#define MAYSPACE "[ \t]*"

#define ROCKETFUEL_MAPS_LINE \
  START "(-*[0-9]+)" SPACE "(@[?A-Za-z0-9,+]+)" SPACE \
  "(\\+)*" MAYSPACE "(bb)*" MAYSPACE \
  "\\(([0-9]+)\\)" SPACE "(&[0-9]+)*" MAYSPACE \
  "->" MAYSPACE "(<[0-9 \t<>]+>)*" MAYSPACE \
  "(\\{-[0-9\\{\\} \t-]+\\})*" SPACE \
  "=([A-Za-z0-9.!-]+)" SPACE "r([0-9])" \
  MAYSPACE END

int
scan_maps_line (struct shell *shell, char *buf, regex_t *regexp,
                int *nodeid, char **location, int *dnsflag, int *bbflag,
                int *nnbr, int *next, char **neighbors, char **externs,
                char **name, int *radius)
{
  int i, ret;
  char *endptr;
  char *line;
  int argc;
  char *argv[REGMATCH_MAX];
  regmatch_t regmatch[REGMATCH_MAX];

  ret = regexec (regexp, buf, REGMATCH_MAX, regmatch, 0);
  if (ret == REG_NOMATCH)
    {
      fprintf (shell->terminal, "match failed: %s", buf);
      return -1;
    }
  else
    fprintf (shell->terminal, "matched: %s", buf);

  line = strdup (buf);
  argc = 0;

  /* regmatch[0] is the entire strings that matched */
  for (i = 1; i < REGMATCH_MAX; i++)
    {
      if (regmatch[i].rm_so == -1)
        argv[i-1] = NULL;
      else
        {
          line[regmatch[i].rm_eo] = '\0';
          argv[i-1] = &line[regmatch[i].rm_so];
          argc = i;
        }
    }

#if 1
  for (i = 0; i < argc; i++)
    fprintf (shell->terminal, "argv[%d]: %s\n", i, argv[i]);
#endif /*0*/

  *nodeid = strtol (argv[0], &endptr, 0);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid node id: %s\n", argv[0]);
      return -1;
    }

  *location = strdup (argv[1]);
  if (*location == NULL)
    {
      fprintf (shell->terminal, "strdup for location failed: %s\n",
               strerror (errno));
      return -1;
    }

  if (argv[2])
    *dnsflag = 1;
  else
    *dnsflag = 0;

  if (argv[3])
    *bbflag = 1;
  else
    *bbflag = 0;

  /* the first char should be '&' */
  *nnbr = strtod (argv[4], &endptr);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid #nbr: %s\n", argv[4]);
      return -1;
    }

  /* the first char should be '&' */
  if (argv[5])
    {
      *next = strtol (&argv[5][1], &endptr, 0);
      if (*endptr != '\0')
        {
          fprintf (shell->terminal, "invalid #ext: %s\n", argv[5]);
          return -1;
        }
    }

  /* neighbors */
  if (argv[6])
    {
      *neighbors = strdup (argv[6]);
      if (*neighbors == NULL)
        {
          fprintf (shell->terminal, "strdup for neighbors failed: %s\n",
                   strerror (errno));
          return -1;
        }
    }

  /* externs */
  if (argv[7])
    {
      *externs = strdup (argv[7]);
      if (*externs == NULL)
        {
          fprintf (shell->terminal, "strdup for externs failed: %s\n",
                   strerror (errno));
          return -1;
        }
    }

  /* name */
  if (argv[8])
    {
      *name = strdup (argv[8]);
      if (*name == NULL)
        {
          fprintf (shell->terminal, "strdup for name failed: %s\n",
                   strerror (errno));
          return -1;
        }
    }

  *radius = strtol (argv[9], &endptr, 0);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid radius: %s\n", argv[9]);
      return -1;
    }

#if 1
/* uid @loc [+] [bb] (num_neigh) [&ext] -> <nuid-1> <nuid-2> ... {-euid} ... =name[!] rn */
  fprintf (shell->terminal,
           "Load Node[%d]: location: %s flags: %s%s "
           "neighbors: \"%s\"(%d) externals: \"%s\"(%d) "
           "name: %s radius: %d\n",
           *nodeid, *location,
           (*dnsflag ? "dns" : ""), (*bbflag ? "bb" : ""),
           *neighbors, *nnbr,
           (*externs ? *externs : ""), *next,
           (*name ? *name : ""), *radius);
#else
  fprintf (shell->terminal, "Load Node[%d].\n", *nodeid);
#endif /*0*/

  free (line);
  return 0;
}

int
read_maps_file (struct shell *shell, struct graph *G, char *filename)
{
  FILE *fp;
  int ret;
  char buf[8192];
  regex_t regex;

/* uid @loc [+] [bb] (num_neigh) [&ext] -> <nuid-1> <nuid-2> ... {-euid} ... =name[!] rn */
  int nodeid, dnsflag, bbflag, nnbr, next, radius;
  char *location, *neighbors, *externs, *name;

  struct node *s, *t;
  struct link *link;
  char *stringp, *neighbor;
  char *endptr;
  int neighbor_id;

  fp = fopen (filename, "r");
  if (fp == NULL)
    {
      fprintf (shell->terminal, "no such file: %s\n", filename);
      return -1;
    }

  ret = regcomp (&regex, ROCKETFUEL_MAPS_LINE, REG_EXTENDED|REG_NEWLINE);
  if (ret != 0)
    {
      regerror (ret, &regex, buf, sizeof (buf));
      fprintf (shell->terminal, "regcomp failed: %s: %s\n", buf,
               ROCKETFUEL_MAPS_LINE);
      return -1;
    }

  while (fgets (buf, sizeof (buf), fp))
    {
      if (strlen (buf) == sizeof (buf) - 1)
        fprintf (shell->terminal, "*** buffer too short *** : "
                 "will fail match.\n");

      ret = scan_maps_line (shell, buf, &regex,
                            &nodeid, &location, &dnsflag, &bbflag,
                            &nnbr, &next, &neighbors, &externs,
                            &name, &radius);

      if (ret < 0)
        continue;

      if (radius > 0)
        continue;

      s = node_get (nodeid, G);

      stringp = neighbors;
      while ((neighbor = strsep (&stringp, " \t")) != NULL)
        {
          assert (neighbor[0] == '<');
          neighbor_id = strtol (&neighbor[1], &endptr, 0);
          assert (endptr[0] == '>');
          t = node_get (neighbor_id, G);

          link = link_get (s, t, G);
          link->bandwidth = 100; /* XXX */
          link = link_get (t, s, G);
          link->bandwidth = 100; /* XXX */
        }
    }

  regfree (&regex);
  return 0;
}

DEFINE_COMMAND(import_graph_rocketfuel_maps,
               "import rocketfuel maps <FILENAME>",
               "import from other data\n"
               "import from RocketFuel\n"
               "import from RocketFuel maps file\n"
               "specify RocketFuel filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  read_maps_file (shell, G, argv[3]);
  command_config_add (G->config, argc, argv);
}

#define ROCKETFUEL_WEIGHTS_LINE \
  START "([^ \t]+)" SPACE "([^ \t]+)" SPACE "([0-9.]+)" MAYSPACE END

int
scan_weights_line (struct shell *shell, char *buf, regex_t *regexp,
                   char **sname, char **tname, double *weight)
{
  int i, ret;
  char *endptr;
  char *line;
  int argc;
  char *argv[REGMATCH_MAX];
  regmatch_t regmatch[REGMATCH_MAX];

  ret = regexec (regexp, buf, REGMATCH_MAX, regmatch, 0);
  if (ret == REG_NOMATCH)
    {
      fprintf (shell->terminal, "match failed: %s", buf);
      return -1;
    }
  else
    fprintf (shell->terminal, "matched: %s", buf);

  line = strdup (buf);
  argc = 0;

  /* regmatch[0] is the entire strings that matched */
  for (i = 1; i < REGMATCH_MAX; i++)
    {
      if (regmatch[i].rm_so == -1)
        argv[i-1] = NULL;
      else
        {
          line[regmatch[i].rm_eo] = '\0';
          argv[i-1] = &line[regmatch[i].rm_so];
          argc = i;
        }
    }

#if 0
  for (i = 0; i < argc; i++)
    fprintf (shell->terminal, "argv[%d]: %s\n", i, argv[i]);
#endif /*0*/

  *sname = strdup (argv[0]);
  if (*sname == NULL)
    {
      fprintf (shell->terminal, "strdup for sname failed: %s\n",
               strerror (errno));
      return -1;
    }

  *tname = strdup (argv[1]);
  if (*tname == NULL)
    {
      fprintf (shell->terminal, "strdup for tname failed: %s\n",
               strerror (errno));
      return -1;
    }

  *weight = strtod (argv[2], &endptr);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid weight: %s\n", argv[2]);
      return -1;
    }

#if 1
  fprintf (shell->terminal,
           "Load Edge: sname: %s tname: %s weight: %f\n",
           *sname, *tname, *weight);
#endif /*0*/

  free (line);
  return 0;
}

int
read_weights_file_graph (struct shell *shell, struct graph *G, char *filename)
{
  FILE *fp;
  int ret;
  char buf[8192];
  regex_t regex;

  char *sname, *tname;
  double weight;

  struct node *s, *t;

  struct table *node_table;
  int index = 0;

  fp = fopen (filename, "r");
  if (fp == NULL)
    {
      fprintf (shell->terminal, "no such file: %s\n", filename);
      return -1;
    }

  ret = regcomp (&regex, ROCKETFUEL_WEIGHTS_LINE, REG_EXTENDED|REG_NEWLINE);
  if (ret != 0)
    {
      regerror (ret, &regex, buf, sizeof (buf));
      fprintf (shell->terminal, "regcomp failed: %s: %s\n", buf,
               ROCKETFUEL_WEIGHTS_LINE);
      return -1;
    }

  node_table = table_create ();

  while (fgets (buf, sizeof (buf), fp))
    {
      if (strlen (buf) == sizeof (buf) - 1)
        fprintf (shell->terminal, "*** buffer too short *** : "
                 "will fail match.\n");

      ret = scan_weights_line (shell, buf, &regex,
                               &sname, &tname, &weight);

      if (ret < 0)
        continue;

      s = table_lookup (sname, strlen (sname) * 8, node_table);
      if (! s)
        {
          s = node_get (index, G);
          table_add (sname, strlen (sname) * 8, s, node_table);
          s->name = strdup (sname);
          fprintf (shell->terminal, "%d: %s\n", index, s->name);
          index++;
        }

      t = table_lookup (tname, strlen (tname) * 8, node_table);
      if (! t)
        {
          t = node_get (index, G);
          table_add (tname, strlen (tname) * 8, t, node_table);
          t->name = strdup (tname);
          fprintf (shell->terminal, "%d: %s\n", index, t->name);
          index++;
        }

      link_set (s, t, G);
    }

  table_delete (node_table);

  regfree (&regex);
  return 0;
}

DEFINE_COMMAND(import_graph_rocketfuel_weights,
               "import rocketfuel weights <FILENAME>",
               "import from other data\n"
               "import from RocketFuel\n"
               "import from RocketFuel weights file\n"
               "specify RocketFuel filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  read_weights_file_graph (shell, G, argv[3]);
  command_config_add (G->config, argc, argv);
}

int
read_weights_file_weight (struct shell *shell, struct weight *W, char *filename)
{
  struct graph *G = W->G;
  FILE *fp;
  int ret;
  char buf[8192];
  regex_t regex;

  char *sname, *tname;
  double weight;

  struct node *s, *t;
  struct link *link;

  struct table *node_table;
  int index = 0;

  fp = fopen (filename, "r");
  if (fp == NULL)
    {
      fprintf (shell->terminal, "no such file: %s\n", filename);
      return -1;
    }

  ret = regcomp (&regex, ROCKETFUEL_WEIGHTS_LINE, REG_EXTENDED|REG_NEWLINE);
  if (ret != 0)
    {
      regerror (ret, &regex, buf, sizeof (buf));
      fprintf (shell->terminal, "regcomp failed: %s: %s\n", buf,
               ROCKETFUEL_WEIGHTS_LINE);
      return -1;
    }

  node_table = table_create ();

  while (fgets (buf, sizeof (buf), fp))
    {
      if (strlen (buf) == sizeof (buf) - 1)
        fprintf (shell->terminal, "*** buffer too short *** : "
                 "will fail match.\n");

      ret = scan_weights_line (shell, buf, &regex,
                               &sname, &tname, &weight);

      if (ret < 0)
        continue;

      s = table_lookup (sname, strlen (sname) * 8, node_table);
      if (! s)
        {
          s = node_lookup (index, G);
          if (! s || strcmp (s->name, sname))
            {
              fprintf (shell->terminal, "graph mismatch: s: %s %s\n",
                       (s ? s->name : "NULL"), sname);
              return -1;
            }
          table_add (sname, strlen (sname) * 8, s, node_table);
          fprintf (shell->terminal, "%d: %s\n", index, s->name);
          index++;
        }

      t = table_lookup (tname, strlen (tname) * 8, node_table);
      if (! t)
        {
          t = node_lookup (index, G);
          if (! s || strcmp (t->name, tname))
            {
              fprintf (shell->terminal, "graph mismatch: t: %s %s\n",
                       (t ? t->name : "NULL"), tname);
              return -1;
            }
          table_add (tname, strlen (tname) * 8, t, node_table);
          fprintf (shell->terminal, "%d: %s\n", index, t->name);
          index++;
        }

      link = link_lookup (s, t, G);
      if (! link)
        {
          fprintf (shell->terminal, "graph mismatch: link not found: %s-%s\n",
                   s->name, t->name);
          return -1;
        }

      if (link->id >= W->nedges)
        {
          fprintf (shell->terminal, "graph mismatch: link number mismatch\n");
          return -1;
        }

      W->weight[link->id] = (weight_t) (weight * 100);
    }

  table_delete (node_table);

  regfree (&regex);
  return 0;
}

DEFINE_COMMAND (weight_setting_import_rocketfuel,
                "weight-setting import rocketfuel <FILENAME>",
                "weight setting command\n"
                "import from other data\n"
                "import from RocketFuel\n"
                "specify RocketFuel filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct weight *W = (struct weight *) shell->context;
  read_weights_file_weight (shell, W, argv[3]);
  command_config_add (W->config, argc, argv);
}

DEFINE_COMMAND (import_weight_rocketfuel_weights,
                "import rocketfuel weights <FILENAME>",
                "import from other data\n"
                "import from RocketFuel\n"
                "specify RocketFuel filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct weight *W = (struct weight *) shell->context;
  read_weights_file_weight (shell, W, argv[3]);
  command_config_add (W->config, argc, argv);
}


