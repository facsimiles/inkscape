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
 * Inkscape:
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
 */
