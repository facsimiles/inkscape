// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Functions to keep a listing of all modules in the system.  Has its
 * own file mostly for abstraction reasons, but is pretty simple
 * otherwise.
 *
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2002-2004 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "db.h"
#include "input.h"
#include "output.h"
#include "effect.h"

/* Globals */

/* Namespaces */

namespace Inkscape {
namespace Extension {

/** This is the actual database object.  There is only one of these */
DB db;

/* Types */

DB::DB (void) = default;


struct ModuleInputCmp {
  bool operator()(Input* module1, Input* module2) const {

    // Ensure SVG files are at top
    int n1 = 0;
    int n2 = 0;
    //                             12345678901234567890123456789012
    if (strncmp(module1->get_id(),"org.inkscape.input.svg",  22) == 0) n1 = 1;
    if (strncmp(module2->get_id(),"org.inkscape.input.svg",  22) == 0) n2 = 1;
    if (strncmp(module1->get_id(),"org.inkscape.input.svgz", 23) == 0) n1 = 2;
    if (strncmp(module2->get_id(),"org.inkscape.input.svgz", 23) == 0) n2 = 2;

    if (n1 != 0 && n2 != 0) return (n1 < n2);
    if (n1 != 0) return true;
    if (n2 != 0) return false;

    // GDK filetypenames begin with lower case letters and thus are sorted at the end.
    // Special case "sK1" which starts with a lower case letter to keep it out of GDK region.
    if (strncmp(module1->get_id(),"org.inkscape.input.sk1",  22) == 0) {
      return ( strcmp("SK1", module2->get_filetypename()) <= 0 );
    }
    if (strncmp(module2->get_id(),"org.inkscape.input.sk1",  22) == 0) {
      return ( strcmp(module1->get_filetypename(), "SK1" ) <= 0 );
    }

    return ( strcmp(module1->get_filetypename(), module2->get_filetypename()) <= 0 );
  }
};


struct ModuleOutputCmp {
  bool operator()(Output* module1, Output* module2) const {

    // Ensure SVG files are at top
    int n1 = 0;
    int n2 = 0;
    //                             12345678901234567890123456789012
    if (strncmp(module1->get_id(),"org.inkscape.output.svg.inkscape",  32) == 0) n1 = 1;
    if (strncmp(module2->get_id(),"org.inkscape.output.svg.inkscape",  32) == 0) n2 = 1;
    if (strncmp(module1->get_id(),"org.inkscape.output.svg.plain",     29) == 0) n1 = 2;
    if (strncmp(module2->get_id(),"org.inkscape.output.svg.plain",     29) == 0) n2 = 2;
    if (strncmp(module1->get_id(),"org.inkscape.output.svgz.inkscape", 33) == 0) n1 = 3;
    if (strncmp(module2->get_id(),"org.inkscape.output.svgz.inkscape", 33) == 0) n2 = 3;
    if (strncmp(module1->get_id(),"org.inkscape.output.svgz.plain",    30) == 0) n1 = 4;
    if (strncmp(module2->get_id(),"org.inkscape.output.svgz.plain",    30) == 0) n2 = 4;
    if (strncmp(module1->get_id(),"org.inkscape.output.scour",         25) == 0) n1 = 5;
    if (strncmp(module2->get_id(),"org.inkscape.output.scour",         25) == 0) n2 = 5;
    if (strncmp(module1->get_id(),"org.inkscape.output.ZIP",           23) == 0) n1 = 6;
    if (strncmp(module2->get_id(),"org.inkscape.output.ZIP",           23) == 0) n2 = 6;
    if (strncmp(module1->get_id(),"org.inkscape.output.LAYERS",        26) == 0) n1 = 7;
    if (strncmp(module2->get_id(),"org.inkscape.output.LAYERS",        26) == 0) n2 = 7;

    if (n1 != 0 && n2 != 0) return (n1 < n2);
    if (n1 != 0) return true;
    if (n2 != 0) return false;

    // Special case "sK1" which starts with a lower case letter.
    if (strncmp(module1->get_id(),"org.inkscape.output.sk1",  23) == 0) {
      return ( strcmp("SK1", module2->get_filetypename()) <= 0 );
    }
    if (strncmp(module2->get_id(),"org.inkscape.output.sk1",  23) == 0) {
      return ( strcmp(module1->get_filetypename(), "SK1" ) <= 0 );
    }

    return ( strcmp(module1->get_filetypename(), module2->get_filetypename()) <= 0 );
  }
};


/**
	\brief     Add a module to the module database
	\param     module  The module to be registered.
*/
void
DB::register_ext (Extension *module)
{
	g_return_if_fail(module != nullptr);
	g_return_if_fail(module->get_id() != nullptr);

	//printf("Registering: '%s' '%s' add:%d\n", module->get_id(), module->get_name(), add_to_list);
	moduledict[module->get_id()] = module;
}

/**
	\brief     This function removes a module from the database
	\param     module  The module to be removed.
*/
void
DB::unregister_ext (Extension * module)
{
	g_return_if_fail(module != nullptr);
	g_return_if_fail(module->get_id() != nullptr);

	// printf("Extension DB: removing %s\n", module->get_id());
	moduledict.erase(moduledict.find(module->get_id()));
}

/**
	\return    A reference to the Inkscape::Extension::Extension specified by the input key.
	\brief     This function looks up a Inkscape::Extension::Extension by using its unique
	           id.  It then returns a reference to that module.
	\param     key   The unique ID of the module

	Retrieves a module by name; if non-NULL, it refs the returned
  	module; the caller is responsible for releasing that reference
	when it is no longer needed.
*/
Extension *
DB::get (const gchar *key)
{
        if (key == nullptr) return nullptr;

	Extension *mod = moduledict[key];
	if ( !mod || mod->deactivated() )
		return nullptr;

	return mod;
}

/**
 * \brief Small filter function to only choose extensions of a particular type
 */
template <typename T>
static void extension_type_filter(Extension *ext, std::list<T> &list)
{
    auto typed_ext = dynamic_cast<T>(ext);
    if (typed_ext != nullptr) {
        list.push_back(typed_ext);
    }
}

/**
	\brief  Creates a list of all the Input extensions
	\param  ou_list  The list that is used to put all the extensions in

	Calls the database \c foreach function with \c input_internal.
*/
DB::InputList &
DB::get_input_list (DB::InputList &ou_list)
{
    foreach ([&ou_list](Extension *ext) { extension_type_filter(ext, ou_list); })
        ;
    ou_list.sort(ModuleInputCmp());
    return ou_list;
}

/**
	\brief  Creates a list of all the Output extensions
	\param  ou_list  The list that is used to put all the extensions in

	Calls the database \c foreach function with \c output_internal.
*/
DB::OutputList &
DB::get_output_list (DB::OutputList &ou_list)
{
    foreach ([&ou_list](Extension *ext) { extension_type_filter(ext, ou_list); })
        ;
    ou_list.sort(ModuleOutputCmp());
    return ou_list;
}

/**
	\brief  Creates a list of all the Effect extensions
	\param  ou_list  The list that is used to put all the extensions in

	Calls the database \c foreach function with \c effect_internal.
*/
DB::EffectList &
DB::get_effect_list (DB::EffectList &ou_list)
{
    foreach ([&ou_list](Extension *ext) { extension_type_filter(ext, ou_list); })
        ;
    return ou_list;
}

} } /* namespace Extension, Inkscape */
