#include "nr-filter-dropshadow.h"

namespace Inkscape {
namespace Filters {

void FilterDropShadow::set_dx(double dx) { this->dx = dx; }
void FilterDropShadow::set_dy(double dy) { this->dy = dy; }
void FilterDropShadow::set_std_deviation(double stdDeviation) { this->stdDeviation = stdDeviation; }
void FilterDropShadow::set_flood_color(guint32 color) { this->floodColor = color; }

double FilterDropShadow::get_dx() const { return dx; }
double FilterDropShadow::get_dy() const { return dy; }
double FilterDropShadow::get_std_deviation() const { return stdDeviation; }
guint32 FilterDropShadow::get_flood_color() const { return floodColor; }

} // namespace Filters
} // namespace Inkscape