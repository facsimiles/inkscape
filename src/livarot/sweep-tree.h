// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_LIVAROT_SWEEP_TREE_H
#define INKSCAPE_LIVAROT_SWEEP_TREE_H

#include "livarot/AVL.h"
#include <2geom/point.h>

class Shape;
class SweepEvent;
class SweepEventQueue;
class SweepTreeList;


/**
 * One node in the AVL tree of edges.
 * Note that these nodes will be stored in a dynamically allocated array, hence the Relocate() function.
 */

/**
 * A node in the sweep tree. For details about the sweep tree, what it is, what we do with it,
 * why it's needed, check out SweepTreeList's documentation.
 *
 * Explaination for what and why is stored in evt:
 * Say you have two edges in the sweepline left and right and an intersection is detected between
 * the two. An intersection event (of type SweepEvent) is created and that event object stores
 * pointer to the left and right edges (of type SweepTree). The left edge's evt[RIGHT]/evt[1]
 * stores the pointer to the intersection event and the right edge's evt[LEFT]/evt[0] also stores
 * it. This is done for a very important reason. If any point in time, either the LEFT or the RIGHT
 * edge have to change their position in the sweepline for any reason at all (before the
 * intersection point comes), we need to immediately delete that event from our list, cuz the edges
 * are no longer together.
 */
class SweepTree:public AVLTree
{
public:
    SweepEvent *evt[2];   /*!< Intersection with the edge on the left and right (if any). */

    Shape *src;           /*!< Shape from which the edge comes.  (When doing boolean operation on polygons,
                               edges can come from 2 different polygons.) */
    int bord;             /*!< Edge index in the Shape. */
    bool sens;            /*!< true = top->bottom; false = bottom->top. */
    int startPoint;       /*!< point index in the result Shape associated with the upper end of the edge */

    SweepTree();
    ~SweepTree() override;

    // Inits a brand new node.

    /**
     * Does what usually a constructor does.
     *
     * @param iSrc The pointer to the shape from which this edge comes from.
     * @param iBord The edge index in the shape.
     * @param iWeight The weight of the edge. Used along with edge's orientation to determine
     * sens.
     * @param iStartPoint Point index in the *result* Shape associated with the upper end of the
     * edge
     */
    void MakeNew(Shape *iSrc, int iBord, int iWeight, int iStartPoint);
    // changes the edge associated with this node
    // goal: reuse the node when an edge follows another, which is the most common case

    /**
     * Reuse this node by just changing the variables.
     *
     * This is useful when you have one edge ending at a point and another one starting at the
     * same point. So instead of deleting one and adding another at exactly the same location,
     * you can just reuse the old one and change the variables.
     *
     * @param iSrc The pointer to the shape from which this edge comes from.
     * @param iBord The edge index in the shape.
     * @param iWeight The weight of the edge. Used along with edge's orientation to determine
     * sens.
     * @param iStartPoint Point index in the *result* Shape associated with the upper end of the
     * edge
     */
    void ConvertTo(Shape *iSrc, int iBord, int iWeight, int iStartPoint);

    // Delete the contents of node.

    /**
     * Delete this node. Make sure to change the pointers in any intersection event (that points to
     * this node).
     */
    void MakeDelete();

    // utilites

    // the find function that was missing in the AVLTrree class
    // the return values are defined in LivarotDefs.h

    /**
     * Find where the new edge needs to go.
     *
     *
     */
    int Find(Geom::Point const &iPt, SweepTree *newOne, SweepTree *&insertL,
             SweepTree *&insertR, bool sweepSens = true);

    /**
     * Find the place for a point (not an edge).
     *
     * @image html livarot-images/find-point.svg
     *
     * To learn the algorithm, check the comments in the function body while referring back to
     * the picture shown above. A brief summary follows.
     *
     * We start by taking our edge vector and if it goes bottom to top, or is horizontal and goes
     * right to left, we flip its direction. In the picture bNorm shows this edge vector after any
     * flipping. We rotate bNorm by 90 degrees counter-clockwise to get the normal vector.
     * Then we take the start point of the edge vector (the original start point not the
     * one after flipping) and draw a vector from the start point to the point whose position we
     * are trying to find (iPt), we call this the diff vector. In the picture I have drawn these
     * for three points red, blue and green. Now we take the dot product of this diff with the
     * normal vectors. As you would now, a dot product has the formula:
     * \f[ \vec{A}\cdot\vec{B} = |\vec{A}||\vec{B}|\cos\theta \f]
     * \f$ \theta \f$ here is the angle between the two. As you would know, \f$ \cos \f$ is
     * positive as long as the angle is between +90 and -90. At 90 degrees it becomes zero and
     * greater than 90 or smaller than -90 it becomes negative. Thus the sign of the dot product
     * can be used as an indicator of the angle between the normal vector and the diff vector.
     * If this angle is within 90 to -90, it means the point lies to the right of the original
     * edge. If it lies on 90 or -90, it means the point lies on the same line as the edge and
     * if it's greater than 90 or smaller than -90, the point lies on the left side of the
     * original edge.
     *
     * One important point to see here is that the edge vector will be flipped however it's start
     * point remains the same as the original one, this is not a problem as you can see in the
     * image below. I chose the other point as the start point and everything works out the same.
     *
     * @image html livarot-images/find-point-2.svg
     *
     * I changed the starting point and redrew the diff vectors. Then I projected them such that
     * their origin is the same as that of the normal. Measuring the angles again, everything
     * remains the same.
     *
     * There is one more confusion part you'd find in this function. The part where left and right
     * child are checked and you'd see child as well as elem pointers being used.
     *
     * @image html livarot-images/sweep-tree.svg
     *
     * The picture above shows you a how the sweepline tree structure looks like. I've used numbers
     * instead of actual edges just for the purpose of illustration. The structure is an AVL tree
     * as well as double-linked list. The nodes are arranged in a tree structure that balances
     * itself but each node has two more points elem[LEFT] and elem[RIGHT] that can be used to
     * navigate the linked list. In reality, the linked list structure is what's needed, but having
     * an AVL tree makes searching really easy.
     *
     * See the comments in the if blocks in the function body. I've given examples from this
     * diagram to explain stuff.
     *
     * @param iPt The point whose position we are trying to find.
     * @param insertL The edge on the left (from the location where this point is supposed to go)
     * @param insertR The edge on the right (from the location where this point is supposed to go)
     *
     * @return The found_* codes from LivarotDefs.h. See the file to learn about them.
     */
    int Find(Geom::Point const &iPt, SweepTree *&insertL, SweepTree *&insertR);

    /// Remove sweepevents attached to this node.
    void RemoveEvents(SweepEventQueue &queue);

    void RemoveEvent(SweepEventQueue &queue, Side s);

    // overrides of the AVLTree functions, to account for the sorting in the tree
    // and some other stuff
    int Remove(SweepTreeList &list, SweepEventQueue &queue, bool rebalance = true);
    int Insert(SweepTreeList &list, SweepEventQueue &queue, Shape *iDst,
               int iAtPoint, bool rebalance = true, bool sweepSens = true);
    int InsertAt(SweepTreeList &list, SweepEventQueue &queue, Shape *iDst,
                 SweepTree *insNode, int fromPt, bool rebalance = true, bool sweepSens = true);

    /// Swap nodes, or more exactly, swap the edges in them.
    void SwapWithRight(SweepTreeList &list, SweepEventQueue &queue);

    void Avance(Shape *dst, int nPt, Shape *a, Shape *b);

    void Relocate(SweepTree *to);
};


#endif /* !INKSCAPE_LIVAROT_SWEEP_TREE_H */

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
