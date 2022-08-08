// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
/*
 * attribute-sort-util.cpp
 *
 *  Created on: Jun 10, 2016
 *      Author: Tavmjong Bah
 */

/**
 * Utility functions for sorting attributes by name.
 */

#include <algorithm>    // std::sort
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>      // std::pair
#include <vector>

#include <glibmm/ustring.h>

#include "attribute-sort-util.h"

#include "util/optstr.h"

#include "xml/repr.h"
#include "xml/attribute-record.h"
#include "xml/sp-css-attr.h"

#include "attributes.h"

using Inkscape::XML::Node;

static void sp_attribute_sort_recursive(Node& repr);
static void sp_attribute_sort_element(Node& repr);
static void sp_attribute_sort_style(Node& repr);
static void sp_attribute_sort_style(Node& repr, SPCSSAttr& css);

/**
 * Sort attributes by name.
 */
void sp_attribute_sort_tree(Node& repr) {

  sp_attribute_sort_recursive( repr );
}

/**
 * Sort recursively over all elements.
 */
static void sp_attribute_sort_recursive(Node& repr) {

  if( repr.type() == Inkscape::XML::NodeType::ELEMENT_NODE ) {
    Glib::ustring element = repr.name();

    // Only sort elements in svg namespace
    if( element.substr(0,4) == "svg:" ) {
      sp_attribute_sort_element( repr );
    }
  }
  
  for(Node *child=repr.firstChild() ; child ; child = child->next()) {
    sp_attribute_sort_recursive( *child );
  }
}

/**
 * Compare function
 */
static bool cmp(std::pair<char const*, Glib::ustring> const &a,
                std::pair<char const*, Glib::ustring> const &b)
{
    auto val_a = sp_attribute_lookup(a.first);
    auto val_b = sp_attribute_lookup(b.first);
    if (val_a == SPAttr::INVALID) return false; // Unknown attributes at end.
    if (val_b == SPAttr::INVALID) return true;  // Unknown attributes at end.
    return val_a < val_b;
}

/**
 * Sort attributes on an element
 */
static void sp_attribute_sort_element(Node& repr) {
  g_return_if_fail (repr.type() == Inkscape::XML::NodeType::ELEMENT_NODE);

  sp_attribute_sort_style(repr);

  // Sort attributes:
  std::vector<std::pair<char const*, Glib::ustring>> my_list;
  for (auto const &iter : repr.attributeList()) {
      auto const attribute = g_quark_to_string(iter.key);
      auto &&value = Glib::ustring(*iter.value);

      // Removing "inkscape:label" results in crash when Layers dialog is open.
      if (std::strcmp(attribute, "inkscape:label") != 0) {
          my_list.emplace_back(attribute, value);
      }
  }
  std::sort(my_list.begin(), my_list.end(), cmp);
  // Delete all attributes.
  for (const auto& it : my_list) {
      repr.removeAttribute(it.first);
  }
  // Insert all attributes in proper order
  for (const auto& it : my_list) {
      repr.setAttribute(it.first, it.second.c_str());
  } 
}


/**
 * Sort CSS style on an element.
 */
static void sp_attribute_sort_style(Node& repr) {

  g_return_if_fail (repr.type() == Inkscape::XML::NodeType::ELEMENT_NODE);

  // Find element's style
  SPCSSAttr *css = sp_repr_css_attr( &repr, "style" );
  sp_attribute_sort_style(repr, *css);

  // Convert css node's properties data to string and set repr node's attribute "style" to that string.
  // sp_repr_css_set( repr, css, "style"); // Don't use as it will cause loop.
  Glib::ustring value;
  sp_repr_css_write_string(css, value);
  repr.setAttributeOrRemoveIfEmpty("style", value);

  sp_repr_css_attr_unref( css );
}


/**
 * Sort CSS style on an element.
 */
static void sp_attribute_sort_style(Node& repr, SPCSSAttr& css) {

  // Loop over all properties in "style" node.
  std::vector<std::pair<char const*, Glib::ustring>> my_list;
  for (auto const &iter : css.attributeList()) {
      auto const property = g_quark_to_string(iter.key);
      auto &&value = Glib::ustring(*iter.value);

      my_list.emplace_back(property, value);
  }
  std::sort(my_list.begin(), my_list.end(), cmp);
  // Delete all attributes.
  for (const auto& it : my_list) {
      sp_repr_css_set_property(&css, it.first, nullptr);
  }
  // Insert all attributes in proper order
  for (const auto& it : my_list) {
      sp_repr_css_set_property(&css, it.first, it.second.c_str());
  } 
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
