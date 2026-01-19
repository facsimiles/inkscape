// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <2geom/svg-path-parser.h>

#include "colors/color.h"
#include "colors/manager.h"
#include "renderer/context.h"
#include "renderer/surface.h"

#include "surface-testbase.h"

using namespace Inkscape::Renderer;
using namespace Inkscape::Colors;

class RenderContextPatternTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        size = {21, 21};
        bounds = {0, 0, 21, 21};
        scale = {1, 1};
        cmyk = Manager::get().find(Space::Type::CMYK);

        surface = std::make_shared<Surface>(size, 1, cmyk);
        context = std::make_unique<Context>(surface, bounds, scale);
    }

    void clear()
    {
        surface->run_pixel_filter(ClearPixels());
    }

    Geom::IntPoint size = {21, 21};
    Geom::IntRect bounds;
    Geom::Scale scale;
    std::shared_ptr<Space::AnySpace> cmyk;
    std::shared_ptr<Surface> surface;
    std::unique_ptr<Context> context;
};

TEST_F(RenderContextPatternTest, SetPatternSolidColor)
{

    auto pattern = std::make_unique<SolidColorPattern>(Color(cmyk, {0.0, 1.0, 0.0, 0.6, 1.0}));

    context->setSource(*pattern);
    context->rectangle(Geom::Rect(3, 3, 18, 18));
    context->fill();

    EXPECT_IMAGE_IS(*surface,
                    "       "
                    " nnnnn "
                    " nnnnn "
                    " nnnnn "
                    " nnnnn "
                    " nnnnn "
                    "       ");
}

TEST_F(RenderContextPatternTest, SetPatternSurface)
{
    auto image_s = std::make_shared<Surface>(Geom::IntPoint(9, 9), 1, cmyk);
    auto image_ct = std::make_unique<Context>(image_s, bounds, scale);

    image_ct->setSource(Color(cmyk, {0.7, 0, 0.7, 0.2, 0.7}));
    image_ct->setLineWidth(1);
    image_ct->moveTo({0, 0});
    image_ct->lineTo({9, 9});
    image_ct->stroke();

    auto pattern = std::make_unique<Pattern>(*image_s);
    pattern->setExtend(Cairo::Pattern::Extend::REFLECT);
    context->setSource(*pattern);
    context->rectangle(Geom::Rect(3, 3, 18, 18));
    context->fill();

    EXPECT_IMAGE_IS(*surface,
                    "       "
                    " R  R  "
                    "  RR   "
                    "  RR   "
                    " R  R  "
                    "     R "
                    "       ");
}

TEST_F(RenderContextPatternTest, PatternMatrixAndLinearGradient)
{
    auto image_s = std::make_shared<Surface>(Geom::IntPoint(21, 21), 1, cmyk);

    auto pattern = std::make_unique<LinearGradientPattern>(cmyk, 0.0, 0.0, 21.0, 21.0);
    pattern->addColorStop(0.0, Colors::Color(cmyk, {1.0, 0.0, 0.0, 0.0, 1.0}));
    pattern->addColorStop(0.5, Colors::Color(cmyk, {0.0, 1.0, 0.0, 0.0, 1.0}));
    pattern->addColorStop(1.0, Colors::Color(cmyk, {0.0, 0.0, 0.0, 1.0, 0.0}));

    pattern->setExtend(Cairo::Pattern::Extend::REFLECT);
    context->setSource(*pattern);
    context->rectangle(Geom::Rect(3, 3, 18, 18));
    context->fillPreserve();

    EXPECT_IMAGE_IS(*surface,
                    "       "
                    " 28888 "
                    " 88888 "
                    " 88888 "
                    " 88888 "
                    " 8888f "
                    "       ");

    pattern->setMatrix(Geom::Rotate(45));
    context->fill();

    EXPECT_IMAGE_IS(*surface,
                    "       "
                    " 22888 "
                    " 22888 "
                    " 22988 "
                    " 22688 "
                    " 22288 "
                    "       ");
}

TEST_F(RenderContextPatternTest, RadialGradient)
{
    auto image_s = std::make_shared<Surface>(Geom::IntPoint(21, 21), 1, cmyk);

    auto pattern = std::make_unique<RadialGradientPattern>(cmyk, 10.0, 10.0, 9.0, 10.0, 10.0, 0.0);
    pattern->addColorStop(0.0, Colors::Color(cmyk, {0.0, 0.0, 0.0, 0.0, 0.0}));
    pattern->addColorStop(1.0, Colors::Color(cmyk, {1.0, 0.0, 0.0, 0.0, 1.0}));

    context->setSource(*pattern);
    context->rectangle(Geom::Rect(3, 3, 18, 18));
    context->fill();

    EXPECT_IMAGE_IS(*surface,
                    "       "
                    "  ..   "
                    " .221  "
                    " .222  "
                    "  121  "
                    "       "
                    "       ");
}

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
