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

#include "network/graph.h"

#define REGMATCH_MAX 16
#define BRITE_NODE_LINE \
  "^([0-9]+) ([0-9.]+) ([0-9.]+) ([0-9]+) ([0-9]+) ([0-9-]+) ([a-zA-Z_]+)[ ]*$"
#define BRITE_EDGE_LINE \
  "^([0-9]+) ([0-9]+) ([0-9]+) ([0-9.]+) ([0-9.]+) ([0-9.]+) ([0-9-]+) ([0-9-]+) ([a-zA-Z_]+)[ ]*([a-zA-Z_]*)[ ]*$"


int
scan_node_line (struct shell *shell, char *buf, regex_t *regex_node,
                int *nodeid, double *xpos, double *ypos,
                int *indegree, int *outdegree, int *asid, char **type)
{
  int i, ret;
  char *endptr;
  char *line;
  char **p, *argv[REGMATCH_MAX];
  regmatch_t regmatch[REGMATCH_MAX];

  ret = regexec (regex_node, buf, REGMATCH_MAX, regmatch, 0);
  if (ret == REG_NOMATCH)
    return 0;

  line = buf;
  p = argv;

  for (i = 1; i < REGMATCH_MAX && regmatch[i].rm_so >= 0; i++)
    {
      line[regmatch[i].rm_eo] = '\0';
      *p++ = &line[regmatch[i].rm_so];
    }
  *p = NULL;

  *nodeid = strtol (argv[0], &endptr, 0);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid node id: %s\n", argv[0]);
      return -1;
    }

  *xpos = strtod (argv[1], &endptr);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid xpos: %s\n", argv[1]);
      return -1;
    }

  *ypos = strtod (argv[2], &endptr);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid ypos: %s\n", argv[2]);
      return -1;
    }

  *indegree = strtol (argv[3], &endptr, 0);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid indegree: %s\n", argv[3]);
      return -1;
    }

  *outdegree = strtol (argv[4], &endptr, 0);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid outdegree: %s\n", argv[4]);
      return -1;
    }

  *asid = strtol (argv[5], &endptr, 0);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid asid: %s\n", argv[5]);
      return -1;
    }

  *type = argv[6];

#if 0
  fprintf (shell->terminal,
           "load Node[%d]: xpos: %f ypos: %f "
           "indegree: %d outdegree: %d ASid: %d type: %s\n",
           *nodeid, *xpos, *ypos, *indegree, *outdegree, *asid, *type);
#else
  fprintf (shell->terminal, "Load Node[%d].\n", *nodeid);
#endif /*0*/

  return 1;
}

int
scan_edge_line (struct shell *shell, char *buf, regex_t *regex_edge,
                int *edgeid, int *from, int *to, double *length, double *delay,
                double *bandwidth, int *asfrom, int *asto, char **type)
{
  int i, ret;
  char *endptr;
  char *line;
  char **p, *argv[REGMATCH_MAX];
  regmatch_t regmatch[REGMATCH_MAX];

  ret = regexec (regex_edge, buf, REGMATCH_MAX, regmatch, 0);
  if (ret == REG_NOMATCH)
    return 0;

  line = buf;
  p = argv;

  for (i = 1; i < REGMATCH_MAX && regmatch[i].rm_so >= 0; i++)
    {
      line[regmatch[i].rm_eo] = '\0';
      *p++ = &line[regmatch[i].rm_so];
    }
  *p = NULL;

  *edgeid = strtol (argv[0], &endptr, 0);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid edge id: %s\n", argv[0]);
      return -1;
    }

  *from = strtol (argv[1], &endptr, 0);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid from: %s\n", argv[1]);
      return -1;
    }

  *to = strtol (argv[2], &endptr, 0);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid to: %s\n", argv[2]);
      return -1;
    }

  *length = strtod (argv[3], &endptr);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid length: %s\n", argv[3]);
      return -1;
    }

  *delay = strtod (argv[4], &endptr);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid delay: %s\n", argv[4]);
      return -1;
    }

  *bandwidth = strtod (argv[5], &endptr);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid bandwidth: %s\n", argv[5]);
      return -1;
    }

  *asfrom = strtol (argv[6], &endptr, 0);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid asfrom: %s\n", argv[6]);
      return -1;
    }

  *asto = strtol (argv[7], &endptr, 0);
  if (*endptr != '\0')
    {
      fprintf (shell->terminal, "invalid asto: %s\n", argv[7]);
      return -1;
    }

  *type = argv[8];

#if 0
  fprintf (shell->terminal,
           "load Edge[%d]: from: %d to: %d length: %f "
           "delay: %f bandwidth: %f ASfrom: %d ASto: %d type: %s\n",
           *edgeid, *from, *to, *length, *delay, *bandwidth, *asfrom,
           *asto, *type);
#else
  fprintf (shell->terminal, "Load Edge[%d]: from: %d to: %d delay: %f bw: %f\n",
           *edgeid, *from, *to, *delay, *bandwidth);
#endif /*0*/

  return 1;
}

int
read_brite_file (struct shell *shell, struct graph *G, char *filename)
{
  FILE *fp;
  int ret;
  char buf[256];
  regex_t regex_node, regex_edge;

  fp = fopen (filename, "r");
  if (fp == NULL)
    {
      fprintf (shell->terminal, "no such file: %s\n", filename);
      return -1;
    }

  ret = regcomp (&regex_node, BRITE_NODE_LINE, REG_EXTENDED|REG_NEWLINE);
  if (ret != 0)
    {
      regerror (ret, &regex_node, buf, sizeof (buf));
      fprintf (shell->terminal, "regcomp for Node line failed: %s\n", buf);
      return -1;
    }

  ret = regcomp (&regex_edge, BRITE_EDGE_LINE, REG_EXTENDED|REG_NEWLINE);
  if (ret != 0)
    {
      regerror (ret, &regex_edge, buf, sizeof (buf));
      fprintf (shell->terminal, "regcomp for Edge line failed: %s\n", buf);
      return -1;
    }

  while (fgets (buf, sizeof (buf), fp))
    {
      int nodeid, indegree, outdegree, asid;
      double xpos, ypos;
      int edgeid, from, to, asfrom, asto;
      double length, delay, bandwidth;
      char *type;

      /* Node line match */
      ret = scan_node_line (shell, buf, &regex_node, &nodeid, &xpos, &ypos,
                            &indegree, &outdegree, &asid, &type);
      if (ret < 0)
        return -1;
      else if (ret == 1)
        {
          struct node *node;
          node = node_get (nodeid, G);
          node->xpos = xpos;
          node->ypos = ypos;
        }

      /* Edge line match */
      ret = scan_edge_line (shell, buf, &regex_edge, &edgeid, &from, &to, &length,
                            &delay, &bandwidth, &asfrom, &asto, &type);
      if (ret < 0)
        return -1;
      else if (ret == 1)
        {
          struct node *s, *t;
          struct link *link;
          s = node_get (from, G);
          t = node_get (to, G);
          link = link_get (s, t, G);
          link->bandwidth = bandwidth;
          link->delay = delay;
          link->length = length;
          link = link_get (t, s, G);
          link->bandwidth = bandwidth;
          link->delay = delay;
          link->length = length;
        }
    }

  regfree (&regex_node);
  regfree (&regex_edge);
  return 0;
}

DEFINE_COMMAND(import_brite,
               "import brite <FILENAME>",
               "import from other data\n"
               "import from BRITE\n"
               "specify BRITE filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  read_brite_file (shell, G, argv[2]);
  command_config_add (G->config, argc, argv);
}



