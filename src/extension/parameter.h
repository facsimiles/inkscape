#ifndef __INK_EXTENSION_PARAM_H__
#define __INK_EXTENSION_PARAM_H__

/** \file
 * Parameters for extensions.
 */

/*
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *
 * Copyright (C) 2005 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#include <gtkmm/widget.h>

#include <xml/document.h>
#include "extension-forward.h"

namespace Inkscape {
namespace Extension {

class Parameter {
private:
    /** \brief  Which extension is this parameter attached to? */
    Inkscape::Extension::Extension * extension;
    /** \brief  The name of this parameter. */
    gchar *       _name;

protected:
    /** \brief  Text for the GUI selection of this. */
    gchar *       _text;
    gchar *       pref_name (void);

public:
    Parameter (const gchar * name, const gchar * guitext, Inkscape::Extension::Extension * ext);
    virtual ~Parameter(void);
    bool          get_bool   (const Inkscape::XML::Document * doc);
    int           get_int    (const Inkscape::XML::Document * doc);
    float         get_float  (const Inkscape::XML::Document * doc);
    const gchar * get_string (const Inkscape::XML::Document * doc);

    bool          set_bool   (bool in,          Inkscape::XML::Document * doc);
    int           set_int    (int  in,          Inkscape::XML::Document * doc);
    float         set_float  (float in,         Inkscape::XML::Document * doc);
    const gchar * set_string (const gchar * in, Inkscape::XML::Document * doc);

    const gchar * name       (void) {return _name;}

    static Parameter * make (Inkscape::XML::Node * in_repr, Inkscape::Extension::Extension * in_ext);
    virtual Gtk::Widget * get_widget (void);
    virtual Glib::ustring * string (void);
};


}  /* namespace Extension */
}  /* namespace Inkscape */

#endif /* __INK_EXTENSION_PARAM_H__ */

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
