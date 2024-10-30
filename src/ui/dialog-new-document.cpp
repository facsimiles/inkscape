// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * The job of this window is to ask the user for document size when creating a new document.
 */

#include "dialog-new-document.h"

namespace Inskcape::UI {

NewDocumentDialog::NewDocumentDialog()
    : _builder(create_builder("new-document.glade")) {

};

} // namespace Inkscape::UI
