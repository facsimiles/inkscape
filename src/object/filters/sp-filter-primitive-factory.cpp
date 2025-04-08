#include "nr-filter-dropshadow.h"

std::unique_ptr<Inkscape::Filters::FilterPrimitive> create_filter_primitive(SPFilterPrimitive const *primitive)
{
    if (primitive->get_type() == "feDropShadow") {
        auto drop_shadow = std::make_unique<Inkscape::Filters::FilterDropShadow>();
        drop_shadow->set_dx(primitive->get_dx());
        drop_shadow->set_dy(primitive->get_dy());
        drop_shadow->set_std_deviation(primitive->get_std_deviation());
        drop_shadow->set_flood_color(primitive->get_flood_color());
        return drop_shadow;
    }

    // Existing logic for other primitives...
}