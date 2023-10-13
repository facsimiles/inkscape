// SPDX-License-Identifier: GPL-2.0-or-later
#include "ctrl-handle-styling.h"

#include <unordered_map>
#include <boost/functional/hash.hpp>
#include <glibmm/fileutils.h>
#include <glibmm/regex.h>

#include "3rdparty/libcroco/src/cr-selector.h"
#include "3rdparty/libcroco/src/cr-doc-handler.h"
#include "3rdparty/libcroco/src/cr-string.h"
#include "3rdparty/libcroco/src/cr-term.h"
#include "3rdparty/libcroco/src/cr-parser.h"
#include "3rdparty/libcroco/src/cr-rgb.h"
#include "3rdparty/libcroco/src/cr-utils.h"

#include "display/cairo-utils.h" // argb32_from_rgba()

#include "io/resource.h"

namespace Inkscape {
namespace {

/**
 * For Handle Styling (shared between all handles)
 */
std::unordered_map<Handle, HandleStyle> handle_styles;
bool parsed = false;

/**
 * Conversion maps for ctrl types (CSS parsing).
 */
std::unordered_map<std::string, CanvasItemCtrlType> const ctrl_type_map = {
    {"*", CANVAS_ITEM_CTRL_TYPE_DEFAULT},
    {".inkscape-adj-handle", CANVAS_ITEM_CTRL_TYPE_ADJ_HANDLE},
    {".inkscape-adj-skew", CANVAS_ITEM_CTRL_TYPE_ADJ_SKEW},
    {".inkscape-adj-rotate", CANVAS_ITEM_CTRL_TYPE_ADJ_ROTATE},
    {".inkscape-adj-center", CANVAS_ITEM_CTRL_TYPE_ADJ_CENTER},
    {".inkscape-adj-salign", CANVAS_ITEM_CTRL_TYPE_ADJ_SALIGN},
    {".inkscape-adj-calign", CANVAS_ITEM_CTRL_TYPE_ADJ_CALIGN},
    {".inkscape-adj-malign", CANVAS_ITEM_CTRL_TYPE_ADJ_MALIGN},
    {".inkscape-anchor", CANVAS_ITEM_CTRL_TYPE_ANCHOR},
    {".inkscape-point", CANVAS_ITEM_CTRL_TYPE_POINT},
    {".inkscape-rotate", CANVAS_ITEM_CTRL_TYPE_ROTATE},
    {".inkscape-margin", CANVAS_ITEM_CTRL_TYPE_MARGIN},
    {".inkscape-center", CANVAS_ITEM_CTRL_TYPE_CENTER},
    {".inkscape-sizer", CANVAS_ITEM_CTRL_TYPE_SIZER},
    {".inkscape-shaper", CANVAS_ITEM_CTRL_TYPE_SHAPER},
    {".inkscape-marker", CANVAS_ITEM_CTRL_TYPE_MARKER},
    {".inkscape-lpe", CANVAS_ITEM_CTRL_TYPE_LPE},
    {".inkscape-node-auto", CANVAS_ITEM_CTRL_TYPE_NODE_AUTO},
    {".inkscape-node-cusp", CANVAS_ITEM_CTRL_TYPE_NODE_CUSP},
    {".inkscape-node-smooth", CANVAS_ITEM_CTRL_TYPE_NODE_SMOOTH},
    {".inkscape-node-symmetrical", CANVAS_ITEM_CTRL_TYPE_NODE_SYMETRICAL},
    {".inkscape-mesh", CANVAS_ITEM_CTRL_TYPE_MESH},
    {".inkscape-invisible", CANVAS_ITEM_CTRL_TYPE_INVISIPOINT}
};

/**
 * Conversion maps for ctrl shapes (CSS parsing).
 */
std::unordered_map<std::string, CanvasItemCtrlShape> const ctrl_shape_map = {
    {"'square'", CANVAS_ITEM_CTRL_SHAPE_SQUARE},
    {"'diamond'", CANVAS_ITEM_CTRL_SHAPE_DIAMOND},
    {"'circle'", CANVAS_ITEM_CTRL_SHAPE_CIRCLE},
    {"'triangle'", CANVAS_ITEM_CTRL_SHAPE_TRIANGLE},
    {"'triangle-angled'", CANVAS_ITEM_CTRL_SHAPE_TRIANGLE_ANGLED},
    {"'cross'", CANVAS_ITEM_CTRL_SHAPE_CROSS},
    {"'plus'", CANVAS_ITEM_CTRL_SHAPE_PLUS},
    {"'plus'", CANVAS_ITEM_CTRL_SHAPE_PLUS},
    {"'pivot'", CANVAS_ITEM_CTRL_SHAPE_PIVOT},
    {"'arrow'", CANVAS_ITEM_CTRL_SHAPE_DARROW},
    {"'skew-arrow'", CANVAS_ITEM_CTRL_SHAPE_SARROW},
    {"'curved-arrow'", CANVAS_ITEM_CTRL_SHAPE_CARROW},
    {"'side-align'", CANVAS_ITEM_CTRL_SHAPE_SALIGN},
    {"'corner-align'", CANVAS_ITEM_CTRL_SHAPE_CALIGN},
    {"'middle-align'", CANVAS_ITEM_CTRL_SHAPE_MALIGN}
};

/**
 * A global vector needed for parsing (between functions).
 */
std::vector<std::pair<HandleStyle *, int>> selected_handles;

/**
 * Parses the CSS selector for handles.
 */
std::optional<std::pair<Handle, int>> configure_selector(CRSelector *a_selector)
{
    cr_simple_sel_compute_specificity(a_selector->simple_sel);
    int specificity = a_selector->simple_sel->specificity;
    auto selector_str = reinterpret_cast<char const *>(cr_simple_sel_one_to_string(a_selector->simple_sel));
    std::vector<std::string> tokens = Glib::Regex::split_simple(":", selector_str);
    CanvasItemCtrlType type;
    int token_iterator = 0;
    if (auto it = ctrl_type_map.find(tokens[token_iterator]); it != ctrl_type_map.end()) {
        type = it->second;
        token_iterator++;
    } else {
        std::cerr << "Unrecognized/unhandled selector: " << selector_str << std::endl;
        return {};
    }
    auto selector = Handle{type};
    for (; token_iterator < tokens.size(); token_iterator++) {
        if (tokens[token_iterator] == "*") {
            continue;
        } else if (tokens[token_iterator] == "selected") {
            selector.selected = true;
        } else if (tokens[token_iterator] == "hover") {
            specificity++;
            selector.hover = true;
        } else if (tokens[token_iterator] == "click") {
            specificity++;
            selector. click = true;
        } else {
            std::cerr << "Unrecognized/unhandled selector: " << selector_str << std::endl;
            return {};
        }
    }

    return {{ selector, specificity }};
}

bool handle_fits(Handle const &selector, Handle const &handle)
{
    // Type must match for non-default selectors.
    if (selector.type != CANVAS_ITEM_CTRL_TYPE_DEFAULT && selector.type != handle.type) {
        return false;
    }
    // Any state set in selector must be set in handle.
    return !((selector.selected && !handle.selected) ||
             (selector.hover && !handle.hover) ||
             (selector.click && !handle.click));
}

/**
 * Selects fitting handles from all handles based on selector.
 */
void set_selectors(CRDocHandler *a_handler, CRSelector *a_selector, bool is_users)
{
    while (a_selector) {
        if (auto const ret = configure_selector(a_selector)) {
            auto const &[selector, specificity] = *ret;
            for (auto &[handle, style] : handle_styles) {
                if (handle_fits(selector, handle)) {
                    selected_handles.emplace_back(&style, specificity + 10000 * is_users);
                }
            }
        }
        a_selector = a_selector->next;
    }
}

template <bool is_users>
void set_selectors(CRDocHandler *a_handler, CRSelector *a_selector)
{
    set_selectors(a_handler, a_selector, is_users);
}

/**
 * Parse and set the properties for selected handles.
 */
void set_properties(CRDocHandler *a_handler, CRString *a_name, CRTerm *a_value, gboolean a_important)
{
    auto gvalue = cr_term_to_string(a_value);
    auto gproperty = cr_string_peek_raw_str(a_name);
    if (!gvalue || !gproperty) {
        std::cerr << "Empty or improper value or property, skipped." << std::endl;
        return;
    }
    std::string const value = reinterpret_cast<char *>(gvalue);
    std::string const property = gproperty;
    g_free(gvalue);
    if (property == "shape") {
        if (auto it = ctrl_shape_map.find(value); it != ctrl_shape_map.end()) {
            for (auto &[handle, specificity] : selected_handles) {
                handle->shape.setProperty(it->second, specificity + 100000 * a_important);
            }
        } else {
            std::cerr << "Unrecognized value for " << property << ": " << value << std::endl;
            return;
        }
    } else if (property == "fill" || property == "stroke" || property == "outline") {
        auto rgb = cr_rgb_new();
        CRStatus status = cr_rgb_set_from_term(rgb, a_value);

        if (status == CR_OK) {
            ASSEMBLE_ARGB32(color, 255, (uint8_t)rgb->red, (uint8_t)rgb->green, (uint8_t)rgb->blue)
            for (auto &[handle, specificity] : selected_handles) {
                if (property == "fill") {
                    handle->fill.setProperty(color, specificity + 100000 * a_important);
                } else if (property == "stroke") {
                    handle->stroke.setProperty(color, specificity + 100000 * a_important);
                } else { // outline
                    handle->outline.setProperty(color, specificity + 100000 * a_important);
                }
            }
        } else {
            std::cerr << "Unrecognized value for " << property << ": " << value << std::endl;
        }

        cr_rgb_destroy(rgb);
    } else if (property == "opacity" || property == "fill-opacity" ||
               property == "stroke-opacity" || property == "outline-opacity") {
        if (!a_value->content.num) {
            std::cerr << "Invalid value for " << property << ": " << value << std::endl;
            return;
        }

        double val;
        if (a_value->content.num->type == NUM_PERCENTAGE) {
            val = a_value->content.num->val / 100.0f;
        } else if (a_value->content.num->type == NUM_GENERIC) {
            val = a_value->content.num->val;
        } else {
            std::cerr << "Invalid type for " << property << ": " << value << std::endl;
            return;
        }

        if (val > 1 || val < 0) {
            std::cerr << "Invalid value for " << property << ": " << value << std::endl;
            return;
        }
        for (auto &[handle, specificity] : selected_handles) {
            if (property == "opacity") {
                handle->opacity.setProperty(val, specificity + 100000 * a_important);
            } else if (property == "fill-opacity") {
                handle->fill_opacity.setProperty(val, specificity + 100000 * a_important);
            } else if (property == "stroke-opacity") {
                handle->stroke_opacity.setProperty(val, specificity + 100000 * a_important);
            } else { // outline opacity
                handle->outline_opacity.setProperty(val, specificity + 100000 * a_important);
            }
        }
    } else if (property == "stroke-width" || property == "outline-width") {
        // Assuming px value only, which stays the same regardless of the size of the handles.
        int val;
        if (!a_value->content.num) {
            std::cerr << "Invalid value for " << property << ": " << value << std::endl;
            return;
        }
        if (a_value->content.num->type == NUM_LENGTH_PX) {
            val = int(a_value->content.num->val);
        } else {
            std::cerr << "Invalid type for " << property << ": " << value << std::endl;
            return;
        }

        for (auto &[handle, specificity] : selected_handles) {
            if (property == "stroke-width") {
                handle->stroke_width.setProperty(val, specificity + 100000 * a_important);
            } else {
                handle->outline_width.setProperty(val, specificity + 100000 * a_important);
            }
        }
    } else {
        std::cerr << "Unrecognized property: " << property << std::endl;
    }
}

/**
 * Clean-up for selected handles vector.
 */
void clear_selectors(CRDocHandler *a_handler, CRSelector *a_selector)
{
    selected_handles.clear();
}

/**
 * Parse and set handle styles from css.
 */
void parse_handle_styles()
{
    for (int type_i = CANVAS_ITEM_CTRL_TYPE_DEFAULT; type_i <= CANVAS_ITEM_CTRL_TYPE_INVISIPOINT; type_i++) {
        auto type = static_cast<CanvasItemCtrlType>(type_i);
        for (auto state_bits = 0; state_bits < 8; state_bits++) {
            bool selected = state_bits & (1<<2);
            bool hover = state_bits & (1<<1);
            bool click = state_bits & (1<<0);
            handle_styles[Handle{type, selected, hover, click}] = {};
        }
    }

    auto sac = cr_doc_handler_new();
    sac->property = set_properties;
    sac->end_selector = clear_selectors;

    auto parse = [&] (IO::Resource::Domain domain) {
        auto const css_path = IO::Resource::get_path_string(domain, IO::Resource::UIS, "node-handles.css");
        if (Glib::file_test(css_path, Glib::FILE_TEST_EXISTS)) {
            auto parser = cr_parser_new_from_file(reinterpret_cast<unsigned char const *>(css_path.c_str()), CR_ASCII);
            cr_parser_set_sac_handler(parser, sac);
            cr_parser_parse(parser);
            cr_parser_destroy(parser);
        }
    };

    sac->start_selector = set_selectors<false>;
    parse(IO::Resource::SYSTEM);

    sac->start_selector = set_selectors<true>;
    parse(IO::Resource::USER);

    cr_doc_handler_destroy(sac);
}

uint32_t combine_rgb_a(uint32_t rgb, float a)
{
    EXTRACT_ARGB32(rgb, _, r, g, b)
    return Display::AssembleARGB32(r, g, b, a * 255);
}

} // namespace

uint32_t HandleStyle::getFill() const
{
    return combine_rgb_a(fill(), fill_opacity() * opacity());
}

uint32_t HandleStyle::getStroke() const
{
    return combine_rgb_a(stroke(), stroke_opacity() * opacity());
}

uint32_t HandleStyle::getOutline() const
{
    return combine_rgb_a(outline(), outline_opacity() * opacity());
}

void ensure_handle_styles_parsed()
{
    [[unlikely]] if (!parsed) {
        parse_handle_styles();
        parsed = true;
    }
}

HandleStyle const *lookup_handle_style(Handle const &handle)
{
    assert(parsed);

    auto const it = handle_styles.find(handle);
    if (it == handle_styles.end()) {
        return nullptr;
    }

    return &it->second;
}

} // namespace Inkscape

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
