/*
 * Copyright (C) 2010  Yasuhiro Ohara
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
#include "table.h"

#include "network/graph.h"
#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"
#include "network/network.h"

#include "interface/ospf.h"
#include "interface/snmp.h"
#include "interface/ospf_snmp.h"

#include "routing/dijkstra.h"

#include "network/graph_cmd.h"

#define BUG_ADDRESS "yasu@jaist.ac.jp"

extern char *optarg;
extern int optind, opterr, optopt;

char *progname;
int debug = 0;

struct option longopts[] =
{
  { "debug",          no_argument,       NULL, 'd'},
  { "help",           no_argument,       NULL, 'h'},
  { "version",        no_argument,       NULL, 'v'},
  { 0 }
};

void
print_version ()
{
}

void
usage ()
{
  printf ("Usage: %s [-d|-h|-v] <community> [<host>] [<area>]\n",
          progname);
  printf ("Report bugs to %s\n", BUG_ADDRESS);
}

int
main (int argc, char **argv)
{
  char *p;
  int ret;

  char *host;
  char *community;
  char *area;

  struct table *lsdb;
  struct routing *routing;
  struct weight *weight;

  struct graph *graph, *routing_graph;

  struct vector_node *vn;

  /* Preserve name of the program itself. */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  /* Command line argument treatment. */
  while (1)
    {
      ret = getopt_long (argc, argv, "dhv", longopts, 0);

      if (ret == EOF)
        break;

      switch (ret)
        {
        case 0:
          break;

        case 'd':
          debug = 1;
          argc--;
          argv++;
          break;

        case 'v':
          print_version ();
          exit (0);
          break;

        case 'h':
          usage ();
          exit (0);
          break;

        default:
          usage ();
          exit (1);
          break;
        }
    }

  community = (argc > 1 ? argv[1] : NULL);
  host = (argc > 2 ? argv[2] : "localhost");
  area = (argc > 3 ? argv[3] : "0.0.0.0");

  if (! community)
    {
      usage ();
      exit (1);
    }

  lsdb = ospf_lsdb_get_by_snmp (host, community, area);

  graph = graph_import_ospf (lsdb);
  weight = weight_import_ospf (lsdb, graph);

  routing = routing_create ();
  routing->G = graph;
  routing->W = weight;
  routing->nnodes = graph_nodes (graph);
  routing->route = route_table_create (routing->nnodes);

  routing_dijkstra_all_node (routing);

  for (vn = vector_head (graph->nodes); vn; vn = vector_next (vn))
    {
      struct node *dest = (struct node *) vn->data;
      struct graph *routing_graph;

      routing_graph = graph_import_routing (routing, dest);

#if 0
      graphviz_import_graph (graphviz, routing_graph);
      graphviz_layout (graphviz, "fdp");
      graphviz_render (graphviz, filename);
#endif

      graph_delete (routing_graph);
    }

  graph_delete (graph);

  return 0;
}


