// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Control point selection - stores a set of control points and applies transformations
 * to them
 */
/* Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2009 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_TOOL_CONTROL_POINT_SELECTION_H
#define INKSCAPE_UI_TOOL_CONTROL_POINT_SELECTION_H

#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <cstddef>
#include <sigc++/sigc++.h>
#include <2geom/forward.h>
#include <2geom/point.h>
#include <2geom/rect.h>
#include "ui/tool/commit-events.h"
#include "ui/tool/manipulator.h"
#include "ui/tool/node-types.h"
#include "snap-candidate.h"

class SPDesktop;

namespace Inkscape {
class CanvasItemGroup;
struct KeyPressEvent;
struct ButtonReleaseEvent;

namespace UI {
class TransformHandleSet;
class SelectableControlPoint;

class ControlPointSelection
    : public Manipulator
    , public sigc::trackable
{
public:
    ControlPointSelection(SPDesktop *d, Inkscape::CanvasItemGroup *th_group);
    ~ControlPointSelection() override;
    using set_type = std::unordered_set<SelectableControlPoint*>;
    using Set = set_type; // convenience alias

    using iterator = set_type::iterator;
    using const_iterator = set_type::const_iterator;
    using size_type = set_type::size_type;
    using value_type = SelectableControlPoint*;
    using key_type = SelectableControlPoint*;

    // size
    bool empty() const { return _points.empty(); }
    size_type size() const { return _points.size(); }

    // iterators
    iterator begin() { return _points.begin(); }
    const_iterator begin() const { return _points.begin(); }
    iterator end() { return _points.end(); }
    const_iterator end() const { return _points.end(); }

    // insert
    std::pair<iterator, bool> insert(const value_type& x, bool notify = true, bool to_update = true);
    template <class InputIterator>
    void insert(InputIterator first, InputIterator last) {
        for (; first != last; ++first) {
            insert(*first, false, false);
        }
        _update();
        signal_selection_changed.emit(std::vector<key_type>(first, last), true);
    }

    // erase
    void clear();
    void erase(iterator pos, bool to_update = true);
    size_type erase(const key_type& k, bool notify = true);
    void erase(iterator first, iterator last);

    // find
    iterator find(const key_type &k) {
        return _points.find(k);
    }

    // Sometimes it is very useful to keep a list of all selectable points.
    set_type const &allPoints() const { return _all_points; }
    set_type &allPoints() { return _all_points; }
    // ...for example in these methods. Another useful case is snapping.
    void selectAll();
    void selectArea(Geom::Path const &, bool invert = false);
    void invertSelection();
    void spatialGrow(SelectableControlPoint *origin, int dir);

    bool event(Inkscape::UI::Tools::ToolBase *tool, CanvasEvent const &event) override;

    void transform(Geom::Affine const &m);
    void align(Geom::Dim2 d, AlignTargetNode target = AlignTargetNode::MID_NODE);
    void distribute(Geom::Dim2 d);

    Geom::OptRect pointwiseBounds();
    Geom::OptRect bounds();
    std::optional<Geom::Point> firstSelectedPoint() const;

    bool transformHandlesEnabled() { return _handles_visible; }
    void showTransformHandles(bool v, bool one_node);
    // the two methods below do not modify the state; they are for use in manipulators
    // that need to temporarily hide the handles, for example when moving a node
    void hideTransformHandles();
    void restoreTransformHandles();
    void toggleTransformHandlesMode();

    sigc::signal<void ()> signal_update;
    // It turns out that emitting a signal after every point is selected or deselected is not too efficient,
    // so this can be done in a massive group once the selection is finally changed.
    sigc::signal<void (std::vector<SelectableControlPoint *>, bool)> signal_selection_changed;
    sigc::signal<void (CommitEvent)> signal_commit;

    void getOriginalPoints(std::vector<Inkscape::SnapCandidatePoint> &pts);
    void getUnselectedPoints(std::vector<Inkscape::SnapCandidatePoint> &pts);
    void setOriginalPoints();
    //the purpose of this list is to keep track of first and last selected
    std::list<SelectableControlPoint *> _points_list;

private:
    // The functions below are invoked from SelectableControlPoint.
    // Previously they were connected to handlers when selecting, but this
    // creates problems when dragging a point that was not selected.
    void _pointGrabbed(SelectableControlPoint *);
    void _pointDragged(Geom::Point &, MotionEvent const &);
    void _pointUngrabbed();
    bool _pointClicked(SelectableControlPoint *, ButtonReleaseEvent const &);
    void _mouseoverChanged();

    void _update();
    void _updateTransformHandles(bool preserve_center);
    void _updateBounds();
    bool _keyboardMove(KeyPressEvent const &, Geom::Point const &);
    bool _keyboardRotate(KeyPressEvent const &, int);
    bool _keyboardScale(KeyPressEvent const &, int);
    bool _keyboardFlip(Geom::Dim2);
    void _keyboardTransform(Geom::Affine const &);
    void _commitHandlesTransform(CommitEvent ce);
    double _rotationRadius(Geom::Point const &);

    set_type _points;

    set_type _all_points;
    std::unordered_map<SelectableControlPoint *, Geom::Point> _original_positions;
    std::unordered_map<SelectableControlPoint *, Geom::Affine> _last_trans;
    std::optional<double> _rot_radius;
    std::optional<double> _mouseover_rot_radius;
    std::optional<Geom::Point> _first_point;
    Geom::OptRect _bounds;
    TransformHandleSet *_handles;
    SelectableControlPoint *_grabbed_point, *_farthest_point;
    unsigned _dragging         : 1;
    unsigned _handles_visible  : 1;
    unsigned _one_node_handles : 1;

    friend class SelectableControlPoint;
};

} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_TOOL_CONTROL_POINT_SELECTION_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
