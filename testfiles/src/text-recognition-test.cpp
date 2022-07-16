// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Tests for Tesseract OCR text recognititon class
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2020 Authors
 *
 * Released under GNU GPL version 2 or later, read the file 'COPYING' for more information
 */

#include <gtest/gtest.h>
#include <src/extension/internal/text-recognition.h>
#include <src/inkscape.h>

class DetectTextTest : public ::testing::Test
{
protected:
    Inkscape::Extension::Internal::DetectText *instance;

    void SetUp() override
    {
        // setup dependency
        Inkscape::Extension::Internal::DetectText::init();
        instance = new Inkscape::Extension::Internal::DetectText();
    }
};

TEST_F(DetectTextTest, loadFunctionReturnsTrue)
{
    ASSERT_TRUE(instance->load(nullptr));
}