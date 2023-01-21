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

#include "vector.h"
#include "shell.h"
#include "command.h"
#include "command_shell.h"
#include "termio.h"
#include "module.h"

#include "network/graph.h"
#include "network/graph_cmd.h"
#include "network/group.h"
#include "traffic-model/demand.h"
#include "network/weight.h"
#include "network/path.h"
#include "network/routing.h"
#include "network/network.h"

#include "interface/simrouting_file.h"

#define BUG_ADDRESS "yasu@sfc.wide.ad.jp"

extern char *optarg;
extern int optind, opterr, optopt;

char *progname;
int debug = 0;

struct option longopts[] =
{
  { "debug",          no_argument,       NULL, 'd'},
  { "help",           no_argument,       NULL, 'h'},
  { "version",        no_argument,       NULL, 'v'},
  { "interactive",    no_argument,       NULL, 'i'},
  { 0 }
};

void
print_version ()
{
}

void
usage ()
{
  printf ("Usage : %s [-h|-i|-v] [FILE1,FILE2...]\n\n\
FILEs are read and executed by interpreting each line \n\
as commands. Otherwise intractive shell will be run.\n\n\
Report bugs to %s\n", progname, BUG_ADDRESS);
}

void
simrouting_run_shell (struct shell *shell)
{
  shell_set_terminal (shell, 0, 1);
  shell->flag = 0;

  INSTALL_COMMAND (shell->cmdset, exit);
  INSTALL_COMMAND (shell->cmdset, quit);
  INSTALL_COMMAND (shell->cmdset, logout);

  termio_init ();
  while (shell_running (shell))
    shell_read (shell);
  termio_finish ();
}

int
main (int argc, char **argv)
{
  char *p;
  int i, ret;
  struct shell *shell = NULL;
  int interactive = 0;

  /* Preserve name of myself. */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  /* Command line argument treatment. */
  while (1)
    {
      ret = getopt_long (argc, argv, "df:hiv", longopts, 0);

      if (ret == EOF)
        break;

      switch (ret)
        {
        case 0:
          break;

        case 'd':
          debug = 1;
          break;
        case 'v':
          print_version ();
          exit (0);
          break;
        case 'h':
          usage ();
          exit (0);
          break;
        case 'i':
          interactive++;
          break;
        default:
          usage ();
          exit (1);
          break;
        }
    }

  command_shell_init ();
  module_init ();

  INSTALL_COMMAND (cmdset_default, load_file);
  INSTALL_COMMAND (cmdset_default, clear_file);
  INSTALL_COMMAND (cmdset_default, show_config);
  INSTALL_COMMAND (cmdset_default, write_config);
  INSTALL_COMMAND (cmdset_default, save_config);

  shell = command_shell_create ();
  prompt_default = "simrouting> ";
  shell_set_prompt (shell, prompt_default);
  shell_set_terminal (shell, 0, 1);

  if (optind < argc)
    {
      for (i = optind; i < argc; i++)
        {
          ret = simrouting_read_file (shell, argv[i]);
          if (ret < 0)
            break;
        }
    }
  else
    interactive++;

  if (interactive)
    simrouting_run_shell (shell);

  shell_delete (shell);

  module_finish ();
  command_shell_finish ();

  return 0;
}




