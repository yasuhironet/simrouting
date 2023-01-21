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
#include "shell.h"
#include "command.h"
#include "command_shell.h"
#include "table.h"
#include "module.h"

#include "network/graph.h"
#include "network/graph_cmd.h"
#include "network/weight.h"

#include "interface/rocketfuel.h"
#include "interface/ospf.h"
#include "interface/graphviz.h"
#include "interface/simrouting_file.h"

struct command_set *cmdset_weight;
struct vector *weights;

struct weight *
weight_create ()
{
  struct weight *weight;
  weight = (struct weight *) malloc (sizeof (struct weight));
  memset (weight, 0, sizeof (struct weight));
  weight->config = vector_create ();
  return weight;
}

void
weight_delete (struct weight *weight)
{
  if (weight->name)
    free (weight->name);
  if (weight->weight)
    free (weight->weight);
  command_config_clear (weight->config);
  vector_delete (weight->config);
  free (weight);
}

void
weight_setting_graph (struct weight *W, struct graph *G)
{
  W->G = G;
  W->nedges = graph_edges (G);
  if (W->weight)
    free (W->weight);
  W->weight = (weight_t *) malloc (sizeof (weight_t) * W->nedges);
  memset (W->weight, 0, sizeof (weight_t) * W->nedges);
}

DEFINE_COMMAND (weight_graph,
                "weight-graph NAME",
                "weight's base graph\n"
                "specify graph ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct weight *W = (struct weight *) shell->context;
  struct graph *G = NULL;
  G = (struct graph *) instance_lookup ("graph", argv[1]);
  if (G == NULL)
    {
      fprintf (shell->terminal, "no such graph: graph-%s\n", argv[1]);
      return;
    }
  weight_setting_graph (W, G);
  command_config_add (W->config, argc, argv);
}

DEFINE_COMMAND (weight_link,
                "link <0-4294967295> <0-4294967295> weight <0-4294967295>",
                "specify link's weight\n"
                "link's source node id\n"
                "link's sink node id\n"
                "specify weight\n"
                "specify weight\n")
{
  struct shell *shell = (struct shell *) context;
  struct weight *W = (struct weight *) shell->context;
  unsigned long sid, tid;
  struct link *link;
  unsigned long weight;

  if (! W->G)
    {
      fprintf (shell->terminal, "no graph specified\n");
      return;
    }

  sid = strtoul (argv[1], NULL, 0);
  tid = strtoul (argv[2], NULL, 0);
  link = link_lookup_by_node_id (sid, tid, W->G);
  if (! link)
    return;

  weight = strtoul (argv[4], NULL, 0);
  W->weight[link->id] = weight;

  command_config_add (W->config, argc, argv);
}

void
show_weight_summary_header (FILE *terminal)
{
  fprintf (terminal, "%-8s %-12s %-12s\n",
           "Weight", "Name", "Weight-Graph");
}

void
show_weight_summary (FILE *terminal, void *instance)
{
  struct weight *W = (struct weight *) instance;
  if (W->G)
    fprintf (terminal, "%-8lu %-12s Graph-%lu\n",
             W->id, W->name, W->G->id);
  else
    fprintf (terminal, "%-8lu %-12s Graph-%s\n",
             W->id, W->name, "None");
}

void
show_weight_instance (FILE *terminal, void *instance)
{
  struct weight *W = (struct weight *) instance;
  int i;
  struct link *link = NULL;
  fprintf (terminal, "Weight %lu:\n", W->id);
  for (i = 0; i < W->nedges; i++)
    {
      link = (struct link *) vector_get (W->G->links, i);
      if (link == NULL)
        fprintf (terminal, "Edge[%2d] (%2s -> %2s): %lu\n",
                 i, "-", "-", W->weight[i]);
      else
        fprintf (terminal, "Edge[%2d] (%2d -> %2d): %lu\n",
                 i, link->from->id, link->to->id, W->weight[i]);
    }
}

DEFINE_COMMAND (show_weight,
                "show weight",
                "display information\n"
                "display weight information\n"
                "specify weight ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct weight *weight = (struct weight *) shell->context;
  show_weight_instance (shell->terminal, weight);
}


void
weight_setting_minimum_hop (struct weight *W)
{
  int i;
  for (i = 0; i < W->nedges; i++)
    W->weight[i] = (weight_t) 1;
}

DEFINE_COMMAND (weight_setting_minhop,
                "weight-setting minimum-hop",
                "weight setting command\n"
                "minimum hop routing\n")
{
  struct shell *shell = (struct shell *) context;
  struct weight *weight = (struct weight *) shell->context;
  weight_setting_minimum_hop (weight);
  command_config_add (weight->config, argc, argv);
}

void
weight_setting_inverse_capacity (struct weight *W)
{
  int i;
  struct link *link = NULL;
  for (i = 0; i < W->nedges; i++)
    {
      link = (struct link *) vector_get (W->G->links, i);
      if (link == NULL)
        W->weight[i] = (weight_t) 100 * 1000 ;
      else
        W->weight[i] = (weight_t) 100 * 1000 / link->bandwidth;
      if (W->weight[i] == 0)
        W->weight[i] = 1;
    }
}

DEFINE_COMMAND (weight_setting_invcap,
                "weight-setting inverse-capacity",
                "weight setting command\n"
                "inversely propotional to link capacity\n")
{
  struct shell *shell = (struct shell *) context;
  struct weight *weight = (struct weight *) shell->context;
  weight_setting_inverse_capacity (weight);
  command_config_add (weight->config, argc, argv);
}

void
weight_clear_config (struct weight *w)
{
  command_config_clear (w->config);
}

void
weight_save_config (struct weight *w)
{
  char buf[128];
  int i;
  struct link *link;

  if (! w->G)
    return;

  snprintf (buf, sizeof (buf), "  weight-graph %s", w->G->name);
  vector_add (strdup (buf), w->config);

  for (i = 0; i < w->nedges; i++)
    {
      link = link_lookup_by_id (i, w->G);
      snprintf (buf, sizeof (buf), "  link %u %u weight %lu",
                link->from->id, link->to->id, w->weight[i]);
      vector_add (strdup (buf), w->config);
    }
}

DEFINE_COMMAND (save_weight_config,
                "save weight config",
                "save information\n"
                "save weight information\n")
{
  struct shell *shell = (struct shell *) context;
  struct weight *weight = (struct weight *) shell->context;
  weight_clear_config (weight);
  weight_save_config (weight);
}

void
weight_init ()
{
  weights = vector_create ();
  cmdset_weight = command_set_create ();

  INSTALL_COMMAND (cmdset_weight, weight_graph);
  INSTALL_COMMAND (cmdset_weight, show_weight);
  INSTALL_COMMAND (cmdset_weight, weight_setting_minhop);
  INSTALL_COMMAND (cmdset_weight, weight_setting_invcap);
  INSTALL_COMMAND (cmdset_weight, weight_link);

  INSTALL_COMMAND (cmdset_weight, weight_setting_import_rocketfuel);
  INSTALL_COMMAND (cmdset_weight, import_weight_rocketfuel_weights);
  INSTALL_COMMAND (cmdset_weight, import_ospf_weight);
  INSTALL_COMMAND (cmdset_weight, import_simrouting_file_inside_clause);

#ifdef HAVE_GRAPHVIZ
  INSTALL_COMMAND (cmdset_weight, export_graphviz_with_weight);
  INSTALL_COMMAND (cmdset_weight, export_graphviz_with_weight_domain);
#endif /*HAVE_GRAPHVIZ*/

  INSTALL_COMMAND (cmdset_weight, save_weight_config);
}

void
weight_finish ()
{
  struct vector_node *vn;
  struct weight *weight;
  for (vn = vector_head (weights); vn; vn = vector_next (vn))
    {
      weight = (struct weight *) vn->data;
      if (weight)
        weight_delete (weight);
    }
  vector_delete (weights);
}

void *
create_weight (char *id)
{
  struct weight *w;
  w = weight_create ();
  w->name = strdup (id);
  return (void *) w;
}

char *
weight_name (void *instance)
{
  struct weight *weight = (struct weight *) instance;
  return weight->name;
}

void
weight_config_write (void *instance, FILE *out)
{
  struct weight *weight = (struct weight *) instance;
  command_config_write (weight->config, out);
}

struct module mod_weight =
{
  "weight",
  weight_init,
  weight_finish,
  &weights,
  create_weight,
  weight_name,
  show_weight_summary_header,
  show_weight_summary,
  show_weight_instance,
  &cmdset_weight,
  weight_config_write
};


