// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Fatma Omara <ftomara647@gmail.com>
 *
 * Copyright (C) 2025 authors
 */

#include "recolor-art-manager.h"

#include "object/sp-gradient.h"
#include "object/sp-mask.h"
#include "object/sp-pattern.h"
#include "object/sp-use.h"
#include "style.h"

namespace Inkscape::UI::Widget {
namespace {

bool has_colors_pattern(SPItem const *item)
{
    std::set<std::string> colors;
    SPPattern *patternstroke = nullptr;
    SPPattern *patternfill = nullptr;
    SPPattern *pattern = nullptr;
    if (item && item->style) {
        patternstroke = cast<SPPattern>(item->style->getStrokePaintServer());
        patternfill = cast<SPPattern>(item->style->getFillPaintServer());
    }

    if (patternstroke)
        pattern = patternstroke;
    if (patternfill)
        pattern = patternfill;
    if (!pattern)
        return false;
    SPPattern *root = pattern->rootPattern();
    for (auto &child : root->children) {
        if (auto group = cast<SPGroup>(&child)) {
            for (auto &child : group->children) {
                if (auto c = dynamic_cast<SPItem *>(&child)) {
                    if (c->style->fill.isColor()) {
                        std::string rgba = c->style->fill.getColor().toString(true);
                        colors.insert(rgba);
                    }
                    if (c->style->stroke.isColor()) {
                        std::string rgba = c->style->stroke.getColor().toString(true);
                        colors.insert(rgba);
                    }
                }
            }
        }
        auto item = cast<SPItem>(&child);
        if (!item || !item->style)
            continue;

        if (item->style->fill.isColor()) {
            std::string rgba = item->style->fill.getColor().toString(true);
            colors.insert(rgba);
        }
        if (item->style->stroke.isColor()) {
            std::string rgba = item->style->stroke.getColor().toString(true);
            colors.insert(rgba);
        }
    }
    return colors.size() > 1;
}

} // namespace

RecolorArtManager &RecolorArtManager::get()
{
    static RecolorArtManager instance;
    return instance;
}

RecolorArtManager::RecolorArtManager()
{
    _recolorPopOver.set_autohide(false);
    _recolorPopOver.set_position(Gtk::PositionType::LEFT);
    _recolorPopOver.set_child(_recolor_widget);
}

bool RecolorArtManager::checkSelection(Inkscape::Selection *selection)
{
    auto group = cast<SPGroup>(selection->single());
    auto use_group = cast<SPUse>(selection->single());
    auto item = cast<SPItem>(selection->single());
    bool pattern_colors = false;
    SPMask *mask = nullptr;
    if (item) {
        mask = cast<SPMask>(item->getMaskObject());
        pattern_colors = has_colors_pattern(item);
    }
    return selection->size() > 1 || group || use_group || mask || pattern_colors;
}

bool RecolorArtManager::checkMeshObject(Inkscape::Selection *selection)
{
    if (selection->items().empty()) {
        return false;
    }
    auto fill_gradient = cast<SPPaintServer>(selection->single()->style->getFillPaintServer());
    auto stroke_gradient = cast<SPPaintServer>(selection->single()->style->getStrokePaintServer());
    SPGradient *gradient = fill_gradient ? cast<SPGradient>(fill_gradient) : cast<SPGradient>(stroke_gradient);
    return gradient && gradient->hasPatches();
}

void RecolorArtManager::setDesktop(SPDesktop *desktop)
{
    _recolor_widget.setDesktop(desktop);
}

void RecolorArtManager::performUpdate()
{
    _recolor_widget.performUpdate();
}

void RecolorArtManager::performMarkerUpdate(SPMarker *marker)
{
    _recolor_widget.performMarkerUpdate(marker);
}

Gtk::Popover &RecolorArtManager::getPopOver()
{
    return _recolorPopOver;
}

} // namespace Inkscape::UI::Widget
