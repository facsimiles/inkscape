// SPDX-License-Identifier: GPL-2.0-or-later

#include <glibmm.h>
#include <fstream>

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

namespace {

const char *IGNORED_WORDS_FILE = "inkscape/ignored-words.txt";
const char *ACCEPTED_WORDS_FILE = "inkscape/accepted-words.txt";

constexpr uint32_t SQUIGGLE_COLOR_RED = 0xff0000ff; // Color for squiggles

std::string get_user_ignore_path()
{
    return Glib::build_filename(Glib::get_user_config_dir(), IGNORED_WORDS_FILE);
}

std::string get_user_dict_path()
{
    return Glib::build_filename(Glib::get_user_config_dir(), ACCEPTED_WORDS_FILE);
}

void ensure_file_exists(const std::string& path)
{
    std::ofstream f(path, std::ios::app);
}

static inline double safe_pow(double x, double p) {
    return std::pow(x < 0.0 ? 0.0 : x, p);
}

Geom::Point catmull_rom_centripetal(const Geom::Point &P0,
                                    const Geom::Point &P1,
                                    const Geom::Point &P2,
                                    const Geom::Point &P3,
                                    double u,
                                    double alpha = 0.5)
{
    // knot parameters
    double t0 = 0.0;
    double t1 = t0 + safe_pow(Geom::distance(P0, P1), alpha);
    double t2 = t1 + safe_pow(Geom::distance(P1, P2), alpha);
    double t3 = t2 + safe_pow(Geom::distance(P2, P3), alpha);

    // avoid degenerate denominators
    const double eps = 1e-8;
    if (t1 - t0 < eps) t1 = t0 + eps;
    if (t2 - t1 < eps) t2 = t1 + eps;
    if (t3 - t2 < eps) t3 = t2 + eps;

    // tangents (Hermite form)
    Geom::Point m1 = ((P2 - P1) / (t2 - t1) - (P1 - P0) / (t1 - t0)) * ((t2 - t1) / (t2 - t0));
    Geom::Point m2 = ((P3 - P2) / (t3 - t2) - (P2 - P1) / (t2 - t1)) * ((t2 - t1) / (t3 - t1));

    // Hermite basis
    double u2 = u * u;
    double u3 = u2 * u;
    double h1 =  2.0*u3 - 3.0*u2 + 1.0;
    double h2 = -2.0*u3 + 3.0*u2;
    double h3 =      u3 - 2.0*u2 + u;
    double h4 =      u3 -     u2;

    return P1 * h1 + P2 * h2 + m1 * h3 + m2 * h4;
}

}


OnCanvasSpellCheck::OnCanvasSpellCheck(SPDesktop *desktop)
    : _desktop(desktop)
{
    // Get the current document root
    auto doc = desktop->getDocument();
    if (!doc) return;
    _root = static_cast<SPObject*>(doc->getRoot());

    // Get the default spelling provider
    _provider = spelling_provider_get_default();

    // Choose a language (for example, the first available)
    auto prefs = Inkscape::Preferences::get();
    _lang_code = prefs->getString("/dialogs/spellcheck/live_lang", "en_US");

    // Create the checker
    _checker = GObjectPtr(spelling_checker_new(_provider, _lang_code.c_str()));

    // Open Ignored Words File and add ignored words to checker
    ensure_file_exists(get_user_ignore_path());
    std::ifstream ignore_file(get_user_ignore_path());
    if (ignore_file.is_open()) {
        std::string word;
        while (std::getline(ignore_file, word)) {
            if (!word.empty()) {
                _ignored_words.push_back(word);
                spelling_checker_ignore_word(_checker.get(), word.c_str());
            }
        }
        ignore_file.close();
    }

    // Open Accepted Words File and add accepted words to checker
    ensure_file_exists(get_user_dict_path());
    std::ifstream dict_file(get_user_dict_path());
    if (dict_file.is_open()) {
        std::string word;
        while (std::getline(dict_file, word)) {
            if (!word.empty()) {
                _added_words.push_back(word);
                spelling_checker_add_word(_checker.get(), word.c_str());
            }
        }
        dict_file.close();
    }

    // If live spellcheck is enabled, start scanning the document
    if(prefs->getBool("/dialogs/spellcheck/live", false)) {   
        scanDocument();
    }
}

void OnCanvasSpellCheck::allTextItems(SPObject *root, std::vector<SPItem *> &list, bool hidden, bool locked)
{
    if (is<SPDefs>(root) || !std::strcmp(root->getRepr()->name(), "svg:metadata")) {
        return; // we're not interested in items in defs and metadata
    }

    if (_desktop) {
        for (auto &child: root->children) {
            if (auto item = cast<SPItem>(&child)) {
                if (!child.cloned && !_desktop->layerManager().isLayer(item)) {
                    if ((hidden || !_desktop->itemIsHidden(item)) && (locked || !item->isLocked())) {
                        if (is<SPText>(item) || is<SPFlowtext>(item)) {
                            list.push_back(item);
                        }
                    }
                }
            }
            allTextItems(&child, list, hidden, locked);
        }
    }
}

void OnCanvasSpellCheck::scanDocument()
{
    // Clear any previously tracked items
    _tracked_items.clear();

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
    //Delete Item from TrackedTextItems if it exists
    auto it_item = std::find_if(_tracked_items.begin(), _tracked_items.end(),
                             [item](const TrackedTextItem& tracked) { return tracked.item == item; });
    if (it_item != _tracked_items.end()) {
        // Disconnect connections for the item
        it_item->modified_connection.disconnect();
        it_item->release_connection.disconnect();
        _tracked_items.erase(it_item);
    }
    // Create a new tracked item
    TrackedTextItem tracked_item;
    tracked_item.item = item;

    // Scaning the item for misspelled words
    std::vector <MisspelledWord> misspelled_words;
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

        // Try to link with the next word if separated by an apostrophe
        {
            SPObject *char_item = nullptr;
            Glib::ustring::iterator text_iter;
            layout->getSourceOfCharacter(end, &char_item, &text_iter);
            if (char_item && is<SPString>(char_item)) {
                int ch = *text_iter;
                if (ch == '\'' || ch == 0x2019) {
                    auto tempEnd = end;
                    tempEnd.nextCharacter();
                    layout->getSourceOfCharacter(tempEnd, &char_item, &text_iter);
                    if (char_item && is<SPString>(char_item)) {
                        int nextChar = *text_iter;
                        if (g_ascii_isalpha(nextChar))
                            end.nextEndOfWord();
                    }
                }
            }
        }

        Glib::ustring _word = sp_te_get_string_multiline(item, begin, end);

        // Check if word is empty
        if(_word.empty())
        {
            it = end;
            continue;
        }

        auto prefs = Inkscape::Preferences::get();

        // Skip words containing digits
        if(prefs->getInt("/dialogs/spellcheck/ignorenumbers") != 0 &&
            std::any_of(_word.begin(), _word.end(), [](gchar c) { return g_unichar_isdigit(c); })) {
            it = end;
            continue;
        }

        // Skip ALL-CAPS words
        if(prefs->getInt("/dialogs/spellcheck/ignoreallcaps") != 0 &&
            std::all_of(_word.begin(), _word.end(), [](gchar c) { return g_unichar_isupper(c); })) {
            it = end;
            continue;
        }

        if (!_word.empty() && !spelling_checker_check_word(_checker.get(), _word.c_str(), _word.length())) {
            std::cout<<"Misspelled word: " << _word << std::endl;
            // auto squiggle = createSquiggle(item, _word, begin, end);
            // _misspelled_words.push_back({item, _word, begin, end, std::move(squiggle)});
            misspelled_words.push_back(MisspelledWord{_word, begin, end, {}});
            createSquiggle(misspelled_words.back(), item, layout);
        }
        it = end;
    }

    // Add the misspelled words to the tracked item
    tracked_item.misspelled_words = std::move(misspelled_words);

    // Add signals for the item
    tracked_item.modified_connection = item->connectModified(
        [this, item] (auto, auto) { 
            auto it = std::find_if(_tracked_items.begin(), _tracked_items.end(),
                               [item](const TrackedTextItem& tracked) { return tracked.item == item; });
            if (it != _tracked_items.end()) {
                onObjModified(*it);
            }
        }
    );
    tracked_item.release_connection = item->connectRelease(
        [this, item] (auto) {
            auto it = std::find_if(_tracked_items.begin(), _tracked_items.end(),
                               [item](const TrackedTextItem& tracked) { return tracked.item == item; });
            if (it != _tracked_items.end()) {
                onObjReleased(*it);
            }
        }
    );

    // Add the tracked item to the list of tracked items
    _tracked_items.push_back(std::move(tracked_item));

}

void OnCanvasSpellCheck::createSquiggle(MisspelledWord& misspelled, SPItem* item, const Text::Layout* layout)
{
    if (!layout) {
        return; // No layout available
    }
    // Get the selection shape (bounding box) for the word
    auto points = layout->createSelectionShape(misspelled.begin, misspelled.end, item->i2dt_affine());
    if (points.size() < 4) {
        return; // Not enough points to draw a squiggle
    }

    // Initially get the bottom left and bottom right points
    std::vector<Geom::Point> squiggle_points_temp;
    for(int i=0; i<points.size()-3; i+=4)
    {
        squiggle_points_temp.push_back(points[i+3]);
        squiggle_points_temp.push_back(points[i+2]);
    }

    // Replace Bottom Left and Bottom Right points with midpoints to create smooth squiggle
    std::vector<Geom::Point> squiggle_points_temp2;
    squiggle_points_temp2.push_back(squiggle_points_temp[0]);

    for(int i=1; i<squiggle_points_temp.size()-1; i+=2)
    {
        squiggle_points_temp2.push_back((squiggle_points_temp[i]+squiggle_points_temp[i+1])/2);
    }
    squiggle_points_temp2.push_back(squiggle_points_temp.back());

    std::vector<Geom::Point> squiggle_points;
    // Create Catmull-Rom spline points for smooth squiggle
    squiggle_points.push_back(squiggle_points_temp2[0]);
    for(int i=1; i<squiggle_points_temp2.size()-2;i++)
    {
        squiggle_points.push_back(squiggle_points_temp2[i]);
        squiggle_points.push_back(catmull_rom_centripetal(
            squiggle_points_temp2[i-1], squiggle_points_temp2[i], squiggle_points_temp2[i+1], squiggle_points_temp2[i+2], 0.5));
    }
    squiggle_points.push_back(squiggle_points_temp2[squiggle_points_temp2.size()-2]);
    squiggle_points.push_back(squiggle_points_temp2.back());
    
    // Create the squiggle segments between the points
    for(int i=0; i<squiggle_points.size()-1; i++) {
        misspelled.squiggle.push_back( CanvasItemPtr<CanvasItemSquiggle>(
            new Inkscape::CanvasItemSquiggle(_desktop->getCanvasSketch(), squiggle_points[i], squiggle_points[i+1])
        ));
        misspelled.squiggle.back()->set_color(SQUIGGLE_COLOR_RED);
        misspelled.squiggle.back()->set_visible(true);
    }
}

void OnCanvasSpellCheck::onObjModified(TrackedTextItem &tracked_item)
{
    // When an object is modified, we need to re-check it for misspelled words
    // This will remove the old squiggles and create new ones if necessary
    checkTextItem(tracked_item.item);
}

void OnCanvasSpellCheck::onObjReleased(TrackedTextItem &tracked_item)
{
    // When an object is released, we can remove it from the tracked items
    auto it = std::find_if(_tracked_items.begin(), _tracked_items.end(),
                             [&tracked_item](const TrackedTextItem& tracked) { return tracked.item == tracked_item.item; });
    if (it != _tracked_items.end()) {
        // Disconnect connections for the item
        it->modified_connection.disconnect();
        it->release_connection.disconnect();
        _tracked_items.erase(it);
    }
}

void OnCanvasSpellCheck::addTrackedItem(SPItem* item)
{
    checkTextItem(item);
}

bool OnCanvasSpellCheck::isMisspelled(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end) const
{
    // Check if the word is misspelled in the tracked items
    auto it = std::find_if(_tracked_items.begin(), _tracked_items.end(),
                           [item](const TrackedTextItem& tracked) { return tracked.item == item; });
    if (it != _tracked_items.end()) {
        for (const auto& misspelled : it->misspelled_words) {
            if (misspelled.begin == begin && misspelled.end == end) {
                return true; // Found a match
            }
        }
    }
    return false; // Not found
}

std::vector<Glib::ustring> OnCanvasSpellCheck::getCorrections(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end) const
{
    if(!isMisspelled(item, begin, end))
    {
        return {}; // No corrections if the word is not misspelled
    }

    auto word = sp_te_get_string_multiline(item, begin, end);

    auto corrections = list_corrections(_checker.get(), word.c_str());

    return corrections;
    
}

void OnCanvasSpellCheck::replaceWord(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end, const Glib::ustring &replacement)
{
    // Replace the word in the text item
    sp_te_replace(item, begin, end, replacement.c_str());

    // After replacing, we need to re-check the item for misspelled words
    checkTextItem(item);
}

void OnCanvasSpellCheck::ignoreOnce(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end)
{
    // Ignore the word once
    auto it = std::find_if(_tracked_items.begin(), _tracked_items.end(),
                           [item](const TrackedTextItem& tracked) { return tracked.item == item; });
    if (it != _tracked_items.end()) {

        auto mispelled_it = std::find_if(it->misspelled_words.begin(), it->misspelled_words.end(),
                                 [begin, end](const MisspelledWord& misspelled) {
                                     return misspelled.begin == begin && misspelled.end == end;
                                 });
        if (mispelled_it != it->misspelled_words.end()) {
            // Remove the squiggle
            if (mispelled_it->squiggle.size() > 0) {
                for(auto &squiggle_part: mispelled_it->squiggle) {
                    squiggle_part->set_visible(false);
                }
                mispelled_it->squiggle.clear();
            }
            // Remove the misspelled word from the list
            it->misspelled_words.erase(mispelled_it);
        }
    }
}

void OnCanvasSpellCheck::ignore(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end)
{
    // Ignore the word permanently
    auto word = sp_te_get_string_multiline(item, begin, end);
    if (std::find(_ignored_words.begin(), _ignored_words.end(), word) == _ignored_words.end()) {
        _ignored_words.push_back(word);
        spelling_checker_ignore_word(_checker.get(), word.c_str());

        // Save the ignored word to the file
        std::ofstream ignore_file(get_user_ignore_path(), std::ios::app);
        if (ignore_file.is_open()) {
            ignore_file << word << std::endl;
            ignore_file.close();
        }
    }

    scanDocument(); // Re-scan the document to update the squiggles
}

void OnCanvasSpellCheck::addToDictionary(SPItem *item, Text::Layout::iterator begin, Text::Layout::iterator end)
{
    // Add the word to the dictionary
    auto word = sp_te_get_string_multiline(item, begin, end);
    if (std::find(_added_words.begin(), _added_words.end(), word) == _added_words.end()) {
        _added_words.push_back(word);
        spelling_checker_add_word(_checker.get(), word.c_str());

        // Save the accepted word to the file
        std::ofstream dict_file(get_user_dict_path(), std::ios::app);
        if (dict_file.is_open()) {
            dict_file << word << std::endl;
            dict_file.close();
        }
    }

    scanDocument();
}

OnCanvasSpellCheck::~OnCanvasSpellCheck() = default;

}