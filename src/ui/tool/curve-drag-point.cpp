// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2009 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/tool/curve-drag-point.h"

#include <glib/gi18n.h>

#include "desktop.h"
#include "object/sp-namedview.h"
#include "ui/modifiers.h"
#include "ui/tool/control-point-selection.h"
#include "ui/tool/multi-path-manipulator.h"
#include "ui/tool/path-manipulator.h"
#include "ui/widget/events/canvas-event.h"

namespace Inkscape {
namespace UI {


bool CurveDragPoint::_drags_stroke = false;
bool CurveDragPoint::_segment_was_degenerate = false;

CurveDragPoint::CurveDragPoint(PathManipulator &pm) :
    ControlPoint(pm._multi_path_manipulator._path_data.node_data.desktop, Geom::Point(), SP_ANCHOR_CENTER,
                 Inkscape::CANVAS_ITEM_CTRL_TYPE_INVISIPOINT,
                 pm._multi_path_manipulator._path_data.dragpoint_group),
      _pm(pm)
{
    _canvas_item_ctrl->set_name("CanvasItemCtrl:CurveDragPoint");
    setVisible(false);
}

bool CurveDragPoint::_eventHandler(Inkscape::UI::Tools::ToolBase *event_context, CanvasEvent const &event)
{
    // do not process any events when the manipulator is empty
    if (_pm.empty()) {
        setVisible(false);
        return false;
    }
    return ControlPoint::_eventHandler(event_context, event);
}

void CurveDragPoint::setIterator(NodeList::iterator iterator)
{
    if (iterator == NodeList::iterator()) {
        _curve_event_handler.reset();
        return;
    }
    if (iterator == first) {
        return;
    }
    first = iterator;
    _curve_event_handler.reset();
    if (!first) {
        return;
    }

    if (auto end_node = first.next()) {
        _curve_event_handler = end_node->createEventHandlerForPrecedingCurve();
    }
}

bool CurveDragPoint::grabbed(MotionEvent const &/*event*/)
{
    _pm._selection.hideTransformHandles();
    NodeList::iterator second = first.next();

    if (_curve_event_handler) {
        _curve_event_handler->pointGrabbed(first, second);
    }
    return false;
}

void CurveDragPoint::_adjustPointToSnappedPosition(Geom::Point &point_to_adjust, CanvasEvent const &event,
                                                   SPItem const &item_to_ignore) const
{
    if (!is_drag_initiated()) {
        return;
    }
    if (auto const *snap_inhibitor = Modifiers::Modifier::get(Modifiers::Type::MOVE_SNAPPING);
        snap_inhibitor && snap_inhibitor->active(event.modifiers)) {
        return;
    }

    auto &snap_manager = _desktop->getNamedView()->snap_manager;
    snap_manager.setup(_desktop, true, &item_to_ignore);
    const SnapCandidatePoint candidate_point{point_to_adjust, Inkscape::SNAPSOURCE_OTHER_HANDLE};
    point_to_adjust = snap_manager.freeSnap(candidate_point, {}, false).getPoint();
    snap_manager.unSetup();
}

void CurveDragPoint::dragged(Geom::Point &new_pos, MotionEvent const &event)
{
    if (!first || !first.next()) return;
    NodeList::iterator second = first.next();

    const auto *path_item = cast<SPItem>(_pm._path);
    if (!_curve_event_handler || !path_item) {
        return;
    }

    _adjustPointToSnappedPosition(new_pos, event, *path_item);
    if (_curve_event_handler->pointDragged(first, second, _t, position(), new_pos, event)) {
        _pm.update();
    }
}

void CurveDragPoint::ungrabbed(ButtonReleaseEvent const *)
{
    _pm._updateDragPoint(_desktop->d2w(position()));
    _pm._commit(_("Drag curve"));
    _pm._selection.restoreTransformHandles();
}

bool CurveDragPoint::clicked(ButtonReleaseEvent const &event)
{
    // This check is probably redundant
    if (!first || event.button != 1) return false;
    // the next iterator can be invalid if we click very near the end of path
    NodeList::iterator second = first.next();
    if (!second) return false;

    // insert nodes on Ctrl+Alt+click
    if (held_ctrl(event) && held_alt(event)) {
        _insertNode(false);
        return true;
    }

    if (held_shift(event)) {
        // if both nodes of the segment are selected, deselect;
        // otherwise add to selection
        if (first->selected() && second->selected())  {
            _pm._selection.erase(first.ptr());
            _pm._selection.erase(second.ptr());
        } else {
            _pm._selection.insert(first.ptr());
            _pm._selection.insert(second.ptr());
        }
    } else {
        // without Shift, take selection
        _pm._selection.clear();
        _pm._selection.insert(first.ptr(), false, false);
        _pm._selection.insert(second.ptr());
    }
    return true;
}

bool CurveDragPoint::doubleclicked(ButtonReleaseEvent const &event)
{
    if (event.button != 1 || !first || !first.next()) return false;
    if (held_ctrl(event)) {
        _pm.deleteSegments();
        _pm.update(true);
        _pm._commit(_("Remove segment"));
    } else if (held_alt(event)) {
        _pm.setSegmentType(Inkscape::UI::SEGMENT_STRAIGHT);
        _pm.update(true);
        _pm._commit(_("Straighten segments"));
    } else {
        _pm._updateDragPoint(_desktop->d2w(position()));
        _insertNode(true);
    }
    return true;
}

void CurveDragPoint::_insertNode(bool take_selection)
{
    // The purpose of this call is to make way for the just created node.
    // Otherwise clicks on the new node would only work after the user moves the mouse a bit.
    // PathManipulator will restore visibility when necessary.
    setVisible(false);

    _pm.insertNode(first, _t, take_selection);
}

Glib::ustring CurveDragPoint::_getTip(unsigned state) const
{
    if (_pm.empty() || !first || !first.next() || !_curve_event_handler) {
        return {};
    }
    return _curve_event_handler->getTooltip(state, first);
}

} // namespace UI
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
