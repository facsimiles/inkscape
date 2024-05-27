//
// Created by Michael Kowalski on 6/18/24.
//

#ifndef COLOR_PICKER_METADATA_H
#define COLOR_PICKER_METADATA_H

#include <vector>

#include "actions/actions-helper.h"
#include "colors/color.h"

namespace Inkscape::Util::ColorPicker {

struct ChannelRange {
    double lower;
    double upper;
};

struct ChannelData {
    // channel label, can be in markup
    Glib::ustring name;
    // range for a slider in the UI, like 0..360 for HSL
    ChannelRange range;
};

struct RainbowSlider {
    // which color space type to use to create a rainbow of available hues
    Colors::Space::Type rect;
    // which color space type to use to create a circle of available hues
    Colors::Space::Type circle;
    // at which lightness to render rainbow map
    double lightness;
    // which channel to vary to produce color map
    unsigned int hue_channel;
    // where is saturation
    unsigned int saturation_channel;
    // ...and lightness channel
    unsigned int lightness_channel;
};

struct ColorPickerData {
    // color space type name, like "RGB" or "OKHSL"
    Glib::ustring name;
    // name of the icon to use for this color space type
    Glib::ustring icon;
    // color space type for a picker
    Colors::Space::Type type;
    // channels in this space type
    std::vector<ChannelData> channels;
    // if color picker first slider shows a "rainbow" of all colors, this info is used
    RainbowSlider rainbow;
    //
};

const std::vector<ColorPickerData>& get_color_picker_metadata();

const ColorPickerData& get_color_picker_data_for_type(Colors::Space::Type type);

}

#endif //COLOR_PICKER_METADATA_H
