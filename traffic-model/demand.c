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

#include "file.h"
#include "vector.h"
#include "shell.h"
#include "command.h"
#include "command_shell.h"
#include "module.h"

#include "network/graph.h"
#include "network/graph_cmd.h"

#include "interface/ampl.h"

#include "demand.h"

struct demand_matrix *
demand_matrix_create (u_int nnodes)
{
  int i;
  struct demand_matrix *demands;

  demands = (struct demand_matrix *) malloc (sizeof (struct demand_matrix));
  memset (demands, 0, sizeof (struct demand_matrix));
  demands->nnodes = nnodes;

  demands->traffic = (demand_t **) malloc (sizeof (demand_t *) * nnodes);
  memset (demands->traffic, 0, sizeof (demand_t *) * nnodes);

  for (i = 0; i < nnodes; i++)
    {
      demands->traffic[i] = (demand_t *) malloc (sizeof (demand_t) * nnodes);
      memset (demands->traffic[i], 0, sizeof (demand_t) * nnodes);
    }

  return demands;
}

void
demand_matrix_delete (struct demand_matrix *demands)
{
  int i;
  for (i = 0; i < demands->nnodes; i++)
    free (demands->traffic[i]);
  free (demands->traffic);
  free (demands);
}

struct demand_matrix *
demand_matrix_copy (struct demand_matrix *d)
{
  int i, j;
  struct demand_matrix *demands;
  demands = demand_matrix_create (d->nnodes);
  for (i = 0; i < d->nnodes; i++)
    for (j = 0; j < d->nnodes; j++)
      demands->traffic[i][j] = d->traffic[i][j];
  demands->total = d->total;
  return demands;
}

void
random_model (struct graph *G, demand_t min, demand_t max,
              struct demand_matrix *D)
{
  int i, j;
  demand_t range = max - min;

  for (i = 0; i < D->nnodes; i++)
    for (j = 0; j < D->nnodes; j++)
      {
        D->traffic[i][j] = random () % ((unsigned int) range) + min;
      }
}

DEFINE_COMMAND (traffic_model_random,
                "traffic-model randome between <[-]ddd.ddd> and <[-]ddd.ddd>",
                "traffic's model\n"
                "randome traffic model\n"
                "between MIN and MAX\n"
                "specify minimum traffic demand\n"
                "between MIN and MAX\n"
                "specify maximum traffic demand\n")
{
  struct shell *shell = (struct shell *) context;
  struct traffic *T = (struct traffic *) shell->context;
  double min, max;
  if (T->G == NULL || T->demands == NULL)
    {
      fprintf (shell->terminal, "Need traffic-graph to execute model.\n");
      return;
    }
  min = strtod (argv[3], NULL);
  max = strtod (argv[5], NULL);
  random_model (T->G, min, max, T->demands);
  command_config_add (T->config, argc, argv);
}

void
fortz_thorup_model (struct graph *G,
                    double alpha,
                    struct demand_matrix *D)
{
  int i, j;
  //double alpha;
  double *o;
  double *d;
  double **c;
  double **eu;
  double delta;

  o = (double *) malloc (sizeof (double) * D->nnodes);
  memset (o, 0, sizeof (double) * D->nnodes);
  d = (double *) malloc (sizeof (double) * D->nnodes);
  memset (d, 0, sizeof (double) * D->nnodes);

  c = (double **) malloc (sizeof (double *) * D->nnodes);
  for (i = 0; i < D->nnodes; i++)
    c[i] = (double *) malloc (sizeof (double) * D->nnodes);

  eu = (double **) malloc (sizeof (double *) * D->nnodes);
  for (i = 0; i < D->nnodes; i++)
    eu[i] = (double *) malloc (sizeof (double) * D->nnodes);

  //alpha = 1000.0;
  for (i = 0; i < D->nnodes; i++)
    o[i] = (double) random () / RAND_MAX;
  for (i = 0; i < D->nnodes; i++)
    d[i] = (double) random () / RAND_MAX;

  for (i = 0; i < D->nnodes; i++)
    for (j = 0; j < D->nnodes; j++)
      c[i][j] = (double) random () / RAND_MAX;

  /* Euclidean distance */
  for (i = 0; i < D->nnodes; i++)
    for (j = 0; j < D->nnodes; j++)
      {
        struct node *ni = node_get (i, G);
        struct node *nj = node_get (j, G);
        double xij, yij;
        assert (ni && nj);

        xij = (ni->xpos - nj->xpos);
        yij = (ni->ypos - nj->ypos);
        eu[i][j] = sqrt (xij * xij + yij * yij);
      }

  /* Largiest Euclidean distance */
  delta = 0.0;
  for (i = 0; i < D->nnodes; i++)
    for (j = 0; j < D->nnodes; j++)
      if (eu[i][j] > delta)
        delta = eu[i][j];

  /* Fortz - Thorup Model */
  for (i = 0; i < D->nnodes; i++)
    for (j = 0; j < D->nnodes; j++)
      D->traffic[i][j] = alpha * o[i] * d[j] * c[i][j] *
                           exp ((-1.0 * eu[i][j]) / (2 * delta));

  free (o);
  free (d);

  for (i = 0; i < D->nnodes; i++)
    free (c[i]);
  free (c);

  for (i = 0; i < D->nnodes; i++)
    free (eu[i]);
  free (eu);
}


struct traffic *
traffic_create ()
{
  struct traffic *T;
  T = (struct traffic *) malloc (sizeof (struct traffic));
  memset (T, 0, sizeof (struct traffic));
  T->name = strdup ("noname");
  T->demands = NULL;
  T->config = vector_create ();
  return T;
}

void
traffic_delete (struct traffic *T)
{
  if (T->name)
    free (T->name);
  if (T->demands)
    demand_matrix_delete (T->demands);
  command_config_clear (T->config);
  vector_delete (T->config);
  free (T);
}

struct command_set *cmdset_traffic;
struct vector *traffics;

DEFINE_COMMAND (traffic_graph,
                "traffic-graph NAME",
                "traffic's base graph\n"
                "specify graph ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct traffic *T = (struct traffic *) shell->context;
  struct graph *G = NULL;
  G = (struct graph *) instance_lookup ("graph", argv[1]);
  if (G == NULL)
    {
      fprintf (shell->terminal, "no such graph: graph-%s\n", argv[1]);
      return;
    }
  T->G = G;
  if (T->demands)
    demand_matrix_delete (T->demands);
  T->demands = demand_matrix_create (graph_nodes (T->G));
  command_config_add (T->config, argc, argv);
}

DEFINE_COMMAND (traffic_seed,
                "traffic-seed <0-4294967295>",
                "traffic's random seed\n"
                "specify random seed\n")
{
  struct shell *shell = (struct shell *) context;
  struct traffic *T = (struct traffic *) shell->context;
  T->seed = (unsigned int) strtoul (argv[1], NULL, 0);
  srandom (T->seed);
  command_config_add (T->config, argc, argv);
}

DEFINE_COMMAND (traffic_model_fortz_thorup,
                "traffic-model fortz-thorup alpha <[-]ddd.ddd>",
                "traffic's model\n"
                "B. Fortz and M. Thorup Model\n")
{
  struct shell *shell = (struct shell *) context;
  struct traffic *T = (struct traffic *) shell->context;
  double alpha = 1.0;
  if (T->G == NULL || T->demands == NULL)
    {
      fprintf (shell->terminal, "Need traffic-graph to execut model.\n");
      return;
    }
  alpha = strtod (argv[3], NULL);
  fortz_thorup_model (T->G, alpha, T->demands);
  command_config_add (T->config, argc, argv);
}

DEFINE_COMMAND (traffic_set_demand,
                "traffic-set <0-4294967295> <0-4294967295> bandwidth <[-]ddd.ddd>",
                "set traffic flow on source-sink pair\n"
                "source node\n"
                "sink node\n"
                "bandwidth\n"
                "bandwidth\n")
{
  struct shell *shell = (struct shell *) context;
  struct traffic *T = (struct traffic *) shell->context;
  u_int source, sink;
  double bandwidth = 0.0;
  if (T->G == NULL || T->demands == NULL)
    {
      fprintf (shell->terminal, "Need traffic-graph to execut model.\n");
      return;
    }
  source = strtoul (argv[1], NULL, 0);
  sink = strtoul (argv[2], NULL, 0);
  bandwidth = strtod (argv[4], NULL);
  fprintf (stderr, "Place traffic-demand on %d->%d: bandwidth: %f\n",
           source, sink, bandwidth);
  T->demands->traffic[source][sink] = bandwidth;
  command_config_add (T->config, argc, argv);
}

DEFINE_COMMAND (traffic_set_random_demand,
                "traffic-set random random bandwidth <[-]ddd.ddd>",
                "set traffic flow on source-sink pair\n"
                "source node\n"
                "sink node\n"
                "bandwidth\n"
                "bandwidth\n")
{
  struct shell *shell = (struct shell *) context;
  struct traffic *T = (struct traffic *) shell->context;
  u_int source, sink;
  double bandwidth = 0.0;
  if (T->G == NULL || T->demands == NULL)
    {
      fprintf (shell->terminal, "Need traffic-graph to execut model.\n");
      return;
    }
  bandwidth = strtod (argv[4], NULL);
  source = random () % graph_nodes (T->G);
  do {
    sink = random () % graph_nodes (T->G);
  } while (sink == source);
  fprintf (stderr, "Place traffic-demand on %d->%d: bandwidth: %f\n",
           source, sink, bandwidth);
  T->demands->traffic[source][sink] = bandwidth;
  command_config_add (T->config, argc, argv);
}

void
show_traffic_summary_header (FILE *terminal)
{
  fprintf (terminal, "%-8s %-12s %-12s\n",
           "Traffic", "Name", "Traffic-Graph");
}

void
show_traffic_summary (FILE *terminal, void *instance)
{
  struct traffic *T = (struct traffic *) instance;
  if (T->G)
    fprintf (terminal, "%-8lu %-12s Graph-%lu\n",
             T->id, T->name, T->G->id);
  else
    fprintf (terminal, "%-8lu %-12s Graph-%s\n",
             T->id, T->name, "None");
}

void
show_traffic_instance (FILE *terminal, void *instance)
{
  struct traffic *T = (struct traffic *) instance;
  int i, j;

  fprintf (terminal, " %3s ", " ");
  for (j = 0; j < T->demands->nnodes; j++)
    fprintf (terminal, " %-3d", j);
  fprintf (terminal, "\n");

  for (i = 0; i < T->demands->nnodes; i++)
    {
      fprintf (terminal, " %3d", i);
      for (j = 0; j < T->demands->nnodes; j++)
        fprintf (terminal, " %3.0f", T->demands->traffic[i][j]);
      fprintf (terminal, "\n");
    }
}

DEFINE_COMMAND (show_traffic,
                "show traffic",
                "display information\n"
                "display traffic information\n"
                "specify traffic ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct traffic *traffic = (struct traffic *) shell->context;
  show_traffic_instance (shell->terminal, traffic);
}

void
traffic_init ()
{
  traffics = vector_create ();
  cmdset_traffic = command_set_create ();

  INSTALL_COMMAND (cmdset_traffic, traffic_graph);
  INSTALL_COMMAND (cmdset_traffic, traffic_seed);
  INSTALL_COMMAND (cmdset_traffic, traffic_model_fortz_thorup);
  INSTALL_COMMAND (cmdset_traffic, traffic_model_random);
  INSTALL_COMMAND (cmdset_traffic, traffic_set_demand);
  INSTALL_COMMAND (cmdset_traffic, traffic_set_random_demand);
  INSTALL_COMMAND (cmdset_traffic, show_traffic);
  INSTALL_COMMAND (cmdset_traffic, export_ampl_append_traffic);
}

void
traffic_finish ()
{
  struct vector_node *vn;
  struct traffic *traffic;
  for (vn = vector_head (traffics); vn; vn = vector_next (vn))
    {
      traffic = (struct traffic *) vn->data;
      if (traffic)
        traffic_delete (traffic);
    }
  vector_delete (traffics);
}

void *
create_traffic (char *id)
{
  struct traffic *w;
  w = traffic_create ();
  w->name = strdup (id);
  return (void *) w;
}

char *
traffic_name (void *instance)
{
  struct traffic *traffic = (struct traffic *) instance;
  return traffic->name;
}

void
traffic_config_write (void *instance, FILE *out)
{
  struct traffic *traffic = (struct traffic *) instance;
  command_config_write (traffic->config, out);
}

struct module mod_traffic =
{
  "traffic",
  traffic_init,
  traffic_finish,
  &traffics,
  create_traffic,
  traffic_name,
  show_traffic_summary_header,
  show_traffic_summary,
  show_traffic_instance,
  &cmdset_traffic,
  traffic_config_write
};

