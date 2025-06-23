
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
    CanvasItemPtr<CanvasItemSquiggle> squiggle;
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
    OnCanvasSpellCheck();
    ~OnCanvasSpellCheck();

private:
    SPObject *_root = nullptr;
    SPDesktop *_desktop = nullptr;

    SpellingProvider* _provider = nullptr;

    Util::GObjectPtr<SpellingChecker> _checker;

    Preferences &_prefs;

    std::string _lang_code;

    std::vector<TrackedTextItem> _tracked_items;

    // Get all text items in the document
    void allTextItems(SPObject *r, std::vector<SPItem *> &l, bool hidden, bool locked);

    // Scanning the document for misspelled words
    void scanDocument();

    // Check a specific text item for misspelled words
    void checkTextItem(SPItem* item);

    // Create a squiggle for a misspelled word
    void createSquiggle(MisspelledWord& misspelled, SPItem* item);

    // Object Modified handler
    void onObjModified(TrackedTextItem &tracked_item);

    // Object Released handler
    void onObjReleased(TrackedTextItem &tracked_item);

public:
    void addTrackedItem(SPItem* item);

};

} // namespace Inkscape::UI

#endif // INKSCAPE_UI_ON_CANVAS_SPELLCHECK_H