// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INKSCAPE_UI_ON_CANVAS_SPELLCHECK_H
#define INKSCAPE_UI_ON_CANVAS_SPELLCHECK_H

// #include "ui/libspelling-wrapper.h"

#include <string>
#include <vector>

#include <libspelling.h>

#include "text-editing.h"
#include "util/gobjectptr.h"
#include "display/control/canvas-item-ptr.h"
#include <sigc++/scoped_connection.h>
#include "display/control/canvas-item-squiggle.h"

class SPObject;
class SPItem;

namespace Inkscape {
class Preferences;
class CanvasItemSquiggle;
} //namespace Inkscape


namespace Inkscape::UI {

struct MisspelledWord {
    Glib::ustring word;
    Text::Layout::iterator begin;
    Text::Layout::iterator end;
    std::vector< CanvasItemPtr<CanvasItemSquiggle> > squiggle;

    // Constructor for initialization
    MisspelledWord(Glib::ustring w, Text::Layout::iterator b, Text::Layout::iterator e,
                   std::vector<CanvasItemPtr<CanvasItemSquiggle>> s = {})
        : word(std::move(w)), begin(b), end(e), squiggle(std::move(s)) {}

    // Move-only
    MisspelledWord() = default;
    MisspelledWord(MisspelledWord&&) = default;
    MisspelledWord& operator=(MisspelledWord&&) = default;
    MisspelledWord(const MisspelledWord&) = delete;
    MisspelledWord& operator=(const MisspelledWord&) = delete;
};

struct TrackedTextItem {
    SPItem* item;
    sigc::scoped_connection modified_connection;
    sigc::scoped_connection release_connection;
    std::vector<MisspelledWord> misspelled_words;
};

class OnCanvasSpellCheck
{
public:
    OnCanvasSpellCheck(SPDesktop *desktop);
    ~OnCanvasSpellCheck();

private:
    SPObject *_root = nullptr;
    SPDesktop *_desktop = nullptr;

    SpellingProvider* _provider = nullptr;

    Util::GObjectPtr<SpellingChecker> _checker;

    std::string _lang_code;

    std::vector<TrackedTextItem> _tracked_items;

    std::vector<Glib::ustring> _ignored_words;

    std::vector<Glib::ustring> _added_words;

    // Get all text items in the document
    void allTextItems(SPObject *r, std::vector<SPItem *> &l, bool hidden, bool locked);

    // Scanning the document for misspelled words
    void scanDocument();

    // Check a specific text item for misspelled words
    void checkTextItem(SPItem* item);

    // Create a squiggle for a misspelled word
    void createSquiggle(MisspelledWord& misspelled, SPItem* item, const Text::Layout* layout);

    // Object Modified handler
    void onObjModified(TrackedTextItem &tracked_item);

    // Object Released handler
    void onObjReleased(TrackedTextItem &tracked_item);

public:
    void addTrackedItem(SPItem* item);

    // Check is passed word exists in our MispelledWords vector
    bool isMisspelled(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end) const;

    // Get List of Correct Words for SpellCheck
    std::vector<Glib::ustring> getCorrections(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end) const;

    void replaceWord(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end, const Glib::ustring &replacement);

    void ignoreOnce(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end);

    void ignore(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end);

    void addToDictionary(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end);

};

} // namespace Inkscape::UI

#endif // INKSCAPE_UI_ON_CANVAS_SPELLCHECK_H