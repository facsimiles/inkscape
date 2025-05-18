// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Authors:
 *   Rafał Siejakowski <rs@rs-math.net>
 *
 * @copyright
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/desktop/desktop-affine.h"

#include <2geom/affine.h>
#include <2geom/transforms.h>
#include <gtest/gtest.h>

namespace Inkscape {

using namespace ::testing;

TEST(DesktopAffineTest, SetScale)
{
    auto const testScale = Geom::Scale{2.0, -3.0};
    auto const testScale2 = Geom::Scale{-1.0, 12.0};

    DesktopAffine testAffine;
    testAffine.setScale(testScale);
    EXPECT_EQ(testAffine.d2w(), testScale);

    testAffine.setScale(testScale2);
    EXPECT_EQ(testAffine.d2w(), testScale2);
}

TEST(DesktopAffineTest, AddScale)
{
    auto const testScale = Geom::Scale{2.0, -3.0};
    auto const testScale2 = Geom::Scale{-1.0, 12.0};

    DesktopAffine testAffine;
    testAffine.addScale(testScale);
    EXPECT_EQ(testAffine.d2w(), testScale);

    testAffine.addScale(testScale2);
    auto const expected = testScale * testScale2;
    EXPECT_EQ(testAffine.d2w(), expected);
}

TEST(DesktopAffineTest, SetRotate)
{
    auto const testRotate = Geom::Rotate{15.0};
    auto const testRotate2 = Geom::Rotate{-60.0};

    DesktopAffine testAffine;
    testAffine.setRotate(testRotate);
    EXPECT_EQ(testAffine.d2w(), testRotate);

    testAffine.setRotate(testRotate2);
    EXPECT_EQ(testAffine.d2w(), testRotate2);
}

TEST(DesktopAffineTest, AddRotate)
{
    auto const testRotate = Geom::Rotate{15.0};
    auto const testRotate2 = Geom::Rotate{-60.0};

    DesktopAffine testAffine;
    testAffine.addRotate(testRotate);
    EXPECT_EQ(testAffine.d2w(), testRotate);

    testAffine.addRotate(testRotate2);
    auto const expected = testRotate * testRotate2;
    EXPECT_EQ(testAffine.d2w(), expected);
}

TEST(DesktopAffineTest, AddRotateAndScale)
{
    auto const testRotate = Geom::Rotate{15.0};
    auto const testScale = Geom::Scale{2.0, -3.0};

    DesktopAffine testAffine;
    testAffine.addRotate(testRotate);
    testAffine.addScale(testScale);

    auto const expected = testScale * testRotate;
    EXPECT_EQ(testAffine.d2w(), expected);
}

TEST(DesktopAffineTest, AddScaleAndRotate)
{
    auto const testScale = Geom::Scale{2.0, -3.0};
    auto const testRotate = Geom::Rotate{15.0};

    DesktopAffine testAffine;
    testAffine.addScale(testScale);
    testAffine.addRotate(testRotate);

    auto const expected = testScale * testRotate;
    EXPECT_EQ(testAffine.d2w(), expected);
}

TEST(DesktopAffineTest, AddScaleAndFlip)
{
    auto const testScale = Geom::Scale{2.0, -3.0};
    auto const testFlip = CanvasFlip::FLIP_HORIZONTAL;

    DesktopAffine testAffine;
    testAffine.addScale(testScale);
    testAffine.addFlip(testFlip);

    auto const expected = testScale * Geom::Scale(-1.0, 1.0);
    EXPECT_EQ(testAffine.d2w(), expected);

    // unflip now
    testAffine.addFlip(testFlip);
    EXPECT_EQ(testAffine.d2w(), testScale);
}

TEST(DesktopAffineTest, AddMultipleFlips)
{
    DesktopAffine testAffine;
    testAffine.addFlip(CanvasFlip::FLIP_HORIZONTAL);
    EXPECT_EQ(testAffine.d2w(), Geom::Scale(-1.0, 1.0));

    testAffine.addFlip(CanvasFlip::FLIP_VERTICAL);
    EXPECT_EQ(testAffine.d2w(), Geom::Scale(-1.0, -1.0));

    testAffine.addFlip(CanvasFlip::FLIP_HORIZONTAL);
    EXPECT_EQ(testAffine.d2w(), Geom::Scale(1.0, -1.0));

    testAffine.addFlip(CanvasFlip::FLIP_VERTICAL);
    EXPECT_EQ(testAffine.d2w(), Geom::identity());
}

TEST(DesktopAffineTest, GetZoom)
{
    DesktopAffine testAffine;
    EXPECT_DOUBLE_EQ(testAffine.getZoom(), 1.0);

    testAffine.addScale(Geom::Scale(2.0));
    EXPECT_DOUBLE_EQ(testAffine.getZoom(), 2.0);

    testAffine.addFlip(CanvasFlip::FLIP_VERTICAL);
    EXPECT_DOUBLE_EQ(testAffine.getZoom(), 2.0);

    testAffine.addScale(Geom::Scale(3.0));
    EXPECT_DOUBLE_EQ(testAffine.getZoom(), 6.0);
}

TEST(DesktopAffineTest, GetRotation)
{
    DesktopAffine testAffine;
    EXPECT_EQ(testAffine.getRotation(), Geom::Rotate());

    auto const testRotation = Geom::Rotate(32.0);
    testAffine.addRotate(testRotation);
    EXPECT_EQ(testAffine.getRotation(), testRotation);

    // A flip shouldn't change the rotation
    testAffine.addFlip(CanvasFlip::FLIP_HORIZONTAL);
    EXPECT_EQ(testAffine.getRotation(), testRotation);
}

TEST(DesktopAffineTest, IsFlipped)
{
    DesktopAffine testAffine;
    EXPECT_FALSE(testAffine.isFlipped(CanvasFlip::FLIP_HORIZONTAL));
    EXPECT_FALSE(testAffine.isFlipped(CanvasFlip::FLIP_VERTICAL));

    // Add a horizontal flip
    testAffine.addFlip(CanvasFlip::FLIP_HORIZONTAL);
    EXPECT_TRUE(testAffine.isFlipped(CanvasFlip::FLIP_HORIZONTAL));
    EXPECT_FALSE(testAffine.isFlipped(CanvasFlip::FLIP_VERTICAL));

    // Add a vertical flip
    testAffine.addFlip(CanvasFlip::FLIP_VERTICAL);
    EXPECT_TRUE(testAffine.isFlipped(CanvasFlip::FLIP_HORIZONTAL));
    EXPECT_TRUE(testAffine.isFlipped(CanvasFlip::FLIP_VERTICAL));

    // Remove the horizontal flip
    testAffine.addFlip(CanvasFlip::FLIP_HORIZONTAL);
    EXPECT_FALSE(testAffine.isFlipped(CanvasFlip::FLIP_HORIZONTAL));
    EXPECT_TRUE(testAffine.isFlipped(CanvasFlip::FLIP_VERTICAL));

    // Check that the result doesn't change when we rotate
    testAffine.addRotate(90.0);
    EXPECT_FALSE(testAffine.isFlipped(CanvasFlip::FLIP_HORIZONTAL));
    EXPECT_TRUE(testAffine.isFlipped(CanvasFlip::FLIP_VERTICAL));
}
} // namespace Inkscape
