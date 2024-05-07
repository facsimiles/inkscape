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
#include <gtkmm.h>

namespace Inkscape {
namespace Extension {
namespace Internal {

bool DetectText::load(Inkscape::Extension::Extension * /*module*/)
{
    return TRUE;
}

Gtk::Widget *DetectText::textWidget()
{
    static Gtk::Label *detectedtextlabel =
        Gtk::manage(new Gtk::Label("The Detected Text will appear here", Gtk::Align::START));
    return detectedtextlabel;
}

Gtk::Widget *DetectText::languageWidget()
{
    static Gtk::ComboBoxText *languageComboBox = Gtk::manage(new Gtk::ComboBoxText());
    return languageComboBox;
}

void DetectText::loadLanguages()
{
    tesseract::TessBaseAPI *tess = new tesseract::TessBaseAPI();
    if (tess->Init(NULL, "eng")) {
        std::cerr << "Could not initialize tesseract!" << std::endl;
        return;
    }
    std::vector<std::string> languages;
    tess->GetAvailableLanguagesAsVector(&languages);
    Gtk::ComboBoxText *languageComboBox = dynamic_cast<Gtk::ComboBoxText *>(languageWidget());
    int languageCount = languages.size();
    for (int i = 0; i < languageCount; i++) {
        auto language = languages[i];
        languageComboBox->append(language, language);
    }
    languageComboBox->set_active_id("eng");
    tess->End();
    delete tess;
    return;
}

void DetectText::effect(Inkscape::Extension::Effect *module, SPDesktop *desktop,
                        Inkscape::Extension::Implementation::ImplementationDocumentCache * /*docCache*/)
{
    Inkscape::Selection *selection = desktop->getSelection();
    std::vector<SPItem *> items(selection->items().begin(), selection->items().end());
    selection->clear();

    Gtk::ComboBoxText *languageComboBox = dynamic_cast<Gtk::ComboBoxText *>(languageWidget());
    std::string detectedtext = "";

    for (auto spitem : items) {
        Inkscape::XML::Node *node = reinterpret_cast<Inkscape::XML::Node *>(spitem->getRepr());
        if (!strcmp(node->name(), "image") || !strcmp(node->name(), "svg:image")) {
            SPImage *spimage = dynamic_cast<SPImage *>(spitem);

            tesseract::TessBaseAPI *tess = new tesseract::TessBaseAPI();
            char *text;
            if (tess->Init(NULL, languageComboBox->get_active_text().c_str())) {
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
    Gtk::Label *detectedTextLabel = dynamic_cast<Gtk::Label *>(textWidget());
    detectedTextLabel->set_label(detectedtext);
    return;
}

Gtk::Widget *DetectText::prefs_effect(Inkscape::Extension::Effect *module, SPDesktop *desktop,
                                      sigc::signal<void> *changeSignal,
                                      Inkscape::Extension::Implementation::ImplementationDocumentCache * /*docCache*/)
{
    Gtk::Box *gui = Gtk::manage(new Gtk::Box(Gtk::Orientation::VERTICAL));
    gui->show();
    //gui->set_border_width(InxParameter::GUI_BOX_MARGIN);
    gui->set_spacing(InxParameter::GUI_BOX_SPACING);

    Gtk::Widget *selectLang = Gtk::manage(new Gtk::Label("Select Language:", Gtk::Align::START));
    selectLang->show();
    //gui->pack_start(*selectLang, false, true, 0);
    gui->append(*selectLang);

    Gtk::Widget *language = languageWidget();
    language->show();
    loadLanguages();
    //gui->pack_start(*language, false, true, 0);
    gui->append(*language);

    Gtk::Widget *detectedText = textWidget();
    detectedText->show();
    //gui->pack_start(*detectedText, false, true, 0);
    gui->append(*detectedText);

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
        "</inkscape-extension>\n" , std::make_unique<DetectText>());
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
