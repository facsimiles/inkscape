// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2014 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
/*
 *  Path.h
 *  nlivarot
 *
 *  Created by fred on Tue Jun 17 2003.
 *
 */

#ifndef my_path
#define my_path

#include <vector>
#include "LivarotDefs.h"
#include <2geom/point.h>

struct PathDescr;
struct PathDescrLineTo;
struct PathDescrArcTo;
struct PathDescrCubicTo;
struct PathDescrBezierTo;
struct PathDescrIntermBezierTo;

class SPStyle;

/*
 * the Path class: a structure to hold path description and their polyline approximation (not kept in sync)
 * the path description is built with regular commands like MoveTo() LineTo(), etc
 * the polyline approximation is built by a call to Convert() or its variants
 * another possibility would be to call directly the AddPoint() functions, but that is not encouraged
 * the conversion to polyline can salvage data as to where on the path each polyline's point lies; use
 * ConvertWithBackData() for this. after this call, it's easy to rewind the polyline: sequences of points
 * of the same path command can be reassembled in a command
 */

// polyline description commands
enum
{
  polyline_lineto = 0,  // a lineto 
  polyline_moveto = 1,  // a moveto
  polyline_forced = 2   // a forced point, ie a point that was an angle or an intersection in a previous life
                        // or more realistically a control point in the path description that created the polyline
                        // forced points are used as "breakable" points for the polyline -> cubic bezier patch operations
                        // each time the bezier fitter encounters such a point in the polyline, it decreases its treshhold,
                        // so that it is more likely to cut the polyline at that position and produce a bezier patch
};

class Shape;

// path creation: 2 phases: first the path is given as a succession of commands (MoveTo, LineTo, CurveTo...); then it
// is converted in a polyline
// a polylone can be stroked or filled to make a polygon

/**
 * An object to store path descriptions and line segment approximations.
 *
 * This object stores path descriptions similar to the `d' attribute of an SVG path node
 * and line segment approximations of those path descriptions.
 *
 * Create a new instance of the object. Call the instruction functions such as Path::MoveTo,
 * Path::LineTo, Path::CubicTo, etc to create a path description. Then call one of Path::Convert,
 * Path::ConvertEvenLines or Path::ConvertWithBackData to do more interesting things.
 *
 *     Path *path = new Path;
 *     path.MoveTo(Geom::Point(10, 10));
 *     path.LineTo(Geom::Point(100, 10));
 *     path.LineTo(Geom::Point(100, 100));
 *     path.Close();
 *     path.ConvertEvenLines(0.001); // You can use the other variants too
 *     // insteresting stuff here
 *
 */
class Path
{
  friend class Shape;

public:

  // flags for the path construction
  enum
  {
    descr_ready = 0,        
    descr_adding_bezier = 1, // we're making a bezier spline, so you can expect  pending_bezier_* to have a value
    descr_doing_subpath = 2, // we're doing a path, so there is a moveto somewhere
    descr_delayed_bezier = 4,// the bezier spline we're doing was initiated by a TempBezierTo(), so we'll need an endpoint
    descr_dirty = 16         // the path description was modified
  };

  // some data for the construction: what's pending, and some flags
  int         descr_flags;
  int         pending_bezier_cmd;
  int         pending_bezier_data;
  int         pending_moveto_cmd;
  int         pending_moveto_data;

  std::vector<PathDescr*> descr_cmd; /*!< A vector of pointers to path description objects. Freed later by the deconstructor. */

  /**
   * Structure to store points for the line segment approximation.
   */
  struct path_lineto
  {
    path_lineto(bool m, Geom::Point pp) : isMoveTo(m), p(pp), piece(-1), t(0), closed(false) {}
    path_lineto(bool m, Geom::Point pp, int pie, double tt) : isMoveTo(m), p(pp), piece(pie), t(tt), closed(false) {}
    
    int isMoveTo;    /*!< A flag that stores one of polyline_lineto, polyline_moveto, polyline_forced */
    Geom::Point  p;  /*!< The point itself. */
    int piece;       /*!< The path description index which this point comes from. */
    double t;        /*!< The time in that description that it comes from. 0 is beginning and 1 is the end. */
    bool closed;     /*!< True indicates that subpath is closed (this point is the last point of a closed subpath) */
  };
  
  std::vector<path_lineto> pts; /*!< A vector storing the line segment approximation points. */

  bool back; /*!< A flag that when true, indicates that the line segment approximation is going to have backdata.
                  You don't need to set this manually though. When you call Convert or its variants it's set automatically. */

  Path();
  virtual ~Path();

  // creation of the path description

  /**
   * Clears all descriptions and description related flags.
   */
  void Reset();

  /**
   * Clear all descriptions of the current path and copy the passed one.
   *
   * @param who Pointer to the Path object whose descripions to copy.
   */
  void Copy (Path * who);

  /**
   * Add a forced point at the last point of the path. If there wasn't any commands before this
   * one, it won't work.
   *
   * @return -1 if didn't go well otherwise the index of the description added.
   */
  int ForcePoint();

  /**
   * Close the path.
   *
   * @return -1 if didn't go well otherwise the index of the description added.
   */
  int Close();

  /** A MoveTo command.
   *
   * @param ip Reference to the point to move to.
   *
   * @return The index of the path description added.
   */
  int MoveTo ( Geom::Point const &ip);

  /** A LineTo command.
   *
   * @param ip Reference to the point to draw a line to.
   *
   * @return The index of the path description added.
   */
  int LineTo ( Geom::Point const &ip);

  /**
   * A CubicBezier command.
   *
   * In order to understand the parameter let p0, p1, p2, p3 denote the four points of a
   * cubic bezier curve. p0 is the start point. p3 is the end point. p1 and p2 are the
   * two control points.
   *
   * @param ip The final point of the bezier curve or p3.
   * @param iStD 3 * (p1 - p0). Weird way to store it but that's how it is.
   * @param iEnD 3 * (p3 - p2). Weird way to store it but that's how it is.
   *
   * @return The index of the path description added.
   */
  int CubicTo ( Geom::Point const &ip,  Geom::Point const &iStD,  Geom::Point const &iEnD);

  /**
   * An ArcTo command. Identical to the SVG elliptical arc description.
   *
   * @param ip The final point of the arc.
   * @param iRx The radius in the x direction.
   * @param iRy The radius in the y direction.
   * @param angle The angle w.r.t x axis in degrees. TODO
   * @param iLargeArc True means take the larger arc otherwise the smaller one.
   * @param iClockwise  True means take the clockwise arc otherwise take the anti-clockwise one.
   *
   * @return The index of the path description added.
   */
  int ArcTo ( Geom::Point const &ip, double iRx, double iRy, double angle, bool iLargeArc, bool iClockwise);

  /**
   * Adds a control point to the Nth degree bezier curve that was last inserted with a call to
   * Path::BezierTo.
   *
   * @param ip The control point.
   *
   * @return The index of the path description added.
   */
  int IntermBezierTo ( Geom::Point const &ip);	// add a quadratic bezier spline control point

  /**
   * An Nth degree bezier curve.
   *
   * No need to specify the degree. That'll be done automatically as you call Path::IntermBezierTo
   * to add the control points. The sequence of instructions are like:
   * 1. Call Path::BezierTo with the final point.
   * 2. Call Path::IntermBezierTo with control points. One call for each control point.
   * 3. Call Path::EndBezierTo() to mark the end of this bezier description.
   *
   * @param ip The final point of this quadratic bezier spline.
   *
   * @return The index of the path description added.
   */
  int BezierTo ( Geom::Point const &ip);	// quadratic bezier spline to this point (control points can be added after this)

  /**
   * Called to mark the end of the Nth order bezier stuff.
   *
   * @return -1 All the time.
   */
  int EndBezierTo();

  /**
   * Start a quadratic bezier spline decription whose final point you want to specify later.
   *
   * The sequence of instructions would be:
   * 1. Path::TempBezierTo() to start.
   * 2. Path::IntermBezierTo to specify control points. One call for each control point.
   * 3. Path::EndBezierTo to specify the final point of the quadratic bezier spline and also end
   * this description.
   *
   * @return Index of the description added.
   */
  int TempBezierTo();	// start a quadratic bezier spline (control points can be added after this)

  /**
   * Ends the quadratic bezier spline description that got started with Path::TempBezierTo.
   *
   * @param ip The final point.
   *
   * @return -1 all the time.
   */
  int EndBezierTo ( Geom::Point const &ip);	// ends a quadratic bezier spline (for curves started with TempBezierTo)

  // transforms a description in a polyline (for stroking and filling)
  // treshhold is the max length^2 (sort of)

  /**
   * Approximate the path description by line segments. Doesn't store any backdata. Doesn't split
   * line segments into smaller line segments.
   *
   * Threshold has no strict definition but you can say it's (length^2) sort of.
   *
   * @param threshhold The error threshold used to approximate curves by line segments. The smaller
   * this is, the more line segments there will be.
   */
  void Convert (double treshhold);

  /**
   * Approximate the path description by line segments. Doesn't store any backdata. Splits line
   * segments into further smaller line segments that satisfy the threshold criteria.
   *
   * Threshold has no strict definition but you can say it's (length^2) sort of.
   *
   * @param threshhold The error threshold used to approximate curves and line segments by smaller line segments.
   * The smaller this is, the more line segments there will be.
   */
  void ConvertEvenLines (double treshhold);	// decomposes line segments too, for later recomposition

  /**
   * Approximate the path description by line segments. Store any backdata that is used to later
   * reconstruct the original segments. Splits line segments into further smaller line segments
   * that satisfy the threshold criteria.
   *
   * Threshold has no strict definition but you can say it's (length^2) sort of.
   *
   * @param threshhold The error threshold used to approximate curves and line segments by smaller line segments.
   * The smaller this is, the more line segments there will be.
   */
  void ConvertWithBackData (double treshhold);

  // creation of the polyline (you can tinker with these function if you want)

  /**
   * Sets the back variable to the value passed in and clear any existing line segment
   * approximation points.
   *
   * @param nVal True if we are going to have backdata and false otherwise.
   */
  void SetBackData (bool nVal);	// has back data?

  /**
   * Clears all existing line segment approximation points.
   */
  void ResetPoints(); // resets to the empty polyline

  /**
   * Add a point to the line segment approximation list without backdata.
   *
   * This is used internally by Path::Convert and its variants but you could use it manually
   * if you wanna go down that road. :-D
   *
   * If back variable is set to true, dummy back data will be used with the point. Piece being -1
   * and time being 0. Since this function doesn't take any backdata you have to do something.
   *
   * @param iPt The point itself.
   * @param mvto If true, it's a moveTo otherwise it's a lineto.
   *
   * @return -1 if the point previous to this point is exactly the same as the last one added
   * otherwise the index of the newly added point.
   */
  int AddPoint ( Geom::Point const &iPt, bool mvto = false);	// add point

  /**
   * Add a point to the line segment approximation list with backdata.
   *
   * This is used internally by Path::Convert and its variants but you could use it manually
   * if you wanna go down that road. :-D
   *
   * @param iPt The point itself.
   * @param ip The index of the path description that this point belows to.
   * @param it The time in that path description at which this point exists. 0 is beginning and 1
   * is end.
   * @param mvto If true, it's a moveTo otherwise it's a lineto.
   *
   * @return -1 if the point previous to this point is exactly the same as the last one added
   * otherwise the index of the newly added point.
   */
  int AddPoint ( Geom::Point const &iPt, int ip, double it, bool mvto = false);

  /**
   * Add a forced point without any backdata.
   *
   * The forced point that gets marked is the same as the last point added. Argument
   * is useless.
   *
   * @param iPt The point itself. Useless argument.
   *
   * @return -1 if No points exist already or the last point added is not a lineto. Otherwise the
   * index of the forced point is returned.
   */
  int AddForcedPoint ( Geom::Point const &iPt);	// add point

  /**
   * Add a forced point with backdata.
   *
   * The forced point that gets marked is the same as the last point that wasadded. Arguments
   * are useless. The point as well as the backdata values are used from the last point that was added.
   *
   * @param iPt The point itself. Useless argument.
   * @param ip The index of the path description that this point belongs to. Useless argument.
   * @param it The time at which the point exists in the path description referred by ip.
   *
   * @return -1 if No points exist already or the last point added is not a lineto. Otherwise the
   * index of the forced point is returned.
   */
  int AddForcedPoint ( Geom::Point const &iPt, int ip, double it);

  /**
   * Replace the last point added with this one.
   *
   * @param iPt The point value to replace the last one with.
   *
   * @return -1 If no points exist already, the index of the last one otherwise.
   */
  int ReplacePoint(Geom::Point const &iPt);  // replace point

  // transform in a polygon (in a graph, in fact; a subsequent call to ConvertToShape is needed)
  //  - fills the polyline; justAdd=true doesn't reset the Shape dest, but simply adds the polyline into it
  // closeIfNeeded=false prevent the function from closing the path (resulting in a non-eulerian graph
  // pathID is a identification number for the path, and is used for recomposing curves from polylines
  // give each different Path a different ID, and feed the appropriate orig[] to the ConvertToForme() function

  /**
   * Fill the shape with the line segment approximation stored in pts array.
   *
   * For each line segment an edge would be added between its two points.
   *
   * One important point to highlight is the closeIfNeeded variable here. For each path (where a
   * path is a moveTo followed by one or more lineTo points) you can either have the start and end
   * point being identical or very close (a closed contour) or have them apart (an open contour).
   * If you set closeIfNeeded to true, it'll automatically add a closing segment if needed and
   * close an open contour by itself. If your contour is already closed, it makes sure that the
   * first and last point are the same ones in the graph (instead of being two indentical points).
   * If closeIfNeeded is false, it just doesn't care at all. Even if your contour is closed, the
   * first and last point will be separate (even though they are duplicates).
   *
   * @param dest A pointer to the shape to fill stuff in.
   * @param pathID A unique number for this path. The shape will associate this number each edge
   * that comes this path. Later on, when you use Shape::ConvertToForme you'll pass an array of
   * Path objects (named orig) and the shape will use that pathID to do orig[pathID] and get the
   * original path information.
   * @param justAdd If set to true, this will function will just fill stuff in without resetting
   * any existing stuff in Shape. If set to false, it'll make sure to reset the shape and already
   * make room for the maximum number of possible points and edges.
   * @param closeIfNeeded If set to true, the graph will be closed always. Otherwise, it won't be
   * closed.
   * @param invert If set to true, the graph is drawn exactly in the manner opposite to the actual
   * line segment approximation that this object stores.
   */
  void Fill(Shape *dest, int pathID = -1, bool justAdd = false,
            bool closeIfNeeded = true, bool invert = false);

  // - stroke the path; usual parameters: type of cap=butt, type of join=join and miter (see LivarotDefs.h)
  // doClose treat the path as closed (ie a loop)
  void Stroke(Shape *dest, bool doClose, double width, JoinType join,
              ButtType butt, double miter, bool justAdd = false);

  // build a Path that is the outline of the Path instance's description (the result is stored in dest)
  // it doesn't compute the exact offset (it's way too complicated, but an approximation made of cubic bezier patches
  //  and segments. the algorithm was found in a plugin for Impress (by Chris Cox), but i can't find it back...
  void Outline(Path *dest, double width, JoinType join, ButtType butt,
               double miter);

  // half outline with edges having the same direction as the original
  void OutsideOutline(Path *dest, double width, JoinType join, ButtType butt,
                      double miter);

  // half outline with edges having the opposite direction as the original
  void InsideOutline (Path * dest, double width, JoinType join, ButtType butt,
		      double miter);

  // polyline to cubic bezier patches

  /**
   * Simplify the path.
   *
   * Fit the least possible number of cubic bezier patches on the line segment approximation
   * currently stored while respecting the threshold. The function clears all existing path
   * descriptions and the new cubic bezier patches will be stored in this object.
   *
   * The algorithm to fit cubic beziers on the line segment approximation.
   *
   * http://www.cs.mtu.edu/~shene/COURSES/cs3621/NOTES/INT-APP/CURVE-APP-global.html
   *
   * @param threshold The threshold for simplification.
   */
  void Simplify (double treshhold);

  /**
   * Simplify the path with a different approach.
   *
   * This function is also supposed to do simplification but by merging (coalescing) existing
   * path descriptions instead of doing any fitting. But I seriously doubt whether this is useful
   * at all or works at all. More experimentation needed to see if this is useful at all. TODO
   *
   * @param threshold The threshold for simplification.
   */
  void Coalesce (double tresh);

  // utilities
  // piece is a command no in the command list
  // "at" is an abcissis on the path portion associated with this command
  // 0=beginning of portion, 1=end of portion.
  void PointAt (int piece, double at, Geom::Point & pos);
  void PointAndTangentAt (int piece, double at, Geom::Point & pos, Geom::Point & tgt);

  // last control point before the command i (i included)
  // used when dealing with quadratic bezier spline, cause these can contain arbitrarily many commands
  const Geom::Point PrevPoint (const int i) const;
  
  // dash the polyline
  // the result is stored in the polyline, so you lose the original. make a copy before if needed
  void  DashPolyline(float head,float tail,float body,int nbD,float *dashs,bool stPlain,float stOffset);

  void  DashPolylineFromStyle(SPStyle *style, float scale, float min_len);
  
  //utilitaire pour inkscape

  /**
   * Load a lib2geom Geom::Path in this path object.
   *
   * @param path A reference to the Geom::Path object.
   * @param tr A transformation matrix.
   * @param doTransformation If set to true, the transformation matrix tr is applied on the path
   * before it's loaded in this path object.
   * @param append If set to true, any existing path descriptions in this object are retained. If
   * set to false, any existing descriptions will be reset.
   */
  void  LoadPath(Geom::Path const &path, Geom::Affine const &tr, bool doTransformation, bool append = false);

  /**
   * Load a lib2geom Geom::PathVector in this path object. (supports transformation)
   *
   * @param pv A reference to the Geom::PathVector object to load.
   * @param tr A transformation to apply on each path.
   * @param doTransformation If set to true, the transformation in tr is applied.
   */
  void  LoadPathVector(Geom::PathVector const &pv, Geom::Affine const &tr, bool doTransformation);

  /**
   * Load a lib2geom Geom::PathVector in this path object.
   *
   * @param pv A reference to the Geom::PathVector object to load.
   */
  void  LoadPathVector(Geom::PathVector const &pv);

  /**
   * Create a lib2geom Geom::PathVector from this path description.
   *
   * Looks like the time this was written Geom::PathBuilder didn't exist or maybe
   * the author wasn't aware of it.
   *
   * @return The Geom::PathVector created.
   */
  Geom::PathVector MakePathVector();

  /**
   * Apply a transformation on all path descriptions.
   *
   * Done by calling the transform method on each path description.
   *
   * @param trans The transformation to apply.
   */
  void  Transform(const Geom::Affine &trans);

  // decompose le chemin en ses sous-chemin
  // killNoSurf=true -> oublie les chemins de surface nulle
  Path**      SubPaths(int &outNb,bool killNoSurf);
  // pour recuperer les trous
  // nbNest= nombre de contours
  // conts= debut de chaque contour
  // nesting= parent de chaque contour
  Path**      SubPathsWithNesting(int &outNb,bool killNoSurf,int nbNest,int* nesting,int* conts);
  // surface du chemin (considere comme ferme)
  double      Surface();
  void        PolylineBoundingBox(double &l,double &t,double &r,double &b);
  void        FastBBox(double &l,double &t,double &r,double &b);
  // longueur (totale des sous-chemins)
  double      Length();
  
  void             ConvertForcedToMoveTo();
  void             ConvertForcedToVoid();
  struct cut_position {
    int          piece;
    double        t;
  };
  cut_position*    CurvilignToPosition(int nbCv,double* cvAbs,int &nbCut);
  cut_position    PointToCurvilignPosition(Geom::Point const &pos, unsigned seg = 0) const;
  //Should this take a cut_position as a param?
  double           PositionToLength(int piece, double t);
  
  // caution: not tested on quadratic b-splines, most certainly buggy
  void             ConvertPositionsToMoveTo(int nbPos,cut_position* poss);
  void             ConvertPositionsToForced(int nbPos,cut_position* poss);

  void  Affiche();
  char *svg_dump_path() const;
  
  bool IsLineSegment(int piece);

    private:
  // utilitary functions for the path construction
  void CancelBezier ();
  void CloseSubpath();
  void InsertMoveTo (Geom::Point const &iPt,int at);
  void InsertForcePoint (int at);
  void InsertLineTo (Geom::Point const &iPt,int at);
  void InsertArcTo (Geom::Point const &ip, double iRx, double iRy, double angle, bool iLargeArc, bool iClockwise,int at);
  void InsertCubicTo (Geom::Point const &ip,  Geom::Point const &iStD,  Geom::Point const &iEnD,int at);
  void InsertBezierTo (Geom::Point const &iPt,int iNb,int at);
  void InsertIntermBezierTo (Geom::Point const &iPt,int at);
  
  // creation of dashes: take the polyline given by spP (length spL) and dash it according to head, body, etc. put the result in
  // the polyline of this instance
  void DashSubPath(int spL, int spP, std::vector<path_lineto> const &orig_pts, float head,float tail,float body,int nbD,float *dashs,bool stPlain,float stOffset);

  // Functions used by the conversion.
  // they append points to the polyline
  void DoArc ( Geom::Point const &iS,  Geom::Point const &iE, double rx, double ry,
	      double angle, bool large, bool wise, double tresh);
  /**
   * Approximate the passed cubic bezier with line segments.
   *
   * Basically the function checks if the passed cubic bezier is "small enough" and if
   * it is, it does nothing, if it however isn't "small enough", it splits the bezier
   * curve into two bezier curves (split at mid point), recursively calls itself on the
   * left, add the midpoint to the line segment approximation, call itself on the right
   * cubic and done. lev is the maximum recursion possible, once it's reached, the function
   * returns doing nothing immediately. See the code to understand more about maxL.
   *
   * The way the algorithm checks if the curve is "small enough" is maths so I'll try to
   * explain it here so you can see the equations printed and probably refer it in code.
   *
   * Let \f$\vec{p_{0}}\f$, \f$\vec{p_{1}}\f$, \f$\vec{p_{2}}\f$ and \f$\vec{p_{3}}\f$ be the four
   * points that define a cubic bezier. The first is the start point, last is the end point,
   * the two in between are the control points. Given this let me relate these points to the
   * arguments that were passed in.
   *
   * \f[ \vec{iS} = \vec{p_{0}}\f]
   * \f[ \vec{iE} = \vec{p_{3}}\f]
   * \f[ \vec{iSd} = 3 (\vec{p_{1}} - \vec{p_{0}})\f]
   * \f[ \vec{iEd} = 3 (\vec{p_{3}} - \vec{p_{2}})\f]
   *
   * This is just how livarot represents a Cubic Bezier, nothing I can do about that. The code
   * starts by calculating a vector from start point to end point.
   *
   * \f[ \vec{se} = \vec{iE} - \vec{iS} ]\f
   *
   * If the length of \f$\vec{se}\f$ is smaller than 0.01, then the cubic bezier's endpoints are
   * kinda close, but if the control points are too far away, it can still be a huge tall curve,
   * so let's see the control points and see how far away they are from the \f$\vec{se}\f$ vector.
   * To do that, we measure the lengths of \f$\vec{iSd}\f$ and \f$\vec{iEd}\f$. If both are below
   * threshold, we return immediately since it indicates the cubic bezier is "small enough".
   *
   * if the length is greater than 0.01, we still check the y projections of the control handles
   * on the line between start and end points, if these projections are limited by the threshold
   * and we didn't mess up the maxL restriction, we are good.
   *
   * If we ran out of recursion levels, we return anyways. In case this cubic bezier isn't small
   * enough, we split it in two parts. There are math equations in the code that do this and I
   * spent hours deriving it and they are totally correct. Basically take the usual maths to split
   * a cubic bezier into two parts and just account for the factor of 3 in the control handles
   * that livarot adds and you'll end up with correct equations. TODO: Add derivation here maybe?
   *
   */
  void RecCubicTo ( Geom::Point const &iS,  Geom::Point const &iSd,  Geom::Point const &iE,  Geom::Point const &iEd, double tresh, int lev,
		   double maxL = -1.0);
  void RecBezierTo ( Geom::Point const &iPt,  Geom::Point const &iS,  Geom::Point const &iE, double treshhold, int lev, double maxL = -1.0);

  void DoArc ( Geom::Point const &iS,  Geom::Point const &iE, double rx, double ry,
	      double angle, bool large, bool wise, double tresh, int piece);
  void RecCubicTo ( Geom::Point const &iS,  Geom::Point const &iSd,  Geom::Point const &iE,  Geom::Point const &iEd, double tresh, int lev,
		   double st, double et, int piece);
  void RecBezierTo ( Geom::Point const &iPt,  Geom::Point const &iS, const  Geom::Point &iE, double treshhold, int lev, double st, double et,
		    int piece);

  // don't pay attention
  struct offset_orig
  {
    Path *orig;
    int piece;
    double tSt, tEn;
    double off_dec;
  };
  void DoArc ( Geom::Point const &iS,  Geom::Point const &iE, double rx, double ry,
	      double angle, bool large, bool wise, double tresh, int piece,
	      offset_orig & orig);
  void RecCubicTo ( Geom::Point const &iS,  Geom::Point const &iSd,  Geom::Point const &iE,  Geom::Point const &iEd, double tresh, int lev,
		   double st, double et, int piece, offset_orig & orig);
  void RecBezierTo ( Geom::Point const &iPt,  Geom::Point const &iS,  Geom::Point const &iE, double treshhold, int lev, double st, double et,
		    int piece, offset_orig & orig);

  static void ArcAngles ( Geom::Point const &iS,  Geom::Point const &iE, double rx,
                         double ry, double angle, bool large, bool wise,
                         double &sang, double &eang);
  static void QuadraticPoint (double t,  Geom::Point &oPt,   Geom::Point const &iS,   Geom::Point const &iM,   Geom::Point const &iE);
  static void CubicTangent (double t,  Geom::Point &oPt,  Geom::Point const &iS,
			     Geom::Point const &iSd,  Geom::Point const &iE,
			     Geom::Point const &iEd);

  struct outline_callback_data
  {
    Path *orig;
    int piece;
    double tSt, tEn;
    Path *dest;
    double x1, y1, x2, y2;
    union
    {
      struct
      {
	double dx1, dy1, dx2, dy2;
      }
      c;
      struct
      {
	double mx, my;
      }
      b;
      struct
      {
	double rx, ry, angle;
	bool clock, large;
	double stA, enA;
      }
      a;
    }
    d;
  };

  typedef void (outlineCallback) (outline_callback_data * data, double tol,  double width);
  struct outline_callbacks
  {
    outlineCallback *cubicto;
    outlineCallback *bezierto;
    outlineCallback *arcto;
  };

  void SubContractOutline (int off, int num_pd,
			   Path * dest, outline_callbacks & calls,
			   double tolerance, double width, JoinType join,
			   ButtType butt, double miter, bool closeIfNeeded,
			   bool skipMoveto, Geom::Point & lastP, Geom::Point & lastT);
  void DoStroke(int off, int N, Shape *dest, bool doClose, double width, JoinType join,
		ButtType butt, double miter, bool justAdd = false);

  static void TangentOnSegAt(double at, Geom::Point const &iS, PathDescrLineTo const &fin,
			     Geom::Point &pos, Geom::Point &tgt, double &len);
  static void TangentOnArcAt(double at, Geom::Point const &iS, PathDescrArcTo const &fin,
			     Geom::Point &pos, Geom::Point &tgt, double &len, double &rad);
  static void TangentOnCubAt (double at, Geom::Point const &iS, PathDescrCubicTo const &fin, bool before,
			      Geom::Point &pos, Geom::Point &tgt, double &len, double &rad);
  static void TangentOnBezAt (double at, Geom::Point const &iS,
			      PathDescrIntermBezierTo & mid,
			      PathDescrBezierTo & fin, bool before,
			      Geom::Point & pos, Geom::Point & tgt, double &len, double &rad);
  static void OutlineJoin (Path * dest, Geom::Point pos, Geom::Point stNor, Geom::Point enNor,
			   double width, JoinType join, double miter, int nType);

  static bool IsNulCurve (std::vector<PathDescr*> const &cmd, int curD, Geom::Point const &curX);

  static void RecStdCubicTo (outline_callback_data * data, double tol,
			     double width, int lev);
  static void StdCubicTo (outline_callback_data * data, double tol,
			  double width);
  static void StdBezierTo (outline_callback_data * data, double tol,
			   double width);
  static void RecStdArcTo (outline_callback_data * data, double tol,
			   double width, int lev);
  static void StdArcTo (outline_callback_data * data, double tol, double width);


  // fonctions annexes pour le stroke
  static void DoButt (Shape * dest, double width, ButtType butt, Geom::Point pos,
		      Geom::Point dir, int &leftNo, int &rightNo);
  static void DoJoin (Shape * dest, double width, JoinType join, Geom::Point pos,
		      Geom::Point prev, Geom::Point next, double miter, double prevL,
		      double nextL, int *stNo, int *enNo);
  static void DoLeftJoin (Shape * dest, double width, JoinType join, Geom::Point pos,
			  Geom::Point prev, Geom::Point next, double miter, double prevL,
			  double nextL, int &leftStNo, int &leftEnNo,int pathID=-1,int pieceID=0,double tID=0.0);
  static void DoRightJoin (Shape * dest, double width, JoinType join, Geom::Point pos,
			   Geom::Point prev, Geom::Point next, double miter, double prevL,
			   double nextL, int &rightStNo, int &rightEnNo,int pathID=-1,int pieceID=0,double tID=0.0);
    static void RecRound (Shape * dest, int sNo, int eNo,
            Geom::Point const &iS, Geom::Point const &iE,
            Geom::Point const &nS, Geom::Point const &nE,
            Geom::Point &origine,float width);


  /**
   * Simplify a Path (a series of points).
   *
   * @param off The offset to the first point to process.
   * @param N The total number of points in the sequence.
   * @param threshhold The threshold to respect during simplification. The higher this number is,
   * the more relaxed you're making the simplifier. The smaller the number, the more strict you're
   * making the simplifier.
   */
  void DoSimplify(int off, int N, double treshhold);

  /**
   * Fit a cubic bezier patch. (From sratch).
   *
   * @param off The index of the first point in pts.
   * @param N The total number of points you want to fit on starting at index off.
   * @param res Reference to the Cubic Bezier description where the resulting control points will
   * be stored.
   * @param worstP Reference to a point index. This will be changed to whichever point measures at
   * highest deviation from the fitted curve.
   *
   * @return True if the fit respected threshold, false otherwise.
   */
  bool AttemptSimplify(int off, int N, double treshhold, PathDescrCubicTo &res, int &worstP);
  /*
   * The actual fitting algorithm that takes a sequence and fits stuff on it.
   *
   * Totally based on the algorithm from:
   * http://www.cs.mtu.edu/~shene/COURSES/cs3621/NOTES/INT-APP/CURVE-APP-global.html
   *
   * @param start The start point of the cubic bezier which is already known.
   * @param res The Cubic Bezier description that function will populate after doing the maths.
   * @param Xk The array of X coordinates of the point to fit.
   * @param Yk The array of Y coordinates of the point to fit.
   * @param Qk An array to store some intermediate values (math stuff again).
   * @param tk The time values for the points.
   * @param nbPt The total points to fit on.
   *
   * @return True if the fit was done correctly, false if something bad happened. (Non-invertible
   * matrix).
   */
  static bool FitCubic(Geom::Point const &start,
		       PathDescrCubicTo &res,
		       double *Xk, double *Yk, double *Qk, double *tk, int nbPt);
  /**
   * Structure to keep some data for fitting.
   */
  struct fitting_tables {
    int      nbPt;   /*!< The points to fit on in a particular iteration */
    int      maxPt;  /*!< Maximum number of points these arrays here can store */
    int      inPt;   /*!< Total points whose X, Y, lk are all populated here */
    double   *Xk;    /*!< X coordinate of the point */
    double   *Yk;    /*!< Y coordinate of the point */
    double   *Qk;    /*!< A special value needed by the fitting algorithm */
    double   *tk;    /*!< A number between 0 and 1 that tells the fraction (length b/w first point to this point)/(total length) */
    double   *lk;    /*!< Length of the line segment from the previous point to this point */
    char     *fk;    /*!< A flag if 0x01 indicates forced point and if 0x00 indicates a normal point */
    double   totLen; /*!< Total length of the polyline or you can say the sum of lengths of all line segments. */
  };

  /**
   * Fit Cubic Bezier patch using the fitting table data.
   *
   * @param data The fitting_tables data needed for fitting. ExtendFit sets that up for this
   * function.
   * @param threshhold The threshold to respect.
   * @param res The Cubic Bezier description which this function will populate.
   * @param worstP The point with the worst error.
   */
  bool   AttemptSimplify (fitting_tables &data,double treshhold, PathDescrCubicTo & res,int &worstP);

  /**
   * Fit Cubic Bezier patch on the points.
   *
   * This uses data already calculated by probably the same function if it exists.
   * The data that's reused is apparently the X, Y and lk values. However, I think there is a
   * problem with this caching mechanism. See the inline comments of ExtendFit.
   *
   * This function prepares data in fitting tables and calls the AttemptSimplify version that takes
   * fitting_tables data.
   *
   * @param off The offset to the first point.
   * @param N The total number of points in that sequence.
   * @param data The data structure which keeps data saved for later use by the same function.
   * @param threshhold The threshold to respect.
   * @param res Cubic Bezier description where this function will store the handles.
   * @param worstP Function will store the point with the worst error here.
   *
   * @return True if the threshold was respected, otherwise false.
   */
  bool   ExtendFit(int off, int N, fitting_tables &data,double treshhold, PathDescrCubicTo & res,int &worstP);
  /**
   * Peform an iteration of Newton-Raphson to improve t values.
   * I have the mathematical derivation of how this works in my head.
   * TODO: Place them here with embedded latex maybe.
   */
  double RaffineTk (Geom::Point pt, Geom::Point p0, Geom::Point p1, Geom::Point p2, Geom::Point p3, double it);
  void   FlushPendingAddition(Path* dest,PathDescr *lastAddition,PathDescrCubicTo &lastCubic,int lastAD);

private:
  /**
   * Add a Geom::Curve's equivalent path description.
   *
   * Any straight curve (line or otherwise that's straight) is added as line. CubicBezier
   * and EllipticalArcs are handled manually, while any other Geom::Curve type is handled by
   * converting to cubic beziers using Geom::cubicbezierpath_from_sbasis and recursively calling
   * the same function.
   *
   * There is one special reason for using is_straight_curve to figure out if a CubicBezier is
   * actually a line and making sure that it is added as a line not as a CubicBezier (which is
   * straight). Sometimes when you're drawing straight line segments with the Bezier (pen) tool,
   * Inkscape would place a straight CubicBezier instead of a line segment. The call to
   * Path::Convert or Path::ConvertWithBackData would break up this line segment into smaller line
   * segments which is not what we want (we want it to break only real curves) not curves that are
   * actually just straight lines.
   *
   * @param c The Geom::Curve whose path description to create/add.
   */
    void  AddCurve(Geom::Curve const &c);

};
#endif

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
