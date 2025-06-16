
#ifndef INKSCAPE_UI_ON_CANVAS_SPELLCHECK_H
#define INKSCAPE_UI_ON_CANVAS_SPELLCHECK_H

// #include "ui/libspelling-wrapper.h"

#include <string>
#include <vector>

#include <libspelling.h>

#include "text-editing.h"
#include "util/gobjectptr.h"
#include "display/control/canvas-item-ptr.h"

class SPObject;
class SPItem;

namespace Inkscape {
class Preferences;
class CanvasItemSquiggle;
} //namespace Inkscape


namespace Inkscape::UI {

struct MisspelledWord {
    SPItem* item;
    Glib::ustring word;
    Text::Layout::iterator begin;
    Text::Layout::iterator end;
    CanvasItemPtr<CanvasItemSquiggle> squiggle;
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

    std::vector<MisspelledWord> _misspelled_words;

    void allTextItems(SPObject *r, std::vector<SPItem *> &l, bool hidden, bool locked);
    void scanDocument();
    void checkTextItem(SPItem* item);

    void createSquiggle(MisspelledWord& misspelled);
};

} // namespace Inkscape::UI

#endif // INKSCAPE_UI_ON_CANVAS_SPELLCHECK_H