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

#include "module.h"

#include "interface/simrouting_file.h"

struct vector *modules;

extern struct module mod_graph;
extern struct module mod_weight;
extern struct module mod_traffic;
extern struct module mod_routing;
extern struct module mod_network;

void
module_list ()
{
  vector_add (&mod_graph, modules);
  vector_add (&mod_weight, modules);
  vector_add (&mod_traffic, modules);
  vector_add (&mod_routing, modules);
  vector_add (&mod_network, modules);
  //vector_add (mod_group, modules);
}

struct module *
module_lookup (char *module_name, struct vector *modules)
{
  struct module *match = NULL;
  struct vector_node *vn;
  for (vn = vector_head (modules); vn; vn = vector_next (vn))
    {
      struct module *m = (struct module *) vector_data (vn);
      if (! strcmp (module_name, m->name))
        match = m;
    }
  return match;
}

void *
instance_lookup (char *module_name, char *instance_name)
{
  struct module *module;
  void *match = NULL;
  struct vector_node *vn;

  module = module_lookup (module_name, modules);
  if (! module)
    return NULL;

  for (vn = vector_head (*module->instances); vn; vn = vector_next (vn))
    {
      void *instance = (void *) vector_data (vn);
      char *name = (*module->instance_name) (instance);
      if (! strcmp (name, instance_name))
        match = instance;
    }

  return match;
}

DEFINE_COMMAND (enter_instance_clause,
  "module NAME",
  "enter instance clause\n"
  "specify instance ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct module *module = NULL;
  void *instance = NULL;
  char *module_name = argv[0];
  char *instance_name = argv[1];
  char prompt[64];

  module = module_lookup (module_name, modules);
  instance = instance_lookup (module_name, instance_name);
  if (! instance)
    {
      instance = (*module->create_instance) (instance_name);
      vector_add (instance, *module->instances);
    }

  shell->module = module;
  shell->context = instance;
  snprintf (prompt, sizeof (prompt), "config-%s-%s> ",
            module_name, instance_name);
  shell_set_prompt (shell, prompt);
  shell->cmdset = *module->cmdset;
}

DEFINE_COMMAND (show_module,
  "show module",
  "show information\n"
  "show module instances\n")
{
  struct shell *shell = (struct shell *) context;
  struct module *m = NULL;
  char *module_name = argv[1];
  struct vector_node *vn;

  m = module_lookup (module_name, modules);

  if (m->show_summary_header)
    (*m->show_summary_header) (shell->terminal);

  for (vn = vector_head (*m->instances); vn; vn = vector_next (vn))
    {
      void *instance = vector_data (vn);
      if (m->show_summary)
        (*m->show_summary) (shell->terminal, instance);
    }
}

DEFINE_COMMAND (show_module_instance,
  "show module NAME",
  "show information\n"
  "show module instances\n"
  "specify instance ID\n")
{
  struct shell *shell = (struct shell *) context;
  struct module *m = NULL;
  void *instance = NULL;
  char *module_name = argv[1];
  char *instance_name = argv[2];

  m = module_lookup (module_name, modules);
  instance = instance_lookup (module_name, instance_name);
  if (instance)
    {
      if (m->show_instance)
        (*m->show_instance) (shell->terminal, instance);
    }
}

DEFINE_COMMAND (exit_instance_clause,
  "exit",
  "exit this clause\n")
{
  struct shell *shell = (struct shell *) context;
  shell->context = NULL;
  shell_set_prompt (shell, prompt_default);
  shell->cmdset = cmdset_default;
}

void
install_enter_command (struct module *m)
{
  char cmdstr[1024], helpstr[1024];

  /* tweak cmdstr to simulate ALIAS_COMMAND */
  snprintf (cmdstr, sizeof (cmdstr), "%s NAME", m->name);
  snprintf (helpstr, sizeof (helpstr),
            "enter %s clause\n" "specify %s ID\n", m->name, m->name);
  command_install (cmdset_default, cmdstr, helpstr,
                   enter_instance_clause_func);
}

void
install_show_module_command (struct module *m)
{
  char cmdstr[1024], helpstr[1024];

  /* tweak cmdstr to simulate ALIAS_COMMAND */
  snprintf (cmdstr, sizeof (cmdstr), "show %s", m->name);
  snprintf (helpstr, sizeof (helpstr),
            "show information\n" "show %s information\n", m->name);
  command_install (cmdset_default, cmdstr, helpstr,
                   show_module_func);
}

void
install_show_module_instance_command (struct module *m)
{
  char cmdstr[1024], helpstr[1024];

  /* tweak cmdstr to simulate ALIAS_COMMAND */
  snprintf (cmdstr, sizeof (cmdstr), "show %s NAME", m->name);
  snprintf (helpstr, sizeof (helpstr),
            "show information\n" "show %s information\n"
            "show %s instance\n", m->name, m->name);
  command_install (cmdset_default, cmdstr, helpstr,
                   show_module_instance_func);
}

void
install_exit_command (struct module *m)
{
  INSTALL_COMMAND (*m->cmdset, exit_instance_clause);
}

void
install_export_simrouting_file_command (struct module *m)
{
  INSTALL_COMMAND (*m->cmdset, export_simrouting_file);
}

void
module_init ()
{
  struct vector_node *vn;

  modules = vector_create ();
  module_list ();

  for (vn = vector_head (modules); vn; vn = vector_next (vn))
    {
      struct module *m = (struct module *) vector_data (vn);
      if (! m)
        continue;

      if (m->init)
        (*m->init) ();

      install_enter_command (m);
      install_show_module_command (m);
      install_show_module_instance_command (m);
      install_exit_command (m);
      install_export_simrouting_file_command (m);
    }
}

void
module_finish ()
{
  struct vector_node *vn;

  for (vn = vector_head (modules); vn; vn = vector_next (vn))
    {
      struct module *m = (struct module *) vector_data (vn);
      if (! m)
        continue;

      if (m->finish)
        (*m->finish) ();
    }

  vector_delete (modules);
}


