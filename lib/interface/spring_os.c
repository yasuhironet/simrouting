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

#include <includes.h>

#include "file.h"
#include "vector.h"
#include "shell.h"
#include "command.h"
#include "command_shell.h"
#include "module.h"

#include "network/graph.h"

#include "interface/spring_os.h"

struct command_set *cmdset_spring_os;
struct vector *spring_oses;

void *
spring_os_create (char *id)
{
  struct spring_os *s;
  s = (struct spring_os *) malloc (sizeof (struct spring_os));
  memset (s, 0, sizeof (struct spring_os));
  s->config = vector_create ();
  s->name = strdup (id);
  s->scenario = vector_create ();
  return (void *) s;
}

void
spring_os_delete (struct spring_os *s)
{
  struct vector_node *vn;
  for (vn = vector_head (s->scenario); vn; vn = vector_next (vn))
    {
       char *scline = (char *) vector_data (vn);
       free (scline);
    }
  vector_delete (s->scenario);

  if (s->name)
    free (s->name);
  command_config_clear (s->config);
  vector_delete (s->config);
  free (s);
}

#if 0
void
scenario_set_line (struct vector *scenario, int argc, char *argv)
{
}

DEFINE_COMMAND (rmanager_ipaddr_port,
  "rmanager ipaddr A.B.C.D port <0-65535>",
  "rmanager\n"
  "IP address\n"
  "specify IP address\n"
  "port\n"
  "specify port number\n")
{
  struct shell *shell = (struct shell *) context;
  struct spring_os *spring_os = (struct spring_os *) shell->context;
  scenario_add (spring_os->scenario, argc, argv);
}
#endif

DEFINE_COMMAND (export_spring_os_graph,
  "export spring-os graph <FILENAME>",
  "export information\n"
  "export to SpringOS scenario\n"
  "export graph to SpringOS scenario\n"
  "specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  struct graph *g = (struct graph *) shell->context;
  FILE *fp;
  struct vector_node *vn;
  struct vector *done;

  char *diskimage = "ftp://starbed:starbed@192.168.255.255:/diskimages/diskimage.gz";

  fp = fopen_create (argv[3], "w");
  if (! fp)
    {
      fprintf (stderr, "Cannot open file %s: %s\n",
               argv[3], strerror (errno));
      return;
    }

  fprintf (fp, "nodeclass clrouter {\n");
  fprintf (fp, "  method \"HDD\"\n");
  fprintf (fp, "  partition 1\n");
  fprintf (fp, "  ostype \"FreeBSD\"\n");
  fprintf (fp, "  diskimage \"%s\"\n", diskimage);
  fprintf (fp, "  netif media fastethernet\n");
  fprintf (fp, "  scenario {\n");
  fprintf (fp, "    netiffit pathifscan\n");
  fprintf (fp, "    send \"booted\"\n");
  fprintf (fp, "    loop {\n");
  fprintf (fp, "      recv val\n");
  fprintf (fp, "      msgswitch val {\n");
  fprintf (fp, "        \"netconfig\" {\n");
  fprintf (fp, "          send \"done-netconfig\"\n");
  fprintf (fp, "        }\n");
  fprintf (fp, "        \"halt\" {\n");
  fprintf (fp, "          call \"/sbin/shutdown\" \"-h\" \"now\"\n");
  fprintf (fp, "        }\n");
  fprintf (fp, "      }\n");
  fprintf (fp, "    }\n");
  fprintf (fp, "  }\n");
  fprintf (fp, "}\n");
  fprintf (fp, "\n");

  fprintf (fp, "netclass clnetwork {\n");
  fprintf (fp, "  media fastethernet\n");
  fprintf (fp, "}\n");
  fprintf (fp, "\n");

  fprintf (fp, "nodeset router class clrouter num %d\n",
           g->nodes->size);
  fprintf (fp, "netset network class clnetwork num %d\n",
           g->links->size / 2);
  fprintf (fp, "\n");

  fprintf (fp, "for (i = 0; i < %d; i++) {\n", g->nodes->size);
  fprintf (fp, "  callw \"/sbin/ifconfig\" router[i].rname "
           "\"192.168.0.\" + tostring (i)\n");
  fprintf (fp, "}\n");
  fprintf (fp, "\n");

  done = vector_create ();
  for (vn = vector_head (g->links); vn; vn = vector_next (vn))
    {
      struct link *link = (struct link *) vector_data (vn);
      int from_index = -1;
      int to_index = -1;

      assert (link->inverse);

      if (vector_lookup (link, done))
        continue;

      from_index = vector_lookup_index (link, link->from->olinks);
      to_index = vector_lookup_index (link->inverse, link->to->olinks);
      assert (from_index >= 0 && to_index >= 0);

      fprintf (fp, "attach router[%d].netif[%d] network[%d]\n",
               link->from->id, from_index, link->id / 2);
      fprintf (fp, "attach router[%d].netif[%d] network[%d]\n",
               link->to->id, to_index, link->id / 2);
      vector_add (link, done);
      if (link->inverse)
        vector_add (link->inverse, done);
    }
  vector_delete (done);
  fprintf (fp, "\n");

  fclose (fp);
}

void
spring_os_init ()
{
  spring_oses = vector_create ();
  cmdset_spring_os = command_set_create ();

  //INSTALL_COMMAND (cmdset_spring_os, show_spring_os);
  INSTALL_COMMAND (cmdset_spring_os, export_spring_os_graph);
}

void
scenario_add (struct vector *scenario, int argc, char **argv)
{
  char buf[4048], *p;
  int i;
  buf[0] = '\0';
  for (i = 0; i < argc; i++)
    {
      p = index (buf, '\0');
      snprintf (p, buf + sizeof (buf) - p - 1, "%s", argv[i]);
      p = index (buf, '\0');
      if (i + 1 < argc)
        snprintf (p, buf + sizeof (buf) - p - 1, " ");
    }
  vector_add (strdup (buf), scenario);
}

void
spring_os_finish ()
{
  struct vector_node *vn;
  struct spring_os *spring_os;
  for (vn = vector_head (spring_oses); vn; vn = vector_next (vn))
    {
      spring_os = (struct spring_os *) vn->data;
      if (spring_os)
        spring_os_delete (spring_os);
    }
  vector_delete (spring_oses);
}

char *
spring_os_name (void *instance)
{
  struct spring_os *spring_os = (struct spring_os *) instance;
  return spring_os->name;
}

void
spring_os_config_write (void *instance, FILE *out)
{
  struct spring_os *spring_os = (struct spring_os *) instance;
  command_config_write (spring_os->config, out);
}

void
show_spring_os_summary_header (FILE *fp)
{
}

void
show_spring_os_summary (FILE *fp, void *instance)
{
}

void
show_spring_os_instance (FILE *fp, void *instance)
{
}

struct module mod_spring_os =
{
  "spring-os",
  spring_os_init,
  spring_os_finish,
  &spring_oses,
  spring_os_create,
  spring_os_name,
  show_spring_os_summary_header,
  show_spring_os_summary,
  show_spring_os_instance,
  &cmdset_spring_os,
  spring_os_config_write
};


