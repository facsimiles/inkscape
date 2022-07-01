// SPDX-License-Identifier: GPL-2.0-or-later
/**
    \file text-recognition.cpp

    A plug-in to detect text.
*/
/*
 * Authors:
 *
 *   Bashar Ahmed <basharahmed00000@gmail.com>
 *
 * Copyright (C) 2005 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "text-recognition.h"

#include <vector>

#include "desktop.h"
#include "display/cairo-utils.h"
#include "document.h"
#include "extension/effect.h"
#include "extension/prefdialog/parameter.h"
#include "extension/system.h"
#include "object/sp-image.h"
#include "object/sp-item.h"
#include "selection.h"

namespace Inkscape {
namespace Extension {
namespace Internal {

bool DetectText::load(Inkscape::Extension::Extension * /*module*/)
{
    return TRUE;
}

Gtk::Widget *DetectText::updateGUI(std::string detectedtext)
{
    static Gtk::Label *detectedtextlabel = Gtk::manage(new Gtk::Label("", Gtk::ALIGN_START));
    detectedtextlabel->set_label(detectedtext);
    return detectedtextlabel;
}

void DetectText::effect(Inkscape::Extension::Effect *module, Inkscape::UI::View::View *view,
                        Inkscape::Extension::Implementation::ImplementationDocumentCache *docCache)
{
    auto desktop = dynamic_cast<SPDesktop *>(view);
    if (!desktop) {
        std::cerr << "DetectText::effect: view is not desktop!" << std::endl;
        return;
    }
    Inkscape::Selection *selection = desktop->getSelection();

    std::vector<SPItem *> items(selection->items().begin(), selection->items().end());
    selection->clear();
    std::string detectedtext = "";

    for (auto spitem : items) {
        Inkscape::XML::Node *node = reinterpret_cast<Inkscape::XML::Node *>(spitem->getRepr());
        if (!strcmp(node->name(), "image") || !strcmp(node->name(), "svg:image")) {
            SPImage *spimage = dynamic_cast<SPImage *>(spitem);

            tesseract::TessBaseAPI *tess = new tesseract::TessBaseAPI();
            char *text;
            if (tess->Init(NULL, "eng")) {
                std::cerr << "Could not initialize tesseract!" << std::endl;
                return;
            }

            std::shared_ptr<Inkscape::Pixbuf const> pixbuf = spimage->pixbuf;
            tess->SetImage(pixbuf->pixels(), pixbuf->width(), pixbuf->height(), pixbuf->rowstride() / pixbuf->width(),
                           pixbuf->rowstride());
            text = tess->GetUTF8Text();
            detectedtext.append(text);
            detectedtext.append("\n");
            tess->End();
            delete tess;
            delete[] text;
        }
    }
    // std::cout << detectedtext << std::endl;
    updateGUI(detectedtext);
    return;
}

Gtk::Widget *DetectText::prefs_effect(Inkscape::Extension::Effect *module, Inkscape::UI::View::View * /*view*/,
                                      sigc::signal<void> *changeSignal,
                                      Inkscape::Extension::Implementation::ImplementationDocumentCache * /*docCache*/)
{
    Gtk::Widget *detectedtext = updateGUI("The Detected Text will appear here");

    Gtk::Widget *guitext = Gtk::manage(new Gtk::Label("Detected Text: ", Gtk::ALIGN_START));
    Gtk::Widget *separatortop = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    Gtk::Widget *separatorbottom = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    guitext->show();
    detectedtext->show();
    separatortop->show();
    separatorbottom->show();

    Gtk::Box *gui = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    gui->set_border_width(InxParameter::GUI_BOX_MARGIN);
    gui->set_spacing(InxParameter::GUI_BOX_SPACING);

    gui->pack_start(*guitext, false, true, 0);
    gui->pack_start(*separatortop, false, true, 0);
    gui->pack_start(*detectedtext, false, true, 0);
    gui->pack_start(*separatorbottom, false, true, 0);

    gui->show();
    return gui;
}

#include "clear-n_.h"

void DetectText::init()
{
    // clang-format off
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>" N_("Detect Text") "</name>\n"
            "<id>org.inkscape.effect.detecttext</id>\n"
            "<param name=\"detected-text\" gui-text=\"" N_("Detected Text:") "\" gui-description=\"" N_("Text detected in the current Inkscape Document") "\" type=\"string\">Here the detected text will be displayed</param>\n"
            "<effect>\n"
                "<object-type>all</object-type>\n"
                "<effects-menu>\n"
                    "<submenu name=\"" N_("Text") "\" />\n"
                "</effects-menu>\n"
            "</effect>\n"
        "</inkscape-extension>\n" , new DetectText());
    // clang-format on
    return;
}

}; /* namespace Internal */
}; /* namespace Extension */
}; /* namespace Inkscape */

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
