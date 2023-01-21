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

#ifndef _MODULE_H_
#define _MODULE_H_

struct module
{
  char *name;
  void (*init) ();
  void (*finish) ();
  struct vector **instances;
  void * (*create_instance) (char *id);
  char * (*instance_name) (void *instance);
  void (*show_summary_header) (FILE *fp);
  void (*show_summary) (FILE *fp, void *instance);
  void (*show_instance) (FILE *fp, void *instance);
  struct command_set **cmdset;
  void (*command_config_write) (void *instance, FILE *out);
};

extern struct vector *modules;

void module_init ();
void module_finish ();

void *
instance_lookup (char *module_name, char *instance_name);

#endif /*_MODULE_H_*/

