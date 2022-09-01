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
#include <src/extension/effect.h>
#include "desktop.h"
#include <src/extension/internal/text-recognition.h>
#include <src/inkscape.h>
#include <gtkmm.h>

using namespace Inkscape;
class DetectTextTest : public ::testing::Test
{
protected:
    std::shared_ptr<Extension::Internal::DetectText> instance;

    void SetUp() override
    {
        // setup dependency
        Application::create(false);
        Gtk::Application::create();
        Extension::Internal::DetectText::init();

        instance = std::make_shared<Extension::Internal::DetectText>();
    }
};

TEST_F(DetectTextTest, loadFunctionReturnsTrue)
{
    ASSERT_TRUE(instance->load(nullptr));
}

TEST_F(DetectTextTest, defaultLanguageIsEnglish) 
{   
    instance->loadLanguages();
    Gtk::ComboBoxText *languageComboBox = dynamic_cast<Gtk::ComboBoxText *>(instance->languageWidget());
    ASSERT_EQ(languageComboBox->get_active_id(),"eng");
}

TEST_F(DetectTextTest, defaultTextIsCorrect) 
{
    Gtk::Label *detectedTextLabel = dynamic_cast<Gtk::Label *>(instance->textWidget());
    ASSERT_EQ(detectedTextLabel->get_label(),"The Detected Text will appear here");
}

TEST_F(DetectTextTest, testEmptyDocument) 
{
    SPDesktop* view=new SPDesktop();
    instance->effect(nullptr,view,nullptr);
    Gtk::Label *detectedTextLabel = dynamic_cast<Gtk::Label *>(instance->textWidget());
    ASSERT_EQ(detectedTextLabel->get_label(),"");
}