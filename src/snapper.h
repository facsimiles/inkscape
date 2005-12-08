#ifndef SEEN_SNAPPER_H
#define SEEN_SNAPPER_H

/**
 *    \file src/snapper.h
 *    \brief Snapper class.
 *
 *    Authors:
 *      Carl Hetherington <inkscape@carlh.net>
 *
 *    Released under GNU GPL, read the file 'COPYING' for more information.
 */

#include <map>
#include <list>
#include "libnr/nr-coord.h"
#include "libnr/nr-point.h"

struct SPNamedView;
struct SPItem;

namespace Inkscape
{

/// The result of an attempt to snap.  If a snap occurred, the
/// first element is the snapped point and the second element is
/// the distance from the original point to the snapped point.
/// If no snap occurred, the first element is the original point
/// and the second element is set to NR_HUGE.
typedef std::pair<NR::Point, NR::Coord> SnappedPoint;

/// Parent for classes that can snap points to something
class Snapper
{
public:
    Snapper(SPNamedView const *nv, NR::Coord const d);
    virtual ~Snapper() {}

    /// Point types to snap.
    enum PointType {
        SNAP_POINT,
        BBOX_POINT
    };

    typedef std::pair<PointType, NR::Point> PointWithType;

    void setSnapTo(PointType t, bool s);
    void setDistance(NR::Coord d);

    bool getSnapTo(PointType t) const;
    NR::Coord getDistance() const;

    bool willSnapSomething() const;

    SnappedPoint freeSnap(PointType t,
                          NR::Point const &p,
                          SPItem const *it) const;

    SnappedPoint freeSnap(PointType t,
                          NR::Point const &p,
                          std::list<SPItem const *> const &it) const;

    SnappedPoint constrainedSnap(PointType t,
                                 NR::Point const &p,
                                 NR::Point const &c,
                                 SPItem const *it) const;

    SnappedPoint constrainedSnap(PointType t,
                                 NR::Point const &p,
                                 NR::Point const &c,
                                 std::list<SPItem const *> const &it) const;
protected:
    SPNamedView const *_named_view;
    
private:

    /**
     *  Try to snap a point to whatever this snapper is interested in.  Any
     *  snap that occurs will be to the nearest "interesting" thing (e.g. a
     *  grid or guide line)
     *
     *  \param p Point to snap (desktop coordinates).
     *  \param it Items that should not be snapped to.
     *  \return Snapped point.
     */
    virtual SnappedPoint _doFreeSnap(NR::Point const &p,
                                     std::list<SPItem const *> const &it) const = 0;

    /**
     *  Try to snap a point to whatever this snapper is interested in, where
     *  the snap point is constrained to lie along a specified vector from the
     *  original point.
     *
     *  \param p Point to snap (desktop coordinates).
     *  \param c Vector to constrain the snap to.
     *  \param it Items that should not be snapped to.
     *  \return Snapped point.
     */    
    virtual SnappedPoint _doConstrainedSnap(NR::Point const &p,
                                            NR::Point const &c,
                                            std::list<SPItem const *> const &it) const = 0;
    
    NR::Coord _distance; ///< snap distance (desktop coordinates)
    std::map<PointType, bool> _snap_to; ///< point types that we will snap to
};

}

#endif /* !SEEN_SNAPPER_H */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
