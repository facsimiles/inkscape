// SPDX-License-Identifier: GPL-2.0-or-later

#include "canvas-item-squiggle.h"

#include <cmath>

#include "display/cairo-utils.h"

namespace Inkscape {

namespace {

// Helper Functions

// Evaluate cubic Bezier at t in [0..1]
static inline Geom::Point eval_cubic(const Geom::Point &p0, const Geom::Point &c1, const Geom::Point &c2, const Geom::Point &p3, double t)
{
    double u = 1.0 - t;

    // Bernstein form
    return p0 * (u*u*u) + c1 * (3.0 * u*u * t) + c2 * (3.0 * u * t*t) + p3 * (t*t*t);
}

/* Build a sequence of cubic bezier segments that approximate a Centripetal Catmull-Rom spline through the input points.
   Uses alpha = 0.5.
   Converts each Catmull segment (P0,P1,P2,P3) into a cubic Bezier (B0,B1,B2,B3) via Hermite tangents -> Bezier control points.

   If knot distances are degenerate (very small) we use Uniform Catmull-Rom conversion for robustness.
*/
void _build_catmull_beziers_from_points(const std::vector<Geom::Point> &pts, std::vector<std::array<Geom::Point,4>> &out_beziers)
{
    out_beziers.clear();
    if (pts.size() < 2) return;

    // If only two points, just make a straight cubic (control points on the line).
    if (pts.size() == 2) {
        Geom::Point p0 = pts[0];
        Geom::Point p1 = pts[1];
        Geom::Point c1 = p0 + (p1 - p0) * (1.0 / 3.0);
        Geom::Point c2 = p0 + (p1 - p0) * (2.0 / 3.0);
        out_beziers.push_back({ p0, c1, c2, p1 });
        return;
    }

    constexpr double alpha = 0.5;         // centripetal
    constexpr double eps = 1e-8;         // small epsilon to avoid /0

    size_t n = pts.size();
    for (size_t i = 0; i < n - 1; ++i) {
        // neighbors (clamped at ends)
        Geom::Point P0 = (i == 0) ? pts[i] : pts[i - 1];
        Geom::Point P1 = pts[i];
        Geom::Point P2 = pts[i + 1];
        Geom::Point P3 = (i + 2 < n) ? pts[i + 2] : pts[i + 1];

        // Compute chord lengths ^ alpha (centripetal knots)
        double d01 = Geom::L2(P1 - P0);
        double d12 = Geom::L2(P2 - P1);
        double d23 = Geom::L2(P3 - P2);

        // Avoid exact zeros (which would make knot differences zero)
        if (d01 < eps) d01 = eps;
        if (d12 < eps) d12 = eps;
        if (d23 < eps) d23 = eps;

        double t0 = 0.0;
        double t1 = t0 + std::pow(d01, alpha);
        double t2 = t1 + std::pow(d12, alpha);
        double t3 = t2 + std::pow(d23, alpha);

        // If denominators are too small or knots collapsed, use Uniform Catmull-Rom formula.
        bool small_denom = ( (t2 - t0) < eps ) || ( (t3 - t1) < eps );

        Geom::Point B0 = P1;
        Geom::Point B3 = P2;
        Geom::Point B1, B2;

        if (small_denom) {
            // Uniform Catmull-Rom
            B1 = P1 + (P2 - P0) * (1.0 / 6.0);
            B2 = P2 - (P3 - P1) * (1.0 / 6.0);
        } else {
            // Compute tangent (Hermite) vectors M1 and M2 for interval [t1,t2]
            // Use stable coefficient formulas
            double denom1 = (t1 - t0);
            double denom2 = (t2 - t1);
            double denom3 = (t3 - t2);
            double denomA = (t2 - t0); // for c1,c2
            double denomB = (t3 - t1); // for d1,d2

            // Precompute finite-difference velocities where safe
            Geom::Point v10 = (denom1 > eps) ? ((P1 - P0) / denom1) : Geom::Point(0.0, 0.0);
            Geom::Point v21 = (denom2 > eps) ? ((P2 - P1) / denom2) : Geom::Point(0.0, 0.0);
            Geom::Point v32 = (denom3 > eps) ? ((P3 - P2) / denom3) : Geom::Point(0.0, 0.0);

            // coefficients
            double c1 = (denomA > eps) ? ((t2 - t1) / denomA) : 0.0;
            double c2 = (denomA > eps) ? ((t1 - t0) / denomA) : 0.0;

            double d1 = (denomB > eps) ? ((t3 - t2) / denomB) : 0.0;
            double d2 = (denomB > eps) ? ((t2 - t1) / denomB) : 0.0;

            // M1 and M2 scaled to the interval [t1,t2] (see formulas in description)
            Geom::Point M1 = (v10 * c1 + v21 * c2) * (t2 - t1);
            Geom::Point M2 = (v21 * d1 + v32 * d2) * (t2 - t1);

            // Convert Hermite (P1,P2,M1,M2) to Bezier control points
            B1 = P1 + M1 * (1.0 / 3.0);
            B2 = P2 - M2 * (1.0 / 3.0);
        }

        out_beziers.push_back({ B0, B1, B2, B3 });
    }
}

// Flatten a list of cubic beziers to dense polyline (sampled points), also produce cumulated lengths
void _flatten_beziers(const std::vector<std::array<Geom::Point,4>> &beziers, std::vector<Geom::Point> &out_poly, std::vector<double> &out_cumlen, double dt)
{
    out_poly.clear();
    out_cumlen.clear();
    if (beziers.empty()) return;

    // sample each bezier
    out_poly.push_back(beziers[0][0]);
    out_cumlen.push_back(0.0);

    for (const auto &bz : beziers) {
        // sample t from dt..1.0 inclusive (first t=0 already added)
        for (double t = dt; t <= 1.0 + 1e-12; t += dt) {
            double tt = t;
            if (tt > 1.0) tt = 1.0;
            Geom::Point pt = eval_cubic(bz[0], bz[1], bz[2], bz[3], tt);
            if (pt != out_poly.back()) {
                double seg = Geom::L2(pt - out_poly.back());
                out_poly.push_back(pt);
                out_cumlen.push_back(out_cumlen.back() + seg);
            }
        }
    }
}

/* sample on the flattened polyline by arc-length s (0..total_len)
   produce interpolated point and tangent (approx from neighboring samples) */
void _sample_poly_by_arc(const std::vector<Geom::Point> &poly, const std::vector<double> &cumlen, double s, Geom::Point &out_pt, Geom::Point &out_tangent)
{
    if (poly.empty()) {
        out_pt = Geom::Point(0,0);
        out_tangent = Geom::Point(1,0);
        return;
    }

    double total = cumlen.back();
    if (s <= 0.0) {
        out_pt = poly.front();
        out_tangent = (poly.size() > 1) ? (poly[1] - poly[0]) : Geom::Point(1,0);
        return;
    }
    if (s >= total) {
        out_pt = poly.back();
        out_tangent = (poly.size() > 1) ? (poly.back() - poly[poly.size()-2]) : Geom::Point(1,0);
        return;
    }

    // binary search for segment
    auto it = std::lower_bound(cumlen.begin(), cumlen.end(), s);
    size_t idx = std::distance(cumlen.begin(), it);
    if (idx == 0) idx = 1;

    // interpolate between idx-1 and idx
    double s0 = cumlen[idx-1];
    double s1 = cumlen[idx];
    double frac = (s1 - s0) > 1e-12 ? (s - s0) / (s1 - s0) : 0.0;
    Geom::Point p0 = poly[idx-1];
    Geom::Point p1 = poly[idx];
    out_pt = p0 + (p1 - p0) * frac;

    // tangent approx by neighbor
    Geom::Point ahead, behind;
    if (idx + 1 < poly.size()) ahead = poly[idx+1];
    else ahead = p1;
    if (idx >= 2) behind = poly[idx-2];
    else behind = p0;

    out_tangent = (ahead - behind);
}

}

CanvasItemSquiggle::CanvasItemSquiggle(CanvasItemGroup *group, Geom::Point const &start, Geom::Point const &end, uint32_t color)
    : CanvasItem(group)
    , _start(start)
    , _end(end)
    , _color(color)
{
    _name = "CanvasItemSquiggle";
    _pickable = false;

    // defaults
    _amplitude = 3.5;
    _wavelength = 8.0;
    _sample_dt = 0.02; // sampling step for flattening beziers

    request_update();
}

CanvasItemSquiggle::CanvasItemSquiggle(CanvasItemGroup *group, std::vector<Geom::Point> const &points, uint32_t color)
    : CanvasItem(group)
    , _points(points)
    , _color(color)
{
    _name = "CanvasItemSquiggle";
    _pickable = false;

    // defaults
    _amplitude = 3.5;
    _wavelength = 8.0;
    _sample_dt = 0.02;

    request_update();
}

void CanvasItemSquiggle::set_points(Geom::Point start, Geom::Point end)
{
    _points.clear();
    if (_start != start || _end != end) {
        _start = start;
        _end = end;
        request_update();
    }
}

void CanvasItemSquiggle::set_points(std::vector<Geom::Point> const &points)
{
    if (_points != points) {
        _points = points;
        request_update();
    }
}

void CanvasItemSquiggle::set_squiggle_params(double amplitude, double wavelength, double sample_dt)
{
    _amplitude = amplitude;
    _wavelength = wavelength;
    _sample_dt = sample_dt > 0.0 ? sample_dt : 0.02;
    request_update();
}

void CanvasItemSquiggle::set_color(uint32_t color)
{
    if (_color != color) {
        _color = color;
        request_redraw();
    }
}

void CanvasItemSquiggle::_rebuild_squiggle()
{
    // Transform start/end/points from document to canvas units
    Geom::Affine aff = affine();

    // collect base points in canvas coords
    std::vector<Geom::Point> base_pts;
    if (!_points.empty()) {
        base_pts.reserve(_points.size());
        for (auto const &p : _points) base_pts.push_back(p * aff);
    } else {
        Geom::Point s = _start * aff;
        Geom::Point e = _end * aff;
        base_pts.push_back(s);
        base_pts.push_back(e);
    }

    _squiggle_path.clear();

    // minimum length threshold
    constexpr double min_canvas_len = 4.0;
    // compute straight-line length as a quick check
    double approx_len = 0.0;
    for (size_t i = 1; i < base_pts.size(); ++i) approx_len += Geom::L2(base_pts[i] - base_pts[i-1]);
    if (approx_len < min_canvas_len) {
        return;
    }

    // 1) Build Catmull-Rom -> cubic beziers
    std::vector<std::array<Geom::Point,4>> beziers;
    _build_catmull_beziers_from_points(base_pts, beziers);

    // 2) Flatten to a dense polyline and arc-length table
    std::vector<Geom::Point> poly;
    std::vector<double> cumlen;
    _flatten_beziers(beziers, poly, cumlen, _sample_dt);

    if (poly.size() < 2) {
        return;
    }

    double total_len = cumlen.back();

    // squiggle params (already in canvas/screen units)
    double amplitude = _amplitude;    // in canvas units
    double wavelength = _wavelength;  // in canvas units
    int n = std::max(1, int(total_len / wavelength));
    double step = total_len / n;

    // Build squiggle by sampling along baseline and offsetting perpendicular
    Geom::Path path;
    // start at baseline first point (no offset)
    Geom::Point first_base = poly.front();
    path.start(first_base);

    // previous baseline point (for computing ctrl midpoints)
    Geom::Point prev_base = first_base;
    // previous offset point — for the first segment it equals first_base (no offset)
    Geom::Point prev_offset = first_base;

    for (int i = 1; i <= n; ++i) {
        double s = i * step;
        Geom::Point base_pt, tangent;
        _sample_poly_by_arc(poly, cumlen, s, base_pt, tangent);

        // compute perpendicular; normalize tangent first
        double tlen = Geom::L2(tangent);
        Geom::Point dir = (tlen > 1e-8) ? (tangent / tlen) : Geom::Point(1.0, 0.0);
        Geom::Point perp(-dir[1], dir[0]);

        double sign = (i % 2 == 0) ? 1.0 : -1.0;
        Geom::Point offset_pt = base_pt + perp * (amplitude * sign);

        // control point placed at mid baseline between prev_base and base_pt and shifted by same sign*amplitude
        Geom::Point baseline_mid = prev_base + (base_pt - prev_base) * 0.5;
        Geom::Point ctrl = baseline_mid + perp * (amplitude * sign);

        // create a cubic using ctrl repeated as symmetric control points (like you had earlier)
        path.appendNew<Geom::CubicBezier>(ctrl, ctrl, offset_pt);

        // advance
        prev_base = base_pt;
        prev_offset = offset_pt;
    }

    // store the result path (in canvas coords)
    _squiggle_path.push_back(path);
}

void CanvasItemSquiggle::_update(bool)
{
    request_redraw();

    _rebuild_squiggle();

    // Set bounds based on either _points or _start/_end
    Geom::Rect bounds_doc;
    if (!_points.empty()) {
        bounds_doc = Geom::Rect(_points.front(), _points.back());
        for (auto const &p : _points) bounds_doc.unionWith(Geom::Rect(p, p));
    } else {
        bounds_doc = Geom::Rect(_start, _end);
    }

    bounds_doc.expandBy(5.0); // Expand by 5 canvas units, convert to doc units
    _bounds = bounds_doc;

    *_bounds *= affine();

    request_redraw();
}

void CanvasItemSquiggle::_render(CanvasItemBuffer &buf) const
{
    if (_squiggle_path.empty()) {
        return;
    }

    buf.cr->save();

    buf.cr->set_tolerance(0.5);
    buf.cr->begin_new_path();

    // Draw in screen coordinates but no affine transformation cause it is already in canvas coordinates
    feed_pathvector_to_cairo(buf.cr->cobj(), _squiggle_path, Geom::Affine(), buf.rect, true, 0);

    ink_cairo_set_source_color(buf.cr, Colors::Color(_color));
    buf.cr->set_line_width(1.5);
    buf.cr->stroke();

    buf.cr->restore();
}

} // namespace Inkscape