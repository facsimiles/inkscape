#include "on-canvas-spellcheck.h"
#include "inkscape.h"
#include "document.h"
#include "desktop.h"
#include "layer-manager.h"
#include "ui/libspelling-wrapper.h"
#include "ui/dialog/inkscape-preferences.h"
#include "object/sp-object.h"
#include "object/sp-root.h"
#include "object/sp-defs.h"
#include "object/sp-text.h"
#include "object/sp-flowtext.h"
#include "display/control/canvas-item-squiggle.h"



namespace Inkscape::UI {


OnCanvasSpellCheck::OnCanvasSpellCheck()
    : _prefs{*Preferences::get()}
{
    // Get the current document root
    auto doc = SP_ACTIVE_DOCUMENT;
    if (!doc) return;
    _root = static_cast<SPObject*>(doc->getRoot());

    // Get the active desktop
    auto desktop = SP_ACTIVE_DESKTOP;
    if (!desktop) return;
    _desktop = desktop;

    // Get the default spelling provider
    _provider = spelling_provider_get_default();

    // Choose a language (for example, the first available)
    list_language_names_and_codes(_provider,
        [&](auto name, auto code) { _lang_code = code; return false; }); // store first code

    // Create the checker
    _checker = GObjectPtr(spelling_checker_new(_provider, _lang_code.c_str()));

    scanDocument();
}

void OnCanvasSpellCheck::allTextItems(SPObject *r, std::vector<SPItem *> &l, bool hidden, bool locked)
{
    if (is<SPDefs>(r)) {
        return; // we're not interested in items in defs
    }

    if (!std::strcmp(r->getRepr()->name(), "svg:metadata")) {
        return; // we're not interested in metadata
    }

    if (_desktop) {
        for (auto &child: r->children) {
            if (auto item = cast<SPItem>(&child)) {
                if (!child.cloned && !_desktop->layerManager().isLayer(item)) {
                    if ((hidden || !_desktop->itemIsHidden(item)) && (locked || !item->isLocked())) {
                        if (is<SPText>(item) || is<SPFlowtext>(item)) {
                            l.push_back(item);
                        }
                    }
                }
            }
            allTextItems(&child, l, hidden, locked);
        }
    }
}

void OnCanvasSpellCheck::scanDocument()
{
    std::vector<SPItem*> items;
    // Uses similar logic as SpellCheck::allTextItems to collect all SPText/SPFlowText
    allTextItems(_root, items, false, true);
    // For each item:
    for (auto item : items) {
        checkTextItem(item);
    }
}

void OnCanvasSpellCheck::checkTextItem(SPItem* item)
{
    auto layout = te_get_layout(item);
    auto it = layout->begin();
    while (it != layout->end()) {
        if (!layout->isStartOfWord(it)) {
            it.nextStartOfWord();
            if (it == layout->end()) break;
        }
        auto begin = it;
        auto end = it;
        end.nextEndOfWord();
        Glib::ustring _word = sp_te_get_string_multiline(item, begin, end);
        if (!_word.empty() && !spelling_checker_check_word(_checker.get(), _word.c_str(), _word.length())) {
            std::cout<<"Misspelled word: " << _word << std::endl;
            // auto squiggle = createSquiggle(item, _word, begin, end);
            // _misspelled_words.push_back({item, _word, begin, end, std::move(squiggle)});
            _misspelled_words.push_back({item, _word, begin, end, nullptr});
            createSquiggle(_misspelled_words.back());
        }
        it = end;
    }
}

void OnCanvasSpellCheck::createSquiggle(MisspelledWord& misspelled)
{
    auto layout = te_get_layout(misspelled.item);
    if (!layout) {
        return; // No layout available
    }
    // Get the selection shape (bounding box) for the word
    auto points = layout->createSelectionShape(misspelled.begin, misspelled.end, misspelled.item->i2dt_affine());
    if (points.size() < 4) {
        return; // Not enough points to draw a squiggle
    }

    // Use the bottom left and bottom right corners for the squiggle
    Geom::Point start_doc = points[3]; // bottom left
    Geom::Point end_doc   = points[2]; // bottom right

    // Create the squiggle (in document coordinates)
    misspelled.squiggle = CanvasItemPtr<CanvasItemSquiggle>(
        new Inkscape::CanvasItemSquiggle(_desktop->getCanvasSketch(), start_doc, end_doc)
    );
    misspelled.squiggle->set_color(0xff0000ff);
    misspelled.squiggle->set_visible(true);
}


OnCanvasSpellCheck::~OnCanvasSpellCheck() = default;

}