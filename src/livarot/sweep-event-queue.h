// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A container of intersection events.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2010 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_LIVAROT_SWEEP_EVENT_QUEUE_H
#define SEEN_LIVAROT_SWEEP_EVENT_QUEUE_H

#include <2geom/forward.h>
class SweepEvent;
class SweepTree;


/**
 * The structure to hold the intersections events encountered during the sweep.  It's an array of
 * SweepEvent (not allocated with "new SweepEvent[n]" but with a malloc).  There's a list of
 * indices because it's a binary heap: inds[i] tell that events[inds[i]] has position i in the
 * heap.  Each SweepEvent has a field to store its index in the heap, too.
 */
class SweepEventQueue
{
public:
    SweepEventQueue(int s);
    virtual ~SweepEventQueue();

    int size() const { return nbEvt; }

    /** Look for the topmost intersection in the heap
     */
    bool peek(SweepTree * &iLeft, SweepTree * &iRight, Geom::Point &oPt, double &itl, double &itr);

    /** Extract the topmost intersection from the heap
     */
    bool extract(SweepTree * &iLeft, SweepTree * &iRight, Geom::Point &oPt, double &itl, double &itr);

    /** Add one intersection in the binary heap
     */
    SweepEvent *add(SweepTree *iLeft, SweepTree *iRight, Geom::Point &iPt, double itl, double itr);

    /**
     * Remove event from the event queue. Make sure to clear the evt pointers from the nodes
     * involved.
     *
     * @param e The event to remove.
     */
    void remove(SweepEvent *e);

    /**
     * Relocate the event e to the location to.
     *
     * This will place all data of e in `to` and also update any
     * evt pointers held by the intersection nodes.
     *
     * @param e The SweepEvent to relocate.
     * @param to The index of the location where we want to relocate `e` to.
     */
    void relocate(SweepEvent *e, int to);

private:
    int nbEvt;    ///< Number of events currently in the heap.
    int maxEvt;   ///< Allocated size of the heap.
    int *inds;    ///< Indices.
    SweepEvent *events;  ///< Sweep events.
};

#endif /* !SEEN_LIVAROT_SWEEP_EVENT_QUEUE_H */

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
