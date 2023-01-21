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

#include "graph.h"
#include "group.h"
#include "table.h"
#include "module.h"

#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"

#include "interface/brite.h"
#include "interface/graphviz.h"
#include "interface/ospf.h"
#include "interface/rocketfuel.h"
#include "interface/ampl.h"
#include "interface/spring_os.h"

#include "function/connectivity.h"
#include "function/reliability.h"

struct command_set *cmdset_graph;
struct vector *graphs;

void *
create_graph (char *id)
{
  struct graph *g;
  g = graph_create ();
  g->name = strdup (id);
  return (void *) g;
}

void
show_graph_summary_header (FILE *terminal)
{
  fprintf (terminal, "%-5s %-16s %6s %6s\n",
           "Graph", "Name", "#Nodes", "#Edges");
}

void
show_graph_summary (FILE *terminal, void *instance)
{
  struct graph *G = (struct graph *) instance;
  fprintf (terminal, "%-5lu %-16s %6d %6d\n",
           G->id, G->name, graph_nodes (G), graph_edges (G));
}

void
show_graph_instance (FILE *terminal, void *instance)
{
  struct graph *g = (struct graph *) instance;
  struct node *v;
  struct link *e;
  struct vector_node *vn, *vno;

  fprintf (terminal, "Graph %s (%lu):\n  %d nodes, %d edges\n\n",
           g->name, g->id, graph_nodes (g), graph_edges (g));
  fprintf (terminal, "nodes:\n");
  for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
    {
      v = (struct node *) vn->data;
      if (v == NULL)
        continue;
      fprintf (terminal, "  node[%d]: ", v->id);
      fprintf (terminal, "%-32s: ", v->name);
      fprintf (terminal, "neighbor: ");
      for (vno = vector_head (v->olinks); vno; vno = vector_next (vno))
        {
          e = (struct link *) vno->data;
          fprintf (terminal, " %d", e->to->id);
        }
      fprintf (terminal, "\n");
    }

  fprintf (terminal, "positions:\n");
  for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
    {
      v = (struct node *) vn->data;
      if (v == NULL)
        continue;
      fprintf (terminal, "  node[%d]: ", v->id);
      fprintf (terminal, "xpos: %f ypos: %f\n", v->xpos, v->ypos);
    }

  fprintf (terminal, "domainnames:\n");
  for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
    {
      v = (struct node *) vn->data;
      if (v == NULL)
        continue;
      fprintf (terminal, "  node[%d]: ", v->id);
      fprintf (terminal, "%s\n", v->domain_name);
    }

  fprintf (terminal, "links:\n");
  for (vn = vector_head (g->links); vn; vn = vector_next (vn))
    {
      e = (struct link *) vn->data;
      if (e == NULL)
        continue;
      fprintf (terminal, "  link[%2d]: %2d-%2d bandwidth: %f delay: %f length: %f\n",
              e->id, e->from->id, e->to->id,
              e->bandwidth, e->delay, e->length);
    }
}


char *
graph_name (void *instance)
{
  struct graph *graph = (struct graph *) instance;
  return graph->name;
}

void
graph_config_write (void *instance, FILE *out)
{
  struct graph *graph = (struct graph *) instance;
  command_config_write (graph->config, out);
}

void
graph_clear_config (struct graph *g)
{
  command_config_clear (g->config);
}

void
graph_save_config (struct graph *g)
{
  struct vector_node *vn;
  char buf[128];

  for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
    {
      struct node *v = (struct node *) vn->data;

      if (! v)
        continue;

      if (v->name)
        {
          snprintf (buf, sizeof (buf), "  node %d name %s", v->id, v->name);
          vector_add (strdup (buf), g->config);
        }

      if (v->domain_name)
        {
          snprintf (buf, sizeof (buf), "  node %d domain-name %s",
                    v->id, v->domain_name);
          vector_add (strdup (buf), g->config);
        }

      if (v->descr)
        {
          snprintf (buf, sizeof (buf), "  node %d description %s", v->id, v->descr);
          vector_add (strdup (buf), g->config);
        }

      if (v->type)
        {
          if (v->type == NODE_TYPE_ROUTER)
            snprintf (buf, sizeof (buf), "  node %d type router", v->id);
          else if (v->type == NODE_TYPE_NETWORK)
            snprintf (buf, sizeof (buf), "  node %d type network", v->id);
          vector_add (strdup (buf), g->config);
        }
    }

  for (vn = vector_head (g->links); vn; vn = vector_next (vn))
    {
      struct link *e = (struct link *) vn->data;

      if (! e)
        continue;

      snprintf (buf, sizeof (buf), "  link %d source-node %d sink-node %d",
                e->id, e->from->id, e->to->id);
      vector_add (strdup (buf), g->config);

      if (e->descr)
        {
          snprintf (buf, sizeof (buf), "  link %d description %s", e->id, e->descr);
          vector_add (strdup (buf), g->config);
        }
    }
}

DEFINE_COMMAND(node_name,
               "node <0-4294967295> name NAME",
               "node command\n"
               "specify node id\n"
               "specify node name\n"
               "specify node name\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  struct node *node;
  unsigned int id;

  id = strtoul (argv[1], NULL, 0);
  node = node_get (id, graph);

  if (node->name)
    free (node->name);
  node->name = strdup (argv[3]);

  command_config_add (graph->config, argc, argv);
}

DEFINE_COMMAND(node_domain_name,
               "node <0-4294967295> domain-name NAME",
               "node command\n"
               "specify node id\n"
               "specify node domain-name\n"
               "specify node domain-name\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  struct node *node;
  unsigned int id;

  id = strtoul (argv[1], NULL, 0);
  node = node_get (id, graph);

  if (node->domain_name)
    free (node->domain_name);
  node->domain_name = strdup (argv[3]);

  command_config_add (graph->config, argc, argv);
}

DEFINE_COMMAND(node_position,
               "node <0-4294967295> xpos <[-]ddd.ddd> ypos <[-]ddd.ddd>",
               "node command\n"
               "specify node id\n"
               "specify node x-position\n"
               "specify node x-position\n"
               "specify node y-position\n"
               "specify node y-position\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  struct node *node;
  unsigned int id;

  id = strtoul (argv[1], NULL, 0);
  node = node_get (id, graph);

  node->xpos = strtod (argv[3], NULL);
  node->ypos = strtod (argv[5], NULL);

  command_config_add (graph->config, argc, argv);
}

DEFINE_COMMAND(node_description,
               "node <0-4294967295> description LINE",
               "node command\n"
               "specify node id\n"
               "specify node description\n"
               "specify node description\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  struct node *node;
  unsigned int id;

  id = strtoul (argv[1], NULL, 0);
  node = node_get (id, graph);

  if (node->descr)
    free (node->descr);
  node->descr = strdup (argv[3]);

  command_config_add (graph->config, argc, argv);
}

DEFINE_COMMAND(node_type_router,
               "node <0-4294967295> type router",
               "node command\n"
               "specify node id\n"
               "specify node type\n"
               "specify node type as router\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  struct node *node;
  unsigned int id;

  id = strtoul (argv[1], NULL, 0);
  node = node_get (id, graph);

  if (! strcmp (argv[3], "router"))
    node->type = NODE_TYPE_ROUTER;
  if (! strcmp (argv[3], "network"))
    node->type = NODE_TYPE_NETWORK;

  command_config_add (graph->config, argc, argv);
}

ALIAS_COMMAND(node_type_network,
              node_type_router,
               "node <0-4294967295> type network",
               "node command\n"
               "specify node id\n"
               "specify node type\n"
               "specify node type as network\n")

DEFINE_COMMAND(link_source_sink,
               "link <0-4294967295> source-node <0-4294967295> sink-node <0-4294967295>",
               "link command\n"
               "specify link id\n"
               "specify source node\n"
               "specify source node's id\n"
               "specify sink node\n"
               "specify sink node's id\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  struct link *link;
  unsigned int id, source, sink;
  struct node *s, *t;

  id = strtoul (argv[1], NULL, 0);
  link = link_lookup_by_id (id, graph);

  source = strtoul (argv[3], NULL, 0);
  s = node_get (source, graph);

  sink = strtoul (argv[5], NULL, 0);
  t = node_get (sink, graph);

  if (link && (link->from != s || link->to != t))
    {
      link_delete (link);
      link = NULL;
    }

  if (! link)
    {
      link = link_get_by_id (id, graph);
      link_connect (link, s, t, graph);
    }

  command_config_add (graph->config, argc, argv);
}

DEFINE_COMMAND(link_bandwidth_delay_length,
               "link <0-4294967295> bandwidth <[-]ddd.ddd> delay <[-]ddd.ddd> length <[-]ddd.ddd>",
               "specify link\n"
               "specify link id\n"
               "specify bandwidth\n"
               "specify bandwidth\n"
               "specify delay\n"
               "specify delay\n"
               "specify length\n"
               "specify length\n"
               )
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  struct link *link;
  unsigned int id;

  id = strtoul (argv[1], NULL, 0);
  link = link_get_by_id (id, graph);

  link->bandwidth = strtod (argv[3], NULL);
  link->delay = strtod (argv[5], NULL);
  link->length = strtod (argv[7], NULL);

  command_config_add (graph->config, argc, argv);
}

DEFINE_COMMAND(link_bandwidth_delay_length_all,
               "link all bandwidth <[-]ddd.ddd> delay <[-]ddd.ddd> length <[-]ddd.ddd>",
               "specify link\n"
               "specify link id\n"
               "specify bandwidth\n"
               "specify bandwidth\n"
               "specify delay\n"
               "specify delay\n"
               "specify length\n"
               "specify length\n"
               )
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  struct link *link;
  struct vector_node *vn;

  for (vn = vector_head (graph->links); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;
      link->bandwidth = strtod (argv[3], NULL);
      link->delay = strtod (argv[5], NULL);
      link->length = strtod (argv[7], NULL);
    }

  command_config_add (graph->config, argc, argv);
}

DEFINE_COMMAND(no_link,
               "no link <0-4294967295>",
               "negate command\n"
               "specify link\n"
               "specify link id\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  struct link *link;
  unsigned int id;

  id = strtoul (argv[2], NULL, 0);
  link = link_lookup_by_id (id, graph);

  if (link == NULL)
    return;
  link_delete (link);

  command_config_add (graph->config, argc, argv);
}

DEFINE_COMMAND(no_link_by_node,
               "no link node <0-4294967295> node <0-4294967295>",
               "negate command\n"
               "specify link\n"
               "specify link by node id\n"
               "specify link by node id\n"
               "specify link by node id\n"
               "specify link by node id\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  struct link *link;
  unsigned int s, t;

  s = strtoul (argv[3], NULL, 0);
  t = strtoul (argv[5], NULL, 0);
  link = link_lookup_by_node_id (s, t, graph);

  if (link == NULL)
    return;
  if (link->inverse)
    link_delete (link->inverse);
  link_delete (link);

  command_config_add (graph->config, argc, argv);
}

void
graph_show_config (FILE *terminal, struct graph *g)
{
  fprintf (terminal, "Graph %s:\n", g->name);
  command_config_write (g->config, terminal);
}

DEFINE_COMMAND(graph_name,
               "graph-name NAME",
               "specify graph name\n"
               "specify graph name\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  if (G->name)
    free (G->name);
  G->name = strdup (argv[1]);
}

DEFINE_COMMAND(clear_graph,
               "clear graph",
               "clear\n"
               "clear graph\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  graph_clear (G);
  command_config_clear (G->config);
}

DEFINE_COMMAND (import_graph,
                "import graph NAME",
                "import from other data\n"
                "import from other graph\n"
                "specify graph ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  struct graph *source;
  source = (struct graph *) instance_lookup ("graph", argv[2]);
  if (source)
    graph_copy (G, source);
}

DEFINE_COMMAND (import_routing,
                "import routing <0-4294967295> sink-tree destination <0-4294967295>",
                "import from other data\n"
                "import from routing tree\n"
                "specify routing ID\n"
                "import routing tree for specific destination\n"
                "import routing tree for specific destination\n"
                "specify destination node ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  struct routing *R = NULL;
  unsigned long dest;
  struct node *destnode;

  R = (struct routing *) instance_lookup ("routing", argv[2]);
  if (R == NULL)
    {
      fprintf (shell->terminal, "no such routing: routing-%s\n", argv[2]);
      return;
    }

  dest = strtoul (argv[5], NULL, 0);
  destnode = node_lookup (dest, R->G);
  if (! destnode)
    {
      fprintf (shell->terminal, "no such destination node: %lu\n", dest);
      return;
    }

  graph_delete (G);
  G = graph_import_routing (R, destnode);
  shell->context = G;
}

DEFINE_COMMAND (realloc_identifiers,
                "realloc identifiers",
                "re-allocate identifiers\n"
                "re-allocate identifiers\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  graph_pack_id (G);
}

void
packet_forward_with_fail (struct node *s, struct node *t, struct routing *R,
                          struct node *fs, struct node *ft, int ntrial);

DEFINE_COMMAND (packet_forwarding_failure,
                "packet forwarding source <0-4294967295> destination <0-4294967295> probability routing <0-4294967295> failure-link <0-4294967295> <0-4294967295> trial <0-65535>",
                "display packet forwarding\n"
                "display packet forwarding\n"
                "specify source node\n"
                "specify source node\n"
                "specify destination node\n"
                "specify destination node\n"
                "specify failure link\n"
                "specify failure link's source node-id\n"
                "specify failure link's destination node-id\n"
                "specify number of trial\n"
                "specify number of trial\n"
                )
{
  struct shell *shell = (struct shell *) context;
  struct graph *G = (struct graph *) shell->context;
  unsigned long source_id, dest_id;
  unsigned long f_source_id, f_dest_id;
  struct node *start, *dest;
  struct node *fs, *ft;
  struct routing *R = NULL;
  int ntrial;

  source_id = strtoul (argv[3], NULL, 0);
  dest_id = strtoul (argv[5], NULL, 0);
  f_source_id = strtoul (argv[10], NULL, 0);
  f_dest_id = strtoul (argv[11], NULL, 0);
  ntrial = strtoul (argv[13], NULL, 0);

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

  packet_forward_with_fail (start, dest, R, fs, ft, ntrial);

  fprintf (shell->terminal, "\n");
}

DEFINE_COMMAND (show_graph_id_config,
                "show graph NAME config",
                "show information\n"
                "show graph information\n"
                "specify graph ID\n"
                "show graph configuration\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = NULL;
  graph = (struct graph *) instance_lookup ("graph", argv[2]);
  if (graph == NULL)
    {
      fprintf (shell->terminal, "no such graph: graph-%s\n", argv[2]);
      return;
    }
  graph_show_config (shell->terminal, graph);
}

DEFINE_COMMAND (show_graph,
                "show graph",
                "display information\n"
                "display graph information\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  show_graph_instance (shell->terminal, graph);
}

DEFINE_COMMAND (save_graph_config,
                "save graph config",
                "save information\n"
                "save graph information\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  graph_clear_config (graph);
  graph_save_config (graph);
}

DEFINE_COMMAND (show_graph_config,
                "show graph config",
                "display information\n"
                "display graph information\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  graph_show_config (shell->terminal, graph);
}

DEFINE_COMMAND (graph_scale_link_bandwidth,
                "graph-scale link-bandwidth <[-]ddd.ddd>",
                "scale graph parameter\n"
                "scale link bandwidth\n"
                "specify scale parameter\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *graph = (struct graph *) shell->context;
  struct vector_node *vn;
  struct link *link;
  double scale;
  scale = strtod (argv[2], NULL);
  for (vn = vector_head (graph->links); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;
      link->bandwidth *= scale;
    }
}

int
domain_suffix_match (char *domain_name, char *suffix)
{
  char *p;
  int ret;
  if (domain_name == NULL)
    return 0;
  p = strstr (domain_name, suffix);
  if (p == NULL)
    return 0;
  ret = strcmp (p, suffix);
  if (ret != 0)
    return 0;
  return 1;
}

void
link_open (struct link *link)
{
  if (link->inverse)
    {
      printf ("link_open: delete inverse: %p\n", link->inverse);
      link_delete (link->inverse);
    }
  printf ("link_open: delete real: %p\n", link);
  link_delete (link);
}

void
link_contract (struct link *link, struct node *newnode)
{
  struct vector_node *vnj;
  struct node *s, *t;

  s = link->from;
  t = link->to;

  fprintf (stdout, "contract link: %d-%d into node %d\n",
           s->id, t->id, newnode->id);

  fflush (stdout);

  link_open (link);

  if (s != newnode)
    {
      for (vnj = vector_head (s->olinks); vnj; vnj = vector_next (vnj))
        {
          struct link *link = (struct link *) vnj->data;
          struct node *peer = link->to;
          struct link *np, *pn;

          fprintf (stdout, "  delete: %d-%d set: %d-%d\n",
                   link->from->id, link->to->id, newnode->id, peer->id);

          np = link_get (newnode, peer, link->g);
          pn = link_get (peer, newnode, link->g);

          fprintf (stdout, "    delete %p, %p, set %p, %p\n",
                   link->inverse, link, np, pn);

          link_open (link);
        }

      fprintf (stdout, "  delete node: %d (%s)\n", s->id, s->domain_name);
      assert (s->olinks->size == 0);
      assert (s->ilinks->size == 0);
      node_remove (s, s->g);
      node_delete (s);
    }

  if (t != newnode)
    {
      for (vnj = vector_head (t->olinks); vnj; vnj = vector_next (vnj))
        {
          struct link *link = (struct link *) vnj->data;
          struct node *peer = link->to;
          struct link *np, *pn;

          fprintf (stdout, "  delete: %d-%d set: %d-%d\n",
                   link->from->id, link->to->id, newnode->id, peer->id);

          np = link_get (newnode, peer, link->g);
          pn = link_get (peer, newnode, link->g);
          fprintf (stdout, "    delete %p, %p, set %p, %p\n",
                   link->inverse, link, np, pn);

          link_open (link);
        }

      fprintf (stdout, "  delete node: %d (%s)\n", t->id, t->domain_name);
      assert (t->olinks->size == 0);
      assert (t->ilinks->size == 0);
      node_remove (t, t->g);
      node_delete (t);
    }
}

DEFINE_COMMAND(contract_edges_both_node_group,
               "contract edges both node group GROUPNAME name NAME",
               "contraction of graph edges\n"
               "contraction of graph edges\n"
               "matches when both end-node matches\n"
               "matches when both end-node matches\n"
               "specify match group\n"
               "specify group name\n"
               "specify name of the new node\n"
               "specify name of the new node\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *g = (struct graph *) shell->context;
  char *group_name = argv[5];
  char *newnode_name = argv[7];
  struct node *newnode;
  struct vector_node *vni;
  int count = 0;

  newnode = node_get (vector_empty_index (g->nodes), g);
  newnode->name = strdup (newnode_name);
  newnode->domain_name = strdup (newnode_name);

  fprintf (stdout, "newnode: id: %d domain-name: %s\n",
           newnode->id, newnode->domain_name);

  for (vni = vector_head (g->links); vni; vni = vector_next (vni))
    {
      struct link *e = (struct link *) vni->data;
      struct node *s, *t;

      if (! e)
        continue;

      s = e->from;
      t = e->to;

      fprintf (stdout, "link: %d(%s)-%d(%s)\n",
               s->id, s->domain_name, t->id, t->domain_name);

      if (s != newnode && ! group_match (s, group_name))
        continue;
      if (t != newnode && ! group_match (t, group_name))
        continue;

      link_contract (e, newnode);
      count ++;
    }

  if (count == 0)
    {
      node_remove (newnode, newnode->g);
      node_delete (newnode);
    }
}



DEFINE_COMMAND(contract_edges_both_node_domain_suffix,
               "contract-edges both-node domain-suffix DOMAIN",
               "contraction of graph edges\n"
               "matches when both end-node matches\n"
               "specify domain-name suffix\n"
               "specify domain-name suffix\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *g = (struct graph *) shell->context;
  char *suffix = argv[3];
  struct node *newnode;
  struct vector_node *vni;
  int count = 0;

  newnode = node_get (vector_empty_index (g->nodes), g);
  newnode->name = strdup (suffix);
  newnode->domain_name = strdup (suffix);

  fprintf (stdout, "newnode: id: %d domain-name: %s\n",
           newnode->id, newnode->domain_name);

  for (vni = vector_head (g->links); vni; vni = vector_next (vni))
    {
      struct link *e = (struct link *) vni->data;
      struct node *s, *t;

      if (! e)
        continue;

      s = e->from;
      t = e->to;

      fprintf (stdout, "link: %d(%s)-%d(%s)\n",
               s->id, s->domain_name, t->id, t->domain_name);

      if (! domain_suffix_match (s->domain_name, suffix))
        continue;
      if (! domain_suffix_match (t->domain_name, suffix))
        continue;

      link_contract (e, newnode);
      count ++;
    }

  if (count == 0)
    {
      node_remove (newnode, newnode->g);
      node_delete (newnode);
    }
}

DEFINE_COMMAND(remove_stub_node,
               "remove stub-nodes",
               "transformation: removal\n"
               "remove stub-nodes\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *g = (struct graph *) shell->context;
  struct node *node;
  struct link *out, *in;
  int ocount, icount;
  struct vector_node *vni, *vnj;

  for (vni = vector_head (g->nodes); vni; vni = vector_next (vni))
    {
      node = (struct node *) vni->data;

      if (! node)
        continue;

      out = NULL;
      ocount = 0;
      for (vnj = vector_head (node->olinks); vnj; vnj = vector_next (vnj))
        if (vnj->data)
          {
            out = (struct link *) vnj->data;
            ocount ++;
          }

      in = NULL;
      icount = 0;
      for (vnj = vector_head (node->ilinks); vnj; vnj = vector_next (vnj))
        if (vnj->data)
          {
            in = (struct link *) vnj->data;
            icount ++;
          }

      if (ocount > 1 || icount > 1)
        continue;

      /* else it is stub or independent node */

      if (in && out)
        {
          assert (in->inverse == out);
          assert (out->inverse == in);
        }

      if (in)
        link_open (in);
      else if (out)
        link_open (out);

      fprintf (stdout, "remove stub node: id: %d name: %s domain-name: %s\n",
               node->id, node->name, node->domain_name);
      fprintf (stdout, "                  icount: %d ocount: %d in: %p out: %p\n",
               icount, ocount, in, out);

      node_remove (node, node->g);
      node_delete (node);
    }
}

DEFINE_COMMAND(show_connectivity,
               "show connectivity",
               "display information\n"
               "display connectivity\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *g = (struct graph *) shell->context;

  struct vector_node *vn;
  struct connectivity c;
  struct link *link;
  int i, itop;

  connectivity_init (g->nodes->size, &c);
  for (vn = vector_head (g->links); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;
      if (! link)
        continue;
      connectivity_connect (link->from->id, link->to->id, &c);
    }

  fprintf (shell->terminal, "connectivity:");
  for (i = 0; i < c.N; i++)
    {
      itop = c.node_array[i];
      while (itop != c.node_array[itop])
        itop = c.node_array[itop];
      fprintf (shell->terminal, " %d", itop);
    }
  fprintf (shell->terminal, "\n");

  connectivity_finish (&c);
}

DEFINE_COMMAND(renovate_maximum_connected_component,
               "renovate maximum-connected-component",
               "renovate graph\n"
               "remove connected component other than the maximum one\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *g = (struct graph *) shell->context;

  struct vector_node *vn;
  struct connectivity c;
  struct link *link;
  struct node *node;
  int i, itop;
  int maximum = 0, maximum_top;

  connectivity_init (g->nodes->size, &c);
  for (vn = vector_head (g->links); vn; vn = vector_next (vn))
    {
      link = (struct link *) vn->data;
      connectivity_connect (link->from->id, link->to->id, &c);
    }

  for (i = 0; i < c.N; i++)
    {
      /* compress tree size */
      for (itop = i; itop != c.node_array[itop]; itop = c.node_array[itop])
        c.node_array[itop] = c.node_array[c.node_array[itop]];

      /* get i's tree top */
      itop = c.node_array[i];
      while (itop != c.node_array[itop])
        itop = c.node_array[itop];

      if (maximum < c.tree_size[itop])
        {
          maximum = c.tree_size[itop];
          maximum_top = itop;
        }
    }

  for (vn = vector_head (g->nodes); vn; vn = vector_next (vn))
    {
      node = (struct node *) vn->data;

      /* removed slot is reloaded by vector_next () */
      if (! node)
        continue;

      /* get node's tree top */
      itop = c.node_array[node->id];
      while (itop != c.node_array[itop])
        itop = c.node_array[itop];

      if (maximum_top != itop)
        {
          fprintf (shell->terminal, "remove node[%d]: %s\n",
                   node->id, node->name);
          node_remove (node, g);
          node_delete (node);
        }
    }

  connectivity_finish (&c);

  fflush (shell->terminal);
}

void
graph_init ()
{
  graphs = vector_create ();
  cmdset_graph = command_set_create ();

  INSTALL_COMMAND (cmdset_graph, node_name);
  INSTALL_COMMAND (cmdset_graph, node_domain_name);
  INSTALL_COMMAND (cmdset_graph, node_position);
  INSTALL_COMMAND (cmdset_graph, node_description);
  INSTALL_COMMAND (cmdset_graph, node_type_router);
  INSTALL_COMMAND (cmdset_graph, node_type_network);

  INSTALL_COMMAND (cmdset_graph, link_source_sink);
  INSTALL_COMMAND (cmdset_graph, link_bandwidth_delay_length);
  INSTALL_COMMAND (cmdset_graph, link_bandwidth_delay_length_all);
  INSTALL_COMMAND (cmdset_graph, no_link);
  INSTALL_COMMAND (cmdset_graph, no_link_by_node);

  INSTALL_COMMAND (cmdset_graph, graph_name);

  INSTALL_COMMAND (cmdset_graph, import_graph);
  INSTALL_COMMAND (cmdset_graph, import_brite);
  INSTALL_COMMAND (cmdset_graph, import_routing);
  INSTALL_COMMAND (cmdset_graph, import_graph_rocketfuel_maps);
  INSTALL_COMMAND (cmdset_graph, import_graph_rocketfuel_weights);

  INSTALL_COMMAND (cmdset_graph, realloc_identifiers);

  INSTALL_COMMAND (cmdset_graph, contract_edges_both_node_group);
  INSTALL_COMMAND (cmdset_graph, contract_edges_both_node_domain_suffix);
  INSTALL_COMMAND (cmdset_graph, remove_stub_node);

  INSTALL_COMMAND (cmdset_graph, clear_graph);
  INSTALL_COMMAND (cmdset_graph, graph_scale_link_bandwidth);

  INSTALL_COMMAND (cmdset_graph, show_graph);

  INSTALL_COMMAND (cmdset_graph, show_path);
  INSTALL_COMMAND (cmdset_graph, show_path_source_to_all);
  INSTALL_COMMAND (cmdset_graph, show_path_all_to_dest);
  INSTALL_COMMAND (cmdset_graph, show_path_all);
  INSTALL_COMMAND (cmdset_graph, show_path_cost);
  INSTALL_COMMAND (cmdset_graph, show_path_probability);
  INSTALL_COMMAND (cmdset_graph, show_path_probability_failure);
  INSTALL_COMMAND (cmdset_graph, packet_forwarding_failure);

  INSTALL_COMMAND (cmdset_graph, save_graph_config);
  INSTALL_COMMAND (cmdset_graph, show_graph_config);

#ifdef HAVE_GRAPHVIZ
  INSTALL_COMMAND (cmdset_graph, export_graphviz);
  //INSTALL_COMMAND (cmdset_graph, export_graphviz_layout);
  INSTALL_COMMAND (cmdset_graph, import_graphviz_file);
  INSTALL_COMMAND (cmdset_graph, import_graphviz_position);
  INSTALL_COMMAND (cmdset_graph, import_graphviz_layout);
#endif /*HAVE_GRAPHVIZ*/
  INSTALL_COMMAND (cmdset_graph, export_ampl_append_graph);
  INSTALL_COMMAND (cmdset_graph, export_spring_os_graph);

  INSTALL_COMMAND (cmdset_graph, show_connectivity);
  INSTALL_COMMAND (cmdset_graph, renovate_maximum_connected_component);

  INSTALL_COMMAND (cmdset_graph, calculate_reliability_source_destination);
  INSTALL_COMMAND (cmdset_graph, calculate_reliability_source_destination_detail);
  INSTALL_COMMAND (cmdset_graph, calculate_reliability_source_destination_stat);
  INSTALL_COMMAND (cmdset_graph, calculate_reliability_source_destination_stat_detail);
  INSTALL_COMMAND (cmdset_graph, calculate_reliability_source_destination_stat_detail);
  INSTALL_COMMAND (cmdset_graph, calculate_reliability_source_destination_sdp_stat_detail);
  INSTALL_COMMAND (cmdset_graph, calculate_reliability_source_destination_sdp_stat_detail_detail);
  INSTALL_COMMAND (cmdset_graph, calculate_reliability_all_to_all);
  INSTALL_COMMAND (cmdset_graph, calculate_reliability_all_to_all_stat);
  INSTALL_COMMAND (cmdset_graph, calculate_reliability_all_to_half);
  INSTALL_COMMAND (cmdset_graph, calculate_reliability_all_to_half_stat);

  INSTALL_COMMAND (cmdset_graph, link_all_reliability);
}

void
graph_finish ()
{
  struct vector_node *vn;
  struct graph *graph;
  for (vn = vector_head (graphs); vn; vn = vector_next (vn))
    {
      graph = (struct graph *) vn->data;
      if (graph)
        graph_delete (graph);
    }
  vector_delete (graphs);
}

struct module mod_graph =
{
  "graph",
  graph_init,
  graph_finish,
  &graphs,
  create_graph,
  graph_name,
  show_graph_summary_header,
  show_graph_summary,
  show_graph_instance,
  &cmdset_graph,
  graph_config_write
};



