/*
 * Copyright (C) 2019 Authors:
 *   Alexander Maykl Mychlo <alexmmych@gmail.com>
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

/*
This is the .cpp file which contains the definitions of functions which are necessary 
in order to make a button parameter in inkscape.
*/

/*
This project works under the 0.92.x branch of Inkscape I have tested it and it works but for other newer or older branches
changes would probably be needed in order for it to work
*/



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
The includes in order to make the button itself.
*/
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/stock.h>

#include "button.h"

#include "../extension.h"

/*
An include to effect is required since I need for the button to be made inside of the effect class for it to pass the 
"this" value so the button can apply the effect.
*/

#include "extension/effect.h"

#include "preferences.h"
#include "xml/node.h"
#include "ui/dialog-events.h"
#include "xml/repr.h"
#include "ui/view/view.h"

/*
Some of the includes can probably be removed but I don't know which of them are integral or not.
*/



namespace Inkscape {
namespace Extension {

ButtonParam::ButtonParam(const gchar *name, const gchar *guitext, const gchar *desc, const Parameter::_scope_t scope,
                         bool gui_hidden, const gchar *gui_tip, Inkscape::Extension::Extension *ext,
                         Inkscape::XML::Node *xml)

    : //<---
    Parameter(name, guitext, desc, scope, gui_hidden, gui_tip, ext)
    , _value(false)
    , _indent(0)
{
    if (xml->firstChild() != NULL) {
        defaultval = xml->firstChild()->content();
    }

/*
Don't know what indent does but it must be important since it's present in all of the parameter files.
*/
    const char *indent = xml->attribute("indent");
    if (indent != NULL) {
        _indent = atoi(indent) * 12;
    }

    gchar *pref_name = this->pref_name();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    _value = prefs->getBool(extension_pref_root + pref_name, _value);
    g_free(pref_name);

    return;
}

/*
Sets the value of the boolean to the value specified in the first argument.
*/

bool ButtonParam::set(bool in, SPDocument *doc, Inkscape::XML::Node *node)
{

    _value = in;

    gchar *prefname = this->pref_name();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setBool(extension_pref_root + prefname, _value);
    g_free(prefname);

    return _value;
}

/*
Don't know what string does but it's present in boolean so I kept it.
*/

void ButtonParam::string(std::string &string) const
{
    if (_value) {
        string += "true";
    }
    else {
        string += "false";
    }

    return;
}

/*
The class that makes the button itself.
*/

MakeButtonParam::MakeButtonParam(ButtonParam *param, SPDocument *doc, Inkscape::XML::Node *node,
                                 sigc::signal<void> *changeSignal,Effect* effect)

    : //<---
	//Initializing the pointers.
      _button()
    , _pref(param)
    , _doc(doc)
    , _node(node)
    , _changeSignal(changeSignal) 
	, _effect(effect)
{
	//Sets the label of the button to the value of defaultval.
    set_label(_pref->defaultval);
	//Checks if the button has been clicked and if it does activates "on_click" function.
    this->signal_clicked().connect(sigc::mem_fun(this, &MakeButtonParam::on_click));
    return;
}

void MakeButtonParam::on_click(void)

{
	//Sets boolean value to true.
    _pref->set(true, NULL, NULL);
	//Applies the effect.
    if (_effect != NULL) {
		
		_effect->effect(SP_ACTIVE_DESKTOP);
    }
    else {
        return;
    }
	//Sets boolean value to false.
    _pref->set(false,NULL,NULL);

    return;
}



Gtk::Widget *ButtonParam::get_widget(SPDocument *doc, Inkscape::XML::Node *node, sigc::signal<void> *changeSignal)
{
    if (_gui_hidden) {
        return NULL;
    }

#if WITH_GTKMM_3_0
    Gtk::Box *hbox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
    hbox->set_homogeneous(false);
#else
    Gtk::HBox *hbox = Gtk::manage(new Gtk::HBox(false, 4));
#endif

    Gtk::Label *label = Gtk::manage(new Gtk::Label(_text, Gtk::ALIGN_START));
    label->show();
    hbox->pack_end(*label, true, true);

	/*
	Instead of making a "MakeButtonParam" object inside here I made a function in the effect class which does it there
	so it can pass the "this" parameter and allow for the button to apply the current effect.
	*/

    MakeButtonParam *button = Gtk::manage(_effect->make_button(this, doc, node, changeSignal));

    button->show();
    hbox->pack_start(*button, false, false, _indent);
    hbox->show();

    return dynamic_cast<Gtk::Widget *>(hbox);
}

} // namespace Extension
} // namespace Inkscape