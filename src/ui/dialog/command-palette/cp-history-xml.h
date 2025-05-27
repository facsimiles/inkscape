// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * CommandPalette: Class providing Command Palette feature
 */
/* Author:
 * Abhay Raj Singh <abhayonlyone@gmail.com>
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_COMMAND_PALETTE_HISTORY_H
#define INKSCAPE_COMMAND_PALETTE_HISTORY_H

#include <optional>
#include <string>
#include <vector>

#include "xml/node.h"

namespace Inkscape::UI::Dialog {
enum class HistoryType
{
    LPE,
    ACTION,
    OPEN_FILE,
    IMPORT_FILE,
};

struct History
{
    HistoryType history_type;
    std::string data;

    History(HistoryType ht, std::string &&data)
        : history_type(ht)
        , data(data)
    {}
};

class CPHistoryXML
{
public:
    // constructors, asssignment, destructor
    CPHistoryXML();
    ~CPHistoryXML();

    // Handy wrappers for code clearity
    void add_action(std::string const &full_action_name);

    void add_import(std::string const &uri);
    void add_open(std::string const &uri);

    /// Remember parameter for action
    void add_action_parameter(std::string const &full_action_name, std::string const &param);

    std::optional<History> get_last_operation();

    /// To construct _CPHistory
    std::vector<History> get_operation_history() const;
    /// To get parameter history when an action is selected, LIFO stack like so more recent first
    std::vector<std::string> get_action_parameter_history(std::string const &full_action_name) const;

private:
    void save() const;

    void add_operation(HistoryType const history_type, std::string const &data);

    static std::optional<HistoryType> _get_operation_type(Inkscape::XML::Node *operation);

    std::string const _file_path;

    Inkscape::XML::Document *_xml_doc;
    // handy for xml doc child
    Inkscape::XML::Node *_operations;
    Inkscape::XML::Node *_params;
};

} // namespace Inkscape::UI::Dialog

#endif // !INKSCAPE_COMMAND_PALETTE_HISTORY_H
