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
 * Livarot is a 2D geometry library that %Inkscape relies on for a very
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
 * After you're done you convert it back to something that %Inkscape can understand.
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
 * 3. At this point, if all you want to do is %Path Simplification, you can use
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
 * linked list + an AVL tree for faster searching. This structure should hold the linear order in which the edges
 * intersect with the sweepline. I refer to each entry in this structure as a node. In the code these have the type
 * `SweepTree`. I refeer to this whole tree structure as sweepline list. Secondly, we keep a priority queue
 * (a min heap), to keep track of all the intersections that we detect. The priority queue stores these in such
 * a way that the top most and left most intersection (if there are multiple ones at same y) get popped.
 *
 * We start by sorting all the points of the graph top to bottom (and left to right if there are points at the same
 * y level). After that we place the sweepline at the top most point. To determine which point to go next, we check
 * both the next point in the list of sorted vertices of the directed graph and the earliest intersection event in
 * the priority queue. We choose to go with whichever comes first.
 *
 * There are certain things that we need to do at each point the sweepline lands on (which can either be an intersection
 * point that we detected earlier or an endpoint of the edges). For endpoints, we need to:
 * 1. See if there are edges that need to be added to the sweepline list (they just started). For each edge that you add,
 * you must find the right position where it should go in the sweepline list. Once you find that right position, insert
 * it there and test if this edge intersects with the one on its right and its left in that list. If any intersections
 * are detected, record them. This testing is done by doing maths that can tell if two line segments intersect.
 * 2. See if there are edges that need to be removed from the sweepline list (they ended here). For each edge that you remove,
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
 * This is probably the most complicated part of ConvertToShape (and probably this project). It's very complicated and
 * not documented at all. There have been things in this area that have puzzled me for the whole duration of my GSoC
 * project. This very section of the documentation has changed multiple times. For 90% of the time of this project, I've
 * been feeling as if this part of Livarot is quite useless, as if it does a lot of work to achieve very little. I knew
 * this was very unlikely but I couldn't find purpose to a lot of chaos found in this region. I've recently discovered that
 * my previous assumption was totally incorrect, most (if not all) of this chaos is useful (maybe rarely used), but useful.
 * I now see the purpose behind all of it and reading it makes sense. But I'm still not sure if all code that exists in this
 * region can be triggered by drawings. I know what it's doing and the logic makes sense, but I can't think of an actual path
 * that'll trigger that code. Hopefully it'll make more sense as I progress. I'll try to explain this now as clearly as I can.
 *
 * The purpose of reconstruction is to break the original graph into pieces such that all intersection points are now vertices
 * and the edges of this graph don't intersect with each other (since all intersections would become vertices). Let's start
 * with the lowest level part of this region. It's in the function Shape::DoEdgeTo. The job of this function is to draw an edge.
 * We don't draw edges by specifying the start and end point. Instead, an edge (of the old directed graph) stores the last
 * point till which it was drawn and we give it a new point to draw to. Once it has drawn, the last point will be updated for
 * any future drawing. When I say draw, I just mean creating a new edge. Say you have an edge named X that has A and E as end
 * points, You may have intersections with the edge at points B, C and D and four edges will get created ultimately, A -> B,
 * B -> C, C -> D and D -> E. The function also interpolates back data to create valid back data for the new edges and there is
 * some winding number seed stuff that it takes care of. You can visit the function to see the details.
 *
 * The diagram below shows the whole process for specific shape. The sweepline starts at point 0
 * where edge 0 and edge 1 are added to the sweepline. Their "last point drawn" is of course 0. Then
 * the sweepline reaches point 1, edge 0 is about to end so we draw it till the point 1, change its
 * "last point drawn" and remove the edge. Edge 2 is added and its "last point drawn" is set to point
 * 1. Then the sweepline reaches point 2 and here edge 1 and 2 intersect. We draw both of these
 * edges till the point 2 and update their "last point drawn" to 2. Then the sweepline reaches
 * point 3 and edge 1 is about to end here, so we draw it till point 3, update its "last point
 * drawn" and remove it from the list. Edge 3 is added and its "last point drawn" is set to point
 * 3. Lastly, the sweepline reaches point 4 where both edges are to be removed. They get drawn till
 * point 4, their "last point drawn" is updated and both are removed from the list. The summary of
 * the process is, whenver an event occurs to an edge (addition/removal/intersection), draw till
 * that point and update "last point drawn". Of course there is more detail to this and you'll
 * see that next.
 *
 * @image html livarot-images/convert-reconstruction.svg
 *
 * Now let's get to the meaty stuff. This whole region of code gets called from the main loop of Shape::ConvertToShape. The body
 * of this loop starts with some code that finds the next point that the sweepline should go to. Choosing between the next point
 * in the sorted list of vertices of the original shape and the earliest intersection point. Once this point has been picked, rounded
 * and added in "this" shape's list of points, there is a special block that runs, let's call it the reconstruction block. This code
 * only gets triggered if the y of the current point is at a different (higher) y level than the previous point. If it's not, this code
 * doesn't run. So you can say that the reconstruction block runs only when the y level changes. After this block, there is the standard
 * sweepline stuff that I've already talked about in the previous section. After this loop has ended, the reconstruction block is run for
 * the last time (it has been seperately written after the loop). So, understand this cearly, the loop starts by adding a point, runs the
 * reconstruction block if this point's y is different (higher) than the previous one and then runs the sweepline intersection code. You
 * can imagine a grid of points in your head arranged in rows and columns and you'll realize that the reconstruction block only runs when
 * the left most point of each y level just got added. The following picture shows you this grid and also showns what the variables indicate
 * at the point when the reconstruction block ran right after the current point (lastPointNo) got added. As you can see, lastChgtPt will
 * hold the left most point of the previous y level and lastPointNo holds the point that just got added.
 *
 * @image html livarot-images/lastChgtPt-from-avance.svg
 *
 * The following picture illustrates how these different pieces are called.
 *
 * @image html livarot-images/sequence-convert-shape-loop.svg
 *
 * I discussed the sweepline intersection finding algorithm in the previous section. That section also operates an array called "chgts".
 * Maybe this stands for changes? I don't know. Anyways, this array stores changes that happen to the sweepline at a y level. These can be of
 * three types, an edge addition, an edge removal or an edge intersection. Whenever the sweepline stuff (the last part of the main loop
 * of Shape::ConvertToShape) does any of these three, it adds an entry in chgts for that change. These entries in chgts get processed
 * as soon as the reconstruction block runs, and the reconstruction block runs whenever the y level of the current point changes. So it
 * could run immediately after the current iteration if the y of the next point is different (higher), or you could iterate a few more
 * times until the y changes. It depends on how many more points exist at the y level on which the event was detected and pushed in chgts.
 * If this is the last point, reconstruction block will run after the loop has ended (it has been rewritten after it as I mentioned
 * above). Once the reconstruction block runs, it processes this chgts array and uses it to do the reconstruction and at the end of the
 * reconstruction block, this array is emptied. So you can imagine the process is that we add all events that take place at a y level
 * and as soon as the y level changes, we process all those chgts (doing the reconstruction) and empty the array immediately.
 *
 * The next thing I want to describe are the two variables called leftRnd and rightRnd that are stored for each edge in the array swsData.
 * I don't know how to easily describe what these points do. The best way to put it is how the author puts it, they store the left and
 * right most points in the resultant graph at the current sweepline position (y level). These values are not set if the edge has no
 * event associated with a y level (it has no edge addition/removal/intersection at that y level) (there is an exception to this you'll
 * later discover). However, this explaination makes no sense unless you already understand what these variables do. Recall that the
 * edges get created by the function DoEdgeTo and this function needs an edge and a point to draw to. The edge stores the last drawn
 * point and will automatically draw a new edge from that point to the passed in point. For now, think of leftRnd and rightRnd as
 * storing the range of points which we will supply in the DoEdgeTo calls for that edge, one by one. In all non-horizontal edges,
 * leftRnd and rightRnd are the same point (as far as I know), since, at a particular y level, all non-horizontal edges can have only
 * one point. It'll only be horizontal edges that can have a range of points defined by leftRnd and rightRnd, since its endpoints
 * will all be at the same y level. The function Shape::Avance is what actually does these DoEdgeTo calls with leftRnd and righRnd
 * points.
 *
 * Coming back to chgts, whenever the sweepline intersection stuff (the last bit in the main loop of ConvertToShape) detects an event,
 * which can be an edge addition/removal/intersection, it adds an entry in chgts by calling Shape::AddChgt. This entry stores a few
 * things such as the point at which the event took place (a reference to the point in "this" shape, not the original shape). The type
 * of the event, the edge associated with the event (the edges if an intersection, both left and right). It also stores the edge to
 * the left and right of the edge associated with the event in the sweepline at the time the event took place. There is one other
 * very useful thing this function Shape::AddChgt does, it populates the leftRnd and rightRnd variables associated with the edge that's
 * associated with that event. In the trivial case of non-horizonal edge, you just set them to lastPointNo (the point that just got
 * added) and lead to this event (addition/intersection/removal). You can see the function to see in detail how this is done to handle
 * horizonal cases too. (See the condition ...leftRnd < lastChgtPt). AddChgt also sets leftRnd and rightRnd properly to the leftmost
 * and rightmost rounded points that the edge has at this y level. Again, they'll be just the same point for a vertical edge and only
 * be different when your edge is horizontal. If you see details of AddChgt, you'll realize that for a horizonal edge, leftRnd and
 * rightRnd get set to correct values after AddChgt has been called for both for the edge insertion as well as removal/intersection.
 * Now this populated chgt array and the leftRnd and rightRnd stuff will be used together for edge reconstruction in the reconstruction block.
 *
 * The reconstruction block starts with a call to AssemblePoints. This function is used to sort all points at the previous y level
 * and merge all duplicate points. We do store the new mapping between old and new indices of points so we can update the indices
 * stored in leftRnd, rightRnd and chgt.ptNo. After this sorting and index updating has been done, there is a call to a function named
 * CheckAdjacencies but I'll get to that later. After it there is a call to Shape::CheckEdges. The first thing that CheckEdges does is
 * for all chgt in chgts it checks if its an edge insertion and if yes, it sets "last point drawn" of that edge. This is the same
 * idea that I've talked about previously. It sets that to the current point to mark that the point has only been drawn to its
 * start point or in other words, hasn't been drawn at all. After this, for each chgt in chgts, Shape::Avance is called on the main
 * edge that's associated with the event. The same is done with the right edge if it was intersection event. Shape::Avance does
 * a lot of fancy checks, but it simply calls DoEdgeTo on the edge multiple times for each point in the range leftRnd and rightRnd
 * of that edge. That's how it all works.
 *
 * If you've understood all of this described so far and the sweepline intersection stuff too, I can go into the complicated stuff
 * now. There are certain things I ignored in the description just to explain how the simple and common stuff works. There are particularly
 * two things I missed. The function Shape::CheckAdjacencies and some part of Shape::CheckEdges.
 *
 * Two edges can intersect in two ways, either they have what I call a trivial intersection where the two edges intersect at a single
 * point that's not their endpoint, or they could kinda intersect such that the intersection point is the endpoint of one of the edges.
 * As far as I have looked, the function TesteIntersection detects the former intersections easily however it doesn't detect the latter.
 * It has code that can do it, but somehow it's prevented and never runs. This is still an area I am exploring. Anyways, even if it did,
 * these intersections are not regular intersections and shouldn't be processed that way in Bentley Ottman part (I think).
 * Draw the path: M 500,200 L 500,800 L 200,800 L 500,500 L 200,200 Z
 * and you'll see the longer edge has two edges that intersect with it in this manner. It'd be better to name these Adjacencies instead.
 * Shape::CheckAdjacencies detects if there are points that lie on top of edges. The underlying function it calls Shape::TeseteAdjacency needs
 * a point and an edge to check an adjacency. So the question is, which set of points and edges do we check? The answer is in the function
 * itself but to break it down here:
 * 1. For the unique edge (or the left edge if it's an intersection event), we check for the adjacency of the unique edge with all points
 * before the edge's leftRnd at the previous y level (the highest y that's smaller than lastPointNo's y) and with all the points to the
 * right of leftRnd at the previous y level. These checks are done right to left for the former and left to right for the latter. If adjacencies
 * are detected, leftRnd and rightRnd of the edge are expanded accordingly. Note that I say previous y level in reference to the y value
 * of lastPointNo at the time when Shape::CheckAdjacencies is called.
 * 2. For the right edge (of an intersection event only), we repeat the same process on its leftRnd and rightRnd as 1.
 * 3. We check the edges to the left of the unique edge (in the sweepline at the time of the event) for an adjacency with the points
 * of the previous y level. We firstly test points of the range chLeN..chRiN. These variables come from leftRnd and rightRnd of the unique
 * edge or the right edge, see the function body. We test these right to left and update leftRnd as well as rightRnd of the edge we are
 * checking. We also test for all points between lastChgtPt and chLeN - 1. If no adjacency got detected, we just break right away. If an
 * adjacency is detected, we continue towards left (left edge) using the sweepline node list (a double linked list). Maybe the logic is that
 * if this edge is not adjacent to the point, how can the one on its left be. Note we only do these adjacency checks if the leftRnd of the
 * edge is before lastChgtPt, if not, we do no checks.
 * 4. Exactly identical to 3 but in direction towards right. The points we check are firstly chLeN..chRiN and then chRiN+1..lastPointNo (not included).
 * Note that 3 and 4 are really important, for example in the SVG path written above. You can have another edge that ends or starts at a point that
 * lies on top of this edge. When that edge's insertion/removal event is being processed, code in 3/4 will check for an adjacency and modify the
 * leftRnd and rightRnd of the edge accordingly. This will allow Shape::CheckEdges to break this path at that point (using the leftRnd and rightRnd).
 *
 * Now let's discuss the remaining bits of Shape::CheckEdges. I've already discussed the trivial stuff in Shape::CheckEdges above.
 * However, there is a little more to it that's directly related to point 3 and 4 of the above paragrapah on Shape::CheckAdjacencies. Shape::CheckEdges
 * not only calls Avance on the unique edge (or the left and right in case of intersection). It also calls Avance on the linked list of edges
 * to the left and right of the unique edge at the time of intersection. However, this is only if that edge has a leftRnd set to some point greater
 * than or equal to lastChgtPt. See there are few cases due to which an edge will have a leftRnd >= lastChgtPt. Either the edge had some event
 * associated with it (addition/removal/intersection) at that y level. Or there was some point in the previous y level that was on top of the edge, and
 * thus an adjacency was detected and the leftRnd/rightRnd were set accordingly. If you have neither of these, leftRnd/rightRnd won't be set at
 * all. If the case is former, that the edge had an event at the previous y level, the first block in Shape::CheckEdges will automatically call Avance
 * on the edge. However, for the latter, we have no chgt event associated with the edge. Thus, there is a special block in Shape::CheckEdges that calls
 * Shape::Avance on edges to the left and to the right of the unique (or the left) edge. However, it's called only if leftRnd >= lastChgtPt.
 *
 * There is one more point that I'd like to highlight. The chaos looking code in CheckAdjacencies and CheckEdges has another special
 * purpose that's not that apparent. It does something called "Snap Rounding". As far as I can see, it's based on an algorithm by
 * John D. Hobby described in the paper "Practical segment intersection with finite precision output". This rounding is exactly all
 * that it does. The reason why we are not able to catch this easily is because it becomes useful when your points gets too close to
 * the edges and that rarely happens in mouse drawn paths. By default, you can't even zoom that level in Inkscape.
 *
 * https://www.sciencedirect.com/science/article/pii/S0925772199000218
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
 * @subsection ManipulateEdges Manipulate Edges
 *
 * Once winding numbers have been computed, there is a block of code at the end of Shape::ConvertToShape that does this. The
 * code would iterate through all the edges that were reconstructed and decide between doing one of three things: Keep an
 * edge as it is, invert or flip it or remove it. One key principle that is followed is that inside is kept to the edge's
 * left. So if that's not the case already, the edge gets inverted. Of course the fill rule is taken into account while
 * doing this. Take a close look at the code to understand it all in detail, it's a really simple piece of code to look at.
 *
 *
 * @section ContourExtraction Extracting Contours
 *
 * Once you're done with all the directed graph stuff that's described above you'd want to extract contours out from the
 * directed graph. The procedure to do that is very similiar to the winding number computation algorithm. You do a depth
 * first search but ensure that you're always moving along the edge vector (when moving forward). Whenever you reach a
 * dead end you complete the contour. To see the details you can see Shape::ConvertToForme.
 *
 * There is another version of the same function that takes back data into account. What it does is that it can recognize
 * where the edges originally come from and recreate those path descriptions. So basically, instead of having straight
 * line segments, you get those original curves back (although they'd be broken into pieces depending on the intersection).
 *
 */


