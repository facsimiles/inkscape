//
// Created by Michael Kowalski on 6/18/24.
//

#include "color-picker-metadata.h"

#include <glib/gi18n.h>

namespace Inkscape::Util::ColorPicker {

using namespace Colors::Space;

static RainbowSlider hsv_rainbow = { Type::HSV, Type::HSV, 1.0, 0, 1, 2 };

static std::vector<ColorPickerData> data = {
    { _("RGB"), "color-selector-rgb", Type::RGB, {
        { _("R"), {0, 100} }, { _("G"), {0, 100} }, { _("B"), {0, 100} } },
        hsv_rainbow
    },
    { _("HSL"), "color-selector-hsx", Type::HSL, {
        { _("H"), {0, 360} }, { _("S"), {0, 100} }, { _("L"), {0, 100} } },
        { Type::HSV, Type::HSL, 1.0, 0, 1, 2 }
    },
    { _("HSV"), "color-selector-hsx", Type::HSV, {
        { _("H"), {0, 360} }, { _("S"), {0, 100} }, { _("V"), {0, 100} } },
        hsv_rainbow
    },
    { _("OKHSL"), "color-selector-okhsl", Type::OKHSL, {
        { _("H"), {0, 360} }, { _("S"), {0, 100} }, { _("L"), {0, 100} } },
        { Type::OKHSL, Type::OKHSL, 0.6, 0, 1, 2 }
        // { Type::OKLCH, Type::OKLCH, 0.6, 2, 1, 0 }
    },
    { _("CMYK"), "color-selector-cmyk", Type::CMYK, {
        { _("C"), {0, 100} }, { _("M"), {0, 100} }, { _("Y"), {0, 100} }, { _("K"), {0, 100} } },
        hsv_rainbow
    }
};

const std::vector<ColorPickerData>& get_color_picker_metadata() {
    return data;
}

const ColorPickerData& get_color_picker_data_for_type(Colors::Space::Type type) {
    auto it = std::find_if(begin(data), end(data), [=](auto& el){ return el.type == type; });
    if (it != end(data)) return *it;

    g_warning("Missing metadata for color picker space type %d\n", type);
    static ColorPickerData empty{"", "", Type::NONE, {}, {}};
    return empty;
}

}
