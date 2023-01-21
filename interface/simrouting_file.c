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
#include "command_shell.h"
#include "module.h"

DEFINE_COMMAND (clear_file,
  "clear file <FILENAME>",
  "clear information\n"
  "clear file\n"
  "specify filename\n")
{
  int ret;
  ret = file_truncate (argv[2]);
  if (ret < 0)
    fprintf (stderr, "Cannot clear file: %s\n", argv[2]);
}

int
simrouting_read_file (struct shell *shell, char *file)
{
  FILE *fp;
  int lineno = 0;
  char *retval;
  int ret;
  char *p;
#define SIMROUTING_READ_FILE_BUF_SIZE 512
  char buf[SIMROUTING_READ_FILE_BUF_SIZE];
  int save_writefd;

  fp = fopen (file, "r");
  if (! fp)
    {
      fprintf (stderr, "Can't open file: %s: %s\n",
	       file, strerror (errno));
      return -1;
    }

  shell_clear (shell);

  save_writefd = shell->writefd;
  shell->writefd = -1;

  while ((retval = fgets (buf, sizeof (buf), fp)) != NULL)
    {
      p = strchr (buf, '\n');
      if (p)
        *p = '\0';
      shell_insert (shell, buf);
      if (! p)
        continue;

      lineno++;
      shell_format (shell);

      if (! strlen (shell->command_line))
        continue;

      ret = command_execute (shell->command_line, shell->cmdset, shell);
      command_history_add (shell->command_line, shell->history, shell);
      if (ret < 0)
        break;

      shell_clear (shell);
    }

  shell->writefd = save_writefd;

  if (! feof (fp))
    {
      if (ferror (fp))
        fprintf (stderr, "Read error: %s line %d\n  reason: %s\n",
                 file, lineno, strerror (errno));
      else
        fprintf (stderr, "Command error: %s line %d\n  command: %s\n",
                 file, lineno, shell->command_line);
      fclose (fp);
      return -1;
    }

  fclose (fp);
  return 0;
}

int
simrouting_read_file_inside_clause (struct shell *shell, char *file)
{
  FILE *fp;
  int lineno = 0;
  char *retval;
  int ret;
  char *p;
#define SIMROUTING_READ_FILE_BUF_SIZE 512
  char buf[SIMROUTING_READ_FILE_BUF_SIZE];

  fp = fopen (file, "r");
  if (! fp)
    {
      fprintf (stderr, "Can't open file: %s: %s\n",
	       file, strerror (errno));
      return -1;
    }

  shell_clear (shell);
  while ((retval = fgets (buf, sizeof (buf), fp)) != NULL)
    {
      p = strchr (buf, '\n');
      if (p)
        *p = '\0';
      shell_insert (shell, buf);
      if (! p)
        continue;

      lineno++;
      shell_format (shell);

      if (! strlen (shell->command_line))
        continue;

      ret = command_execute (shell->command_line, shell->cmdset, shell);

      shell_clear (shell);
    }

  fclose (fp);
  return 0;
}

DEFINE_COMMAND (load_file,
  "load file <FILENAME>",
  "load configuration file\n"
  "load configuration file\n"
  "specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  simrouting_read_file (shell, argv[2]);
}

void
config_write (FILE *out)
{
  struct vector_node *vni, *vnj;

  //group_config_write (out);

  for (vni = vector_head (modules); vni; vni = vector_next (vni))
    {
      struct module *m = (struct module *) vector_data (vni);
      for (vnj = vector_head (*m->instances); vnj; vnj = vector_next (vnj))
        {
          void *instance = (void *) vector_data (vnj);
          fprintf (out, "\n");
          fprintf (out, "%s %s\n", m->name, (*m->instance_name) (instance));
          (*m->command_config_write) (instance, out);
          fprintf (out, "exit\n");
        }
    }

  fflush (out);
}

DEFINE_COMMAND (show_config,
                "show config",
                "display information\n"
                "display configuration information\n")
{
  struct shell *shell = (struct shell *) context;
  config_write (shell->terminal);
}

DEFINE_COMMAND (write_config,
  "write config <FILENAME>",
  "write information\n"
  "write configuration information\n"
  "specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  FILE *fp;
  fp = fopen (argv[2], "w+");
  if (fp == NULL)
    {
      fprintf (shell->terminal, "cannot open file: %s\n", argv[2]);
      fprintf (shell->terminal, "fopen (): %s\n", strerror (errno));
      return;
    }
  config_write (fp);
  fclose (fp);
}

ALIAS_COMMAND (save_config,
  write_config,
  "save config <FILENAME>",
  "save information\n"
  "save configuration information\n"
  "specify filename\n");

DEFINE_COMMAND (import_simrouting_file,
  "import simrouting file <FILENAME>",
  "import information\n"
  "import simrouting configuration\n"
  "import simrouting configuration to file\n"
  "specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  simrouting_read_file (shell, argv[3]);
}

DEFINE_COMMAND (import_simrouting_file_inside_clause,
  "import simrouting file <FILENAME>",
  "import information\n"
  "import simrouting configuration\n"
  "import simrouting configuration to file\n"
  "specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  simrouting_read_file_inside_clause (shell, argv[3]);
}

DEFINE_COMMAND (export_simrouting_file,
  "export simrouting file <FILENAME>",
  "export information\n"
  "export simrouting configuration\n"
  "export simrouting configuration to file\n"
  "specify filename\n")
{
  struct shell *shell = (struct shell *) context;
  FILE *fp;
  fp = fopen_create (argv[3], "w");
  if (fp == NULL)
    {
      fprintf (shell->terminal, "cannot open file: %s\n", argv[3]);
      fprintf (shell->terminal, "fopen (): %s\n", strerror (errno));
      return;
    }

  if (shell->context)
    {
      struct module *m = (struct module *) shell->module;
      void *instance = shell->context;

      fprintf (fp, "\n");
      fprintf (fp, "%s %s\n", m->name, (*m->instance_name) (instance));
      (*m->command_config_write) (instance, fp);
      fprintf (fp, "exit\n");
    }
  else
    {
      struct vector_node *vni, *vnj;
    
      for (vni = vector_head (modules); vni; vni = vector_next (vni))
        {
          struct module *m = (struct module *) vector_data (vni);
          for (vnj = vector_head (*m->instances); vnj; vnj = vector_next (vnj))
            {
              void *instance = (void *) vector_data (vnj);
              fprintf (fp, "\n");
              fprintf (fp, "%s %s\n", m->name, (*m->instance_name) (instance));
              (*m->command_config_write) (instance, fp);
              fprintf (fp, "exit\n");
            }
        }
    }

  fclose (fp);
}


