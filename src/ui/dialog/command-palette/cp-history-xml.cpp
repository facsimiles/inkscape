#include "cp-history-xml.h"

#include "io/resource.h"
#include "xml/repr.h"

namespace Inkscape::UI::Dialog {

CPHistoryXML::CPHistoryXML()
    : _file_path(IO::Resource::profile_path("cphistory.xml"))
{
    _xml_doc = sp_repr_read_file(_file_path.c_str(), nullptr);
    if (!_xml_doc) {
        _xml_doc = sp_repr_document_new("cphistory");

        /* STRUCTURE EXAMPLE ------------------ Illustration 1
        <cphistory>
            <operations>
                <action> full.action_name </action>
                <import> uri </import>
                <export> uri </export>
            </operations>
            <params>
                <action name="app.transfor-rotate">
                    <param> 30 </param>
                    <param> 23.5 </param>
                </action>
            </params>
        </cphistory>
        */

        // Just a pointer, we don't own it, don't free/release/delete
        auto root = _xml_doc->root();

        // add operation history in this element
        auto operations = _xml_doc->createElement("operations");
        root->appendChild(operations);

        // add param history in this element
        auto params = _xml_doc->createElement("params");
        root->appendChild(params);

        // This was created by allocated
        Inkscape::GC::release(operations);
        Inkscape::GC::release(params);

        // only save if created new
        save();
    }

    // Only two children :) check and ensure Illustration 1
    _operations = _xml_doc->root()->firstChild();
    _params = _xml_doc->root()->lastChild();
}

CPHistoryXML::~CPHistoryXML()
{
    Inkscape::GC::release(_xml_doc);
}

void CPHistoryXML::add_action(std::string const &full_action_name)
{
    add_operation(HistoryType::ACTION, full_action_name);
}

void CPHistoryXML::add_import(std::string const &uri)
{
    add_operation(HistoryType::IMPORT_FILE, uri);
}

void CPHistoryXML::add_open(std::string const &uri)
{
    add_operation(HistoryType::OPEN_FILE, uri);
}

void CPHistoryXML::add_action_parameter(std::string const &full_action_name, std::string const &param)
{
    /* Creates
     *  <params>
     * +1 <action name="full.action-name">
     * +    <param>30</param>
     * +    <param>60</param>
     * +    <param>90</param>
     * +1 <action name="full.action-name">
     *   <params>
     *
     * + : generally creates
     * +1: creates once
     */
    auto const parameter_node = _xml_doc->createElement("param");
    auto const parameter_text = _xml_doc->createTextNode(param.c_str());

    parameter_node->appendChild(parameter_text);
    Inkscape::GC::release(parameter_text);

    for (auto action_iter = _params->firstChild(); action_iter; action_iter = action_iter->next()) {
        // If this action's node already exists
        if (full_action_name == action_iter->attribute("name")) {
            // If the last parameter was the same don't do anything, inner text is also a node hence 2 times last
            // child
            if (action_iter->lastChild()->lastChild() && action_iter->lastChild()->lastChild()->content() == param) {
                Inkscape::GC::release(parameter_node);
                return;
            }

            // If last current than parameter is different, add current
            action_iter->appendChild(parameter_node);
            Inkscape::GC::release(parameter_node);

            save();
            return;
        }
    }

    // only encountered when the actions element doesn't already exists,so we create that action's element
    auto const action_node = _xml_doc->createElement("action");
    action_node->setAttribute("name", full_action_name.c_str());
    action_node->appendChild(parameter_node);

    _params->appendChild(action_node);
    save();

    Inkscape::GC::release(action_node);
    Inkscape::GC::release(parameter_node);
}

std::optional<History> CPHistoryXML::get_last_operation()
{
    auto last_child = _operations->lastChild();
    if (last_child) {
        if (auto const operation_type = _get_operation_type(last_child); operation_type.has_value()) {
            // inner text is a text Node thus last child
            return History{*operation_type, last_child->lastChild()->content()};
        }
    }
    return std::nullopt;
}
std::vector<History> CPHistoryXML::get_operation_history() const
{
    // TODO: add max items in history
    std::vector<History> history;
    for (auto operation_iter = _operations->firstChild(); operation_iter; operation_iter = operation_iter->next()) {
        if (auto const operation_type = _get_operation_type(operation_iter); operation_type.has_value()) {
            history.emplace_back(*operation_type, operation_iter->firstChild()->content());
        }
    }
    return history;
}

std::vector<std::string> CPHistoryXML::get_action_parameter_history(std::string const &full_action_name) const
{
    std::vector<std::string> params;
    for (auto action_iter = _params->firstChild(); action_iter; action_iter = action_iter->prev()) {
        // If this action's node already exists
        if (full_action_name == action_iter->attribute("name")) {
            // lastChild and prev for LIFO order
            for (auto param_iter = _params->lastChild(); param_iter; param_iter = param_iter->prev()) {
                params.emplace_back(param_iter->content());
            }
            return params;
        }
    }
    // action not used previously so no params;
    return {};
}

void CPHistoryXML::save() const
{
    sp_repr_save_file(_xml_doc, _file_path.c_str());
}

void CPHistoryXML::add_operation(HistoryType const history_type, std::string const &data)
{
    std::string operation_type_name;
    switch (history_type) {
        // see Illustration 1
        case HistoryType::ACTION:
            operation_type_name = "action";
            break;
        case HistoryType::IMPORT_FILE:
            operation_type_name = "import";
            break;
        case HistoryType::OPEN_FILE:
            operation_type_name = "open";
            break;
        default:
            return;
    }
    auto operation_to_add = _xml_doc->createElement(operation_type_name.c_str()); // action, import, open
    auto operation_data = _xml_doc->createTextNode(data.c_str());
    operation_data->setContent(data.c_str());

    operation_to_add->appendChild(operation_data);
    _operations->appendChild(operation_to_add);

    Inkscape::GC::release(operation_data);
    Inkscape::GC::release(operation_to_add);

    save();
}

std::optional<HistoryType> CPHistoryXML::_get_operation_type(Inkscape::XML::Node *operation)
{
    std::string const operation_type_name = operation->name();

    if (operation_type_name == "action") {
        return HistoryType::ACTION;
    } else if (operation_type_name == "import") {
        return HistoryType::IMPORT_FILE;
    } else if (operation_type_name == "open") {
        return HistoryType::OPEN_FILE;
    } else {
        return std::nullopt;
        // unknown HistoryType
    }
}

} // namespace Inkscape::UI::Dialog
