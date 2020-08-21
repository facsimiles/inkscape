// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Doxygen documentation for livarot.
 *//*
 * Authors:
 *   Moazin Khatti <moazinkhatri@gmail.com>
 *
 * Copyright (C) 2020 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

/**
 * @page LivarotOverview Overview of Livarot.
 *
 *
 * @section Introduction
 *
 * Livarot is a 2D geometry library that Inkscape relies on for a very
 * special set of features. These include:
 *
 * - %Path Simplification
 * - %Path Flattening (Ambiguous term but refers to self-intersection removal)
 * - %Path Offsetting
 * - Boolean Operations
 * - Some modes of Tweak Tool
 * - Line Scanning
 *
 * To use Livarot, you have to take your Geom::PathVector through a few stages.
 * Like any other library, you have to convert it to a form that the library (in
 * this case Livarot) can understand. Then do whatever operations you want to.
 * After you're done you convert it back to something that Inkscape can understand.
 *
 * Following are the steps you take when you want to use Livarot's features in
 * %Inkscape:
 *
 * 1. You take a Geom::PathVector or an SPItem and convert it to a Livarot Path
 * object. You can do this by using the methods in src/path/path-util.h. See the
 * functions there for details of what they do.
 * 2. Once you have a Path object, you can call any one of Path::Convert,
 * Path::ConvertEvenLines and Path::ConvertWithBackData and these will create
 * a line segment approximation of the path description that will be stored in
 * the Path object.
 * 3. At this point, if all you want to do is Path Simplification, you can use
 * call Path::Simplify to do so. However, if you want to use other features such
 * as Boolean Operations, Tweaking, Offsetting, etc, you will need to do one
 * additional step.
 * 4. Use Path::Fill and pass a new Shape object. The Shape object can hold a
 * directed graph structure. Calling Path::Fill would create a directed graph
 * in the Shape object from the line segment approximation that the Path object
 * stored.
 * 5. Use Shape::ConvertToShape with a fill rule on the Shape object. This
 * function does something that's very fundamental to Livarot. It removes all
 * intersections from the directed graph and changes the edges such that the
 * "inside" is to the left of every edge.
 * 6. Do your operation. For Boolean Operations, you'd have used step 1-5 on
 * two shapes and can do Shape::Booleen on them now. For Tweaking, you'd use
 * Shape::MakeTweak and for Offsetting you'd use Shape::MakeOffset.
 * 7. If your operation was Simplify, you can just dump the SVG path to get
 * back an SVG path `d' attribute of the resultant path description. If you did
 * other operations that require the creation of a Shape object as mentioned
 * above, you'd need to call Shape::ConvertToForme to extract contours from
 * the directed graph. Basically, it creates a Path object and you can dump
 * an SVG path `d' attribute.
 *
 * @section ApproximateLine Approximation by Line Segments
 *
 * You start with creating a Path object and adding path descriptions by command
 * functions such as Path::MoveTo, Path::LineTo, Path::Close, etc. Once you've done
 * that you use one of Path::Convert, Path::ConvertEvenLines and Path::ConvertWithBackData.
 * These functions create a line segment approximation for the path descriptions that
 * were stored.
 *
 * @subsection PathConvert Path::Convert
 *
 * Simply creates a line segment approximation. All curves get approximated by line
 * segments while respecting the threshold. The smaller this threshold, the more line
 * segments there will be. The bigger this threshold, the less line segments there will
 * be. All sorts of path descriptions that are not line segments get approximated by
 * line segments. Line segments remain as they were. See the following figures, first one
 * shows a bigger threshold value and the second one shows a smaller threshold value.
 *
 * @image html livarot-images/convert-simple-low.svg
 * @image html livarot-images/convert-simple-high.svg
 *
 * @subsection PathConvertEvenLines Path::ConvertEvenLines
 *
 * This is totally identical to Path::Convert except that it breaks up line segments
 * into further smaller line segments.
 *
 * @image html livarot-images/convert-lines.svg
 *
 * @subsection PathConvertWithBackData Path::ConvertWithBackData
 *
 * It is identical to Path::Convert but with only one difference. It stores something called
 * "back data". Basically, it stores information about where the point comes from. It stores
 * two things. Firstly there is "piece" which is the index of the path description that this
 * point comes from and then there is "time" which is the time value at which if you were to
 * evaluate the path description you'd get that point. A time value of 0 means the start of
 * the path description. A time value of 1 means the end of the path description. See the
 * following figure. The 0 and 1 written are indexes of those path descriptions. There are
 * two path descriptions, first is the bezier curve (index 0) and the second is the line
 * segment (index 1).
 *
 * @image html livarot-images/convert-back-data.svg
 *
 * To see the details of how this splitting into line segments process happens, you can go
 * into the details of these functions and the functions they further call such as Path::RecCubicTo.
 *
 * @section GraphFilling Making Directed Graph
 *
 * The following pictures shows the whole process. We start with a path description and approximate it
 * by line segments. The points of these line segments can store back data as seen earlier. Then, calling
 * Path::Fill will create a directed graph consisting of vertices and edges that will be stored in a Shape
 * object. These edges also have back data stored, each edge stores the piece it comes from and the time of
 * its start point as well as the time of its end point.
 *
 * @image html livarot-images/process-flat-to-convert.svg
 *
 * @section ConvertShape Removing intersections and reconstruction
 *
 * This is probably the most fundamental part of Livarot. I'll describe the algorithm in detail here, as much
 * as I could understand it. There are lots of things it does such as:
 * 1. Finding intersections.
 * 2. Reconstruction of the directed graph.
 * 3. Saving some data for winding number seeds computation for later.
 * 4. Remove any doublon edges (edges on top of each other).
 * 5. Compute winding numbers.
 * 6. Manipulate edges (keep them same, flip them or remove them) given the fill rule.
 *
 * Let me describe the algorithm for each one in great detail now.
 *
 * @subsection FindingIntersections Finding self-intersections.
 *
 * The algorithm to find self-intersections is known as Bentley Ottoman sweepline algorithm. You'll easily
 * find the details in literature however I want to give a complete description of the implementation in
 * livarot as comprehensively as I can.
 *
 * The general idea of the sweepline algorithm is that we sweep a horizontal line from top to bottom of the
 * graph and keep track of the linear order in which the edges intersect with the sweepline. Instead of moving
 * the sweepline in a continious fashion (which is not possible in a digital world), we move it only at certain
 * fixed points. These fixed points are the endpoints of the line segments and any possible intersection points
 * that we detect.
 *
 * We keep two data structures for our needs. The first one has to be linear structure that can preserve the
 * order in which items are stored. A double linked list should suffice for this purpose however we use a double
 * linked list + an AVL tree for faster searching. Secondly, we keep a priority queue (a min heap), to keep track
 * of all the intersections that we detect. The priority queue stores these in such a way that the top most and
 * left most intersection (if there are multiple ones at same y) get popped.
 *
 * We start by sorting all the points of the graph top to bottom (and left to right if there are points at the same
 * y level). After that we place the sweepline at the top most point. To determine which point to go next, we check
 * both the next point in the list of sorted vertices of the directed graph and the earliest intersection event in
 * the priority queue. We choose to go with whichever comes first.
 *
 * There are certain things that we need to do at each point the sweepline lands on (which can either be an intersection
 * point that we detected earlier) or an endpoint of the edges. For endpoints, we need to:
 * 1. See if there are edges that need to be added to the sweepline list (they just started). For each edge that you add,
 * you must find the right position where it should go in the sweepline list. Once you find that right position, insert
 * it there and test if this edge intersects with the one on its right and its left in that list. If any intersections
 * are detected, record them. This testing is done by doing maths that can tell if two line segments intersect.
 * 2. See if there are edges that need to be removed the sweepline list (they ended here). For each edge that you remove,
 * unless that edge was either the head or tail of the sweepline list, it'd leave its two neighbors who'd now be side
 * to side. So you must test if these two neighbors intersect with each other and if they do, record that intersection
 * event.
 * Both of these are shown in the figure below.
 *
 * @image html livarot-images/sweepline-list-node-add-removal.svg
 *
 * When the sweepline lands on an intersection point we need to:
 * 1. Swap the two edges that intersect. The one on the left becomes the right and the one on the right becomes left.
 * 2. Detect if the left one has another edge on its left and test if these two intersect with each other.
 * 3. Detect if the right one has another edge on its right and test if these two intersect with each other.
 *
 * This is shown in the figure below.
 *
 * @image html livarot-images/sweepline-list-node-intersection.svg
 *
 * There is one every important final thing that you need to manage throughout this process and I'm discussing it separately
 * because it applies to the whole process. Whenver you detect an intersection, you store this intersection with references
 * to the original nodes that intersect, that's very obvious. But you should also store reference to the intersection event
 * within the sweepline list node. So each intersection event will have two references, one to the left node and the other
 * to the right node. Each node will have two references (to interesection events), right and left. The left one will point
 * to any possible intersection that has the node as a "right" node in the intersection. Similarly, the right one will
 * point to any intersection that has the node as a "left" node in the intersection. This is shown in the following figure.
 * Index 0 is LEFT and Index 1 is RIGHT. There are three intersection events stored namely X, Y and Z. Intersection X is
 * between node A and B. Intersection Y is between node B and C. Intersection Z is between node C and node D. As you can see
 * in the figure, each intersection event stores a pointer to the left node and a pointer to the right node, these are sweep[0]
 * and sweep[1] respectively. You can also see that each node stores two pointers namely evt[0] and evt[1]. evt[0] is the
 * pointer to any intersection event that references this node as its right edge. evt[1] is the pointer to any intersection
 * event that references this node as its left edge. You can see how these are set for the intersections in the figure.
 *
 * @image html livarot-images/sweepline-list-node-event.svg
 *
 * The important thing that you need to take care of, is that anytime two nodes who are going to intersect are together or
 * side by side and it is known that they will intersect at some point, and you have an intersection event for this, if they
 * switch places or any other node gets inserted between them, that intersection event must be deleted immediately and any
 * references to it should be removed too. This can happen mainly at a few points:
 * 1. A node being inserted between two nodes whose intersection has been detected already.
 * 2. Two nodes whose intersection is being processed and they are to be swapped but the nodes happen to have intersections
 * with their other neighbors. Say you have nodes A -> B -> C -> D and we have intersection events of BC and CD. When we
 * process the event of BC, we swap them and sequence becomes A -> C -> B -> D and now CD are no longer side by side, thus
 * their intersection event should be purged immedately and any references that might point to it too.
 * The calls to Remove and RemoveEvent you see in the code are exactly for this purpose.
 *
 * This is mostly what there is to finding self-intersections.
 *
 * @subsection Reconstruction Reconstruction of directed graph
 *
 * The way that livarot does the reconstruction looks simple from an overview but the code underneath that does it is just
 * way too much chaos for me to understand. It's like you can read and understand what's happening but can't really make
 * sense of the big picture (why it's being done). I did a very simple redesign of these parts and only kept whatever seemed
 * sensible to me or you can say whatever big picture I could see. It's working great even in really complex figures. There
 * are two possibilities here. Either Livarot has all that extra code for some edge cases or purpose that I'm unable to see
 * yet or all of that is useless. It's unlikely that it's entirely useless, maybe it had some purpose earlier or was supposed
 * to but no longer has that. I'm not really sure. Whatever the case, as I progress with my redesign I'll hopefully learn
 * more.
 *
 * For now, I'll attempt to describe the big picture that I understood and also way Livarot does it.
 *
 * The way Shape::ConvertToShape is designed is such that you call it on a new Shape object while passing in the actual
 * directed graph Shape as argument. It'll pick up everything from there and you'll have the reconstructed directed graph
 * in the new Shape object. Rounded versions of the end points and intersection points get picked as soon as the sweepline
 * reaches that point. These list of points are kept sorted and if there are any duplicate points, they are merged together.
 * The overview of the algorithm is that each edge in the sweepline list should keep track of the last point until which it
 * was drawn, this would be its upper endpoint when the edge got added to the sweepline. Whenever the edge either encounters
 * an intersection point or it's lower end point (so it's about to be removed from the list), the algorithm should look at
 * the last point drawn and create an edge from that last point to the current point. Remember that this is only done for
 * edges that are about to be removed from the sweepline list or just encountered an intersection point. One more thing, when
 * you do sorting and merging duplicate points, make sure that you update the "last point drawn" variable from each sweepline
 * list node. This would ensure that duplicate points appear as a single vertex in the graph. This is literally how I've done
 * it in my redesign and everything works great as far as I've tested.
 *
 * The process is shown in the figure below.
 *
 * @image html livarot-images/convert-reconstruction.svg
 *
 * Now let me describe how Livarot does it and it's really complicated honestly. Firstly, it records all changes that happen
 * to the sweepline at each y level. I don't know why it does this, but that's how it is. So all edge additions, edge
 * intersections and edge removals that happen at a particular y level stay in an array called "chgts". This array is cleared
 * as soon as the y level of the sweepline changes. Then a function called CheckAdjacencies is called that iterates through
 * these changes but honestly the function is useless. Comment it out and you'd be fine, unless there is some really out of
 * the world complicated scenario that it was written for, I don't know. Then there is a call to CheckEdges and it is what
 * does the reconstruction. Inside swsData, there is a field known as "curPoint" which keeps track of the last point that
 * was drawn. After a lot of loops and blocks that make no sense to me, DoEdgeTo will be called which will create the edge
 * and add it. I've no idea what's the use and purpose of that chaos but ultimately what it does is what I've just described,
 * keep track of last point drawn and add an edge between that point and the current point. That's it, that's all there is
 * to it. Two linked lists are kept in Shape::ConvertToShape named shapeHead and edgeHead and I've no idea what's their use
 * if at all. Then there are points such as leftRnd and rightRnd and I have no idea why they were needed either.
 *
 * @subsection WindingSeeds Winding number seed calculation
 *
 * See the documentation of Shape::GetWindings and you'll see how a seed winding number is needed for the computation of
 * winding numbers. Whenever we add an edge to the sweepline, the point where that edge starts is noted and we see if there
 * is any edge to the left of this newly added edge, if there is, we note that edge too and we can save this information. The
 * next time somebody wants to know the winding number at winding number at that point is, we can take the edge it that was
 * to its left and find its right winding number (whatever right means depending on the direction of the edge).
 *
 * @image html livarot-images/edge-winding-seed.svg
 *
 * Take a close look at the picture above, you'll see two points highlighted with dark circles. The moment these points are
 * touched by the sweepline and the edges start start there are added to the sweepline list, we see which edge is immediately
 * to the left and note it down in pData[point_index].askForWindingB. The red one is on the left so it gets noted. Later on
 * when GetWindings is trying to find the seed winding number, it can refer back to the red edge and ask for its left winding
 * number which is the winding number at the black point. There is one complication here though. When we are sweeping, the edge
 * on the left hasn't really been added yet, so we don't know its edge index (since it hasn't been added) yet. So we just form
 * a linked list of points that refer to this edge and later when the edge gets drawn (or a part of it), we change all the
 * edge indices to the right value (to the new edge index that just got added) and we remove any associate from the old edge
 * index. This is important, because as we sweep, some other point can again start an association with the same edge and again
 * when some part of the edge gets drawn, we'd want that point to be indexed to the newly added edge. This is the case in the
 * picture I show. The second black point also gets associated to the same red edge.
 *
 * @subsection RemovingDoublon Removing Doublon Edges
 *
 * There can be situations where one or more edges are exactly identical (share endpoints) and are in same or the opposite
 * directions. We deal with these situations by removing all identical edges but keepoing only one. We assign it a weight
 * equal to the difference. Say you have three edges in one direction and one in the opposite. We keep one edge from those
 * three and set it weight to 2. This is all done by Shape::AssembleAretes.
 *
 * @subsection ComputeWindings Computing Winding numbers.
 *
 * There is only one fundamental principle that lets us calculate winding numbers. That is, if you know the left and right
 * winding numbers of an edge and want to know the winding numbers of an edge that shares an endpoint with that edge, you
 * can calculate them as long there is no edge between them as I show in the figure below. To see the details of how you
 * find the winding numbers of one edge given the winding numbers of the other edge (that share an endpoint), go to the
 * function Shape::GetWindings.
 *
 * @image html livarot-images/winding-fundamental-principle.svg
 *
 * We combine this idea with a depth first search. It is very simple. You start with an edge and crawling to new edges, it
 * doesn't matter whether you're going in the direction of the edge or against it, as long as you maintain the direction it's
 * fine. Just keep exploring until you reach a dead-end (unable to find any new edge that you haven't seen before). You start
 * back tracking, going back and at each endpoint checking if there are other edges connected that you haven't seen before. If
 * you do find such an edge, start following it and repeat this until there is no new edge to explore.
 *
 */
