// SPDX-License-Identifier: GPL-2.0-or-later
/**@file
 * Filter Effects dialog.
 */
/* Authors:
 *   Nicholas Bishop <nicholasbishop@gmail.org>
 *   Rodrigo Kumpera <kumpera@gmail.com>
 *   Felipe C. da S. Sanches <juca@members.fsf.org>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *   insaner
 *
 * Copyright (C) 2007 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "filter-effects-dialog.h"

#include <map>
#include <set>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <glibmm/convert.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/refptr.h>
#include <glibmm/stringutils.h>
#include <glibmm/ustring.h>
#include <gdkmm/display.h>
#include <gdkmm/general.h>
#include <gdkmm/rgba.h>
#include <gdkmm/seat.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/dragsource.h>
#include <gtkmm/enums.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/frame.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/grid.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/paned.h>
#include <gtkmm/popover.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/searchentry2.h>
#include <gtkmm/sizegroup.h>
#include <gtkmm/snapshot.h>
#include <gtkmm/textview.h>
#include <gtkmm/treeviewcolumn.h>
#include <gtkmm/treeview.h>
#include <gtkmm/widget.h>
#include <pangomm/layout.h>
#include <sigc++/functors/mem_fun.h>

#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "filter-chemistry.h"
#include "filter-enums.h"
#include "inkscape-window.h"
#include "layer-manager.h"
#include "preferences.h"
#include "object/filters/sp-filter-primitive.h"
#include "selection.h"
#include "style.h"
#include "display/nr-filter-types.h"
#include "object/filters/blend.h"
#include "object/filters/colormatrix.h"
#include "object/filters/componenttransfer-funcnode.h"
#include "object/filters/componenttransfer.h"
#include "object/filters/convolvematrix.h"
#include "object/filters/distantlight.h"
#include "object/filters/merge.h"
#include "object/filters/mergenode.h"
#include "object/filters/pointlight.h"
#include "object/filters/spotlight.h"
#include "selection-chemistry.h"
#include "ui/builder-utils.h"
#include "ui/column-menu-builder.h"
#include "ui/controller.h"
#include "ui/dialog/filedialog.h"
#include "ui/icon-names.h"
#include "ui/pack.h"
#include "ui/util.h"
#include "ui/widget/color-picker.h"
#include "ui/widget/completion-popup.h"
#include "ui/widget/custom-tooltip.h"
#include "ui/widget/filter-effect-chooser.h"
#include "ui/widget/popover-menu-item.h"
#include "ui/widget/popover-menu.h"
#include "ui/widget/spin-scale.h"
#include "ui/widget/spinbutton.h"

#define BREAK_LOOSE_CONNECTION 1 // Set to 1 if the behaviour wanted is that dropping an inverted connection on canvas should break it.
// #define CURVE_1 0
#define CURVE_2 1
// #define DELETE_NODES
#define filtered(x) x[current_filter_id]
#define dbg g_message("%d", __LINE__)


/*
Node Editor TODO List:
- Implemeting Auto Arrange: 
    Arrange all the selected nodes based on the height from the bottom most node 
    Each node at a given height -> Distributed according to number of nodes at that height.
- Better way to render connections
- Shouldn't be able to create a connection with the node connected to the output node
- Fix the undo placements to not leave empty transactions etc.

Crashes:
- Behaviour Crashes - Right Clicking while in the middle of another event types
*/


using namespace Inkscape::Filters;

namespace Inkscape::UI::Dialog {

static int input_count(const SPFilterPrimitive* prim);

FilterEditorNode::FilterEditorNode(int node_id, int x, int y, 
                                   Glib::ustring label_text, int num_sources, int num_sinks)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
    , node_id(node_id)
    , x(x)
    , y(y)
    , node(Gtk::Orientation::VERTICAL, 10)
    , source_dock(Gtk::Orientation::HORIZONTAL, 10)
    , sources(0)
    , sink_dock(Gtk::Orientation::HORIZONTAL, 10)
    , sinks(0)
    , connected_down_nodes(0)
    , connected_up_nodes(0)
{
    set_size_request(-1, -1);
    node.set_name("filter-node");
    node.set_size_request(100, -1);

    Glib::RefPtr<Gtk::StyleContext> context = node.get_style_context();
    Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
    Glib::ustring style = Inkscape::IO::Resource::get_filename(Inkscape::IO::Resource::UIS, "node-editor.css");
    provider->load_from_path(style);
    context->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    node.add_css_class("nodebox-alt");
    auto label = Gtk::make_managed<Gtk::Label>(label_text);

    label->set_sensitive(false);
    append(sink_dock);
    for(int i = 0; i != num_sinks; i++){
        // auto sink = new FilterEditorSink(this, 1);
        // auto sink_ptr = std::make_unique<FilterEditorSink>(this, 1);
        auto sink = Gtk::make_managed<FilterEditorSink>(this, 1);
        sinks.push_back(sink);
        sink_dock.append(*sink);
    }
    sink_dock.set_halign(Gtk::Align::CENTER);
    sink_dock.set_name("filter-node-sink-dock");
    node.append(*label);

    append(node);
    append(source_dock);
    for(int i = 0; i != num_sources; i++){
        // auto source = new FilterEditorSource(this);
        // auto source = std::make_unique<FilterEditorSource>(this);
        auto source = Gtk::make_managed<FilterEditorSource>(this);
        sources.push_back(source);
        source_dock.append(*source);
    }
    source_dock.set_halign(Gtk::Align::CENTER);
    source_dock.set_name("filter-node-source-dock");
    is_selected = false;
    add_css_class("node");
}

bool FilterEditorNode::get_selected()
{
    return is_selected;
}
bool FilterEditorNode::toggle_selection(bool selected)
{
    is_selected = selected;
    if (selected) {
        add_css_class("node-selected");
        remove_css_class("node");
    } else {
        add_css_class("node");
        remove_css_class("node-selected");
    }
    return selected;
}

void FilterEditorNode::get_position(double &x, double &y)
{
    x = this->x;
    y = this->y;
}

void FilterEditorNode::update_position(double x, double y)
{
    this->x = x;
    this->y = y;
}

FilterEditorSink* FilterEditorNode::get_next_available_sink(){
    for(auto sink : sinks){
        if(sink->can_add_connection()){
            return sink;
        }
    }   
    return nullptr;
}

void FilterEditorNode::add_connected_node(FilterEditorSource* source, FilterEditorNode* node, FilterEditorConnection* conn){
    // connected_down_nodes[source] = node;
    // connected_down_nodes.insert({source, node});
    connected_down_nodes.push_back({source, node});
    connections.push_back(conn);
}
void FilterEditorNode::add_connected_node(FilterEditorSink* sink, FilterEditorNode* node, FilterEditorConnection* conn){
    // connected_up_nodes[sink] = node;
    // connected_up_nodes.insert({sink, node});
    connected_up_nodes.push_back({sink, node});
    connections.push_back(conn);
}

std::vector<std::pair<FilterEditorSink*, FilterEditorNode*>> FilterEditorNode::get_connected_up_nodes(){ 
    return connected_up_nodes;
}
std::vector<std::pair<FilterEditorSource*, FilterEditorNode*>> FilterEditorNode::get_connected_down_nodes(){
    if(dynamic_cast<FilterEditorPrimitiveNode*>(this) != nullptr){
        for(auto it: connected_down_nodes){
            if(dynamic_cast<FilterEditorPrimitiveNode*>(it.second) != nullptr){
            }
        }
    }
    // for(auto it: connected_down_nodes){
    //     g_message("Connected down node is %s from %s", it.second->get_primitive()->getRepr()->name(), it.first->get_parent_node()->get_primitive()->getRepr()->name());
    // }
    return connected_down_nodes;
}

void FilterEditorNode::set_result_string(std::string _result_string) {
    result_string = _result_string;
}
void FilterEditorNode::set_sink_result(FilterEditorSink* sink, std::string result){
    sink->set_result_inp(-1, result);    
}

void FilterEditorNode::set_sink_result(FilterEditorSink* sink, int inp_index){
    sink->set_result_inp(inp_index);

}
std::string FilterEditorNode::get_result_string(){ 
    return result_string;
}

SPFilterPrimitive* FilterEditorPrimitiveNode::get_primitive(){
    return primitive;
}

FilterEditorSource* FilterEditorPrimitiveNode::get_source(){

    return sources[0];
}

void FilterEditorPrimitiveNode::update_position_from_document(){
    x = primitive->getRepr()->getAttributeDouble("inkscape:filter-x", x);
    y = primitive->getRepr()->getAttributeDouble("inkscape:filter-y", y);
}

void FilterEditorPrimitiveNode::set_result_string(std::string _result_string){
    result_string = _result_string;
    get_primitive()->getRepr()->setAttribute("result", result_string.c_str());
}
void FilterEditorPrimitiveNode::update_sink_results(){
    std::vector<Glib::ustring> attr_strings = {"in", "in2"};
    // g_message("In here");
    // if(dynamic_cast<FilterEditorPrimitiveMergeNode*>(this) != nullptr){
    //     g_message("This shouldn't be happening right?");
    //     return;
    // }
    const std::vector<Glib::ustring> result_inputs = {"SourceGraphic", "SourceAlpha", "BackgroundImage", "BackgroundAlpha", "FillPaint", "StrokePaint"};
    for(int i = 0; i != sinks.size(); i++){
        auto x = get_primitive();
        if(get_primitive()->getRepr() != nullptr){
            if(get_primitive()->getRepr()->attribute(attr_strings[i].c_str()) != nullptr){
                if(std::find(result_inputs.begin(), result_inputs.end(), get_primitive()->getRepr()->attribute(attr_strings[i].c_str())) != result_inputs.end()){
                    set_sink_result(sinks[i], std::find(result_inputs.begin(), result_inputs.end(), get_primitive()->getRepr()->attribute(attr_strings[i].c_str())) - result_inputs.begin());
                }
                else{
                    set_sink_result(sinks[i], get_primitive()->getRepr()->attribute(attr_strings[i].c_str()));
                }
                // sinks[i]->set_result_inp(-1, get_primitive()->getRepr()->attribute(attr_strings[i].c_str()));
            }
            else{
                set_sink_result(sinks[i], 0);
            } 
        }else {
            g_error("The problem is here"); 
        }
    }
}
// Just a random comment

FilterEditorSink* FilterEditorPrimitiveNode::get_sink(int index){
    return sinks[index];
}

void FilterEditorPrimitiveNode::set_sink_result(FilterEditorSink* sink, std::string result_string){
    if(std::find(sinks.begin(), sinks.end(), sink) != sinks.end()){
        if(std::find(sinks.begin(), sinks.end(), sink) - sinks.begin() == 0){
            // _dialog.set_attr(primitive, SPAttr::IN_, result_string.c_str());
            primitive->getRepr()->setAttribute("in", result_string.c_str());
        }
        else if(std::find(sinks.begin(), sinks.end(), sink) - sinks.begin() == 1){
            // _dialog.set_attr(primitive, SPAttr::IN2, result_string.c_str());
            primitive->getRepr()->setAttribute("in2", result_string.c_str());
        }

        sink->set_result_inp(-1, result_string);
    }
}

void FilterEditorPrimitiveNode::set_sink_result(FilterEditorSink* sink, int inp_index){
    // g_message("Entered with value %d", inp_index);
    if(inp_index != -2){
        auto res_string = sink->get_result_inputs(inp_index);
        if (std::find(sinks.begin(), sinks.end(), sink) != sinks.end()) {
            if (std::find(sinks.begin(), sinks.end(), sink) - sinks.begin() == 0) {
                // _dialog.set_attr(primitive, SPAttr::IN_, result_string.c_str());
                primitive->getRepr()->setAttribute("in", res_string.first.c_str());
            } else if (std::find(sinks.begin(), sinks.end(), sink) - sinks.begin() == 1) {
                // _dialog.set_attr(primitive, SPAttr::IN2, result_string.c_str());
                primitive->getRepr()->setAttribute("in2", res_string.first.c_str());
            }

            sink->set_result_inp(inp_index);
        }
    }
    else{
        sink->set_result_inp(inp_index);
        auto res_string = sink->get_result_inputs();
        if (std::find(sinks.begin(), sinks.end(), sink) != sinks.end()) {
            if (std::find(sinks.begin(), sinks.end(), sink) - sinks.begin() == 0) {
                // _dialog.set_attr(primitive, SPAttr::IN_, result_string.c_str());
                primitive->getRepr()->setAttribute("in", res_string.first.c_str());
            } else if (std::find(sinks.begin(), sinks.end(), sink) - sinks.begin() == 1) {
                // _dialog.set_attr(primitive, SPAttr::IN2, result_string.c_str());
                primitive->getRepr()->setAttribute("in2", res_string.first.c_str());
            }
        }
        
    }
}

std::string FilterEditorPrimitiveNode::get_result_string(){
    // g_message("Entered the function");
    if(primitive->getRepr()->attribute("result") == nullptr){
        auto result = cast<SPFilter>(primitive->parent)->get_new_result_name();
        primitive->getRepr()->setAttribute("result", result.c_str());
        result_string = result;
    }
    else{
        result_string = primitive->getRepr()->attribute("result");
    }

    return result_string;
}

// void FilterEditorPrimitiveMergeNode::map_sink_to_node(FilterEditorSink* sink, SPFeMergeNode* node){
//     sink_nodes.erase(sink_nodes.find(sink));
//     sink_nodes.insert({sink, node});
// }

void FilterEditorPrimitiveMergeNode::create_sink_merge_node(FilterEditorSink* sink, FilterEditorPrimitiveNode* prev_node){
    if(sink_nodes.find(sink) == sink_nodes.end()){
        Inkscape::XML::Document *xml_doc = primitive->document->getReprDoc();
        Inkscape::XML::Node *repr = xml_doc->createElement("svg:feMergeNode");
        repr->setAttribute("inkscape:collect", "always");

        primitive->getRepr()->appendChild(repr);
        auto node = cast<SPFeMergeNode>(primitive->document->getObjectByRepr(repr));
        Inkscape::GC::release(repr);
        // _dialog.set_attr(node, SPAttr::IN_, prev_node->get_primitive()->getRepr());

        if (sink_nodes.find(sink) != sink_nodes.end()) {
            // sink_nodes.find(sink)->second = node;
            sink_nodes[sink] = node;
        } else {
            sink_nodes.insert({sink, node});
        }
    }

}

void FilterEditorPrimitiveMergeNode::add_sink(){
    // Create a new sink 
    auto sink = Gtk::make_managed<FilterEditorSink>(this, 1);
    sinks.push_back(sink);
    sink_dock.append(*sink); 
}

void FilterEditorPrimitiveMergeNode::add_sink(SPFeMergeNode* node){
    // g_message("%d", __LINE__);
    add_sink();
    auto sink = sinks.back();
    sink_nodes.insert({sink, node});
}

void FilterEditorPrimitiveMergeNode::remove_extra_sinks(){
    for(auto it = sinks.begin(); it != sinks.end();){
        // if((*it)->get_connections().size() == 0){
        sink_dock.remove(*(*it));
        // auto itt = sink_nodes.find(*it);
        // if (itt != sink_nodes.end()) {
        //     sink_nodes.erase(sink_nodes.find(*it));
        // }
        it = sinks.erase(it); 
    } 
    sink_nodes.clear();
}
// Update the sink result in the document to result, for the node corresponding to the index. Should be called only if the sink has a node mapped to it.
void FilterEditorPrimitiveMergeNode::set_sink_result(FilterEditorSink* sink, std::string result){
    if(sink_nodes.find(sink) != sink_nodes.end()){
        auto node = sink_nodes[sink];
        node->setAttribute("in", result.c_str());
    }
    else{
        // return false;
    }

}

void FilterEditorPrimitiveMergeNode::update_sink_results(){
    const std::vector<Glib::ustring> result_inputs = {"SourceGraphic", "SourceAlpha", "BackgroundImage", "BackgroundAlpha", "FillPaint", "StrokePaint"};
    for(int i = 0; i != sinks.size(); i++){
        
        if(sink_nodes.find(sinks[i]) != sink_nodes.end()){
            auto fe_merge_node = sink_nodes[sinks[i]];
            if(fe_merge_node->getAttribute("in") != nullptr){
                if(std::find(result_inputs.begin(), result_inputs.end(), fe_merge_node->getAttribute("in")) != result_inputs.end()){
                    // set_sink_result(sinks[i], std::find(result_inputs.begin(), result_inputs.end(), fe_merge_node->getAttribute("in"))-result_inputs.begin());
                }
                else{
                    set_sink_result(sinks[i], fe_merge_node->getAttribute("in"));
                }
            }
        }
    }
}


FilterEditorSink* FilterEditorOutputNode::get_sink(){
    return this->sinks[0];
}


void FilterEditorOutputNode::set_sink_result(FilterEditorSink* sink, std::string result_string){
    return;
}
void FilterEditorOutputNode::set_sink_result(FilterEditorSink* sink, int inp_index){
    return;
}
void FilterEditorOutputNode::update_position_from_document(){
    x = filter->getRepr()->getAttributeDouble("inkscape:output-x", x);
    y = filter->getRepr()->getAttributeDouble("inkscape:output-y", y);
}
FilterEditorSource::FilterEditorSource(FilterEditorNode* _node, Glib::ustring _label_string)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
    , label_string(_label_string)
    , node(_node)
{
    set_name("filter-node-source");
    Glib::RefPtr<Gtk::StyleContext> context = get_style_context();
    Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
    set_size_request(15, 15);
    Glib::ustring style = Inkscape::IO::Resource::get_filename(Inkscape::IO::Resource::UIS, "node-editor.css");
    provider->load_from_path(style);
    context->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    add_css_class("nodesource");
};
FilterEditorNode *FilterEditorSource::get_parent_node()
{
    return node;
};

std::vector<FilterEditorConnection*>& FilterEditorSource::get_connections()
{
    return connections;
};

bool FilterEditorSource::add_connection(FilterEditorConnection *connection)
{
    connections.push_back(connection);
    update_width();
    return true;
};
bool connectionCompare(FilterEditorConnection*& a, FilterEditorConnection*& b){
    FilterEditorConnection* t1 = static_cast<FilterEditorConnection*>(a);
    FilterEditorConnection* t2 = static_cast<FilterEditorConnection*>(b);
    double xa1, xa2, ya1, ya2, xb1, xb2, yb1, yb2;
    t1->get_position(xa1, ya1, xa2, ya2);
    t2->get_position(xb1, yb1, xb2, yb2);
    // g_message("Sorting %lf %lf and %lf %lf", xa2, ya2, xb2, yb2);
    if(xa2 < xb2){
        // g_message("Sorting 1");
        return 1;
    }
    return 0;
}
void FilterEditorSource::sort_connections(){
    std::sort(connections.begin(), connections.end(), connectionCompare);
}
bool FilterEditorSource::get_selected(){
    return node->get_selected();
}

bool FilterEditorSink::get_selected(){
    return node->get_selected();
}


FilterEditorFixed::FilterEditorFixed(std::map<int, std::vector<FilterEditorConnection *>> &_connections, FilterEditorCanvas* _canvas, double _x_offset, double _y_offset)
    : canvas(_canvas)
    , x_offset(_x_offset)
    , y_offset(_y_offset)
    , connections(_connections) {};
void FilterEditorFixed::update_positions(double x_offset_new, double y_offset_new)
{
    x_offset = x_offset_new;
    y_offset = y_offset_new;
    for (auto child : get_children()) {
        if(dynamic_cast<FilterEditorPrimitiveNode*>(child) != nullptr){
            double x, y;
            dynamic_cast<FilterEditorPrimitiveNode*>(child)->get_position(x, y);
        }
    }
}

double FilterEditorFixed::get_x_offset()
{
    return x_offset;
}
double FilterEditorFixed::get_y_offset()
{
    return y_offset;
}

void FilterEditorFixed::update_offset(double x, double y)
{
    x_offset = x;
    y_offset = y;
}


// void FilterEditorFixed::sort_connections(std::vector<FilterEditorConnection*>&){


// }
void FilterEditorFixed::snapshot_vfunc(const std::shared_ptr<Gtk::Snapshot> &snapshot)
{
    auto const cr = snapshot->append_cairo(get_allocation());
    const double t = 100;
    if(canvas->current_event_type == FilterEditorCanvas::FilterEditorEvent::CONNECTION_UPDATE){
        cr->set_source_rgba(1.0, 1.0, 1.0, 1.0);
        cr->set_line_width(5.0);
        double x1, y1, x2, y2;
        x1 = canvas->drag_global_coordinates.first.first;
        y1 = canvas->drag_global_coordinates.first.second;
        x2 = canvas->drag_global_coordinates.second.first;
        y2 = canvas->drag_global_coordinates.second.second;
        auto alloc = canvas->starting_source->get_allocation();
        // canvas->starting_source 
        canvas->starting_source->translate_coordinates(*this, alloc.get_width()/2, alloc.get_height()/2, x1, y1);
        double x2_l, y2_l;
        canvas->global_to_local(x2, y2, x2_l, y2_l);
        cr->move_to(x1, y1);
        // cr->curve_to(x1, y1 + t, x2_l, y2_l - t, x2_l, y2_l);
        cr->line_to(x2_l, y2_l);
        cr->stroke();
        cr->close_path();
    }
    else if(canvas->current_event_type == FilterEditorCanvas::FilterEditorEvent::INVERTED_CONNECTION_UPDATE){
        cr->set_source_rgba(1.0, 1.0, 1.0, 1.0);
        cr->set_line_width(5.0);
        double x1, y1, x2, y2;
        x1 = canvas->drag_global_coordinates.first.first;
        y1 = canvas->drag_global_coordinates.first.second;
        x2 = canvas->drag_global_coordinates.second.first;
        y2 = canvas->drag_global_coordinates.second.second;
        auto alloc = canvas->starting_sink->get_allocation();
        canvas->starting_sink->translate_coordinates(*this, alloc.get_width()/2, alloc.get_height()/2, x1, y1);
        double x2_l, y2_l;
        canvas->global_to_local(x2, y2, x2_l, y2_l);
        cr->move_to(x1, y1);
        cr->line_to(x2_l, y2_l);
        // cr->curve_to(x1, y1 - t, x2_l, y2_l + t, x2_l, y2_l);
        cr->stroke();
        cr->close_path();
    }
    // g_message("size of connections is %ld", connections[canvas->current_filter_id].size());
    for (auto conn : canvas->connections[canvas->current_filter_id]) {

        double x1, y1, x2, y2;
        conn->get_source()->sort_connections();
        conn->get_position(x1, y1, x2, y2);
        double zoom_fac = canvas->get_zoom_factor();
        // x1 /= zoom_fac;
        // x2 /= zoom_fac;
        // y1 /= zoom_fac;
        // y2 /= zoom_fac;
        
        auto gradient = Cairo::LinearGradient::create(x1, y1, x2, y2);
        gradient->add_color_stop_rgba(1.0, 0.1, 0.1, 0.1, 1.0);
        // gradient->add_color_stop_rgba(0.0, 0.3607843137254902, 0.0, 0.47843137254901963, 1.0);  // Red at 0%
        // gradient->add_color_stop_rgba(1.0, 0.0, 0.28627450980392155, 0.47843137254901963, 1.0); // Blue at 100%
        

        cr->set_line_cap(Cairo::Context::LineCap::ROUND);
        cr->set_line_join(Cairo::Context::LineJoin::ROUND);

        if (1) {
            auto gradient = Cairo::LinearGradient::create(x1, y1, x2, y2);
            double opacity = 0.1;
            gradient->add_color_stop_rgba(0.0, 1.0, 1.0, 1.0,
                                          opacity + (1.0 - opacity) * conn->get_source_node()->get_selected()); // Red at 0%
            gradient->add_color_stop_rgba(1.0, 1.0, 1.0, 1.0,
                                          opacity + (1.0 - opacity) * conn->get_sink_node()->get_selected()); // Blue at 100%
            cr->set_source(gradient);

            cr->set_line_width(7.0);
#ifdef CURVE_1
            cr->move_to(x1, y1);
            cr->line_to(x1, (y1 + y2) / 2);
            cr->line_to(x2, (y1 + y2) / 2);
            cr->line_to(x2, y2);
#endif
#ifdef CURVE_2
            int threshold = 20;
            if (y2 - y1 < threshold) {
                int extension_length = 20;
                cr->move_to(x1, y1);
                cr->line_to(x1, y1 + extension_length);
                cr->line_to((x1 + x2) / 2, y1 + extension_length);
                cr->line_to((x1 + x2) / 2, y2 - extension_length);
                cr->line_to(x2, y2 - extension_length);
                cr->line_to(x2, y2);
            } else {
                cr->move_to(x1, y1);
                cr->line_to(x1, (y1 + y2) / 2);
                cr->line_to(x2, (y1 + y2) / 2);
                cr->line_to(x2, y2);
            }

#endif
            // cr->move_to(x1, y1);
            // // cr->curve_to(x1, y1 + t, x2, y2 - t, x2, y2);
            // cr->line_to(x1, (y1 + y2) / 2);
            // cr->line_to(x2, (y1 + y2) / 2);
            // // cr->curve_to(x1, y1 + t, x2, y2 - t, x2, y2);
            // cr->line_to(x2, y2);
            cr->stroke();

            cr->close_path();
        }
        cr->set_source(gradient);
        cr->set_line_width(5.0);
        #ifdef CURVE_1
        cr->move_to(x1, y1);
        cr->line_to(x1, (y1+y2)/2);
        cr->line_to(x2, (y1+y2)/2);
        cr->line_to(x2, y2); 
        #endif
        #ifdef CURVE_2
        int threshold = 20;
        if(y2 - y1 < threshold){
            int extension_length = 20;
            cr->move_to(x1, y1);
            cr->line_to(x1, y1+extension_length);
            cr->line_to((x1+x2)/2, y1+extension_length);
            cr->line_to((x1+x2)/2, y2-extension_length);
            cr->line_to(x2, y2-extension_length);
            cr->line_to(x2, y2);
        }
        else{
            cr->move_to(x1, y1);
            cr->line_to(x1, (y1 + y2) / 2);
            cr->line_to(x2, (y1 + y2) / 2);
            cr->line_to(x2, y2);
        }


        #endif
        // cr->curve_to(x1, y1 + t, x2, y2 - t, x2, y2);
        cr->stroke();
        cr->close_path();
    } 

    Gtk::Fixed::snapshot_vfunc(snapshot);
};
void FilterEditorConnection::get_position(double &x1, double &y1, double &x2, double &y2)
{
    double x_o, y_o, x, y;
    auto alloc = source->get_allocation();
    // source->translate_coordinates(*(canvas->get_child()), alloc.get_width() / 2, alloc.get_height() / 2, x, y);
    source->get_connection_starting_coordinates(x_o, y_o, this);
    source->translate_coordinates(*(canvas->get_canvas()), x_o, y_o, x, y);
    x1 = x;
    y1 = y;
    alloc = sink->get_allocation();
    sink->translate_coordinates(*(canvas->get_canvas()), alloc.get_width() / 2, alloc.get_height() / 2, x, y);
    x2 = x;
    y2 = y;
}
FilterEditorNode* FilterEditorConnection::get_source_node(){
    return source_node;
}
FilterEditorNode* FilterEditorConnection::get_sink_node(){
    return sink_node;
}
FilterEditorSource* FilterEditorConnection::get_source()
{
    return source;
}
FilterEditorSink* FilterEditorConnection::get_sink()
{
    return sink;
}


FilterEditorCanvas::FilterEditorCanvas(FilterEffectsDialog& dialog)
    // : Gtk::Viewport()
    : Gtk::ScrolledWindow()
    // : Gtk::Grid()
    // : Gtk::Window()
    , canvas(this->connections, this)
    , _dialog(dialog)
    , _popover_menu(create_menu())
{
    auto ptr = new Gtk::Box(Gtk::Orientation::VERTICAL, 40);
    auto label = new Gtk::Label("Hello");
    ptr->insert_child_at_start(*label);
    set_name("filter-canvas");
    set_focusable();
    canvas.set_focusable();
    canvas.grab_focus();
    auto controllers = observe_controllers();
    int i = 0;
    while(controllers->get_object(i) != nullptr){
        auto obj = controllers->get_typed_object<Gtk::EventControllerScroll>(i);
        if(obj == nullptr){
            g_message("Scroll controller not found");
            
        }
        else{
            g_message("scroll controller found");
            this->remove_controller(obj);
            // break;
        }
        i++;
    }
    // this->remove_controller(this->scroll);
    

    zoom_fac = 1.0;
    set_kinetic_scrolling(false);
    // set_overlay_scrolling(false);
    set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    // set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::NEVER);
    
    set_child(canvas);
    // attach(canvas, 0, 0, 1, 1);
    canvas.set_overflow(Gtk::Overflow::HIDDEN);
    // canvas.set_expand(true);
    Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
    add_css_class("canvas");
    canvas.set_name("filter-canvas-fixed");

    /*TODO: move the testing CSS file to the right place*/
    Glib::ustring style = Inkscape::IO::Resource::get_filename(Inkscape::IO::Resource::UIS, "node-editor.css");
    provider->load_from_path(style);
    // provider->load_from_path("/home/phantomzback/Documents/GSOC_Projs/inkscape_final/testing.css");
    canvas.get_style_context()->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    get_style_context()->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    canvas.add_css_class("canvas-fixed");

    rubberband_rectangle = std::make_shared<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    rubberband_rectangle->set_name("rubberband-rectangle");
    rubberband_rectangle->get_style_context()->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    rubberband_rectangle->add_css_class("rubberband");

    initialize_gestures();
    
    // Setup Canvas
};
        // ~FilterEditorCanvas() override;
std::unique_ptr<UI::Widget::PopoverMenu> FilterEditorCanvas::create_menu()
{
    auto menu = std::make_unique<UI::Widget::PopoverMenu>(Gtk::PositionType::BOTTOM);
    auto append = [&](Glib::ustring const &text, auto const mem_fun)
    {
        auto &item = *Gtk::make_managed<UI::Widget::PopoverMenuItem>(text, true);
        item.signal_activate().connect(sigc::mem_fun(*this, mem_fun));
        menu->append(item);
    };
    append(_("_Duplicate selected nodes"            ), &FilterEditorCanvas::duplicate_nodes      );
    append(_("_Remove selected nodes"               ), &FilterEditorCanvas::delete_nodes         );
    return menu;
}

FilterEditorOutputNode* FilterEditorCanvas::create_output_node(SPFilter* filter, double x, double y, Glib::ustring label_text){
    std::unique_ptr<FilterEditorOutputNode> output_node_derived = std::make_unique<FilterEditorOutputNode>(100, filter, x, y, label_text, 1);
    std::unique_ptr<FilterEditorNode> output_node_new = std::move(output_node_derived); 
    place_node(dynamic_cast<FilterEditorNode*>(output_node_new.get()), x, y, true); 
    if(output_node_new.get() == nullptr)
        g_error("There's some problem here");
    nodes[current_filter_id].push_back(std::move(output_node_new)); 
    auto ret_ptr = nodes[current_filter_id].back().get(); 
    return (dynamic_cast<FilterEditorOutputNode*>(ret_ptr));
}

// TODO: Improve visited performance
// void FilterEditorCanvas::create_nodes_order(FilterEditorPrimitiveNode* node, std::vector<FilterEditorPrimitiveNode*>& nodes_order, std::set<FilterEditorPrimitiveNode*>& visited){
void FilterEditorCanvas::create_nodes_order(FilterEditorPrimitiveNode* prev_node, FilterEditorPrimitiveNode* node, std::vector<FilterEditorPrimitiveNode*>& nodes_order, std::map<FilterEditorPrimitiveNode*, std::pair<int, int>>& visited, bool dir, bool reset){
    // Dir true is up and false is down
    modify_observer(true);
    static int current_iter = 0;
    static bool new_iter = false;
    if(reset){
        current_iter = 0;
    }
    if(new_iter || prev_node == nullptr){
        current_iter++;
        visited.find(node)->second.first = current_iter;
        visited.find(node)->second.second = 0;
        node->get_primitive()->setAttribute("inkscape:vis1", std::to_string(current_iter).c_str());
        node->get_primitive()->setAttribute("inkscape:vis2", std::to_string(0).c_str());
        new_iter = false;
    }
    else{
        auto it = visited.find(node);
        auto it_prev = visited.find(prev_node);
        if(dir == true){
            if(it->second.first == -1 || it_prev->second.second+1 > it->second.second){
                it->second.first = it_prev->second.first;
                
                it->second.second = std::max(it->second.second, it_prev->second.second + 1);
                node->get_primitive()->setAttribute("inkscape:vis1", std::to_string(it->second.first).c_str());
                node->get_primitive()->setAttribute("inkscape:vis2", std::to_string(it->second.second).c_str());
            }
            else{
                return;
            }
            // it->second.first = it_prev->second.first;
            
            // it->second.second = std::max(it->second.second, it_prev->second.second + 1);
        }
        else{
            if(it->second.first == -1 || it_prev->second.second-1 < it->second.second){
                if(it->second.first != -1){
                    it->second.second = std::min(it->second.second, it_prev->second.second - 1);
                }
                else{
                    it->second.second = it_prev->second.second - 1;
                }
                it->second.first = it_prev->second.first;
                node->get_primitive()->setAttribute("inkscape:vis1", std::to_string(it->second.first).c_str());
                node->get_primitive()->setAttribute("inkscape:vis2", std::to_string(it->second.second).c_str());
            }
            else{
                return;
            }
            // if(it->second.first != -1){
            //     it->second.second = std::min(it->second.second, it_prev->second.second - 1);
            // }
            // else{
            //     it->second.second = it_prev->second.second - 1;
            // }
            // it->second.first = it_prev->second.first;

        }
    }
    int i = 0;
    if(!is<SPFeMerge>(node->get_primitive()) || is<SPFeMerge>(node->get_primitive())){
        auto connected_up_nodes = node->get_connected_up_nodes();
        auto connected_down_nodes = node->get_connected_down_nodes();
        // if(connected_up_nodes.size() == 0){
        //     current_iter++;
        // } 
        for(int i = 0; i != connected_up_nodes.size(); i++){
            if(dynamic_cast<FilterEditorPrimitiveNode*>(connected_up_nodes[i].second) != nullptr){
                if(dynamic_cast<FilterEditorPrimitiveNode*>(connected_up_nodes[i].second) == prev_node){
                    continue;
                }
                else{
                    create_nodes_order(node, dynamic_cast<FilterEditorPrimitiveNode *>(connected_up_nodes[i].second),
                                       nodes_order, visited, true, false);
                }

            }
        }
        for(int i = 0; i != connected_down_nodes.size(); i++){
            if (dynamic_cast<FilterEditorPrimitiveNode *>(connected_down_nodes[i].second) == prev_node) {
                continue;
            }
            if(dynamic_cast<FilterEditorPrimitiveNode*>(connected_down_nodes[i].second) != nullptr){
                create_nodes_order(node,
                                   dynamic_cast<FilterEditorPrimitiveNode *>(connected_down_nodes[i].second),
                                   nodes_order, visited, false, false);

            }
        }
        // for(int i = 0; i != connected_up_nodes.size(); i++){
        //     if(dynamic_cast<FilterEditorPrimitiveNode*>(connected_up_nodes[i].second) != nullptr){
        //         if(dynamic_cast<FilterEditorPrimitiveNode*>(connected_up_nodes[i].second) == prev_node){
        //             continue;
        //         }
        //         if(visited.find(dynamic_cast<FilterEditorPrimitiveNode*>(connected_up_nodes[i].second)) != visited.end()){
        //             total_counter++;
        //             auto it = visited.find(dynamic_cast<FilterEditorPrimitiveNode*>(connected_up_nodes[i].second));
        //             auto it_this = visited.find(node);
        //             if (it->second.first == -1 || it_this->second.second + 1 > it->second.second) {
        //                 it->second.first = current_iter; // index of the dag
        //                 it->second.second = std::max(it->second.second, it_this->second.second + 1);

        //                 node->get_primitive()->setAttribute("inkscape:vis1", std::to_string(current_iter).c_str());
        //                 node->get_primitive()->setAttribute("inkscape:vis2", std::to_string(std::max(it->second.second, it_this->second.second + 1)).c_str());
        //                 create_nodes_order(node,
        //                                    dynamic_cast<FilterEditorPrimitiveNode *>(connected_up_nodes[i].second),
        //                                    nodes_order, visited, false);
        //             }
        //         }
        //     }
        // }
        // for(int i = 0; i != connected_down_nodes.size(); i++){
        //     if(dynamic_cast<FilterEditorPrimitiveNode*>(connected_down_nodes[i].second) != nullptr){
        //         if(dynamic_cast<FilterEditorPrimitiveNode*>(connected_down_nodes[i].second) == prev_node){
        //             continue;
        //         }
        //         if(visited.find(dynamic_cast<FilterEditorPrimitiveNode*>(connected_down_nodes[i].second)) != visited.end()){
        //             total_counter++;
        //             auto it = visited.find(dynamic_cast<FilterEditorPrimitiveNode*>(connected_down_nodes[i].second));
        //             auto it_this = visited.find(node);
        //             if (it->second.first == -1 || it_this->second.second - 1 < it->second.second) {
        //                 if(it->second.first == -1 || it->second.second == G_MININT){
        //                     it->second.second = it_this->second.second - 1;
        //                 }
        //                 else{
        //                     it->second.second = std::min(it->second.second, it_this->second.second - 1);
        //                 }
        //                 it->second.first = current_iter; // index of the dag
        //                 node->get_primitive()->setAttribute("inkscape:vis1", std::to_string(current_iter).c_str());
        //                 node->get_primitive()->setAttribute("inkscape:vis2", std::to_string(std::min(it->second.second, it_this->second.second - 1)).c_str());
        //                 create_nodes_order(node,
        //                                    dynamic_cast<FilterEditorPrimitiveNode *>(connected_down_nodes[i].second),
        //                                    nodes_order, visited, false);
        //             }
        //         }
        //     }
        // }
        // if(prev_node == nullptr){
        //     current_iter++;
        //     new_iter = true;
        // }
        // if(total_counter == 0){
        //     current_iter++;
        //     new_iter = true;
        // }
           
    }
    // else{
    //     auto merge_node = dynamic_cast<FilterEditorPrimitiveMergeNode*>(node);
    // }
    // else{
    //     auto merge_node = static_cast<FilterEditorPrimitiveMergeNode*>(node);
    //     for(auto sink:merge_node->sink_nodes){
    //         if(sink.second != nullptr){
    //             auto above_node = sink.first->get_connections()[0]->get_source_node();
    //             auto above_primitive = dynamic_cast<FilterEditorPrimitiveNode*>(above_node)->get_primitive();
    //             auto repr = above_primitive->getRepr();
    //             // const gchar *gres = repr->attribute("result");
    //             modify_observer(true);
    //             auto result_string = above_node->get_result_string();
    //             modify_observer(false);
    //             Glib::ustring in_val;
    //             if (result_string == "") {
    //                 auto result = cast<SPFilter>(above_primitive->parent)->get_new_result_name();
    //                 repr->setAttributeOrRemoveIfEmpty("result", result);
    //                 in_val = result;
    //             } else {
    //                 in_val = result_string;
    //             }
    //             g_message("Connection from %d th sink to above node is: %s and in val is %s", i,
    //                       dynamic_cast<FilterEditorPrimitiveNode *>(above_node)->get_primitive()->getRepr()->name(),
    //                       in_val.c_str());
    //             if (i == 0) {
    //                 _dialog.set_attr(sink.second, SPAttr::IN_, in_val.c_str());
    //             } 
    //             // create_nodes_order(dynamic_cast<FilterEditorPrimitiveNode *>(above_node), nodes_order, visited);
    //         }
    //     }
    // }
    // for(auto it : node->get_connected_up_nodes()){
    //     auto above_node = it.second;
    //     g_message("%s messaging here", node->get_primitive()->getRepr()->name());
        
    //     if(dynamic_cast<FilterEditorPrimitiveNode*>(above_node) != nullptr){
    //         auto above_primitive = dynamic_cast<FilterEditorPrimitiveNode*>(above_node)->get_primitive();
    //         auto repr = above_primitive->getRepr();
    //         const gchar *gres = repr->attribute("result");
    //         Glib::ustring in_val;
    //         if(!gres){
    //             auto result = cast<SPFilter>(above_primitive->parent)->get_new_result_name();
    //             repr->setAttributeOrRemoveIfEmpty("result", result);
    //             in_val = result;
    //         }
    //         else{
    //             in_val = gres;
                
    //         }
    //         if(!is<SPFeMerge>(node->get_primitive())){
    //             i = std::find(node->sinks.begin(), node->sinks.end(), it.first) - node->sinks.begin();
    //             g_message("Connection from %d th sink to above node is: %s and in val is %s", i,
    //                       dynamic_cast<FilterEditorPrimitiveNode *>(above_node)->get_primitive()->getRepr()->name(), in_val.c_str());
    //             if (i == 0) {
    //                 _dialog.set_attr(node->get_primitive(), SPAttr::IN_, in_val.c_str());
    //             }
    //             else{
    //                 _dialog.set_attr(node->get_primitive(), SPAttr::IN2, in_val.c_str());
    //             }
    //         }
    //         else{
    //             auto merge_node = static_cast<FilterEditorPrimitiveMergeNode*>(node);
    //             for(auto sink:merge_node->sink_nodes){
    //                 if(sink.first->get_connections()[0]->get_source_node() == above_node){
    //                     _dialog.set_attr(sink.second, SPAttr::IN_, in_val.c_str());
    //                 }
    //                 // if(sink.second != nullptr){
    //                 //     _dialog.set_attr(sink.second, SPAttr::IN_, in_val.c_str());
    //                 // }     
    //             }
    //         }    
    //         create_nodes_order(dynamic_cast<FilterEditorPrimitiveNode*>(above_node), nodes_order, visited);
    //     }   
    // }
    // if(i==0){
    //     _dialog.set_attr(node->get_primitive(), SPAttr::IN_, "SourceGraphic");
    // }
    nodes_order.push_back(node);
    node->part_of_chain = true;
    modify_observer(false);
    return;

}

void FilterEditorCanvas::delete_nodes_without_prims(){
    // primitive_to_node.empty();
    modify_observer(true);
    // g_message("nodes size is %ld", nodes[current_filter_id].size());
    // if(filter)
    auto filter = _dialog._filter_modifier.get_selected_filter();
    int delete_counter = 0;
    if(filter){
        std::set<SPFilterPrimitive*> prims;
        for (auto &child : _dialog._filter_modifier.get_selected_filter()->children) {
            auto prim = cast<SPFilterPrimitive>(&child);
            if(prim != nullptr){
                prims.insert(prim);
            }
        }
        // g_message("Size of prims: %ld \n Size of nodes: %ld", prims.size(), nodes[current_filter_id].size());
        for (auto it = nodes[current_filter_id].begin(); it != nodes[current_filter_id].end();) {
            // auto node = it->get;
            auto node = it->get();
            FilterEditorPrimitiveNode* prim_node = dynamic_cast<FilterEditorPrimitiveNode*>(node);
            if (prim_node != nullptr && prims.find(prim_node->get_primitive()) == prims.end()) { 
                // g_message("Deleting node at %s", prim_node->get_result_string().c_str());
                delete_counter++;
                prim_node->unparent();
                for (auto connection : node->connections) {
                    destroy_connection(connection, false);
                }
                if (primitive_to_node.find(prim_node->get_primitive()) != primitive_to_node.end()) {
                    primitive_to_node.erase(primitive_to_node.find(prim_node->get_primitive()));
                }
                if (std::find(selected_nodes[current_filter_id].begin(), selected_nodes[current_filter_id].end(),
                              node) != selected_nodes[current_filter_id].end()) {
                    selected_nodes[current_filter_id].erase(std::find(selected_nodes[current_filter_id].begin(),
                                                                      selected_nodes[current_filter_id].end(), node));
                }
                while (node->sink_dock.get_first_child() != nullptr) {
                    node->sink_dock.remove(*node->sink_dock.get_first_child());
                }
                while (node->source_dock.get_first_child() != nullptr) {
                    node->source_dock.remove(*node->source_dock.get_first_child());
                }

                it = nodes[current_filter_id].erase(it);
            } else {
                it++;
            }
        }
        // g_message("Deleted a total of %d nodes", delete_counter);
    }
    else{
        // g_error("Filter doesn't seem to be selected after undoing");
    }
    // if(delete_counter != 0){
    // }
    // g_message("Nodes size for returning is %ld", nodes[current_filter_id].size());
    modify_observer(false);
    
}

void FilterEditorCanvas::delete_nodes(){
    // for(auto node : selected_nodes){

    modify_observer(true);
    int j = 0;
    auto filter = filter_list[current_filter_id];
    int x = 10;

    for(auto &child: filter->children){
        auto prim = cast<SPFilterPrimitive>(&child);
        prim->getRepr()->setPosition(x);
        x--;
    }
        
    // for(auto it = selected_nodes.begin(); it != selected_nodes.end();){
    for(auto it = selected_nodes[current_filter_id].begin(); it != selected_nodes[current_filter_id].end();){
        auto node = *it;
        
        if(dynamic_cast<FilterEditorOutputNode*>(node) == nullptr){
            // Delete all the connections
            for (auto connection : node->connections) {
                destroy_connection(connection);
            }
            while (node->sink_dock.get_first_child() != nullptr) {
                node->sink_dock.remove(*node->sink_dock.get_first_child());
            }
            while (node->source_dock.get_first_child() != nullptr) {
                node->source_dock.remove(*node->source_dock.get_first_child());
            }
            // Delete the node
            auto prim_node = dynamic_cast<FilterEditorPrimitiveNode*>(node);
            if (prim_node == nullptr){
            } else {
                auto prim = prim_node->get_primitive();
                // canvas.remove(*prim_node);
                prim_node->unparent();
                // it = selected_nodes.erase(it);
                it = selected_nodes[current_filter_id].erase(it);
                for(auto it2 = nodes[current_filter_id].begin(); it2 != nodes[current_filter_id].end();){
                    if((it2->get()) == node){
                        it2 = nodes[current_filter_id].erase(it2);
                    }
                    else{
                        it2++;
                    }
                }
                // nodes[current_filter_id].erase(std::remove(nodes[current_filter_id].begin(), nodes[current_filter_id].end(), node), nodes[current_filter_id].end());
                sp_repr_unparent(prim->getRepr()); 
            }
        }
        else{
            it++;
        }
    }
    // clear_selection();
    // canvas.put(*node_cache, 0, 0);
    update_document();
    canvas.queue_draw();
    modify_observer(false);
    

}


bool FilterEditorCanvas::primitive_node_exists(SPFilterPrimitive* primitive){
    if(primitive_to_node.find(primitive) != primitive_to_node.end()){
        return true;
    }
    else{
        return false;
    }
}


void FilterEditorCanvas::remove_filter(SPFilter* filter){
    modify_observer(true);
    if(filter){
        if(std::find(filter_list.begin(), filter_list.end(), filter) != filter_list.end()){
            int filter_id = std::find(filter_list.begin(), filter_list.end(), filter)-filter_list.end();
            selected_nodes[filter_id].clear();
            for(int i = 0; i != nodes[filter_id].size(); i++){
                selected_nodes[filter_id].push_back(nodes[filter_id][i].get());
            }
            delete_nodes();
            if(connections.find(filter_id) != connections.end())
                connections.erase(connections.find(filter_id));
            if (selected_nodes.find(filter_id) != selected_nodes.end())
                selected_nodes.erase(selected_nodes.find(filter_id));
            if (result_manager.find(filter_id) != result_manager.end()) 
                result_manager.erase(result_manager.find(filter_id));
            if (nodes.find(filter_id) != nodes.end())
                nodes.erase(nodes.find(filter_id));

        }
    }
}

void FilterEditorCanvas::update_editor(){
    SPFilter* filter = _dialog._filter_modifier.get_selected_filter();
    if(filter){
        
    }
}



/*
Update the canvas according to the current contents of the document.
Important things to note: This should never update any content of
the document on it's own since that would hamper with the undo system.
Everytime an undo is called, only update canvas is called.
*/
void FilterEditorCanvas::update_canvas_new(){
    // update_canvas_new()
    // g_message("Updating canvas, from new function %d", __LINE__);
    modify_observer(true);
    SPFilter* filter = _dialog._filter_modifier.get_selected_filter();
    clear_nodes();
    delete_nodes_without_prims();
    if(filter){
        auto filter_repr = filter->getRepr();
        if (std::find(filter_list.begin(), filter_list.end(), filter) == filter_list.end()){
            filter_list.push_back(filter);
            current_filter_id = filter_list.size()-1;
            nodes.insert({current_filter_id, std::vector<std::unique_ptr<FilterEditorNode>>()});
            selected_nodes.insert({current_filter_id, std::vector<FilterEditorNode*>()});
            connections.insert({current_filter_id, std::vector<FilterEditorConnection*>()});
            result_manager.insert({current_filter_id, std::map<Glib::ustring, FilterEditorPrimitiveNode*> ()});
            double x_position = filter->getRepr()->getAttributeDouble("inkscape:output-x", 100.0);
            double y_position = filter->getRepr()->getAttributeDouble("inkscape:output-y", 100.0);
            output_node = create_output_node(filter, x_position, y_position, "Output");
            // g_message("Output node created %d", __LINE__);
        }

        current_filter_id = std::find(filter_list.begin(), filter_list.end(), filter) - filter_list.begin();
        // Clear the connections and recreate them.
        if(output_node == nullptr){
            output_node = create_output_node(filter, 100, 100, "Output");
        }
        place_node(output_node, output_node->x, output_node->y);
        output_node->update_filter(filter);
        output_node->update_position_from_document();
        auto connections_copy = connections[current_filter_id];
        for(auto conn : connections_copy){
            destroy_connection(conn, false);
        } 
        connections[current_filter_id].clear();
        int count = 0, reuse_count = 0;
        std::map<Glib::ustring, FilterEditorPrimitiveNode*> result_to_primitive;
        std::vector<FilterEditorPrimitiveNode*> nodes_list;
        for(auto &child : filter->children){
            count++;
            auto prim = cast<SPFilterPrimitive>(&child);

            if(prim == nullptr){
                continue;
            }

            FilterEditorPrimitiveNode* primitive_node = nullptr;  
            if(primitive_node_exists(prim)){
                // g_message("The primitive node exists");
                reuse_count++;
                primitive_node = get_node_from_primitive(prim);
                if(primitive_node == nullptr){
                    g_error("There's some error here %d", __LINE__);
                }
                primitive_node->update_position_from_document();
                place_node(primitive_node, primitive_node->x, primitive_node->y);
            }
            else{
                auto type_id = FPConverter.get_id_from_key(prim->getRepr()->name());
                auto type = static_cast<Filters::FilterPrimitiveType>(type_id);
                int num_sinks = input_count(prim);
                double x_position = prim->getRepr()->getAttributeDouble("inkscape:filter-x", 100.0);
                double y_position = prim->getRepr()->getAttributeDouble("inkscape:filter-y", 50.0 + count * 100.0);
                // g_message("Adding node for %s", prim->getRepr()->name());
                primitive_node = add_primitive_node(prim, x_position, y_position, type, FPConverter.get_label(type), num_sinks);
                if(primitive_node == nullptr){
                    g_error("There's some error here %d", __LINE__);
                } 
            }

            if(primitive_node == nullptr){
                g_error("there's some problem here %d", __LINE__);
            }

            if(cast<SPFeMerge>(prim) != nullptr){
                // TODO: Implement merge
                int counter = 0;
                auto merge_node = dynamic_cast<FilterEditorPrimitiveMergeNode*>(primitive_node);
                if(merge_node == nullptr){
                    continue;
                }
                merge_node->remove_extra_sinks();
                auto merge = cast<SPFeMerge>(prim);
                // g_message("Size of sinks: %ld", merge_node->sinks.size());
                // g_message("Size of merge children %ld", merge->children.size());
                for(auto &child : merge->children){
                    auto merge_child = cast<SPFeMergeNode>(&child);
                    if(merge_child == nullptr){
                        continue;
                    }
                    auto inp = merge_child->getAttribute("in");
                    if (inp == nullptr) {
                        // Shouldn't be happening but should handle anyways

                        // TODO: Verify this behaviour
                        if (nodes_list.size() > 0) {
                            auto source_node = nodes_list.back();
                            auto source = source_node->get_source();
                            merge_node->add_sink(merge_child);
                            create_connection(source, merge_node->get_sink(counter++), true);
                        } else {
                            // Do nothing, we will fix this in the document
                            // merge_child->setAttribute("in", "SourceGraphic");
                        } 
                    } else {
                        if (std::find(result_inputs.begin(), result_inputs.end(), inp) != result_inputs.end()) {
                            continue;
                        } else {
                            if (result_to_primitive.find(inp) != result_to_primitive.end()) {
                                // Perfect, we found the node we were looking for. Now just create a connection between that and this sink of it.
                                auto source_prim = result_to_primitive[inp];
                                auto source = source_prim->get_source();
                                merge_node->add_sink(merge_child); 
                                create_connection(source, merge_node->get_sink(counter++), true);
                            } else {
                                if (nodes_list.size() > 0) {
                                    auto source_node = nodes_list.back();
                                    auto source = source_node->get_source();
                                    merge_node->add_sink(merge_child);
                                    create_connection(source, merge_node->get_sink(counter++), true);
                                } else {
                                    // Do nothing, we will fix this in the document
                                    // merge_child->setAttribute("in", "SourceGraphic");
                                }
                            }
                        }
                    }
                }
                merge_node->add_sink();
                // g_message("Size of sinks: %ld", merge_node->sinks.size());
                // g_message("Size of merge->children: %ld", merge->children.size());
            }
            else{
                int num_sinks = input_count(prim);
                Glib::ustring in_attributes[] = {"in", "in2"};
                for(int i = 0; i != num_sinks; i++){
                    // in_attributes[i];
                    auto inp = prim->getAttribute(in_attributes[i].c_str());
                    FilterEditorSink* sink;
                    if(primitive_node->sinks.size() > i)
                        sink = primitive_node->sinks[i];
                    else
                        g_error("There's an issue here");
                    if(inp == nullptr){
                        // Set to previous or SourceGraphic if previous is none;
                        if(nodes_list.size() > 0){
                            // g_message("Has no result, using previous");
                            auto source = (*nodes_list.rbegin())->get_source();
                            create_connection(source, sink, true);
                        }
                        else{
                            // TODO: Use source graphic, but don't update it over here, by default it should use SourceGraphic
                        }
                    } 
                    else{
                        if(std::find(result_inputs.begin(), result_inputs.end(), inp) != result_inputs.end()){
                            
                        }
                        else{
                            if(result_to_primitive.find(inp) != result_to_primitive.end()){
                                // g_message("Has a result, using result");
                                auto source_prim = result_to_primitive[inp];
                                auto source = source_prim->get_source();
                                create_connection(source, sink, true);
                            }
                            else{
                                // g_message("Has a result, unable to use result");
                                if(nodes_list.size() > 0){
                                    auto source_node = nodes_list.back();
                                    auto source = source_node->get_source();
                                    create_connection(source, sink, true);
                                }
                                else{
                                    // TODO: Use SourceGraphic
                                }
                            }
                        }
                        
                    }
                }
            }
            if(prim->getAttribute("result") != nullptr){
                // result_to_node = ""
                if(result_to_primitive.find(prim->getAttribute("result")) != result_to_primitive.end()){
                    result_to_primitive[prim->getAttribute("result")] = primitive_node;
                }
                else{
                    result_to_primitive.insert({prim->getAttribute("result"), primitive_node});
                }
                // result_to_primitive.insert({prim->getAttribute("result"), primitive_node});
            }
            
            nodes_list.push_back(primitive_node);
            

        }
        // g_message("Size of nodes: %d", )
        if(nodes_list.size() >= 1){
            create_connection(nodes_list.back()->get_source(), output_node->get_sink());

        }
        // Get the nodes corresponding to prim
    }

    modify_observer(false);
    

}

void FilterEditorCanvas::update_canvas(){
    update_canvas_new();
    return;
    g_message("Updating canvas from here %d", __LINE__);
    
    modify_observer(true);
    SPFilter* filter = _dialog._filter_modifier.get_selected_filter();
    
    if(filter){
        int i =0;
        for(auto &child : filter->children){
            auto prim = cast<SPFilterPrimitive>(&child);
            g_message("Index: %d", i);
            g_message("%s has in as %s", prim->getAttribute("id"), prim->getAttribute("in"));
            i++;
        }
    }
    clear_nodes();
    // return;
    
    if(filter){
        auto filter_repr = filter->getRepr();
        if (std::find(filter_list.begin(), filter_list.end(), filter) == filter_list.end()) {
            filter_list.push_back(filter);
            current_filter_id = filter_list.size() - 1;
            nodes.insert({current_filter_id, std::vector<std::unique_ptr<FilterEditorNode>>()});
            selected_nodes.insert({current_filter_id, std::vector<FilterEditorNode *>()});
            connections.insert({current_filter_id, std::vector<FilterEditorConnection *>()});
            result_manager.insert({current_filter_id, std::map<Glib::ustring, FilterEditorPrimitiveNode *>()});
            output_node = create_output_node(filter, 100, 100, "Output");
        }
        current_filter_id = std::find(filter_list.begin(), filter_list.end(), filter) - filter_list.begin();
        if(output_node != nullptr){
            g_message("output node exists");
            if(output_node->connected_up_nodes.size() == 0){
                g_message("output node has no connected up nodes");
            }
            else{

                
            }
        }
        else{
            g_message("output node doesn't exist");
        }

        
        g_message("\n\n\n");
        if (filter) {
            for (auto &child : filter->children) {
                auto prim = cast<SPFilterPrimitive>(&child);
            }
        }
        g_message("\n\n\n");
        delete_nodes_without_prims();

        std::map<Glib::ustring, SPFilterPrimitive*> result_to_primitive;
        std::map<Glib::ustring, Glib::ustring> old_to_new_result;
        int index = 0;

        for(auto &child : filter->children){
            if(cast<SPFilterPrimitive>(&child) != nullptr){
                auto prim = cast<SPFilterPrimitive>(&child);
                g_message("%s has in as %s", prim->getAttribute("id"), prim->getAttribute("in"));
            }
        }



        for(auto &child : filter->children){ 
            if(cast<SPFilterPrimitive>(&child) != nullptr){ 
                auto prim = cast<SPFilterPrimitive>(&child);
                g_message("At index %d: %s", index, prim->getAttribute("id"));
                index++;
                if (cast<SPFeMerge>(prim) == nullptr) {
                    for (int i = 0; i != input_count(prim); i++) {
                        Glib::ustring attr_str;
                        if (i == 0) {
                            attr_str = "in";
                        } else if (i == 1) {
                            attr_str = "in2";
                        }
                        auto inp = prim->getAttribute(attr_str.c_str());
                        g_message("%s has %s", attr_str.c_str(), inp);
                        if (inp == nullptr) {
                            Glib::ustring new_result;
                            if (prim->getPrev() != nullptr) {
                                for (auto it : result_to_primitive) {
                                    if (it.second == prim->getPrev()) {
                                        new_result = it.first;
                                        break;
                                    }
                                }
                            } else {
                                g_message("Changing in to SourceGraphic because prim's prev is null pointer and it currently doesn't have an inp");
                                new_result = "SourceGraphic";
                            }
                            prim->setAttribute(attr_str.c_str(), new_result.c_str());
                        } else {
                            if (std::find(result_inputs.begin(), result_inputs.end(), inp) != result_inputs.end()) {
                                continue;
                            }
                            if (old_to_new_result.find(inp) != old_to_new_result.end()) {
                                Glib::ustring new_result = old_to_new_result.find(inp)->second;
                                prim->setAttribute(attr_str.c_str(), new_result.c_str());
                            } else {
                                Glib::ustring new_result;
                                if (result_to_primitive.find(inp) != result_to_primitive.end()) {
                                    new_result = inp;
                                } else {
                                    if (prim->getPrev() != nullptr) {
                                        for (auto it : result_to_primitive) {
                                            if (it.second == prim->getPrev()) {
                                                new_result = it.first;
                                                break;
                                            }
                                        }
                                    } else {
                                        g_message("Using this for SourceGraphic on node %s", prim->getRepr()->name());
                                        new_result = "SourceGraphic";
                                    }
                                    g_message("Went from %s to %s for %s", inp, new_result.c_str(),
                                              prim->getRepr()->name());
                                }
                                prim->setAttribute(attr_str.c_str(), new_result.c_str());
                            }
                        }
                    }
                    auto result = prim->getAttribute("result");

                    if (result == nullptr) {
                        auto new_result = filter->get_new_result_name();
                        prim->setAttribute("result", new_result);
                        result_to_primitive.insert({new_result, prim});
                    } else {
                        if (result_to_primitive.find(result) == result_to_primitive.end()) {
                            result_to_primitive.insert({result, prim});
                        } else {
                            auto new_result = filter->get_new_result_name();
                            prim->setAttribute("result", new_result);
                            if (old_to_new_result.find(result) != old_to_new_result.end()) {
                                old_to_new_result.erase(old_to_new_result.find(result));
                            }
                            old_to_new_result.insert({result, new_result});
                        }
                    }
                } else {
                    auto merge = cast<SPFeMerge>(prim);
                    for(auto &child : merge->children){
                        auto merge_child = cast<SPFeMergeNode>(&child);
                        auto inp = merge_child->getAttribute("in");
                        if (inp == nullptr) {
                            // Shouldn't be happening but should handle anyways
                            if(merge_child->getPrev() != nullptr){
                                auto prev_prim = merge_child->getPrev();
                                for(auto it : result_to_primitive){
                                    if(it.second == prev_prim){
                                        merge_child->setAttribute("in", it.first);
                                        break;
                                    }
                                }
                            }
                            else{
                                merge_child->setAttribute("in", "SourceGraphic");
                            }
                        }
                        else {
                            if (std::find(result_inputs.begin(), result_inputs.end(), inp) != result_inputs.end()) {
                                continue;
                            }
                            else if (old_to_new_result.find(inp) != old_to_new_result.end()) {
                                auto new_result = old_to_new_result.find(inp)->second;
                                merge_child->setAttribute("in", new_result);
                            } else {
                                if (result_to_primitive.find(inp) != result_to_primitive.end()) {
                                    // Perfect, we found the node we were looking for.
                                } else {
                                    if (merge_child->getPrev() != nullptr) {
                                        auto prev_prim = merge_child->getPrev();
                                        for (auto it : result_to_primitive) {
                                            if (it.second == prev_prim) {
                                                merge_child->setAttribute("in", it.first);
                                                break;
                                            }
                                        }
                                    } else {
                                        merge_child->setAttribute("in", "SourceGraphic");
                                    }
                                }
                            }
                        } 
                    }
                }
            }
        }

        for (auto &node : nodes[current_filter_id]) {
            place_node(node.get(), node->x, node->y);
            if (dynamic_cast<FilterEditorOutputNode *>(node.get()) != nullptr) {
                output_node = dynamic_cast<FilterEditorOutputNode *>(node.get());
            }
        }
        int count = 0;
        for(auto &child : filter->children){
            count++;
            auto prim = cast<SPFilterPrimitive>(&child);
            
            if(primitive_node_exists(prim)){
                // Skip
                // g_message("Node already existed for this? how?");
            }
            else{
                auto type_id = FPConverter.get_id_from_key(prim->getRepr()->name());
                auto type = static_cast<Filters::FilterPrimitiveType>(type_id);
                int num_sinks = input_count(prim);
                double x_position = prim->getRepr()->getAttributeDouble("inkscape:filter-x", 100.0);
                double y_position = prim->getRepr()->getAttributeDouble("inkscape:filter-y", 50.0 + count * 100.0);
                g_message("Adding node for %s", prim->getRepr()->name());
                auto prim_node = add_primitive_node(prim, x_position, y_position, type, FPConverter.get_label(type), num_sinks);
            }
        } 
        // g_message("Size of nodes is %ld", nodes[current_filter_id].size());
        std::multimap<std::string, FilterEditorPrimitiveNode*> result_to_node;
        for(auto &child : filter->children){
            auto prim = cast<SPFilterPrimitive>(&child);
            if(dynamic_cast<FilterEditorPrimitiveNode*>(primitive_to_node.find(prim)->second) != nullptr){
                auto prim_node = dynamic_cast<FilterEditorPrimitiveNode*>(primitive_to_node.find(prim)->second);
                auto prim = prim_node->get_primitive();
                if(dynamic_cast<FilterEditorPrimitiveMergeNode*>(prim_node) == nullptr){
                    for(int sink_index = 0; sink_index != input_count(prim); sink_index++){
                        std::string sink_attr;
                        if (sink_index == 0) {
                            sink_attr = "in";
                        } else {
                            sink_attr = "in2";
                        } 
                        if (prim->getAttribute(sink_attr.c_str()) != nullptr) {
                            auto result = prim->getAttribute(sink_attr.c_str());
                            if (result != nullptr) {
                                // g_message("Prim: %s has result as ")
                                if (std::find(result_inputs.begin(), result_inputs.end(), Glib::ustring(result)) !=
                                    result_inputs.end()) {
                                        prim_node->set_sink_result(prim_node->sinks[sink_index], std::find(result_inputs.begin(), result_inputs.end(), Glib::ustring(result)) -result_inputs.end());
                                }
                                else if (result_to_node.find(result) != result_to_node.end()) {
                                    auto it = result_to_node.find(result);
                                    while(it!= result_to_node.end() && it->first == result){
                                        it++;
                                    }
                                    it--;
                                    auto source_node = it->second;
                                    auto source = dynamic_cast<FilterEditorPrimitiveNode *>(source_node)->get_source();
                                    auto sink = prim_node->sinks[sink_index];
                                    if (sink != nullptr) {
                                        create_connection(source, sink, true); 
                                    }
                                }
                                else{
                                    if (prim->getPrev() != nullptr) {
                                        auto prev_prim_node = dynamic_cast<FilterEditorPrimitiveNode *>(
                                            primitive_to_node.find(static_cast<SPFilterPrimitive *>(prim->getPrev()))
                                                ->second);
                                        prim->setAttribute(sink_attr.c_str(), prev_prim_node->get_result_string());
                                        auto sink = prim_node->sinks[sink_index];
                                        auto source = prev_prim_node->get_source();
                                        create_connection(source, sink, true);
                                    } else {
                                        g_message("Here's a place using SourceGraphic");
                                        prim_node->set_sink_result(prim_node->sinks[sink_index], 0);
                                        // prim->setAttribute("in", "SourceGraphic")
                                    }
                                    // g_message("Setting sink result to SourceGraphic for %s", prim->getRepr()->name());
                                    // prim_node->set_sink_result(prim_node->sinks[0], 0);
                                }
                            }
                            else{
                                prim_node->set_sink_result(prim_node->sinks[sink_index], 0);
                            }
                        }
                        else{
                            if(prim->getPrev() != nullptr){
                                auto prev_prim_node = dynamic_cast<FilterEditorPrimitiveNode*>(primitive_to_node.find(static_cast<SPFilterPrimitive*>(prim->getPrev()))->second);
                                prim->setAttribute(sink_attr.c_str(), prev_prim_node->get_result_string());
                                auto sink = prim_node->sinks[sink_index];
                                auto source = prev_prim_node->get_source();
                                create_connection(source, sink, true);
                            }
                            else{
                                prim_node->set_sink_result(prim_node->sinks[sink_index], 0);
                                // prim->setAttribute("in", "SourceGraphic");
                                // prim_node->sinks[0]->set_label_text("SG");
                            }
                        }
                    }
                }
                else{
                    // auto merge_prim_node = dynamic_cast<FilterEditorPrimitiveMergeNode*>(prim_node);
                    // for(auto &merge_child : merge_prim_node->get_primitive()->children){
                    //     Glib::ustring sink_attr = "in";
                    //     if (merge_child.getAttribute(sink_attr.c_str()) != nullptr) {
                    //         auto result = merge_child.getAttribute(sink_attr.c_str());
                    //         if (result != nullptr) {
                    //             if (std::find(result_inputs.begin(), result_inputs.end(), Glib::ustring(result)) !=
                    //                 result_inputs.end()) {
                    //                     merge_prim_node->set_sink_result(prim_node->sinks[sink_index], std::find(result_inputs.begin(), result_inputs.end(), Glib::ustring(result)) -result_inputs.end());
                    //             }
                    //             else if (result_to_node.find(result) != result_to_node.end()) {
                    //                 auto it = result_to_node.find(result);
                    //                 while(it!= result_to_node.end() && it->first == result){
                    //                     it++;
                    //                 }
                    //                 it--;
                    //                 auto source_node = it->second;
                    //                 auto source = dynamic_cast<FilterEditorPrimitiveNode *>(source_node)->get_source();
                    //                 auto sink = prim_node->sinks[sink_index];
                    //                 if (sink != nullptr) {
                    //                     create_connection(source, sink, true); 
                    //                 }
                    //             }
                    //             else{
                    //                 if (prim->getPrev() != nullptr) {
                    //                     auto prev_prim_node = dynamic_cast<FilterEditorPrimitiveNode *>(
                    //                         primitive_to_node.find(static_cast<SPFilterPrimitive *>(prim->getPrev()))
                    //                             ->second);
                    //                     prim->setAttribute(sink_attr.c_str(), prev_prim_node->get_result_string());
                    //                     auto sink = prim_node->sinks[sink_index];
                    //                     auto source = prev_prim_node->get_source();
                    //                     create_connection(source, sink, true);
                    //                 } else {
                    //                     g_message("Here's a place using SourceGraphic");
                    //                     prim_node->set_sink_result(prim_node->sinks[sink_index], 0);
                    //                     // prim->setAttribute("in", "SourceGraphic");
                    //                     // prim_node->sinks[0]->set_label_text("SG");
                    //                 }
                    //                 // g_message("Setting sink result to SourceGraphic for %s", prim->getRepr()->name());
                    //                 // prim_node->set_sink_result(prim_node->sinks[0], 0);
                    //             }
                    //         }
                    //         else{
                    //             prim_node->set_sink_result(prim_node->sinks[sink_index], 0);
                    //         }
                    //     }


                    // }
                    // // for(auto )
                }
            }
            
            if(primitive_to_node.find(cast<SPFilterPrimitive>(&child)) != primitive_to_node.end()){
                result_to_node.insert({dynamic_cast<FilterEditorPrimitiveNode*>(primitive_to_node.find(cast<SPFilterPrimitive>(&child))->second)->get_result_string(), dynamic_cast<FilterEditorPrimitiveNode*>(primitive_to_node.find(cast<SPFilterPrimitive>(&child))->second)});
            }
            else{

            }
        }
        if(filter->lastChild() != nullptr){
            create_connection(primitive_to_node.find(cast<SPFilterPrimitive>(filter->lastChild()))->second->get_source(), output_node->sinks[0], true);
        }
        // for(auto &node : nodes[current_filter_id]){
        //     if(dynamic_cast<FilterEditorPrimitiveNode*>(node.get()) != nullptr){
        //         auto prim_node = dynamic_cast<FilterEditorPrimitiveNode*>(node.get());
        //         auto prim = prim_node->get_primitive();
        //         if(dynamic_cast<FilterEditorPrimitiveMergeNode*>(node.get()) == nullptr){
        //             if(input_count(prim) >= 1){
        //                 if (prim->getAttribute("in") != nullptr) {
        //                     auto result = prim->getAttribute("in");
        //                     if (result != "") {
        //                         if (result_to_node.find(result) != result_to_node.end()) {
        //                             auto source_node = result_to_node.find(result)->second;
        //                             auto source = dynamic_cast<FilterEditorPrimitiveNode *>(source_node)->get_source();
        //                             auto sink = prim_node->sinks[0];
        //                             if (sink != nullptr) {
        //                                 create_connection(source, sink, true);
        //                             }
        //                         }
        //                     }
        //                 }
        //             }
        //             if(input_count(prim) == 2){
        //                 if (prim->getAttribute("in2") != nullptr) {
        //                     auto result = prim->getAttribute("in2");
        //                     if (result != "") {
        //                         if (result_to_node.find(result) != result_to_node.end()) {
        //                             auto source_node = result_to_node.find(result)->second;
        //                             auto source = dynamic_cast<FilterEditorPrimitiveNode *>(source_node)->get_source();
        //                             auto sink = prim_node->sinks[1];
        //                             if (sink != nullptr) {
        //                                 create_connection(source, sink, true);
        //                             }
        //                         }
        //                     }
        //                 }
        //             }
                    
                    
        //         }

        //     }
        // }
    }   
    // update_document();
    modify_observer(false);
};

void FilterEditorCanvas::duplicate_nodes(){
    // for(auto node : selected_nodes){
    // Should I update the document first?
    modify_observer(true);
    auto filter = _dialog._filter_modifier.get_selected_filter();
    if(!filter){
        return;
    }

    /*
    Approach for duplicating:
    Duplicate the primitives for each of the nodes. To preserve the connections,
    the approach is to copy all the nodes and place them at the start of the document
    since anyways, they wouldn't be connected to the output node after copying,
    and so we can safely place them at the start of the document.
    The order to be placed is the order in which they occur in the document,
    this way connections will be copied
    */
    std::vector<SPFilterPrimitive*> primitives_order;
    for(auto &child : filter->children){
        auto prim = cast<SPFilterPrimitive>(&child);
        if(prim == nullptr){
            continue;
        }
        primitives_order.push_back(prim);

    }
    std::set<std::pair<int, SPFilterPrimitive*>> new_primitives;
    for(auto node : selected_nodes[current_filter_id]){
        if(dynamic_cast<FilterEditorPrimitiveNode*>(node) != nullptr){
            auto prim_node = dynamic_cast<FilterEditorPrimitiveNode *>(node);
            auto prim = prim_node->get_primitive();
            // auto new_prim = prim->getRepr()->duplicate(prim->getRepr()->document());
            auto filter = _dialog._filter_modifier.get_selected_filter();

            // filter->getRepr()->addChildAtPos(new_prim, 0); 
            new_primitives.insert({std::find(primitives_order.begin(), primitives_order.end(), prim) - primitives_order.begin(), prim});
            // filter->getRepr()->addChildAtPost
            // if(dynamic_cast<FilterEditorPrimitiveMergeNode*>(node) != nullptr){
            //     while(new_prim->firstChild() != nullptr){
            //         new_prim->removeChild(new_prim->firstChild());
            //     }
            // }
        }
    }
    for(auto it = new_primitives.rbegin(); it != new_primitives.rend(); it++){
        auto new_prim = it->second->getRepr()->duplicate(it->second->getRepr()->document());
        auto filter = _dialog._filter_modifier.get_selected_filter();
        filter->getRepr()->addChild(new_prim, 0);
        // new_primitives.insert({std::find(primitives_order.begin(), primitives_order.end(), prim), prim});
    }
    DocumentUndo::done(filter->document, _("Duplicated primitives"), INKSCAPE_ICON("dialog-filters"));
    update_canvas_new();
    modify_observer(false);
}

// void FilterEditorCanvas::duplicate_nodes(){
//     for(auto node : selected_nodes){
//         if(dynamic_cast<FilterEditorPrimitiveNode*>(node) != nullptr){
//             auto prim_node = dynamic_cast<FilterEditorPrimitiveNode*>(node);
//             auto prim = prim_node->get_primitive();
//             auto new_prim = prim->getRepr()->duplicate(prim->getRepr()->document());
//             auto filter = _dialog._filter_modifier.get_selected_filter();
//             filter->appendChild(new_prim);
//             auto new_node = add_primitive_node(new_prim, prim_node->x + 100, prim_node->y + 100, Filters::FilterPrimitiveType::NR_FILTER_UNKNOWN, prim->getRepr()->name(), prim->get_num_sinks());
//             for(auto sink:prim_node->sinks){
//                 if(sink->get_connections().size() != 0){
//                     auto connection = sink->get_connections()[0];
//                     auto source_node = connection->get_source_node();
//                     auto source = source_node->get_source();
//                     auto new_sink = new_node->get_next_available_sink();
//                     if(new_sink != nullptr){
//                         create_connection(source, new_sink, false);
//                     }
//                 }
//             }
//         }
//     }
//     update_document();
//     canvas.queue_draw();
// }
void FilterEditorCanvas::clear_nodes(){
    // Allternate approach:
    


    
    // TODO : Reimplement and remove only the nodes of the current filter
    while(canvas.get_first_child() != nullptr){
        canvas.remove(*canvas.get_first_child());
    }

    #ifdef DELETE_NODES // TODO: Remove this
    nodes[current_filter_id].clear();
    primitive_to_node.clear();
    output_node = nullptr;
    connections[current_filter_id].clear();
    #endif
    canvas.queue_draw();

}
void FilterEditorCanvas::update_filter(SPFilter* filter){ 
    if(filter == nullptr){
        current_filter_id = -1;
        return;
    }
    g_message("Filter being updated: %s", filter->getRepr()->name());
    // g_message("Filter is not a nullptr, list size is %ld", filter_list.size());
    // if(std::find(filter_list.begin(), filter_list.end(), filter) == filter_list.end()){
    //     filter_list.push_back(filter);
    //     current_filter_id = filter_list.size() - 1;
        
    //     nodes.insert({current_filter_id, std::vector<std::unique_ptr<FilterEditorNode>>()});
    //     selected_nodes.insert({current_filter_id, std::vector<FilterEditorNode*>()});
    //     connections.insert({current_filter_id, std::vector<FilterEditorConnection*>()});
    //     // create_output_node(filter, 100, 100, std::string(filter->label())+" Output");
    // }

    current_filter_id = std::find(filter_list.begin(), filter_list.end(), filter) - filter_list.begin();
    clear_nodes();
    
    // if (filter) {
    //     auto filter_repr = filter->getRepr();
    //     for (auto &child : filter->children) {
    //         auto prim = cast<SPFilterPrimitive>(&child);
    //         auto type_id = FPConverter.get_id_from_key(prim->getRepr()->name());
    //         auto type = static_cast<Filters::FilterPrimitiveType>(type_id);
    //         int num_sinks = input_count(prim);
    //         double x_position = prim->getRepr()->getAttributeDouble("inkscape:filter-x", 100.0);
    //         double y_position = prim->getRepr()->getAttributeDouble("inkscape:filter-y", 100.0);
    //         add_primitive_node(prim, x_position + 10.0, y_position + 10.0, type, FPConverter.get_label(type),
    //                            num_sinks);
    //     }

    for (auto& node : nodes[current_filter_id]){
        place_node(node.get(), node->x, node->y);
        if(dynamic_cast<FilterEditorOutputNode*>(node.get()) != nullptr){
            output_node = dynamic_cast<FilterEditorOutputNode*>(node.get());
        }
    }
    // }
    

    // if(current_filter_id == -1 || filter_list[current_filter_id] != filter){
    //     current_filter_id = std::find(filter_list.begin(), filter_list.end(), filter) - filter_list.begin();
    //     clear_nodes();
        
    //     if(filter){
    //         auto filter_repr = filter->getRepr();
    //         for(auto &child : filter->children){
    //             auto prim = cast<SPFilterPrimitive>(&child);
    //             auto type_id = FPConverter.get_id_from_key(prim->getRepr()->name());
    //             auto type = static_cast<Filters::FilterPrimitiveType>(type_id);
    //             int num_sinks = input_count(prim);
    //             double x_position = prim->getRepr()->getAttributeDouble("inkscape:filter-x", 100.0);
    //             double y_position = prim->getRepr()->getAttributeDouble("inkscape:filter-y", 100.0);
    //             add_primitive_node(prim, x_position+10.0, y_position+10.0, type, FPConverter.get_label(type), num_sinks);
    //         }
    //     }
    //     for(auto node : nodes[current_filter_id]){
    //         place_node(node, node->x, node->y);
    //     }
    // }
    

}

void FilterEditorCanvas::update_document_new(bool add_undo){
    // This function is to reflect changes made by the user in the canvas
    // onto the document. This should be called after events happen on canvas which are made by the user.
    SPFilter* filter = _dialog._filter_modifier.get_selected_filter();
    modify_observer(true);
    std::vector<FilterEditorPrimitiveNode*> nodes_order;
    delete_nodes_without_prims(); 
    update_canvas();
    // if(output_node == nullptr){
    //     output_node = create_output_node(_dialog._filter_modifier.get_selected_filter(), 100, 100, "Output");
    // }
    // g_message("%d size of connected nodes %d", __LINE__, output_node->connected_up_nodes ==nullptr);
    if(output_node->connected_up_nodes.size()==1){
        std::map<FilterEditorPrimitiveNode*, std::pair<int, int>> visited;
        for(auto& node:nodes[current_filter_id]){
            if(dynamic_cast<FilterEditorPrimitiveNode*>(node.get()) != nullptr){
                dynamic_cast<FilterEditorPrimitiveNode*>(node.get())->part_of_chain = false;
                if(dynamic_cast<FilterEditorPrimitiveNode*>(node.get())->get_primitive()){

                } 
                dynamic_cast<FilterEditorPrimitiveNode*>(node.get())->update_sink_results();
                
                visited.insert({dynamic_cast<FilterEditorPrimitiveNode*>(node.get()), {-1, G_MININT}});
            }
            
        }
        visited.find(static_cast<FilterEditorPrimitiveNode*>(output_node->connected_up_nodes[0].second))->second = {0, 0};
        create_nodes_order(nullptr, static_cast<FilterEditorPrimitiveNode*>(output_node->connected_up_nodes[0].second), nodes_order, visited, true, true);
        for(auto it : visited){
            dbg;
            it.first->update_sink_results();
            if(it.second.first == -1){
                create_nodes_order(nullptr, static_cast<FilterEditorPrimitiveNode*>(it.first), nodes_order, visited, true);
            }
        }
        std::multimap<std::pair<int, int>, FilterEditorPrimitiveNode*> pos_map;

        std::multiset<std::pair<int, std::pair<int, FilterEditorPrimitiveNode*>>> pos_map2;
        for(auto it : visited) {
            pos_map.insert({it.second, it.first});
            
            pos_map2.insert({it.second.first, {it.second.second, it.first}});
        }
        nodes_order.clear();
        int x = 0;
        auto first_node = static_cast<FilterEditorPrimitiveNode*>(output_node->connected_up_nodes[0].second);
        nodes_order.push_back(first_node);
        for(auto it : pos_map2){
            if(it.second.second != first_node){
                nodes_order.push_back(it.second.second);
            }
            x++;
        } 
        for (int i = 0; i != nodes_order.size(); i++) {
            nodes_order[i]->get_primitive()->getRepr()->setPosition(nodes_order.size()-1-i);
        }
    }
    // auto filter = _dialog._filter_modifier.get_selected_filter();
    if(filter)
        filter->requestModified(SP_OBJECT_MODIFIED_FLAG);

    if(add_undo){
        // g_message("Did an undo");
        filter->requestModified(SP_OBJECT_MODIFIED_FLAG);
        DocumentUndo::done(_dialog.getDocument(), _("Update filter"), INKSCAPE_ICON("dialog-filters")); 
    }
    modify_observer(false);
}
void FilterEditorCanvas::update_document(bool add_undo){
    // SPFilter* filter = _dialog._filter_modifier.get_selected_filter();
    // g_assert(filter);

    // int j = 0, total_nodes = 0;
    // g_error("Updating the document");
    // g_message("Updating the document");

    modify_observer(true);
    if(add_undo){
        auto filter = _dialog._filter_modifier.get_selected_filter();
        // g_message("Did an undo");
        filter->requestModified(SP_OBJECT_MODIFIED_FLAG);
        DocumentUndo::done(_dialog.getDocument(), _("Update filter"), INKSCAPE_ICON("dialog-filters")); 
    }
    std::vector<FilterEditorPrimitiveNode*> nodes_order;
    // delete_nodes_without_prims(); 
    // update_canvas();
    if(output_node == nullptr){
        g_error("This should never happen");
    }
    /*Ensure that each primitive is wired correctly:
    1. Each primitive either has correct ins, either a valid result or a SourceGraphic, SourceAlpha, etc.
    */
    


    



    if(output_node->connected_up_nodes.size()==1){
        std::map<FilterEditorPrimitiveNode*, std::pair<int, int>> visited;
        for(auto& node:nodes[current_filter_id]){
            if(dynamic_cast<FilterEditorPrimitiveNode*>(node.get()) != nullptr){
                dynamic_cast<FilterEditorPrimitiveNode*>(node.get())->part_of_chain = false;
                if(dynamic_cast<FilterEditorPrimitiveNode*>(node.get())->get_primitive() != nullptr){
                    auto prim = dynamic_cast<FilterEditorPrimitiveNode*>(node.get())->get_primitive();
                    if(prim == nullptr){
                        g_error("The error is here %d", __LINE__);
                    }
                    auto result = prim->getAttribute("result");
                    if(result == nullptr){
                        auto filter = _dialog._filter_modifier.get_selected_filter();
                        auto new_result = filter->get_new_result_name();
                        prim->setAttribute("result", new_result);
                    }
                }
                visited.insert({dynamic_cast<FilterEditorPrimitiveNode*>(node.get()), {-1, G_MININT}});
            }
            
        }
        visited.find(static_cast<FilterEditorPrimitiveNode*>(output_node->connected_up_nodes[0].second))->second = {0, 0};
        create_nodes_order(nullptr, static_cast<FilterEditorPrimitiveNode*>(output_node->connected_up_nodes[0].second), nodes_order, visited, true, true);
        for(auto it : visited){
            if(dynamic_cast<FilterEditorPrimitiveMergeNode*>(it.first) != nullptr){
                // dynamic_cast<FilterEditorPrimitiveMergeNode*>(it.first)->update_sink_results();
            }
            else{
                it.first->update_sink_results();
            }
            if(it.second.first == -1){
                create_nodes_order(nullptr, static_cast<FilterEditorPrimitiveNode*>(it.first), nodes_order, visited, true);
            }
        }
        std::multimap<std::pair<int, int>, FilterEditorPrimitiveNode*> pos_map;

        std::multiset<std::pair<int, std::pair<int, FilterEditorPrimitiveNode*>>> pos_map2;
        for(auto it : visited) {
            pos_map.insert({it.second, it.first});
            
            pos_map2.insert({it.second.first, {it.second.second, it.first}});
        }
        nodes_order.clear();
        int x = 0;
        auto first_node = static_cast<FilterEditorPrimitiveNode*>(output_node->connected_up_nodes[0].second);
        nodes_order.push_back(first_node);
        for(auto it : pos_map2){
            if(it.second.second != first_node){
                nodes_order.push_back(it.second.second);
            }
            x++;
        } 
        for (int i = 0; i != nodes_order.size(); i++) {
            nodes_order[i]->get_primitive()->getRepr()->setPosition(nodes_order.size()-1-i);
        }
    }
    else{
        // TODO: If the output node is not connected, handle in Canvas Updates
    }
    SPFilter* filter = _dialog._filter_modifier.get_selected_filter();
    if(filter){
        std::map<Glib::ustring, SPFilterPrimitive*> result_to_primitive;
        std::vector<SPFilterPrimitive*> primitive_list;
        std::map<Glib::ustring, Glib::ustring> old_to_new_result;
        
        for (auto &child : filter->children) {
            // count++;
            auto prim = cast<SPFilterPrimitive>(&child);
            if (prim == nullptr) {
                continue;
            }
            if (cast<SPFeMerge>(prim) != nullptr) {
                // TODO: Implement merge
                auto merge = cast<SPFeMerge>(prim);
                for(auto &child : merge->children){
                    auto merge_child = cast<SPFeMergeNode>(&child);
                    auto inp = merge_child->getAttribute("in");
                    if (inp == nullptr) {
                        // Shouldn't be happening but should handle anyways
                        if(primitive_list.size() > 0){
                            auto prev_prim = primitive_list.back();
                            auto prev_prim_result = prev_prim->getAttribute("result");
                            merge_child->setAttribute("in", prev_prim_result);
                        }
                        else{
                            merge_child->setAttribute("in", "SourceGraphic");                         
                        }
                        //TODO: Consider switching to getPrev instead of primitive_list
                        // if (merge_child->getPrev() != nullptr) {
                        //     auto prev_prim = merge_child->getPrev();

                        // } else {
                        //     merge_child->setAttribute("in", "SourceGraphic");
                        // }
                    } else {
                        if (std::find(result_inputs.begin(), result_inputs.end(), inp) != result_inputs.end()) {
                            // prim->setAttribute("in", inp);
                        } else if (old_to_new_result.find(inp) != old_to_new_result.end()) {
                            merge_child->setAttribute("in", old_to_new_result[inp].c_str());
                            // Perfect, needs no updating
                        } else {
                            if (result_to_primitive.find(inp) != result_to_primitive.end()) {
                                // Perfect, we found the node we were looking for.
                            } else {
                                if (primitive_list.size() > 0) {
                                    auto prev_prim = primitive_list.back();
                                    auto prev_prim_result = prev_prim->getAttribute("result");
                                    merge_child->setAttribute("in", prev_prim_result);
                                    
                                } else {
                                    merge_child->setAttribute("in", "SourceGraphic");
                                }
                                // if (merge_child->getPrev() != nullptr) {
                                //     auto prev_prim = merge_child->getPrev();
                                //     for (auto it : result_to_primitive) {
                                //         if (it.second == prev_prim) {
                                //             merge_child->setAttribute("in", it.first);
                                //             break;
                                //         }
                                //     }
                                // } else {
                                //     merge_child->setAttribute("in", "SourceGraphic");
                                // }
                            }
                        }
                    }
                    }
            } else {
                int num_sinks = input_count(prim);
                Glib::ustring in_attributes[] = {"in", "in2"};
                for (int i = 0; i != num_sinks; i++) {
                    // in_attributes[i];
                    auto inp = prim->getAttribute(in_attributes[i].c_str());
                    if (inp == nullptr) {
                        // Set to previous or SourceGraphic if previous is none; 
                        if(primitive_list.size() > 0){
                            auto prev_prim = primitive_list.back();
                            auto prev_prim_result = prev_prim->getAttribute("result");
                            prim->setAttribute(in_attributes[i].c_str(), prev_prim_result);
                        }
                    } else {
                        if (std::find(result_inputs.begin(), result_inputs.end(), inp) != result_inputs.end()) {
                            // TODO: Implement this
                        } else {
                            if(old_to_new_result.find(inp) != old_to_new_result.end()){
                                prim->setAttribute(in_attributes[i].c_str(), old_to_new_result[inp]);
                                // inp = old_to_new_result[inp];
                            }
                            auto inp = prim->getAttribute(in_attributes[i].c_str());
                            if (result_to_primitive.find(inp) != result_to_primitive.end()) {
                                // No problems here, since we find a result that exists.
                                g_message("Has a result, using result");
                                
                            } else {
                                g_message("Has a result, unable to use result");
                                if(primitive_list.size() > 0){
                                    auto prev_prim = primitive_list.back();
                                    auto prev_prim_result = prev_prim->getAttribute("result");
                                    prim->setAttribute(in_attributes[i].c_str(), prev_prim_result);
                                }
                                else{
                                    // Set it to SourceGraphic
                                    prim->setAttribute(in_attributes[i].c_str(), "SourceGraphic");
                                }
                            }
                        }
                    }
                }
                // g_message("%d", __LINE__);
            }
            if (prim->getAttribute("result") == nullptr || result_to_primitive.find(prim->getAttribute("result")) != result_to_primitive.end()) {
                auto new_result = filter->get_new_result_name();
                if(prim->getAttribute("result") != nullptr){
                    // ( old_to_new_result.find(prim->getAttribute("result")) != old_to_new_result.end() ) ? old_to_new_result[prim->getAttribute("result")] = new_result : old_to_new_result.insert({prim->getAttribute("result"), new_result}); 
                    if(old_to_new_result.find("result") != old_to_new_result.end()){
                        old_to_new_result[prim->getAttribute("result")] = new_result;
                    }
                    else{
                        old_to_new_result.insert({prim->getAttribute("result"), new_result});
                    }
                }
                prim->setAttribute("result", new_result);
                
            }
            if(result_to_primitive.find(prim->getAttribute("result")) != result_to_primitive.end()){
                result_to_primitive[prim->getAttribute("result")] = prim;
            }
            else{
                result_to_primitive.insert({prim->getAttribute("result"), prim});
            }
            // result_to_primitive = 
            primitive_list.push_back(prim);
            // nodes_list.push_back(primitive_node);
        }
    }
    // auto filter = _dialog._filter_modifier.get_selected_filter();
    if(filter)
        filter->requestModified(SP_OBJECT_MODIFIED_FLAG);
    
    update_canvas_new();

    
    modify_observer(false);
}

FilterEditorPrimitiveNode* FilterEditorCanvas::get_node_from_primitive(SPFilterPrimitive* prim){
    if(primitive_to_node.find(prim) != primitive_to_node.end()){
        return primitive_to_node[prim];
    }
    else{
        return nullptr;
    }
}
FilterEditorPrimitiveNode *FilterEditorCanvas::add_primitive_node(SPFilterPrimitive *primitive, double x_click, double y_click, Filters::FilterPrimitiveType type, Glib::ustring label_text, int num_sinks)
{
    if(type == Filters::FilterPrimitiveType::NR_FILTER_MERGE){
        // TODO: Fix this
        std::unique_ptr<FilterEditorNode> node = std::make_unique<FilterEditorPrimitiveMergeNode>(100, 0, 0, primitive, num_sinks);
        primitive_to_node[primitive] = static_cast<FilterEditorPrimitiveNode*>(node.get());
        place_node(static_cast<FilterEditorNode *>(node.get()), x_click, y_click, true);
        nodes[current_filter_id].push_back(std::move(node));
        
        return static_cast<FilterEditorPrimitiveNode*>(node.get());

    }
    else{
        std::unique_ptr<FilterEditorPrimitiveNode> prim_node = std::make_unique<FilterEditorPrimitiveNode>(100, 0, 0, label_text, primitive, num_sinks);
        std::unique_ptr<FilterEditorNode> node = std::move(prim_node); //std::make_unique<FilterEditorPrimitiveNode>(100, 0, 0, label_text, primitive, num_sinks);
        primitive_to_node[primitive] = dynamic_cast<FilterEditorPrimitiveNode*>(node.get());
        place_node((node.get()), x_click, y_click, true);
        nodes[current_filter_id].push_back(std::move(node));
        if(dynamic_cast<FilterEditorPrimitiveNode*>(nodes[current_filter_id].back().get()) == nullptr){
            g_error("There's an error here %d", __LINE__);
        }
        return dynamic_cast<FilterEditorPrimitiveNode*>(nodes[current_filter_id].back().get());

    }
};


FilterEditorConnection* FilterEditorCanvas::create_connection(FilterEditorSource *source, FilterEditorSink *sink, bool break_old_connection)
{
    modify_observer(true);
    if(break_old_connection){
        if(sink->can_add_connection()){

        }
        else{
            for(auto connection : sink->get_connections()){
                destroy_connection(connection);
            }
        }
        FilterEditorConnection *connection = new FilterEditorConnection(source, sink, this);
        sink->add_connection(connection);
        connections[current_filter_id].push_back(connection);
        // g_message()
        source->add_connection(connection);
        source->get_parent_node()->add_connected_node(source, sink->get_parent_node(), connection);
        sink->get_parent_node()->add_connected_node(sink, source->get_parent_node(), connection);

        if(dynamic_cast<FilterEditorPrimitiveNode*>(sink->get_parent_node()) != nullptr && dynamic_cast<FilterEditorPrimitiveNode*>(source->get_parent_node()) != nullptr){
            dynamic_cast<FilterEditorPrimitiveNode*>(sink->get_parent_node())->set_sink_result(sink, dynamic_cast<FilterEditorPrimitiveNode*>(source->get_parent_node())->get_result_string());
        }

        //TODO: convert add connected node to a virtual method
        if(dynamic_cast<FilterEditorPrimitiveMergeNode*>(sink->get_parent_node()) != nullptr && dynamic_cast<FilterEditorPrimitiveNode*>(source->get_parent_node()) != nullptr){
            dynamic_cast<FilterEditorPrimitiveMergeNode*>(sink->get_parent_node())->create_sink_merge_node(sink, dynamic_cast<FilterEditorPrimitiveNode*>(source->get_parent_node()));

        }
         
        return connection;
        
        
    }
    else{
        if (sink->can_add_connection()) {
            FilterEditorConnection *connection = new FilterEditorConnection(source, sink, this);
            sink->add_connection(connection);
            connections[current_filter_id].push_back(connection);
            source->add_connection(connection);
            source->get_parent_node()->add_connected_node(source, sink->get_parent_node(), connection);
            sink->get_parent_node()->add_connected_node(sink, source->get_parent_node(), connection);
            //TODO: convert add connected node to a virtual method
            if (dynamic_cast<FilterEditorPrimitiveMergeNode *>(sink->get_parent_node()) != nullptr) {
                dynamic_cast<FilterEditorPrimitiveMergeNode *>(sink->get_parent_node())->add_sink();
            }
            return connection;
        } else {
            return nullptr;
        }
    }
    modify_observer(false);
};

FilterEditorConnection *FilterEditorCanvas::create_connection(FilterEditorPrimitiveNode *source_node, FilterEditorNode *sink_node){
    modify_observer(true);
    auto sink = sink_node->get_next_available_sink();
    if(sink == nullptr){
        return nullptr;
    }
    auto source = source_node->get_source();
    FilterEditorConnection *connection = new FilterEditorConnection(source, sink, this);
    connections[current_filter_id].push_back(connection);

    source_node->add_connected_node(source, sink_node, connection);
    sink_node->add_connected_node(sink, source_node, connection);
    source->add_connection(connection);
    sink->add_connection(connection);
    return connection;
    modify_observer(false);
}

bool FilterEditorCanvas::destroy_connection(FilterEditorConnection *connection, bool update_document)
{
    // modify_observer(true);
    if(std::find(connections[current_filter_id].begin(), connections[current_filter_id].end(), connection) == connections[current_filter_id].end()){
        return false;
    }
    else{
        connections[current_filter_id].erase(std::find(connections[current_filter_id].begin(), connections[current_filter_id].end(), connection));
        connections[current_filter_id].erase(std::remove(connections[current_filter_id].begin(), connections[current_filter_id].end(), connection), connections[current_filter_id].end());
        connection->get_source()->get_connections().erase(
            std::remove(connection->get_source()->get_connections().begin(),
                        connection->get_source()->get_connections().end(), connection),
            connection->get_source()->get_connections().end());
        connection->get_sink()->get_connections().erase(std::remove(connection->get_sink()->get_connections().begin(),
                                                                    connection->get_sink()->get_connections().end(),
                                                                    connection),
                                                        connection->get_sink()->get_connections().end());
        // connection->get_sink_node()->set_sink_result(connection->get_sink(), 0);
        // connection->get_sink()->set_result_inp();

        connection->get_source()->update_width();
        // connection->get_source_node()->connected_down_nodes.erase()
        connection->get_source_node()->connected_down_nodes.erase(std::find(connection->get_source_node()->connected_down_nodes.begin(), connection->get_source_node()->connected_down_nodes.end(), std::make_pair(connection->get_source(), connection->get_sink_node())));
        connection->get_sink_node()->connected_up_nodes.erase(std::find(connection->get_sink_node()->connected_up_nodes.begin(), connection->get_sink_node()->connected_up_nodes.end(), std::make_pair(connection->get_sink(), connection->get_source_node())));
        if(update_document){
            connection->get_sink_node()->set_sink_result(connection->get_sink(), 0);

            if (dynamic_cast<FilterEditorPrimitiveMergeNode *>(connection->get_sink()->get_parent_node()) != nullptr) {
                sp_repr_unparent(
                    dynamic_cast<FilterEditorPrimitiveMergeNode *>(connection->get_sink()->get_parent_node())
                        ->sink_nodes.find(connection->get_sink())
                        ->second->getRepr());
                // dynamic_cast<FilterEditorPrimitiveMergeNode*>(connection->get_sink()->get_parent_node())->sink_nodes.find(connection->get_sink())->second->deleteObject();
                dynamic_cast<FilterEditorPrimitiveMergeNode *>(connection->get_sink()->get_parent_node())
                    ->sink_nodes.erase(connection->get_sink());

                // dynamic_cast<FilterEditorPrimitiveMergeNode*>(connection->get_sink()->get_parent_node())->remove_extra_sinks();
                // dynamic_cast<FilterEditorPrimitiveMergeNode*>(connection->get_sink()->get_parent_node())->add_sink();
            }
        }     
        delete connection;
        // DocumentUndo::maybeDone(_dialog.getDocument(), "deletion", _("Delete connection"), INKSCAPE_ICON("dialog-filters"));
        // g_message("Connection destroyed");
        return true;
    }
    // modify_observer(false);
};
FilterEditorFixed* FilterEditorCanvas::get_canvas(){
    return &canvas;
}
double FilterEditorCanvas::get_zoom_factor()
{
    return zoom_fac;
};
void FilterEditorCanvas::update_offsets(double x, double y)
{
    canvas.update_offset(x, y);
    update_positions();
}
void FilterEditorCanvas::update_positions()
{
    for (auto child : canvas.get_children()) {
        double x, y;
        dynamic_cast<FilterEditorNode*>(child)->get_position(x, y);
        place_node(dynamic_cast<FilterEditorNode*>(child), x, y);
    }
}
Gtk::Widget *FilterEditorCanvas::get_widget_under(double xl, double yl)
{
    auto widget = canvas.pick(xl, yl);
    active_widget = widget;
    return widget;
}
template <typename T>
T *FilterEditorCanvas::resolve_to_type(Gtk::Widget *widget)
{
    while (dynamic_cast<T *>(widget) == nullptr) {
        if (dynamic_cast<FilterEditorCanvas *>(widget) != nullptr) {
            return nullptr;
        }
        widget = widget->get_parent();
    }
    return dynamic_cast<T *>(widget);
}
// FilterEditorSource *FilterEditorCanvas::resolve_to_source(Gtk::Widget *widget)
// {
//     while (dynamic_cast<FilterEditorSource *>(widget) == nullptr) {
//         if (dynamic_cast<FilterEditorCanvas *>(widget) != nullptr) {
//             return nullptr;
//         }
//         widget = widget->get_parent();
//     }
//     return dynamic_cast<FilterEditorSource *>(widget);
// }
// FilterEditorSink *FilterEditorCanvas::resolve_to_sink(Gtk::Widget *widget)
// {
//     while (dynamic_cast<FilterEditorSink *>(widget) == nullptr) {
//         if (dynamic_cast<FilterEditorCanvas *>(widget) != nullptr) {
//             return nullptr;
//         }
//         widget = widget->get_parent();
//     }
//     return dynamic_cast<FilterEditorSink *>(widget);
// }
// NODE_TYPE *FilterEditorCanvas::resolve_to_node(Gtk::Widget *widget)
// {
//     while (dynamic_cast<NODE_TYPE *>(widget) == nullptr) {
//         if (dynamic_cast<FilterEditorCanvas *>(widget) != nullptr) {
//             return nullptr;
//         }
//         widget = widget->get_parent();
//     }
//     return dynamic_cast<NODE_TYPE *>(widget);
// }

SPFilterPrimitive* FilterEditorCanvas::get_selected_primitive(){
    SPFilter* filter = _dialog._filter_modifier.get_selected_filter();
    if(filter){
        current_filter_id = std::find(filter_list.begin(), filter_list.end(), filter) - filter_list.begin();
        if (selected_nodes[current_filter_id].size() == 0) {
            return nullptr;
        }
        if (selected_nodes[current_filter_id].size() == 1) {
            if (dynamic_cast<FilterEditorPrimitiveNode *>(selected_nodes[current_filter_id][0]) == nullptr) {
                return nullptr;
            }
            auto prim =
                dynamic_cast<FilterEditorPrimitiveNode *>(selected_nodes[current_filter_id][0])->get_primitive();
            return dynamic_cast<FilterEditorPrimitiveNode *>(selected_nodes[current_filter_id][0])->get_primitive();
        } else {
            return nullptr;
        }
    }
    return nullptr;
    
}


sigc::signal<void ()>& FilterEditorCanvas::signal_primitive_changed()
{
    return _signal_primitive_changed;
}
/*Selection Based*/
bool FilterEditorCanvas::toggle_node_selection(NODE_TYPE *widget)
{
    widget->toggle_selection(!widget->get_selected());
    if (widget->get_selected()) {
        selected_nodes[current_filter_id].push_back(widget);
    } else {
        selected_nodes[current_filter_id].erase(std::remove(selected_nodes[current_filter_id].begin(), selected_nodes[current_filter_id].end(), widget), selected_nodes[current_filter_id].end());
    }
    return widget->get_selected();
    // _observer->set(get_selected());
    // signal_primitive_changed()();
    // _dialog._color_matrix_values->clear_store();
    _signal_primitive_changed.emit();
}

void FilterEditorCanvas::set_node_selection(NODE_TYPE *widget, bool selected)
{
    widget->toggle_selection(selected);
    selected_nodes[current_filter_id].erase(std::remove(selected_nodes[current_filter_id].begin(), selected_nodes[current_filter_id].end(), widget), selected_nodes[current_filter_id].end());
    if (selected) {
        selected_nodes[current_filter_id].push_back(widget);
    } else {
    }
    _signal_primitive_changed.emit();
}

void FilterEditorCanvas::clear_selection()
{
    for (auto node : selected_nodes[current_filter_id]) {
        node->toggle_selection(false);
    }
    selected_nodes[current_filter_id].clear();
    _signal_primitive_changed.emit();
}

void FilterEditorCanvas::rubberband_select()
{
    /*Construct node list in the given region*/
    std::vector<NODE_TYPE *> nodes_in_region;
    for (auto& node : nodes[current_filter_id]) {
        double x, y;
        // node->get_position(x, y);
        canvas.get_child_position(*node, x, y);
        if (x >= rubberband_x && x <= rubberband_x + rubberband_size_x && y >= rubberband_y &&
            y <= rubberband_y + rubberband_size_y) {
            nodes_in_region.push_back(node.get());
        }
    }
    double mx{}, my{};
    Gdk::ModifierType mask;
    auto const display = get_display();
    auto seat = display->get_default_seat();
    auto device = seat->get_pointer();
    auto const surface = dynamic_cast<Gtk::Native &>(*get_root()).get_surface();
    g_assert(surface);
    surface->get_device_position(device, mx, my, mask);
    // if (SHIFT_DOWN) {
    if((mask & Gdk::ModifierType::SHIFT_MASK) == Gdk::ModifierType::SHIFT_MASK){
        for (auto node : nodes_in_region) {
            set_node_selection(node);
        }
    } else {
        clear_selection();
        for (auto node : nodes_in_region) {
            set_node_selection(node);
        }
    }
    _signal_primitive_changed.emit();
}
// double rubberband_x, rubberband_y, rubberband_size_x, rubberband_size_y;
// double drag_start_x, drag_start_y;
void FilterEditorCanvas::event_handler(double x, double y)
{
    static std::vector<std::pair<NODE_TYPE *, std::pair<double, double>>> start_positions;
    // static FilterEditorSource* starting_source;
    switch (current_event_type){
    case FilterEditorEvent::NONE:
        break; 
    case FilterEditorEvent::SELECT:
        if (SHIFT_DOWN) {
            if (active_widget != nullptr) {
                auto active_node = resolve_to_type<FilterEditorNode>(active_widget);
                if (active_node != nullptr) {
                    toggle_node_selection(active_node);
                }
            }

        } else {
            clear_selection();
            if (active_widget != nullptr) {
                auto active_node = resolve_to_type<FilterEditorNode>(active_widget);
                if (active_node != nullptr) {
                    set_node_selection(active_node);
                }
            }
        }
        // current_event_type = FilterEditorEvent::NONE;
        break;
    case FilterEditorEvent::PAN_START:
        drag_start_x = canvas.get_x_offset();
        drag_start_y = canvas.get_y_offset();
        current_event_type = FilterEditorEvent::PAN_UPDATE;
        break;
    case FilterEditorEvent::PAN_UPDATE:
        double offset_x, offset_y;
        gesture_drag->get_offset(offset_x, offset_y);
        update_offsets(drag_start_x - offset_x, drag_start_y - offset_y);
        break;
    case FilterEditorEvent::PAN_END:
        current_event_type = FilterEditorEvent::NONE;
        break;
    case FilterEditorEvent::MOVE_START:
        active_widget = resolve_to_type<FilterEditorNode>(active_widget);
        if (std::find(selected_nodes[current_filter_id].begin(), selected_nodes[current_filter_id].end(), (NODE_TYPE *)active_widget) ==
            selected_nodes[current_filter_id].end()) {
            clear_selection();
            set_node_selection((NODE_TYPE *)active_widget);
        }
        start_positions.clear();
        for (auto node : selected_nodes[current_filter_id]) {
            double x, y;
            canvas.get_child_position(*node, x, y);
            start_positions.push_back({node, {x, y}});
        }
        current_event_type = FilterEditorEvent::MOVE_UPDATE;
        break;
    case FilterEditorEvent::MOVE_UPDATE:
        // double offset_x, offset_y;
        gesture_drag->get_offset(offset_x, offset_y);
        for (auto pr : start_positions) {
            double x, y;
            x = pr.second.first;
            y = pr.second.second;
            place_node(pr.first, x + offset_x, y + offset_y, true);
        }
        break;
    case FilterEditorEvent::MOVE_END:
    {
        // TODO: Consider moving this to a seperate function
        auto filter = _dialog._filter_modifier.get_selected_filter();
        if(filter)
            DocumentUndo::maybeDone(filter->document,"moving", _("Moved primitive nodes"), INKSCAPE_ICON("dialog-filters"));
        // for(auto pr: start_positions){
        //     auto prim_node = dynamic_cast<FilterEditorPrimitiveNode*>(pr.first);
        //     if(prim_node != nullptr){
        //         auto prim = prim_node->get_primitive();
        //         // prim->getRepr()->setAttribute
        //     }
        // }
        start_positions.clear(); // Clearing the start positions for safety
        current_event_type = FilterEditorEvent::NONE;
        break;
    }
    case FilterEditorEvent::CONNECTION_START:
        if (active_widget != nullptr) {
            if (dynamic_cast<FilterEditorSource *>(active_widget) != nullptr) {
                double x, y;
                gesture_drag->get_start_point(x, y);
                double x_g, y_g;
                local_to_global(x, y, x_g, y_g);
                drag_global_coordinates = {{x_g, y_g}, {x_g, y_g}};
                starting_source = resolve_to_type<FilterEditorSource>(active_widget);
                current_event_type = FilterEditorEvent::CONNECTION_UPDATE;
            }
        }
        break;
    case FilterEditorEvent::CONNECTION_UPDATE:
    {
        double x, y;
        gesture_drag->get_start_point(x, y);
        double x_offset, y_offset;
        gesture_drag->get_offset(x_offset, y_offset);
        double x_end_g, y_end_g;
        local_to_global(x + x_offset, y + y_offset, x_end_g, y_end_g);
        drag_global_coordinates.second = {x_end_g, y_end_g};
        canvas.queue_draw();
        break;
    }
    case FilterEditorEvent::CONNECTION_END:
    {
        g_message("Ending connection");
        double x_start, y_start, x_offset, y_offset, x_end, y_end;
        gesture_drag->get_start_point(x_start, y_start);
        gesture_drag->get_offset(x_offset, y_offset);
        x_end = x_start + x_offset;
        y_end = y_start + y_offset;
        auto widget = get_widget_under(x_end, y_end);
        if (widget != nullptr) {
            auto sink = resolve_to_type<FilterEditorSink>(widget);
            if (sink != nullptr) {
                // TODO: Consider moving this check to another function
                if(output_node->get_connected_up_nodes().size() > 0 && output_node->get_connected_up_nodes()[0].second == starting_source->get_parent_node()){
                    // Don't create a connection, the upper node is connected to the output node.
                    // TODO: Give an error message
                }
                else{
                    create_connection(starting_source, sink);
                }
                g_message("Created a connection between starting source and sink");
                update_document();
            }
        }
        canvas.queue_draw();
        current_event_type = FilterEditorEvent::NONE;
        break;
    }
    case FilterEditorEvent::INVERTED_CONNECTION_START:{
        if (active_widget != nullptr) {
            if (dynamic_cast<FilterEditorSink *>(active_widget) != nullptr) {
                double x, y;
                gesture_drag->get_start_point(x, y);
                double x_g, y_g;
                local_to_global(x, y, x_g, y_g);
                drag_global_coordinates = {{x_g, y_g}, {x_g, y_g}};
                starting_sink = resolve_to_type<FilterEditorSink>(active_widget);
                current_event_type = FilterEditorEvent::INVERTED_CONNECTION_UPDATE;
            }
        }
    }
    case FilterEditorEvent::INVERTED_CONNECTION_UPDATE:{
        double x, y;
        gesture_drag->get_start_point(x, y);
        double x_offset, y_offset;
        gesture_drag->get_offset(x_offset, y_offset);
        double x_end_g, y_end_g;
        local_to_global(x + x_offset, y + y_offset, x_end_g, y_end_g);
        drag_global_coordinates.second = {x_end_g, y_end_g};
        canvas.queue_draw();
        break;
    }
    case FilterEditorEvent::INVERTED_CONNECTION_END:{
        double x_start, y_start, x_offset, y_offset, x_end, y_end;
        gesture_drag->get_start_point(x_start, y_start);
        gesture_drag->get_offset(x_offset, y_offset);
        x_end = x_start + x_offset;
        y_end = y_start + y_offset;
        auto widget = get_widget_under(x_end, y_end);
        if (widget != nullptr) {
            auto source = resolve_to_type<FilterEditorSource>(widget);
            if (source != nullptr) {
                if(output_node->get_connected_up_nodes().size() > 0 && output_node->get_connected_up_nodes()[0].second == source->get_parent_node()){
                    // Don't create a connection, the upper node is connected to the output node.
                }
                else{
                    create_connection(source, starting_sink);
                }
                
                // if(dynamic_cast<FilterEditorPrimitiveMergeNode*>(starting_sink->get_parent_node()) != nullptr){
                //     // dynamic_cast<FilterEditorPrimitiveMergeNode*>(starting_sink->get_parent_node())->remove_extra_sinks();
                //     dynamic_cast<FilterEditorPrimitiveMergeNode*>(starting_sink->get_parent_node())->add_sink();
                // }
            }
            else{
                
                if(dynamic_cast<FilterEditorPrimitiveMergeNode*>(starting_sink->get_parent_node()) != nullptr){
                    dynamic_cast<FilterEditorPrimitiveMergeNode*>(starting_sink->get_parent_node())->remove_extra_sinks();
                    dynamic_cast<FilterEditorPrimitiveMergeNode*>(starting_sink->get_parent_node())->add_sink();
                }
                #ifdef BREAK_LOOSE_CONNECTION
                else if(dynamic_cast<FilterEditorPrimitiveNode*>(starting_sink->get_parent_node()) != nullptr){
                    dynamic_cast<FilterEditorPrimitiveNode*>(starting_sink->get_parent_node())->set_sink_result(starting_sink, 0);
                }
                #endif
            }
        }
        update_document(true);
        canvas.queue_draw();
        current_event_type = FilterEditorEvent::NONE;
        break;
    }
    case FilterEditorEvent::RUBBERBAND_START:
    {
        double x_start, y_start;
        gesture_drag->get_start_point(x_start, y_start);
        canvas.put(*rubberband_rectangle, x_start, y_start);
        rubberband_rectangle->set_size_request(0, 0);
        current_event_type = FilterEditorEvent::RUBBERBAND_UPDATE;
    }
    case FilterEditorEvent::RUBBERBAND_UPDATE:{
        if (rubberband_rectangle->get_parent() == nullptr) {
            canvas.put(*rubberband_rectangle, x, y);
        }
        double x_start, y_start;
        gesture_drag->get_start_point(x_start, y_start);
        double offset_x, offset_y;
        gesture_drag->get_offset(offset_x, offset_y);
        double x_end = x_start + offset_x, y_end = y_start + offset_y;
        if (offset_x < 0) {
            x_start = x_end;
            offset_x = -offset_x;
        }
        if (offset_y < 0) {
            y_start = y_end;
            offset_y = -offset_y;
        }
        canvas.move(*rubberband_rectangle, x_start, y_start);
        rubberband_rectangle->set_size_request(offset_x, offset_y);
        rubberband_x = x_start;
        rubberband_y = y_start;
        rubberband_size_x = offset_x;
        rubberband_size_y = offset_y;
    }
    break;

    case FilterEditorEvent::RUBBERBAND_END:
        canvas.remove(*rubberband_rectangle);
        rubberband_select();
        current_event_type = FilterEditorEvent::NONE;
        break;
    }
}


void FilterEditorCanvas::on_scroll(const Gtk::EventControllerScroll &scroll) {};

void FilterEditorCanvas::initialize_gestures()
{
    gesture_click = Gtk::GestureClick::create();
    gesture_click->set_button(GDK_BUTTON_PRIMARY);

    gesture_click->signal_pressed().connect([this](int n_press, double x, double y) {
        canvas.grab_focus();
        active_widget = get_widget_under(x, y);
        current_event_type = FilterEditorEvent::NONE;
        in_click = true;
        in_drag = false;
    });
    gesture_click->signal_stopped().connect([this]() {
        // g_message("in stopped");
        // if (! in_click) {
        if (current_event_type != FilterEditorEvent::SELECT) {
            in_click = false;
            in_drag = true;
            if (active_widget != nullptr) {
                if (resolve_to_type<FilterEditorSource>(active_widget) != nullptr) {
                    current_event_type = FilterEditorEvent::CONNECTION_START;
                } else if (resolve_to_type<FilterEditorSink>(active_widget) != nullptr){
                    current_event_type = FilterEditorEvent::INVERTED_CONNECTION_START;
                    active_widget = resolve_to_type<FilterEditorSink>(active_widget);
                    if(dynamic_cast<FilterEditorSink*>(active_widget)->get_connections().size() > 0){
                        FilterEditorConnection* conn = dynamic_cast<FilterEditorSink*>(active_widget)->get_connections()[0];
                        FilterEditorSource* source = conn->get_source();
                        FilterEditorNode* source_node = source->get_parent_node();
                        FilterEditorNode* sink_node = dynamic_cast<FilterEditorSink*>(active_widget)->get_parent_node();
                        destroy_connection(conn, false); 
                    }
                } else if (resolve_to_type<FilterEditorNode>(active_widget) != nullptr) {
                    current_event_type = FilterEditorEvent::MOVE_START;
                } else {
                    current_event_type = FilterEditorEvent::RUBBERBAND_START;
                }
            }
            // g_message("Calling Event Handler from here");
            event_handler(0, 0);
        }
        else{
            current_event_type = FilterEditorEvent::NONE;
        }
    });
    gesture_click->signal_released().connect([this](int n_press, double x, double y) {
        // g_message("in released");
        if (!in_drag) {
            if(resolve_to_type<FilterEditorSink>(active_widget) != nullptr){
                auto sink = resolve_to_type<FilterEditorSink>(active_widget);
                if(sink->inp_index != -99){
                    if(dynamic_cast<FilterEditorPrimitiveNode*>(sink->get_parent_node()) != nullptr){
                        for(auto conn: sink->get_connections()){{
                            destroy_connection(conn, false);
                            // TODO: Ensure if this should be false or true. Manually update the document
                        }}
                        dynamic_cast<FilterEditorPrimitiveNode*>(sink->get_parent_node())->set_sink_result(sink, -2);
                        // current_event_type = FilterEditorEvent::SELECT;
                        
                    }
                    else{
                        sink->get_parent_node()->set_sink_result(sink, -2);
                    }
                    current_event_type = FilterEditorEvent::SELECT;
                }
                else{
                    current_event_type = FilterEditorEvent::SELECT;
                    event_handler(x, y); 
                }
            }
            else{
                current_event_type = FilterEditorEvent::SELECT;
                event_handler(x, y);
            }
        }
        in_click = false;
    });
    canvas.add_controller(gesture_click);

    gesture_drag = Gtk::GestureDrag::create();
    gesture_drag->set_button(0);
    gesture_drag->signal_drag_begin().connect([this](double start_x, double start_y) {
        if(current_event_type == FilterEditorEvent::NONE){
        in_drag = false;
        get_widget_under(start_x, start_y);
        if (gesture_drag->get_current_button() == GDK_BUTTON_MIDDLE) {
            current_event_type = FilterEditorEvent::PAN_START;
            event_handler(0, 0);
            in_drag = true;

        } else {
        }}
        else{
            
        }
    });
    gesture_drag->signal_drag_update().connect([this](double x, double y) {
        // if(current_event_type == FilterEditorEvent::NONE){
        //     return;
        // }
        if (in_drag && !in_click) {
            if (gesture_drag->get_current_button() == GDK_BUTTON_PRIMARY) {
                // Left click
                if (active_widget != nullptr) {
                    if (resolve_to_type<FilterEditorSource>(active_widget) != nullptr) {
                        current_event_type = FilterEditorEvent::CONNECTION_UPDATE;
                    } else if (resolve_to_type<FilterEditorSink>(active_widget) != nullptr) {
                        current_event_type = FilterEditorEvent::INVERTED_CONNECTION_UPDATE;
                    }
                    else if (resolve_to_type<FilterEditorNode>(active_widget) != nullptr) {
                        current_event_type = FilterEditorEvent::MOVE_UPDATE;
                    }
                } else {
                    current_event_type = FilterEditorEvent::RUBBERBAND_UPDATE;
                }
                event_handler(x, y);
            } else if (gesture_drag->get_current_button() == GDK_BUTTON_MIDDLE) {
                current_event_type = FilterEditorEvent::PAN_UPDATE;
                event_handler(x, y);
            }
        }
    });
    gesture_drag->signal_drag_end().connect([this](double x, double y) {
        if(current_event_type == FilterEditorEvent::NONE){
            return;
        }
        if (in_drag) {
            if (current_event_type == FilterEditorEvent::CONNECTION_UPDATE) {
                current_event_type = FilterEditorEvent::CONNECTION_END;
            } else if (current_event_type == FilterEditorEvent::INVERTED_CONNECTION_UPDATE){
                current_event_type = FilterEditorEvent::INVERTED_CONNECTION_END;
            } else if (current_event_type == FilterEditorEvent::MOVE_UPDATE) {
                current_event_type = FilterEditorEvent::MOVE_END;
            } else if (current_event_type == FilterEditorEvent::RUBBERBAND_UPDATE) {
                current_event_type = FilterEditorEvent::RUBBERBAND_END;
            }
            event_handler(x, y);
        }
    });

    canvas.add_controller(gesture_drag);

    /*Setting up temporary controllers related to clicks.*/
    gesture_right_click = Gtk::GestureClick::create();
    gesture_right_click->set_button(GDK_BUTTON_SECONDARY);
    gesture_right_click->signal_pressed().connect([this](int n_press, double x, double y) {
        canvas.grab_focus();
        _popover_menu->set_parent(canvas);
        _popover_menu->popup_at(canvas, x, y);
    });
    canvas.add_controller(gesture_right_click);

    key_controller = Gtk::EventControllerKey::create();
    key_controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    key_controller->signal_modifiers().connect(
        [this](Gdk::ModifierType state) -> bool {
            modifier_state = state;
            if ((modifier_state & Gdk::ModifierType::SHIFT_MASK) == Gdk::ModifierType::SHIFT_MASK) {
                // g_message("Shift pressed now");
            } else {
                // g_message("Shift not pressed anymore");
            }
            return true;
        },
        true);

    key_controller->signal_modifiers().connect(
        [this](Gdk::ModifierType state) -> bool {
            modifier_state = state;
            return true;
        },
        true);
    canvas.add_controller(key_controller);

    /*Setting up scroll controller.*/
    scroll_controller = Gtk::EventControllerScroll::create();
    scroll_controller->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL |
                                 Gtk::EventControllerScroll::Flags::HORIZONTAL);
    scroll_controller->set_propagation_phase(Gtk::PropagationPhase::BUBBLE);
    // scroll_controller->set_propagation_limit()
    // this->set_sensitive(false);
    scroll_controller->signal_scroll().connect(
        [this](double dx, double dy) {
            // if(current_event_type != FilterEditorEvent::NONE){
            //     return true;
            // }
            if(current_event_type == FilterEditorEvent::NONE){
                canvas.grab_focus();
                double mx{}, my{};
                Gdk::ModifierType mask;
                auto const display = get_display();
                auto seat = display->get_default_seat();
                auto device = seat->get_pointer();
                auto const surface = dynamic_cast<Gtk::Native &>(*get_root()).get_surface();
                g_assert(surface);
                surface->get_device_position(device, mx, my, mask);
                if ((mask & Gdk::ModifierType::SHIFT_MASK) == Gdk::ModifierType::SHIFT_MASK) {
                    zoom_fac = zoom_fac + dy * 0.1;
                    zoom_fac = std::max(0.5, std::min(2.0, zoom_fac));

                    Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
                    canvas.get_style_context()->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
                    provider->load_from_data(".canvas-fixed { transform: scale(" + std::to_string(zoom_fac) + "); }");
                    return true;
                } else {
                    update_offsets(canvas.get_x_offset() + dx * SCROLL_SENS, canvas.get_y_offset() + dy * SCROLL_SENS);
                    return true;
                }
                canvas.queue_draw();
            }
            else{
            }
            return true;
        },
        true);
    canvas.add_controller(scroll_controller);
    // add_controller(scroll_controller);
}

void FilterEditorCanvas::modify_observer(bool disable){
    static int count = 0;
    if(disable){
        _dialog._filter_modifier._observer->set(nullptr);
        count++;
    }
    else{
        count--;
        if(count == 0){
            _dialog._filter_modifier._observer->set(_dialog._filter_modifier.get_selected_filter());
        }
    }
}
// std::vector<std::pair<NODE_TYPE*, NODE_TYPE*>> connections;

// NODE_TYPE *FilterEditorCanvas::create_node(SPFilterPrimitive *primitive)
// {
//     std::shared_ptr<FilterEditorNode> node = std::make_shared<FilterEditorNode>(100, 0, 0, primitive);
//     nodes.push_back(node);
//     return node.get();
// };
// void remove_node(int node_id);
// void connect_nodes(int node1, int node2);
// void disconnect_nodes(int node1, int node2);
// void set_node_position(int node_id, int x, int y);

/*Geometry related*/
void FilterEditorCanvas::global_to_local(double xg, double yg, double &xl, double &yl)
{
    xl = xg - canvas.get_x_offset();
    yl = yg - canvas.get_y_offset();
};
void FilterEditorCanvas::local_to_global(double xl, double yl, double &xg, double &yg)
{
    xg = xl + canvas.get_x_offset();
    yg = yl + canvas.get_y_offset();
};
void FilterEditorCanvas::place_node(FilterEditorNode *node, double x, double y, bool local)
{
    if(dynamic_cast<FilterEditorPrimitiveNode*>(node) != nullptr){
        if(dynamic_cast<FilterEditorPrimitiveNode*>(node)->get_primitive()->getRepr() != nullptr){
            double update_x, update_y;
            if(local){
                local_to_global(x, y, update_x, update_y);
            }
            else{
                update_x = x;
                update_y = y;
            }
            dynamic_cast<FilterEditorPrimitiveNode*>(node)->get_primitive()->getRepr()->setAttributeSvgDouble("inkscape:filter-x", update_x);
            dynamic_cast<FilterEditorPrimitiveNode*>(node)->get_primitive()->getRepr()->setAttributeSvgDouble("inkscape:filter-y", update_y);
        }
        
    }
    else if(dynamic_cast<FilterEditorOutputNode*>(node) != nullptr){
            double update_x, update_y;
            if (local) {
                local_to_global(x, y, update_x, update_y);
            } else {
                update_x = x;
                update_y = y;
            }
            auto filter = _dialog._filter_modifier.get_selected_filter();  
            if(filter){
                filter->getRepr()->setAttributeSvgDouble("inkscape:output-x", update_x);
                filter->getRepr()->setAttributeSvgDouble("inkscape:output-y", update_y);
            }
    }
    else{

    }
    if (!local) {
        node->update_position(x, y);
        double xl, yl;
        global_to_local(x, y, xl, yl);
        if (node->get_parent() != &canvas) {
            canvas.put(*node, xl, yl);
        } else {
            canvas.move(*node, xl, yl);
        }
    } else {
        double xg, yg;
        local_to_global(x, y, xg, yg);
        node->update_position(xg, yg);
        if (node->get_parent() != &canvas) {
            canvas.put(*node, x, y);
        } else {
            canvas.move(*node, x, y);
        }
    }
};

using Inkscape::UI::Widget::AttrWidget;
using Inkscape::UI::Widget::ComboBoxEnum;
using Inkscape::UI::Widget::DualSpinScale;
using Inkscape::UI::Widget::SpinScale;

constexpr int max_convolution_kernel_size = 10;

static Glib::ustring const prefs_path = "/dialogs/filters";

// Returns the number of inputs available for the filter primitive type
static int input_count(const SPFilterPrimitive* prim)
{
    if(!prim)
        return 0;
    else if(is<SPFeBlend>(prim) || is<SPFeComposite>(prim) || is<SPFeDisplacementMap>(prim))
        return 2;
    else if(is<SPFeMerge>(prim)) {
        // Return the number of feMergeNode connections plus an extra
        return (int) (prim->children.size() + 1);
    }
    else
        return 1;
}

class CheckButtonAttr : public Gtk::CheckButton, public AttrWidget
{
public:
    CheckButtonAttr(bool def, const Glib::ustring& label,
                    Glib::ustring  tv, Glib::ustring  fv,
                    const SPAttr a, char* tip_text)
        : Gtk::CheckButton(label),
          AttrWidget(a, def),
          _true_val(std::move(tv)), _false_val(std::move(fv))
    {
        signal_toggled().connect(signal_attr_changed().make_slot());
        if (tip_text) {
            set_tooltip_text(tip_text);
        }
    }

    Glib::ustring get_as_attribute() const override
    {
        return get_active() ? _true_val : _false_val;
    }

    void set_from_attribute(SPObject* o) override
    {
        const gchar* val = attribute_value(o);
        if(val) {
            if(_true_val == val)
                set_active(true);
            else if(_false_val == val)
                set_active(false);
        } else {
            set_active(get_default()->as_bool());
        }
    }
private:
    const Glib::ustring _true_val, _false_val;
};

class SpinButtonAttr : public Inkscape::UI::Widget::SpinButton, public AttrWidget
{
public:
    SpinButtonAttr(double lower, double upper, double step_inc,
                   double climb_rate, int digits, const SPAttr a, double def, char* tip_text)
        : Inkscape::UI::Widget::SpinButton(climb_rate, digits),
          AttrWidget(a, def)
    {
        if (tip_text) {
            set_tooltip_text(tip_text);
        }
        set_range(lower, upper);
        set_increments(step_inc, 0);

        signal_value_changed().connect(signal_attr_changed().make_slot());
    }

    Glib::ustring get_as_attribute() const override
    {
        const double val = get_value();

        if(get_digits() == 0)
            return Glib::Ascii::dtostr((int)val);
        else
            return Glib::Ascii::dtostr(val);
    }

    void set_from_attribute(SPObject* o) override
    {
        const gchar* val = attribute_value(o);
        if(val){
            set_value(Glib::Ascii::strtod(val));
        } else {
            set_value(get_default()->as_double());
        }
    }
};

template <typename T> class ComboWithTooltip
    : public ComboBoxEnum<T>
{
public:
    ComboWithTooltip(T const default_value, Util::EnumDataConverter<T> const &c,
                     SPAttr const a = SPAttr::INVALID,
                     Glib::ustring const &tip_text = {})
        : ComboBoxEnum<T>(default_value, c, a, false)
    {
        this->set_tooltip_text(tip_text);
    }
};

// Contains an arbitrary number of spin buttons that use separate attributes
class MultiSpinButton : public Gtk::Box
{
public:
    MultiSpinButton(double lower, double upper, double step_inc,
                    double climb_rate, int digits,
                    std::vector<SPAttr> const &attrs,
                    std::vector<double> const &default_values,
                    std::vector<char *> const &tip_text)
    : Gtk::Box(Gtk::Orientation::HORIZONTAL)
    {
        g_assert(attrs.size()==default_values.size());
        g_assert(attrs.size()==tip_text.size());
        set_spacing(4);
        for(unsigned i = 0; i < attrs.size(); ++i) {
            unsigned index = attrs.size() - 1 - i;
            _spins.push_back(Gtk::make_managed<SpinButtonAttr>(lower, upper, step_inc, climb_rate, digits,
                                                               attrs[index], default_values[index], tip_text[index]));
            UI::pack_end(*this, *_spins.back(), true, true);
            _spins.back()->set_width_chars(3); // allow spin buttons to shrink to save space
        }
    }

    std::vector<SpinButtonAttr *> const &get_spinbuttons() const
    {
        return _spins;
    }

private:
    std::vector<SpinButtonAttr*> _spins;
};

// Contains two spinbuttons that describe a NumberOptNumber
class DualSpinButton : public Gtk::Box, public AttrWidget
{
public:
    DualSpinButton(char* def, double lower, double upper, double step_inc,
                   double climb_rate, int digits, const SPAttr a, char* tt1, char* tt2)
        : AttrWidget(a, def), //TO-DO: receive default num-opt-num as parameter in the constructor
          Gtk::Box(Gtk::Orientation::HORIZONTAL),
          _s1(climb_rate, digits), _s2(climb_rate, digits)
    {
        if (tt1) {
            _s1.set_tooltip_text(tt1);
        }
        if (tt2) {
            _s2.set_tooltip_text(tt2);
        }
        _s1.set_range(lower, upper);
        _s2.set_range(lower, upper);
        _s1.set_increments(step_inc, 0);
        _s2.set_increments(step_inc, 0);

        _s1.signal_value_changed().connect(signal_attr_changed().make_slot());
        _s2.signal_value_changed().connect(signal_attr_changed().make_slot());

        set_spacing(4);
        UI::pack_end(*this, _s2, true, true);
        UI::pack_end(*this, _s1, true, true);
    }

    Inkscape::UI::Widget::SpinButton& get_spinbutton1()
    {
        return _s1;
    }

    Inkscape::UI::Widget::SpinButton& get_spinbutton2()
    {
        return _s2;
    }

    Glib::ustring get_as_attribute() const override
    {
        double v1 = _s1.get_value();
        double v2 = _s2.get_value();

        if(_s1.get_digits() == 0) {
            v1 = (int)v1;
            v2 = (int)v2;
        }

        return Glib::Ascii::dtostr(v1) + " " + Glib::Ascii::dtostr(v2);
    }

    void set_from_attribute(SPObject* o) override
    {
        const gchar* val = attribute_value(o);
        NumberOptNumber n;
        if(val) {
            n.set(val);
        } else {
            n.set(get_default()->as_charptr());
        }
        _s1.set_value(n.getNumber());
        _s2.set_value(n.getOptNumber());

    }
private:
    Inkscape::UI::Widget::SpinButton _s1, _s2;
};

class ColorButton : public Widget::ColorPicker, public AttrWidget
{
public:
    ColorButton(unsigned int def, const SPAttr a, char* tip_text)
        : ColorPicker(_("Select color"), tip_text ? tip_text : "", Colors::Color(0x000000ff), false, false),
          AttrWidget(a, def)
    {
        connectChanged([this](Colors::Color const &color){ signal_attr_changed().emit(); });
        if (tip_text) {
            set_tooltip_text(tip_text);
        }
        setColor(Colors::Color(0xffffffff));
    }

    Glib::ustring get_as_attribute() const override
    {
        return get_current_color().toString(false);
    }

    void set_from_attribute(SPObject* o) override
    {
        const gchar* val = attribute_value(o);
        if (auto color = Colors::Color::parse(val)) {
            setColor(*color);
        } else {
            setColor(Colors::Color(get_default()->as_uint()));
        }
    }
};

// Used for tableValue in feComponentTransfer
class EntryAttr : public Gtk::Entry, public AttrWidget
{
public:
    EntryAttr(const SPAttr a, char* tip_text)
        : AttrWidget(a)
    {
        set_width_chars(3); // let it get narrow
        signal_changed().connect(signal_attr_changed().make_slot());
        if (tip_text) {
            set_tooltip_text(tip_text);
        }
    }

    // No validity checking is done
    Glib::ustring get_as_attribute() const override
    {
        return get_text();
    }

    void set_from_attribute(SPObject* o) override
    {
        const gchar* val = attribute_value(o);
        if(val) {
            set_text( val );
        } else {
            set_text( "" );
        }
    }
};

/* Displays/Edits the matrix for feConvolveMatrix or feColorMatrix */
class FilterEffectsDialog::MatrixAttr : public Gtk::Frame, public AttrWidget
{
public:
    MatrixAttr(const SPAttr a, char* tip_text = nullptr)
        : AttrWidget(a), _locked(false)
    {
        _model = Gtk::ListStore::create(_columns);
        _tree.set_model(_model);
        _tree.set_headers_visible(false);
        set_child(_tree);
        if (tip_text) {
            _tree.set_tooltip_text(tip_text);
        }
    }

    std::vector<double> get_values() const
    {
        std::vector<double> vec;
        for(const auto & iter : _model->children()) {
            for(unsigned c = 0; c < _tree.get_columns().size(); ++c)
                vec.push_back(iter[_columns.cols[c]]);
        }
        return vec;
    }

    void set_values(const std::vector<double>& v)
    {
        unsigned i = 0;
        for (auto &&iter : _model->children()) {
            for(unsigned c = 0; c < _tree.get_columns().size(); ++c) {
                if(i >= v.size())
                    return;
                iter[_columns.cols[c]] = v[i];
                ++i;
            }
        }
    }

    Glib::ustring get_as_attribute() const override
    {
        // use SVGOStringStream to output SVG-compatible doubles
        Inkscape::SVGOStringStream os;

        for(const auto & iter : _model->children()) {
            for(unsigned c = 0; c < _tree.get_columns().size(); ++c) {
                os << iter[_columns.cols[c]] << " ";
            }
        }

        return os.str();
    }

    void set_from_attribute(SPObject* o) override
    {
        if(o) {
            if(is<SPFeConvolveMatrix>(o)) {
                auto conv = cast<SPFeConvolveMatrix>(o);
                int cols, rows;
                cols = (int)conv->get_order().getNumber();
                if (cols > max_convolution_kernel_size)
                    cols = max_convolution_kernel_size;
                rows = conv->get_order().optNumIsSet() ? (int)conv->get_order().getOptNumber() : cols;
                update(o, rows, cols);
            }
            else if(is<SPFeColorMatrix>(o))
                update(o, 4, 5);
        }
    }
private:
    class MatrixColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        MatrixColumns()
        {
            cols.resize(max_convolution_kernel_size);
            for(auto & col : cols)
                add(col);
        }
        std::vector<Gtk::TreeModelColumn<double> > cols;
    };

    void update(SPObject* o, const int rows, const int cols)
    {
        if(_locked)
            return;

        _model->clear();

        _tree.remove_all_columns();

        std::vector<gdouble> const *values = nullptr;
        if(is<SPFeColorMatrix>(o))
            values = &cast<SPFeColorMatrix>(o)->get_values();
        else if(is<SPFeConvolveMatrix>(o))
            values = &cast<SPFeConvolveMatrix>(o)->get_kernel_matrix();
        else
            return;

        if(o) {
            for(int i = 0; i < cols; ++i) {
                _tree.append_column_numeric_editable("", _columns.cols[i], "%.2f");
                dynamic_cast<Gtk::CellRendererText &>(*_tree.get_column_cell_renderer(i))
                    .signal_edited().connect(
                        sigc::mem_fun(*this, &MatrixAttr::rebind));
            }

            int ndx = 0;
            for(int r = 0; r < rows; ++r) {
                Gtk::TreeRow row = *(_model->append());
                // Default to identity matrix
                for(int c = 0; c < cols; ++c, ++ndx)
                    row[_columns.cols[c]] = ndx < (int)values->size() ? (*values)[ndx] : (r == c ? 1 : 0);
            }
        }
    }

    void rebind(const Glib::ustring&, const Glib::ustring&)
    {
        _locked = true;
        signal_attr_changed()();
        _locked = false;
    }

    bool _locked;
    Gtk::TreeView _tree;
    Glib::RefPtr<Gtk::ListStore> _model;
    MatrixColumns _columns;
};

// Displays a matrix or a slider for feColorMatrix
class FilterEffectsDialog::ColorMatrixValues : public Gtk::Frame, public AttrWidget
{
public:
    ColorMatrixValues()
        : AttrWidget(SPAttr::VALUES),
          // TRANSLATORS: this dialog is accessible via menu Filters - Filter editor
          _matrix(SPAttr::VALUES, _("This matrix determines a linear transform on color space. Each line affects one of the color components. Each column determines how much of each color component from the input is passed to the output. The last column does not depend on input colors, so can be used to adjust a constant component value.")),
          _saturation("", 1, 0, 1, 0.1, 0.01, 2, SPAttr::VALUES),
          _angle("", 0, 0, 360, 0.1, 0.01, 1, SPAttr::VALUES),
          _label(C_("Label", "None"), Gtk::Align::START),
          _use_stored(false),
          _saturation_store(1.0),
          _angle_store(0)
    {
        _matrix.signal_attr_changed().connect(signal_attr_changed().make_slot());
        _saturation.signal_attr_changed().connect(signal_attr_changed().make_slot());
        _angle.signal_attr_changed().connect(signal_attr_changed().make_slot());
        signal_attr_changed().connect(sigc::mem_fun(*this, &ColorMatrixValues::update_store));

        _label.set_sensitive(false);

        add_css_class("flat");
    }

    void set_from_attribute(SPObject* o) override
    {
        std::string values_string;
        if(is<SPFeColorMatrix>(o)) {
            auto col = cast<SPFeColorMatrix>(o);
            unset_child();

            switch(col->get_type()) {
                case COLORMATRIX_SATURATE:
                    set_child(_saturation);
                    if(_use_stored)
                        _saturation.set_value(_saturation_store);
                    else
                        _saturation.set_from_attribute(o);
                    values_string = Glib::Ascii::dtostr(_saturation.get_value());
                    break;

                case COLORMATRIX_HUEROTATE:
                    set_child(_angle);
                    if(_use_stored)
                        _angle.set_value(_angle_store);
                    else
                        _angle.set_from_attribute(o);
                    values_string = Glib::Ascii::dtostr(_angle.get_value());
                    break;

                case COLORMATRIX_LUMINANCETOALPHA:
                    set_child(_label);
                    break;

                case COLORMATRIX_MATRIX:
                default:
                    set_child(_matrix);
                    if(_use_stored)
                        _matrix.set_values(_matrix_store);
                    else
                        _matrix.set_from_attribute(o);
                    for (auto v : _matrix.get_values()) {
                        values_string += Glib::Ascii::dtostr(v) + " ";
                    }
                    values_string.pop_back();
            }

            // The filter effects widgets derived from AttrWidget automatically update the
            // attribute on use. In this case, however, we must also update "values" whenever
            // "type" is changed.
            auto repr = o->getRepr();
            if (values_string.empty()) {
                repr->removeAttribute("values");
            } else {
                repr->setAttribute("values", values_string);
            }

            _use_stored = true;
        }
    }

    Glib::ustring get_as_attribute() const override
    {
        const Widget* w = get_child();
        if(w == &_label)
            return "";
        if (auto attrw = dynamic_cast<const AttrWidget *>(w))
            return attrw->get_as_attribute();
        g_assert_not_reached();
        return "";
    }

    void clear_store()
    {
        _use_stored = false;
    }
private:
    void update_store()
    {
        const Widget* w = get_child();
        if(w == &_matrix)
            _matrix_store = _matrix.get_values();
        else if(w == &_saturation)
            _saturation_store = _saturation.get_value();
        else if(w == &_angle)
            _angle_store = _angle.get_value();
    }

    MatrixAttr _matrix;
    SpinScale _saturation;
    SpinScale _angle;
    Gtk::Label _label;

    // Store separate values for the different color modes
    bool _use_stored;
    std::vector<double> _matrix_store;
    double _saturation_store;
    double _angle_store;
};

static Inkscape::UI::Dialog::FileOpenDialog * selectFeImageFileInstance = nullptr;

//Displays a chooser for feImage input
//It may be a filename or the id for an SVG Element
//described in xlink:href syntax
class FileOrElementChooser : public Gtk::Box, public AttrWidget
{
public:
    FileOrElementChooser(FilterEffectsDialog& d, const SPAttr a)
        : AttrWidget(a)
        , _dialog(d)
        , Gtk::Box(Gtk::Orientation::HORIZONTAL)
    {
        set_spacing(3);
        UI::pack_start(*this, _entry, true, true);
        UI::pack_start(*this, _fromFile, false, false);
        UI::pack_start(*this, _fromSVGElement, false, false);

        _fromFile.set_image_from_icon_name("document-open");
        _fromFile.set_tooltip_text(_("Choose image file"));
        _fromFile.signal_clicked().connect(sigc::mem_fun(*this, &FileOrElementChooser::select_file));

        _fromSVGElement.set_label(_("SVG Element"));
        _fromSVGElement.set_tooltip_text(_("Use selected SVG element"));
        _fromSVGElement.signal_clicked().connect(sigc::mem_fun(*this, &FileOrElementChooser::select_svg_element));

        _entry.set_width_chars(1);
        _entry.signal_changed().connect(signal_attr_changed().make_slot());

        set_visible(true);
    }

    // Returns the element in xlink:href form.
    Glib::ustring get_as_attribute() const override
    {
        return _entry.get_text();
    }


    void set_from_attribute(SPObject* o) override
    {
        const gchar* val = attribute_value(o);
        if(val) {
            _entry.set_text(val);
        } else {
            _entry.set_text("");
        }
    }

private:
    void select_svg_element() {
        Inkscape::Selection* sel = _dialog.getDesktop()->getSelection();
        if (sel->isEmpty()) return;
        Inkscape::XML::Node* node = sel->xmlNodes().front();
        if (!node || !node->matchAttributeName("id")) return;

        std::ostringstream xlikhref;
        xlikhref << "#" << node->attribute("id");
        _entry.set_text(xlikhref.str());
    }

    void select_file(){

        // Get the current directory for finding files.
        std::string open_path;
        get_start_directory(open_path, "/dialogs/open/path");

        // Create a dialog if we don't already have one.
        if (!selectFeImageFileInstance) {
            selectFeImageFileInstance =
                Inkscape::UI::Dialog::FileOpenDialog::create(
                    *_dialog.getDesktop()->getInkscapeWindow(),
                    open_path,
                    Inkscape::UI::Dialog::SVG_TYPES, /* TODO: any image, not just svg */
                    (char const *)_("Select an image to be used as input.")).release();
        }

        // Show the dialog.
        bool const success = selectFeImageFileInstance->show();
        if (!success) {
            return;
        }

        // User selected something.  Get name and type.
        auto file = selectFeImageFileInstance->getFile();
        if (!file) {
            return;
        }

        auto path = selectFeImageFileInstance->getCurrentDirectory();
        if (!path) {
            return;
        }

        open_path = path->get_path();
        open_path.append(G_DIR_SEPARATOR_S);

        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->setString("/dialogs/open/path", open_path);

        _entry.set_text(file->get_parse_name());
    }

    Gtk::Entry _entry;
    Gtk::Button _fromFile;
    Gtk::Button _fromSVGElement;
    FilterEffectsDialog &_dialog;
};

class FilterEffectsDialog::Settings
{
public:
    typedef sigc::slot<void (const AttrWidget*)> SetAttrSlot;

    Settings(FilterEffectsDialog& d, Gtk::Box& b, SetAttrSlot slot, const int maxtypes)
        : _dialog(d), _set_attr_slot(std::move(slot)), _current_type(-1), _max_types(maxtypes)
    {
        _groups.resize(_max_types);
        _attrwidgets.resize(_max_types);
        _size_group = Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL);

        for(int i = 0; i < _max_types; ++i) {
            _groups[i] = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
            b.set_spacing(4);
            UI::pack_start(b, *_groups[i], UI::PackOptions::shrink);
        }
        //_current_type = 0;  If set to 0 then update_and_show() fails to update properly.
    }

    void show_current_only() {
        for (auto& group : _groups) {
            group->set_visible(false);
        }
        auto t = get_current_type();
        if (t >= 0) {
            _groups[t]->set_visible(true);
        }
    }

    // Show the active settings group and update all the AttrWidgets with new values
    void show_and_update(const int t, SPObject* ob)
    {
        if (t != _current_type) {
            type(t);

            for (auto& group : _groups) {
                group->set_visible(false);
            }
        }

        if (t >= 0) {
            _groups[t]->set_visible(true);
        }

        _dialog.set_attrs_locked(true);
        for(auto & i : _attrwidgets[_current_type])
            i->set_from_attribute(ob);
        _dialog.set_attrs_locked(false);
    }

    int get_current_type() const
    {
        return _current_type;
    }

    void type(const int t)
    {
        _current_type = t;
    }

    void add_no_params()
    {
        auto const lbl = Gtk::make_managed<Gtk::Label>(_("This SVG filter effect does not require any parameters."));
        lbl->set_wrap();
        lbl->set_wrap_mode(Pango::WrapMode::WORD);
        add_widget(lbl, "");
    }

    // LightSource
    LightSourceControl* add_lightsource();

    // Component Transfer Values
    ComponentTransferValues* add_componenttransfervalues(const Glib::ustring& label, SPFeFuncNode::Channel channel);

    // CheckButton
    CheckButtonAttr* add_checkbutton(bool def, const SPAttr attr, const Glib::ustring& label,
                                     const Glib::ustring& tv, const Glib::ustring& fv, char* tip_text = nullptr)
    {
        auto const cb = Gtk::make_managed<CheckButtonAttr>(def, label, tv, fv, attr, tip_text);
        add_widget(cb, "");
        add_attr_widget(cb);
        return cb;
    }

    // ColorButton
    ColorButton* add_color(unsigned int def, const SPAttr attr, const Glib::ustring& label, char* tip_text = nullptr)
    {
        auto const col = Gtk::make_managed<ColorButton>(def, attr, tip_text);
        add_widget(col, label);
        add_attr_widget(col);
        return col;
    }

    // Matrix
    MatrixAttr* add_matrix(const SPAttr attr, const Glib::ustring& label, char* tip_text)
    {
        auto const conv = Gtk::make_managed<MatrixAttr>(attr, tip_text);
        add_widget(conv, label);
        add_attr_widget(conv);
        return conv;
    }

    // ColorMatrixValues
    ColorMatrixValues* add_colormatrixvalues(const Glib::ustring& label)
    {
        auto const cmv = Gtk::make_managed<ColorMatrixValues>();
        add_widget(cmv, label);
        add_attr_widget(cmv);
        return cmv;
    }

    // SpinScale
    SpinScale* add_spinscale(double def, const SPAttr attr, const Glib::ustring& label,
                         const double lo, const double hi, const double step_inc, const double page_inc, const int digits, char* tip_text = nullptr)
    {
        Glib::ustring tip_text2;
        if (tip_text)
            tip_text2 = tip_text;
        auto const spinslider = Gtk::make_managed<SpinScale>("", def, lo, hi, step_inc, page_inc, digits, attr, tip_text2);
        add_widget(spinslider, label);
        add_attr_widget(spinslider);
        return spinslider;
    }

    // DualSpinScale
    DualSpinScale* add_dualspinscale(const SPAttr attr, const Glib::ustring& label,
                                     const double lo, const double hi, const double step_inc,
                                     const double climb, const int digits,
                                     const Glib::ustring tip_text1 = "",
                                     const Glib::ustring tip_text2 = "")
    {
        auto const dss = Gtk::make_managed<DualSpinScale>("", "", lo, lo, hi, step_inc, climb, digits, attr, tip_text1, tip_text2);
        add_widget(dss, label);
        add_attr_widget(dss);
        return dss;
    }

    // SpinButton
    SpinButtonAttr* add_spinbutton(double defalt_value, const SPAttr attr, const Glib::ustring& label,
                                       const double lo, const double hi, const double step_inc,
                                       const double climb, const int digits, char* tip = nullptr)
    {
        auto const sb = Gtk::make_managed<SpinButtonAttr>(lo, hi, step_inc, climb, digits, attr, defalt_value, tip);
        add_widget(sb, label);
        add_attr_widget(sb);
        return sb;
    }

    // DualSpinButton
    DualSpinButton* add_dualspinbutton(char* defalt_value, const SPAttr attr, const Glib::ustring& label,
                                       const double lo, const double hi, const double step_inc,
                                       const double climb, const int digits, char* tip1 = nullptr, char* tip2 = nullptr)
    {
        auto const dsb = Gtk::make_managed<DualSpinButton>(defalt_value, lo, hi, step_inc, climb, digits, attr, tip1, tip2);
        add_widget(dsb, label);
        add_attr_widget(dsb);
        return dsb;
    }

    // MultiSpinButton
    MultiSpinButton* add_multispinbutton(double def1, double def2, const SPAttr attr1, const SPAttr attr2,
                                         const Glib::ustring& label, const double lo, const double hi,
                                         const double step_inc, const double climb, const int digits, char* tip1 = nullptr, char* tip2 = nullptr)
    {
        auto const attrs          = std::vector{attr1, attr2};
        auto const default_values = std::vector{ def1,  def2};
        auto const tips           = std::vector{ tip1,  tip2};
        auto const msb = Gtk::make_managed<MultiSpinButton>(lo, hi, step_inc, climb, digits, attrs, default_values, tips);
        add_widget(msb, label);
        for (auto const i : msb->get_spinbuttons())
            add_attr_widget(i);
        return msb;
    }

    MultiSpinButton* add_multispinbutton(double def1, double def2, double def3, const SPAttr attr1, const SPAttr attr2,
                                         const SPAttr attr3, const Glib::ustring& label, const double lo,
                                         const double hi, const double step_inc, const double climb, const int digits, char* tip1 = nullptr, char* tip2 = nullptr, char* tip3 = nullptr)
    {
        auto const attrs          = std::vector{attr1, attr2, attr3};
        auto const default_values = std::vector{ def1,  def2,  def3};
        auto const tips           = std::vector{ tip1,  tip2,  tip3};
        auto const msb = Gtk::make_managed<MultiSpinButton>(lo, hi, step_inc, climb, digits, attrs, default_values, tips);
        add_widget(msb, label);
        for (auto const i : msb->get_spinbuttons())
            add_attr_widget(i);
        return msb;
    }

    // FileOrElementChooser
    FileOrElementChooser* add_fileorelement(const SPAttr attr, const Glib::ustring& label)
    {
        auto const foech = Gtk::make_managed<FileOrElementChooser>(_dialog, attr);
        add_widget(foech, label);
        add_attr_widget(foech);
        return foech;
    }

    // ComboBoxEnum
    template <typename T> ComboWithTooltip<T>* add_combo(T default_value, const SPAttr attr,
                                                         const Glib::ustring& label,
                                                         const Util::EnumDataConverter<T>& conv,
                                                         const Glib::ustring& tip_text = {})
    {
        auto const combo = Gtk::make_managed<ComboWithTooltip<T>>(default_value, conv, attr, tip_text);
        add_widget(combo, label);
        add_attr_widget(combo);
        return combo;
    }

    // Entry
    EntryAttr* add_entry(const SPAttr attr,
                         const Glib::ustring& label,
                         char* tip_text = nullptr)
    {
        auto const entry = Gtk::make_managed<EntryAttr>(attr, tip_text);
        add_widget(entry, label);
        add_attr_widget(entry);
        return entry;
    }

    Glib::RefPtr<Gtk::SizeGroup> _size_group;

private:
    void add_attr_widget(AttrWidget* a)
    {
        _attrwidgets[_current_type].push_back(a);
        a->signal_attr_changed().connect(sigc::bind(_set_attr_slot, a));
        // add_widget() takes a managed widget, so dtor will delete & disconnect
    }

    /* Adds a new settings widget using the specified label. The label will be formatted with a colon
       and all widgets within the setting group are aligned automatically. */
    void add_widget(Gtk::Widget* w, const Glib::ustring& label)
    {
        g_assert(w->is_managed_());

        auto const hb = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
        hb->set_spacing(6);

        if (label != "") {
            auto const lbl = Gtk::make_managed<Gtk::Label>(label);
            lbl->set_xalign(0.0);
            UI::pack_start(*hb, *lbl, UI::PackOptions::shrink);
            _size_group->add_widget(*lbl);
        }

        UI::pack_start(*hb, *w, UI::PackOptions::expand_widget);
        UI::pack_start(*_groups[_current_type], *hb, UI::PackOptions::expand_widget);
    }

    std::vector<Gtk::Box*> _groups;
    FilterEffectsDialog& _dialog;
    SetAttrSlot _set_attr_slot;
    std::vector<std::vector< AttrWidget*> > _attrwidgets;
    int _current_type, _max_types;
};

// Displays sliders and/or tables for feComponentTransfer
class FilterEffectsDialog::ComponentTransferValues : public Gtk::Frame, public AttrWidget
{
public:
    ComponentTransferValues(FilterEffectsDialog& d, SPFeFuncNode::Channel channel)
        : AttrWidget(SPAttr::INVALID),
          _dialog(d),
          _settings(d, _box, sigc::mem_fun(*this, &ComponentTransferValues::set_func_attr), COMPONENTTRANSFER_TYPE_ERROR),
          _type(ComponentTransferTypeConverter, SPAttr::TYPE, false),
          _channel(channel),
          _funcNode(nullptr),
          _box(Gtk::Orientation::VERTICAL)
    {
        add_css_class("flat");

        set_child(_box);
        _box.prepend(_type);

        _type.signal_changed().connect(sigc::mem_fun(*this, &ComponentTransferValues::on_type_changed));

        _settings.type(COMPONENTTRANSFER_TYPE_LINEAR);
        _settings.add_spinscale(1, SPAttr::SLOPE,     _("Slope"),     -10, 10, 0.1, 0.01, 2);
        _settings.add_spinscale(0, SPAttr::INTERCEPT, _("Intercept"), -10, 10, 0.1, 0.01, 2);

        _settings.type(COMPONENTTRANSFER_TYPE_GAMMA);
        _settings.add_spinscale(1, SPAttr::AMPLITUDE, _("Amplitude"),   0, 10, 0.1, 0.01, 2);
        _settings.add_spinscale(1, SPAttr::EXPONENT,  _("Exponent"),    0, 10, 0.1, 0.01, 2);
        _settings.add_spinscale(0, SPAttr::OFFSET,    _("Offset"),    -10, 10, 0.1, 0.01, 2);

        _settings.type(COMPONENTTRANSFER_TYPE_TABLE);
        _settings.add_entry(SPAttr::TABLEVALUES,  _("Values"), _("List of stops with interpolated output"));

        _settings.type(COMPONENTTRANSFER_TYPE_DISCRETE);
        _settings.add_entry(SPAttr::TABLEVALUES,  _("Values"), _("List of discrete values for a step function"));

        //_settings.type(COMPONENTTRANSFER_TYPE_IDENTITY);
        _settings.type(-1); // Force update_and_show() to show/hide windows correctly
    }

    // FuncNode can be in any order so we must search to find correct one.
    SPFeFuncNode* find_node(SPFeComponentTransfer* ct)
    {
        SPFeFuncNode* funcNode = nullptr;
        bool found = false;
        for(auto& node: ct->children) {
            funcNode = cast<SPFeFuncNode>(&node);
            if( funcNode->channel == _channel ) {
                found = true;
                break;
            }
        }
        if( !found )
            funcNode = nullptr;

        return funcNode;
    }

    void set_func_attr(const AttrWidget* input)
    {
        _dialog.set_attr( _funcNode, input->get_attribute(), input->get_as_attribute().c_str());
    }

    // Set new type and update widget visibility
    void set_from_attribute(SPObject* o) override
    {
        // See componenttransfer.cpp
        if(is<SPFeComponentTransfer>(o)) {
            auto ct = cast<SPFeComponentTransfer>(o);

            _funcNode = find_node(ct);
            if( _funcNode ) {
                _type.set_from_attribute( _funcNode );
            } else {
                // Create <funcNode>
                // SPFilterPrimitive* prim = _dialog._primitive_list.get_selected();
                SPFilterPrimitive* prim = _dialog._filter_canvas.get_selected_primitive();
                if(prim) {
                    Inkscape::XML::Document *xml_doc = prim->document->getReprDoc();
                    Inkscape::XML::Node *repr = nullptr;
                    switch(_channel) {
                        case SPFeFuncNode::R:
                            repr = xml_doc->createElement("svg:feFuncR");
                            break;
                        case SPFeFuncNode::G:
                            repr = xml_doc->createElement("svg:feFuncG");
                            break;
                        case SPFeFuncNode::B:
                            repr = xml_doc->createElement("svg:feFuncB");
                            break;
                        case SPFeFuncNode::A:
                            repr = xml_doc->createElement("svg:feFuncA");
                            break;
                    }

                    //XML Tree being used directly here while it shouldn't be.
                    prim->getRepr()->appendChild(repr);
                    Inkscape::GC::release(repr);

                    // Now we should find it!
                    _funcNode = find_node(ct);
                    if( _funcNode ) {
                        _funcNode->setAttribute( "type", "identity" );
                    } else {
                        //std::cerr << "ERROR ERROR: feFuncX not found!" << std::endl;
                    }
                }
            }

            update();
        }
    }

private:
    void on_type_changed()
    {
        // SPFilterPrimitive* prim = _dialog._primitive_list.get_selected();
        SPFilterPrimitive* prim = _dialog._filter_canvas.get_selected_primitive();
        if(prim) {
            _funcNode->setAttributeOrRemoveIfEmpty("type", _type.get_as_attribute());

            SPFilter* filter = _dialog._filter_modifier.get_selected_filter();
            g_assert(filter);
            filter->requestModified(SP_OBJECT_MODIFIED_FLAG);

            DocumentUndo::done(prim->document, _("New transfer function type"), INKSCAPE_ICON("dialog-filters"));
            update();
        }
    }

    void update()
    {
        SPFilterPrimitive* prim = _dialog._filter_canvas.get_selected_primitive();
        if(prim && _funcNode) {
            _settings.show_and_update(_type.get_active_data()->id, _funcNode);
        }
    }

public:
    Glib::ustring get_as_attribute() const override
    {
        return "";
    }

    FilterEffectsDialog& _dialog;
    Gtk::Box _box;
    Settings _settings;
    ComboBoxEnum<FilterComponentTransferType> _type;
    SPFeFuncNode::Channel _channel; // RGBA
    SPFeFuncNode* _funcNode;
};

// Settings for the three light source objects
class FilterEffectsDialog::LightSourceControl
    : public AttrWidget
    , public Gtk::Box
{
public:
    LightSourceControl(FilterEffectsDialog& d)
        : AttrWidget(SPAttr::INVALID),
          Gtk::Box(Gtk::Orientation::VERTICAL),
          _dialog(d),
          _settings(d, *this, sigc::mem_fun(_dialog, &FilterEffectsDialog::set_child_attr_direct), LIGHT_ENDSOURCE),
          _light_label(_("Light Source:")),
          _light_source(LightSourceConverter),
          _locked(false),
          _light_box(Gtk::Orientation::HORIZONTAL, 6)
    {
        _light_label.set_xalign(0.0);
        _settings._size_group->add_widget(_light_label);
        UI::pack_start(_light_box, _light_label, UI::PackOptions::shrink);
        UI::pack_start(_light_box, _light_source, UI::PackOptions::expand_widget);

        prepend(_light_box);
        _light_source.signal_changed().connect(sigc::mem_fun(*this, &LightSourceControl::on_source_changed));

        // FIXME: these range values are complete crap

        _settings.type(LIGHT_DISTANT);
        _settings.add_spinscale(0, SPAttr::AZIMUTH, _("Azimuth:"), 0, 360, 1, 1, 0, _("Direction angle for the light source on the XY plane, in degrees"));
        _settings.add_spinscale(0, SPAttr::ELEVATION, _("Elevation:"), 0, 360, 1, 1, 0, _("Direction angle for the light source on the YZ plane, in degrees"));

        _settings.type(LIGHT_POINT);
        _settings.add_multispinbutton(/*default x:*/ (double) 0, /*default y:*/ (double) 0, /*default z:*/ (double) 0, SPAttr::X, SPAttr::Y, SPAttr::Z, _("Location:"), -99999, 99999, 1, 100, 0, _("X coordinate"), _("Y coordinate"), _("Z coordinate"));

        _settings.type(LIGHT_SPOT);
        _settings.add_multispinbutton(/*default x:*/ (double) 0, /*default y:*/ (double) 0, /*default z:*/ (double) 0, SPAttr::X, SPAttr::Y, SPAttr::Z, _("Location:"), -99999, 99999, 1, 100, 0, _("X coordinate"), _("Y coordinate"), _("Z coordinate"));
        _settings.add_multispinbutton(/*default x:*/ (double) 0, /*default y:*/ (double) 0, /*default z:*/ (double) 0,
                                      SPAttr::POINTSATX, SPAttr::POINTSATY, SPAttr::POINTSATZ,
                                      _("Points at:"), -99999, 99999, 1, 100, 0, _("X coordinate"), _("Y coordinate"), _("Z coordinate"));
        _settings.add_spinscale(1, SPAttr::SPECULAREXPONENT, _("Specular Exponent:"), 0.1, 100, 0.1, 1, 1, _("Exponent value controlling the focus for the light source"));
        //TODO: here I have used 100 degrees as default value. But spec says that if not specified, no limiting cone is applied. So, there should be a way for the user to set a "no limiting cone" option.
        _settings.add_spinscale(100, SPAttr::LIMITINGCONEANGLE, _("Cone Angle:"), 0, 180, 1, 5, 0, _("This is the angle between the spot light axis (i.e. the axis between the light source and the point to which it is pointing at) and the spot light cone. No light is projected outside this cone."));

        _settings.type(-1); // Force update_and_show() to show/hide windows correctly
    }

private:
    Glib::ustring get_as_attribute() const override
    {
        return "";
    }

    void set_from_attribute(SPObject* o) override
    {
        if(_locked)
            return;

        _locked = true;

        SPObject* child = o->firstChild();

        if(is<SPFeDistantLight>(child))
            _light_source.set_active(0);
        else if(is<SPFePointLight>(child))
            _light_source.set_active(1);
        else if(is<SPFeSpotLight>(child))
            _light_source.set_active(2);
        else
            _light_source.set_active(-1);

        update();

        _locked = false;
    }

    void on_source_changed()
    {
        if(_locked)
            return;

        // SPFilterPrimitive* prim = _dialog._primitive_list.get_selected();
        SPFilterPrimitive* prim = _dialog._filter_canvas.get_selected_primitive();
        if(prim) {
            _locked = true;

            SPObject* child = prim->firstChild();
            const int ls = _light_source.get_active_row_number();
            // Check if the light source type has changed
            if(!(ls == -1 && !child) &&
               !(ls == 0 && is<SPFeDistantLight>(child)) &&
               !(ls == 1 && is<SPFePointLight>(child)) &&
               !(ls == 2 && is<SPFeSpotLight>(child))) {
                if(child)
                    //XML Tree being used directly here while it shouldn't be.
                    sp_repr_unparent(child->getRepr());

                if(ls != -1) {
                    Inkscape::XML::Document *xml_doc = prim->document->getReprDoc();
                    Inkscape::XML::Node *repr = xml_doc->createElement(_light_source.get_active_data()->key.c_str());
                    //XML Tree being used directly here while it shouldn't be.
                    prim->getRepr()->appendChild(repr);
                    Inkscape::GC::release(repr);
                }

                DocumentUndo::done(prim->document, _("New light source"), INKSCAPE_ICON("dialog-filters"));
                update();
            }

            _locked = false;
        }
    }

    void update()
    {
        set_visible(true);

        // SPFilterPrimitive* prim = _dialog._primitive_list.get_selected();
        SPFilterPrimitive* prim = _dialog._filter_canvas.get_selected_primitive();
        if (prim && prim->firstChild()) {
            _settings.show_and_update(_light_source.get_active_data()->id, prim->firstChild());
        }
        else {
            _settings.show_current_only();
        }
    }

    FilterEffectsDialog& _dialog;
    Settings _settings;
    Gtk::Box _light_box;
    Gtk::Label _light_label;
    ComboBoxEnum<LightSource> _light_source;
    bool _locked;
};

FilterEffectsDialog::ComponentTransferValues* FilterEffectsDialog::Settings::add_componenttransfervalues(const Glib::ustring& label, SPFeFuncNode::Channel channel)
{
    auto const ct = Gtk::make_managed<ComponentTransferValues>(_dialog, channel);
    add_widget(ct, label);
    add_attr_widget(ct);
    ct->set_margin_top(4);
    ct->set_margin_bottom(4);
    return ct;
}

FilterEffectsDialog::LightSourceControl* FilterEffectsDialog::Settings::add_lightsource()
{
    auto const ls = Gtk::make_managed<LightSourceControl>(_dialog);
    add_attr_widget(ls);
    add_widget(ls, "");
    return ls;
}

static std::unique_ptr<UI::Widget::PopoverMenu> create_popup_menu(Gtk::Widget &parent,
                                                                  sigc::slot<void ()> dup,
                                                                  sigc::slot<void ()> rem)
{
    auto menu = std::make_unique<UI::Widget::PopoverMenu>(Gtk::PositionType::RIGHT);

    auto mi = Gtk::make_managed<UI::Widget::PopoverMenuItem>(_("_Duplicate"), true);
    mi->signal_activate().connect(std::move(dup));
    menu->append(*mi);

    mi = Gtk::make_managed<UI::Widget::PopoverMenuItem>(_("_Remove"), true);
    mi->signal_activate().connect(std::move(rem));
    menu->append(*mi);

    return menu;
}

/*** FilterModifier ***/
FilterEffectsDialog::FilterModifier::FilterModifier(FilterEffectsDialog& d, Glib::RefPtr<Gtk::Builder> builder)
    :    Gtk::Box(Gtk::Orientation::VERTICAL),
         _builder(std::move(builder)),
         _list(get_widget<Gtk::TreeView>(_builder, "filter-list")),
         _dialog(d),
         _add(get_widget<Gtk::Button>(_builder, "btn-new")),
         _dup(get_widget<Gtk::Button>(_builder, "btn-dup")),
         _del(get_widget<Gtk::Button>(_builder, "btn-del")),
         _select(get_widget<Gtk::Button>(_builder, "btn-select")),
         _menu(create_menu()),
         _observer(std::make_unique<Inkscape::XML::SignalObserver>())
{
    _filters_model = Gtk::ListStore::create(_columns);
    _list.set_model(_filters_model);
    _cell_toggle.set_radio();
    _cell_toggle.set_active(true);
    const int selcol = _list.append_column("", _cell_toggle);
    Gtk::TreeViewColumn* col = _list.get_column(selcol - 1);
    if(col)
       col->add_attribute(_cell_toggle.property_active(), _columns.sel);
    _list.append_column_editable(_("_Filter"), _columns.label);
    dynamic_cast<Gtk::CellRendererText &>(*_list.get_column(1)->get_first_cell()).
        signal_edited().connect(sigc::mem_fun(*this, &FilterEffectsDialog::FilterModifier::on_name_edited));

    _list.append_column(_("Used"), _columns.count);
    _list.get_column(2)->set_sizing(Gtk::TreeViewColumn::Sizing::AUTOSIZE);
    _list.get_column(2)->set_expand(false);
    _list.get_column(2)->set_reorderable(true);

    _list.get_column(1)->set_resizable(true);
    _list.get_column(1)->set_sizing(Gtk::TreeViewColumn::Sizing::FIXED);
    _list.get_column(1)->set_expand(true);

    _list.set_reorderable(false);
    _list.enable_model_drag_dest(Gdk::DragAction::MOVE);

#if 0 // on_filter_move() was commented-out in GTK3, so itʼs removed for GTK4. FIXME if you can...!
    _list.signal_drag_drop().connect( sigc::mem_fun(*this, &FilterModifier::on_filter_move), false );
#endif

    _add.signal_clicked().connect([this]{ add_filter(); });
    _del.signal_clicked().connect([this]{ remove_filter(); });
    _dup.signal_clicked().connect([this]{ duplicate_filter(); });
    _select.signal_clicked().connect([this]{ select_filter_elements(); });

    _cell_toggle.signal_toggled().connect(sigc::mem_fun(*this, &FilterModifier::on_selection_toggled));

    auto const click = Gtk::GestureClick::create();
    click->set_button(3); // right
    click->signal_released().connect(Controller::use_state(sigc::mem_fun(*this, &FilterModifier::filter_list_click_released), *click));
    _list.add_controller(click);

    _list.get_selection()->signal_changed().connect(sigc::mem_fun(*this, &FilterModifier::on_filter_selection_changed));
    _observer->signal_changed().connect(signal_filter_changed().make_slot());
}

// Update each filter's sel property based on the current object selection;
//  If the filter is not used by any selected object, sel = 0,
//  otherwise sel is set to the total number of filters in use by selected objects
//  If only one filter is in use, it is selected
void FilterEffectsDialog::FilterModifier::update_selection(Selection *sel)
{
    if (!sel) {
        return;
    }

    std::set<SPFilter*> used;

    for (auto obj : sel->items()) {
        SPStyle *style = obj->style;
        if (!style || !obj) {
            continue;
        }

        if (style->filter.set && style->getFilter()) {
            //TODO: why is this needed?
            obj->bbox_valid = FALSE;
            used.insert(style->getFilter());
        }
    }

    const int size = used.size();

    for (auto &&item : _filters_model->children()) {
        if (used.count(item[_columns.filter])) {
            // If only one filter is in use by the selection, select it
            if (size == 1) {
                _list.get_selection()->select(item.get_iter());
            }
            item[_columns.sel] = size;
        } else {
            item[_columns.sel] = 0;
        }
    }
    update_counts();
    _signal_filters_updated.emit();
}




std::unique_ptr<UI::Widget::PopoverMenu> FilterEffectsDialog::FilterModifier::create_menu()
{
    auto menu = std::make_unique<UI::Widget::PopoverMenu>(Gtk::PositionType::BOTTOM);
    auto append = [&](Glib::ustring const &text, auto const mem_fun)
    {
        auto &item = *Gtk::make_managed<UI::Widget::PopoverMenuItem>(text, true);
        item.signal_activate().connect(sigc::mem_fun(*this, mem_fun));
        menu->append(item);
    };
    append(_("_Duplicate"            ), &FilterModifier::duplicate_filter      );
    append(_("_Remove"               ), &FilterModifier::remove_filter         );
    append(_("R_ename"               ), &FilterModifier::rename_filter         );
    append(_("Select Filter Elements"), &FilterModifier::select_filter_elements);
    return menu;
}

void FilterEffectsDialog::FilterModifier::on_filter_selection_changed()
{
    _observer->set(get_selected_filter());
    // g_message("calling from selection changed");
    signal_filter_changed()();
}

void FilterEffectsDialog::FilterModifier::on_name_edited(const Glib::ustring& path, const Glib::ustring& text)
{
    if (auto iter = _filters_model->get_iter(path)) {
        SPFilter* filter = (*iter)[_columns.filter];
        filter->setLabel(text.c_str());
        DocumentUndo::done(filter->document, _("Rename filter"), INKSCAPE_ICON("dialog-filters"));
        if (iter) {
            (*iter)[_columns.label] = text;
        }
    }
}

#if 0 // on_filter_move() was commented-out in GTK3, so itʼs removed for GTK4. FIXME if you can...!
bool FilterEffectsDialog::FilterModifier::on_filter_move(const Glib::RefPtr<Gdk::DragContext>& /*context*/, int /*x*/, int /*y*/, guint /*time*/) {

//const Gtk::TreeModel::Path& /*path*/) {
/* The code below is bugged. Use of "object->getRepr()->setPosition(0)" is dangerous!
   Writing back the reordered list to XML (reordering XML nodes) should be implemented differently.
   Note that the dialog does also not update its list of filters when the order is manually changed
   using the XML dialog
  for (auto i = _model->children().begin(); i != _model->children().end(); ++i) {
      SPObject* object = (*i)[_columns.filter];
      if(object && object->getRepr()) ;
        object->getRepr()->setPosition(0);
  }
*/
  return false;
}
#endif

void FilterEffectsDialog::FilterModifier::on_selection_toggled(const Glib::ustring& path)
{
    Gtk::TreeModel::iterator iter = _filters_model->get_iter(path);
    selection_toggled(iter, false);
}

void FilterEffectsDialog::FilterModifier::selection_toggled(Gtk::TreeModel::iterator iter, bool toggle) {
    if (!iter) return;

    SPDesktop *desktop = _dialog.getDesktop();
    SPDocument *doc = desktop->getDocument();
    Inkscape::Selection *sel = desktop->getSelection();
    SPFilter* filter = (*iter)[_columns.filter];

    /* If this filter is the only one used in the selection, unset it */
    if ((*iter)[_columns.sel] == 1 && toggle) {
        filter = nullptr;
    }

    for (auto item : sel->items()) {
        SPStyle *style = item->style;
        g_assert(style != nullptr);

        if (filter && filter->valid_for(item)) {
            sp_style_set_property_url(item, "filter", filter, false);
        } else {
            ::remove_filter(item, false);
        }

        item->requestDisplayUpdate((SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG ));
    }

    update_selection(sel);
    DocumentUndo::done(doc, _("Apply filter"), INKSCAPE_ICON("dialog-filters"));
}

void FilterEffectsDialog::FilterModifier::update_counts()
{
    for (auto&& item : _filters_model->children()) {
        SPFilter* f = item[_columns.filter];
        item[_columns.count] = f->getRefCount();
    }
}

static Glib::ustring get_filter_name(SPFilter* filter) {
    if (!filter) return Glib::ustring();

    if (auto label = filter->label()) {
        return label;
    }
    else if (auto id = filter->getId()) {
        return id;
    }
    else {
        return _("filter");
    }
}

/* Add all filters in the document to the combobox.
   Keeps the same selection if possible, otherwise selects the first element */
void FilterEffectsDialog::FilterModifier::update_filters()
{
    auto document = _dialog.getDocument();
    if (!document) return; // no document at shut down

    std::vector<SPObject *> filters = document->getResourceList("filter");

    _filters_model->clear();
    SPFilter* first = nullptr;

    for (auto filter : filters) {
        Gtk::TreeModel::Row row = *_filters_model->append();
        auto f = cast<SPFilter>(filter);
        row[_columns.filter] = f;
        row[_columns.label] = get_filter_name(f);
        if (!first) {
            first = f;
        }
    }

    update_selection(_dialog.getSelection());
    if (first) {
        select_filter(first);
    }
    _dialog.update_filter_general_settings_view();
    _dialog.update_settings_view();
}

bool FilterEffectsDialog::FilterModifier::is_selected_filter_active() {
    if (auto&& sel = _list.get_selection()) {
        if (Gtk::TreeModel::iterator it = sel->get_selected()) {
            return (*it)[_columns.sel] > 0;
        }
    }

    return false;
}

bool FilterEffectsDialog::FilterModifier::filters_present() const {
    return !_filters_model->children().empty();
}

void FilterEffectsDialog::FilterModifier::toggle_current_filter() {
    if (auto&& sel = _list.get_selection()) {
        selection_toggled(sel->get_selected(), true);
    }
}

SPFilter* FilterEffectsDialog::FilterModifier::get_selected_filter()
{
    if(_list.get_selection()) {
        Gtk::TreeModel::iterator i = _list.get_selection()->get_selected();
        if(i)
            return (*i)[_columns.filter];
    }

    return nullptr;
}

void FilterEffectsDialog::FilterModifier::select_filter(const SPFilter* filter)
{
    if (!filter) return;

    for (auto &&item : _filters_model->children()) {
        if (item[_columns.filter] == filter) {
            _list.get_selection()->select(item.get_iter());
            break;
        }
    }
}

Gtk::EventSequenceState
FilterEffectsDialog::FilterModifier::filter_list_click_released(Gtk::GestureClick const & /*click*/,
                                                                int /*n_press*/,
                                                                double const x, double const y)
{
    const bool sensitive = get_selected_filter() != nullptr;
    auto const &items = _menu->get_items();
    items.at(0)->set_sensitive(sensitive);
    items.at(1)->set_sensitive(sensitive);
    items.at(3)->set_sensitive(sensitive);
    _dialog._popoverbin.setPopover(_menu.get());
    _menu->popup_at(_list, x, y);
    return Gtk::EventSequenceState::CLAIMED;
}

void FilterEffectsDialog::FilterModifier::add_filter()
{
    SPDocument* doc = _dialog.getDocument();
    _observer->set(nullptr);
    SPFilter* filter = new_filter(doc);
    _dialog._filter_canvas.filter_list.push_back(filter);

    const int count = _filters_model->children().size();
    std::ostringstream os;
    os << _("filter") << count;
    filter->setLabel(os.str().c_str());
    // g_message("updating filters");
    update_filters();
    // g_message("selected filter");
    select_filter(filter);
    // g_message("called from here");
    // _dialog._filter_canvas.update_canvas();
    // _dialog._filter_canvas.update_filter(filter);
    _observer->set(filter);

    DocumentUndo::done(doc, _("Add filter"), INKSCAPE_ICON("dialog-filters"));
}

void FilterEffectsDialog::FilterModifier::remove_filter()
{
    SPFilter *filter = get_selected_filter();

    if(filter) {
        auto desktop = _dialog.getDesktop();
        SPDocument* doc = filter->document;

        // Delete all references to this filter
        auto all = get_all_items(desktop->layerManager().currentRoot(), desktop, false, false, true);
        for (auto item : all) {
            if (!item) {
                continue;
            }
            if (!item->style) {
                continue;
            }

            const SPIFilter *ifilter = &(item->style->filter);
            if (ifilter && ifilter->href) {
                const SPObject *obj = ifilter->href->getObject();
                if (obj && obj == (SPObject *)filter) {
                    ::remove_filter(item, false);
                }
            }
        }
        _dialog._filter_canvas.remove_filter(filter);

        //XML Tree being used directly here while it shouldn't be.
        sp_repr_unparent(filter->getRepr());

        DocumentUndo::done(doc, _("Remove filter"), INKSCAPE_ICON("dialog-filters"));

        update_filters();
    
        // select first filter to avoid empty dialog after filter deletion
        auto &&filters = _filters_model->children();
        if (!filters.empty()) {
            _list.get_selection()->select(filters[0].get_iter());
        }
    }
}

void FilterEffectsDialog::FilterModifier::duplicate_filter()
{
    SPFilter* filter = get_selected_filter();

    if (filter) {
        Inkscape::XML::Node *repr = filter->getRepr();
        Inkscape::XML::Node *parent = repr->parent();
        repr = repr->duplicate(repr->document());
        parent->appendChild(repr);

        DocumentUndo::done(filter->document, _("Duplicate filter"), INKSCAPE_ICON("dialog-filters"));

        update_filters();
    }
}

void FilterEffectsDialog::FilterModifier::rename_filter()
{
    _list.set_cursor(_filters_model->get_path(_list.get_selection()->get_selected()), *_list.get_column(1), true);
}

void FilterEffectsDialog::FilterModifier::select_filter_elements()
{
    SPFilter *filter = get_selected_filter();
    auto desktop = _dialog.getDesktop();

    if(!filter)
        return;

    std::vector<SPItem*> items;
    auto all = get_all_items(desktop->layerManager().currentRoot(), desktop, false, false, true);
    for(SPItem *item: all) {
        if (!item->style) {
            continue;
        }

        SPIFilter const &ifilter = item->style->filter;
        if (ifilter.href) {
            const SPObject *obj = ifilter.href->getObject();
            if (obj && obj == (SPObject *)filter) {
                items.push_back(item);
            }
        }
    }
    desktop->getSelection()->setList(items);
}

FilterEffectsDialog::CellRendererConnection::CellRendererConnection()
    : Glib::ObjectBase(typeid(CellRendererConnection))
    , _primitive(*this, "primitive", nullptr)
{}

Glib::PropertyProxy<void*> FilterEffectsDialog::CellRendererConnection::property_primitive()
{
    return _primitive.get_proxy();
}

void FilterEffectsDialog::CellRendererConnection::get_preferred_width_vfunc(Gtk::Widget& widget,
                                                                            int& minimum_width,
                                                                            int& natural_width) const
{
    auto& primlist = dynamic_cast<PrimitiveList&>(widget);
    int count = primlist.get_inputs_count();
    minimum_width = natural_width = size_w * primlist.primitive_count() + primlist.get_input_type_width() * count;
}

void FilterEffectsDialog::CellRendererConnection::get_preferred_width_for_height_vfunc(Gtk::Widget& widget,
                                                                                       int /* height */,
                                                                                       int& minimum_width,
                                                                                       int& natural_width) const
{
    get_preferred_width(widget, minimum_width, natural_width);
}

void FilterEffectsDialog::CellRendererConnection::get_preferred_height_vfunc(Gtk::Widget& widget,
                                                                             int& minimum_height,
                                                                             int& natural_height) const
{
    // Scale the height depending on the number of inputs, unless it's
    // the first primitive, in which case there are no connections.
    auto prim = reinterpret_cast<SPFilterPrimitive*>(_primitive.get_value());
    minimum_height = natural_height = size_h * input_count(prim);
}

void FilterEffectsDialog::CellRendererConnection::get_preferred_height_for_width_vfunc(Gtk::Widget& widget,
                                                                                       int /* width */,
                                                                                       int& minimum_height,
                                                                                       int& natural_height) const
{
    get_preferred_height(widget, minimum_height, natural_height);
}

/*** PrimitiveList ***/
FilterEffectsDialog::PrimitiveList::PrimitiveList(FilterEffectsDialog& d)
    : Glib::ObjectBase{"FilterEffectsDialogPrimitiveList"}
    , WidgetVfuncsClassInit{}
    , Gtk::TreeView{}
    , _dialog(d)
    , _in_drag(0)
    // , _observer(std::make_unique<Inkscape::XML::SignalObserver>())
{
    _inputs_count = FPInputConverter._length;

    auto const click = Gtk::GestureClick::create();
    click->set_button(0); // any
    click->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    click->signal_pressed().connect(Controller::use_state(sigc::mem_fun(*this, &PrimitiveList::on_click_pressed), *click));
    click->signal_released().connect(Controller::use_state(sigc::mem_fun(*this, &PrimitiveList::on_click_released), *click));
    add_controller(click);

    auto const motion = Gtk::EventControllerMotion::create();
    motion->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    motion->signal_motion().connect(sigc::mem_fun(*this, &PrimitiveList::on_motion_motion));
    add_controller(motion);

    _model = Gtk::ListStore::create(_columns);

    set_reorderable(true);

    auto const drag = Gtk::DragSource::create();
    drag->signal_drag_end().connect(sigc::mem_fun(*this, &PrimitiveList::on_drag_end));
    add_controller(drag);

    set_model(_model);
    append_column(_("_Effect"), _columns.type);
    get_column(0)->set_resizable(true);
    set_headers_visible(false);

    // _observer->signal_changed().connect(signal_primitive_changed().make_slot());
    get_selection()->signal_changed().connect(sigc::mem_fun(*this, &PrimitiveList::on_primitive_selection_changed));
    signal_primitive_changed().connect(sigc::mem_fun(*this, &PrimitiveList::queue_draw));

    init_text();

    int cols_count = append_column(_("Connections"), _connection_cell);
    Gtk::TreeViewColumn* col = get_column(cols_count - 1);
    if(col)
        col->add_attribute(_connection_cell.property_primitive(), _columns.primitive);
}

void FilterEffectsDialog::PrimitiveList::css_changed(GtkCssStyleChange *)
{
    bg_color = get_color_with_class(*this, "theme_bg_color");
}

// Sets up a vertical Pango context/layout, and returns the largest
// width needed to render the FilterPrimitiveInput labels.
void FilterEffectsDialog::PrimitiveList::init_text()
{
    // Set up a vertical context+layout
    Glib::RefPtr<Pango::Context> context = create_pango_context();
    const Pango::Matrix matrix = {0, -1, 1, 0, 0, 0};
    context->set_matrix(matrix);
    _vertical_layout = Pango::Layout::create(context);

    // Store the maximum height and width of the an input type label
    // for later use in drawing and measuring.
    _input_type_height = _input_type_width = 0;
    for(unsigned int i = 0; i < FPInputConverter._length; ++i) {
        _vertical_layout->set_text(_(FPInputConverter.get_label((FilterPrimitiveInput)i).c_str()));
        int fontw, fonth;
        _vertical_layout->get_pixel_size(fontw, fonth);
        if(fonth > _input_type_width)
            _input_type_width = fonth;
        if (fontw > _input_type_height)
            _input_type_height = fontw;
    }
}

sigc::signal<void ()>& FilterEffectsDialog::PrimitiveList::signal_primitive_changed()
{
    return _signal_primitive_changed;
}

void FilterEffectsDialog::PrimitiveList::on_primitive_selection_changed()
{
    // _observer->set(get_selected());
    signal_primitive_changed()();
    _dialog._color_matrix_values->clear_store();
}

/* Add all filter primitives in the current to the list.
   Keeps the same selection if possible, otherwise selects the first element */
void FilterEffectsDialog::PrimitiveList::update()
{
    SPFilter* f = _dialog._filter_modifier.get_selected_filter();
    const SPFilterPrimitive* active_prim = get_selected();
    _model->clear();

    if(f) {
        bool active_found = false;
        _dialog._primitive_box->set_sensitive(true);
        _dialog.update_filter_general_settings_view();
        for(auto& prim_obj: f->children) {
            auto prim = cast<SPFilterPrimitive>(&prim_obj);
            if(!prim) {
                break;
            }
            Gtk::TreeModel::Row row = *_model->append();
            row[_columns.primitive] = prim;

            //XML Tree being used directly here while it shouldn't be.
            row[_columns.type_id] = FPConverter.get_id_from_key(prim->getRepr()->name());
            row[_columns.type] = _(FPConverter.get_label(row[_columns.type_id]).c_str());

            if (prim->getId()) {
                row[_columns.id] =  Glib::ustring(prim->getId());
            }

            if(prim == active_prim) {
                get_selection()->select(row.get_iter());
                active_found = true;
            }
        }

        if(!active_found && _model->children().begin())
            get_selection()->select(_model->children().begin());

        columns_autosize();

        int width, height;
        get_size_request(width, height);
        if (height == -1) {
               // Need to account for the height of the input type text (rotated text) as well as the
               // column headers.  Input type text height determined in init_text() by measuring longest
               // string. Column header height determined by mapping y coordinate of visible
               // rectangle to widget coordinates.
               Gdk::Rectangle vis;
               int vis_x, vis_y;
               get_visible_rect(vis);
               convert_tree_to_widget_coords(vis.get_x(), vis.get_y(), vis_x, vis_y);
               set_size_request(width, _input_type_height + 2 + vis_y);
        }
    } else {
        _dialog._primitive_box->set_sensitive(false);
        set_size_request(-1, -1);
    }
}

void FilterEffectsDialog::PrimitiveList::set_menu(sigc::slot<void ()> dup, sigc::slot<void ()> rem)
{
    _primitive_menu = create_popup_menu(_dialog, std::move(dup), std::move(rem));
}

SPFilterPrimitive* FilterEffectsDialog::PrimitiveList::get_selected()
{
    if(_dialog._filter_modifier.get_selected_filter()) {
        Gtk::TreeModel::iterator i = get_selection()->get_selected();
        if(i)
            return (*i)[_columns.primitive];
    }

    return nullptr;
}

void FilterEffectsDialog::PrimitiveList::select(SPFilterPrimitive* prim)
{
    for (auto &&item : _model->children()) {
        if (item[_columns.primitive] == prim) {
            get_selection()->select(item.get_iter());
            break;
        }
    }
}

void FilterEffectsDialog::PrimitiveList::remove_selected()
{
    if (SPFilterPrimitive* prim = get_selected()) {
        // _observer->set(nullptr);
        _model->erase(get_selection()->get_selected());

        //XML Tree being used directly here while it shouldn't be.
        sp_repr_unparent(prim->getRepr());

        DocumentUndo::done(_dialog.getDocument(), _("Remove filter primitive"), INKSCAPE_ICON("dialog-filters"));

        update();
    }
}

void draw_connection_node(const Cairo::RefPtr<Cairo::Context>& cr,
                          std::vector<Geom::Point> const &points,
                          Gdk::RGBA const &fill, Gdk::RGBA const &stroke);

void FilterEffectsDialog::PrimitiveList::snapshot_vfunc(Glib::RefPtr<Gtk::Snapshot> const &snapshot)
{
    parent_type::snapshot_vfunc(snapshot);

    auto const cr = snapshot->append_cairo(get_allocation());

    cr->set_line_width(1.0);
    // In GTK+ 3, the draw function receives the widget window, not the
    // bin_window (i.e., just the area under the column headers).  We
    // therefore translate the origin of our coordinate system to account for this
    int x_origin, y_origin;
    convert_bin_window_to_widget_coords(0,0,x_origin,y_origin);
    cr->translate(x_origin, y_origin);

    auto const fg_color = get_color();
    auto bar_color = mix_colors(bg_color, fg_color, 0.06);
     // color of connector arrow heads and effect separator lines
    auto mid_color = mix_colors(bg_color, fg_color, 0.16);

    SPFilterPrimitive* prim = get_selected();
    int row_count = get_model()->children().size();

    static constexpr int fwidth = CellRendererConnection::size_w;
    Gdk::Rectangle rct, vis;
    Gtk::TreeModel::iterator row = get_model()->children().begin();
    int text_start_x = 0;
    if(row) {
        get_cell_area(get_model()->get_path(row), *get_column(1), rct);
        get_visible_rect(vis);
        text_start_x = rct.get_x() + rct.get_width() - get_input_type_width() * _inputs_count + 1;

        auto w = get_input_type_width();
        auto h = vis.get_height();
        cr->save();
        // erase selection color from selected item
        Gdk::Cairo::set_source_rgba(cr, bg_color);
        cr->rectangle(text_start_x + 1, 0, w * _inputs_count, h);
        cr->fill();
        auto const text_color = change_alpha(fg_color, 0.7);

        // draw vertical bars corresponding to possible filter inputs
        for(unsigned int i = 0; i < _inputs_count; ++i) {
            _vertical_layout->set_text(_(FPInputConverter.get_label((FilterPrimitiveInput)i).c_str()));
            const int x = text_start_x + w * i;
            cr->save();

            Gdk::Cairo::set_source_rgba(cr, bar_color);
            cr->rectangle(x + 1, 0, w - 2, h);
            cr->fill();

            Gdk::Cairo::set_source_rgba(cr, text_color);
            cr->move_to(x + w, 5);
            cr->rotate_degrees(90);
            _vertical_layout->show_in_cairo_context(cr);

            cr->restore();
        }

        cr->restore();
        cr->rectangle(vis.get_x(), 0, vis.get_width(), vis.get_height());
        cr->clip();
    }

    int row_index = 0;
    for(; row != get_model()->children().end(); ++row, ++row_index) {
        get_cell_area(get_model()->get_path(row), *get_column(1), rct);
        const int x = rct.get_x(), y = rct.get_y(), h = rct.get_height();

        // Check mouse state
        double mx{}, my{};
        Gdk::ModifierType mask;
        auto const display = get_display();
        auto seat = display->get_default_seat();
        auto device = seat->get_pointer();
        auto const surface = dynamic_cast<Gtk::Native &>(*get_root()).get_surface();
        g_assert(surface);
        surface->get_device_position(device, mx, my, mask);

        cr->set_line_width(1);

        // Outline the bottom of the connection area
        const int outline_x = x + fwidth * (row_count - row_index);
        cr->save();

        Gdk::Cairo::set_source_rgba(cr, mid_color);

        cr->move_to(vis.get_x(), y + h);
        cr->line_to(outline_x, y + h);
        // Side outline
        cr->line_to(outline_x, y - 1);

        cr->stroke();
        cr->restore();

        std::vector<Geom::Point> con_poly;
        int con_drag_y = 0;
        int con_drag_x = 0;
        bool inside;
        const SPFilterPrimitive* row_prim = (*row)[_columns.primitive];
        const int inputs = input_count(row_prim);

        if(is<SPFeMerge>(row_prim)) {
            for(int i = 0; i < inputs; ++i) {
                inside = do_connection_node(row, i, con_poly, mx, my);

                draw_connection_node(cr, con_poly, inside ? fg_color : mid_color, fg_color);

                if(_in_drag == (i + 1)) {
                    con_drag_y = con_poly[2].y();
                    con_drag_x = con_poly[2].x();
                }

                if(_in_drag != (i + 1) || row_prim != prim) {
                    // draw_connection(cr, row, SPAttr::INVALID, text_start_x, outline_x,
                                    // con_poly[2].y(), row_count, i, fg_color, mid_color);
                }
            }
        }
        else {
            // Draw "in" shape
            inside = do_connection_node(row, 0, con_poly, mx, my);
            con_drag_y = con_poly[2].y();
            con_drag_x = con_poly[2].x();

            draw_connection_node(cr, con_poly, inside ? fg_color : mid_color, fg_color);

            // Draw "in" connection
            if(_in_drag != 1 || row_prim != prim) {
                draw_connection(cr, row, SPAttr::IN_, text_start_x, outline_x,
                                con_poly[2].y(), row_count, -1, fg_color, mid_color);
            }

            if(inputs == 2) {
                // Draw "in2" shape
                inside = do_connection_node(row, 1, con_poly, mx, my);
                if(_in_drag == 2) {
                    con_drag_y = con_poly[2].y();
                    con_drag_x = con_poly[2].x();
                }

                draw_connection_node(cr, con_poly, inside ? fg_color : mid_color, fg_color);

                // Draw "in2" connection
                if(_in_drag != 2 || row_prim != prim) {
                    draw_connection(cr, row, SPAttr::IN2, text_start_x, outline_x,
                                    con_poly[2].y(), row_count, -1, fg_color, mid_color);
                }
            }
        }

        // Draw drag connection
        if(row_prim == prim && _in_drag) {
            cr->save();
            Gdk::Cairo::set_source_rgba(cr, fg_color);
            cr->move_to(con_drag_x, con_drag_y);
            cr->line_to(mx, con_drag_y);
            cr->line_to(mx, my);
            cr->stroke();
            cr->restore();
          }
    }
}

void FilterEffectsDialog::PrimitiveList::draw_connection(const Cairo::RefPtr<Cairo::Context>& cr,
                                                         const Gtk::TreeModel::iterator& input, const SPAttr attr,
                                                         const int text_start_x, const int x1, const int y1,
                                                         const int row_count, const int pos,
                                                         const Gdk::RGBA fg_color, const Gdk::RGBA mid_color)
{
    cr->save();

    int src_id = 0;
    Gtk::TreeModel::iterator res = find_result(input, attr, src_id, pos); 

    const bool is_first = input == get_model()->children().begin();
    const bool is_selected = input == get_selection()->get_selected();
    const bool is_merge = is<SPFeMerge>((SPFilterPrimitive*)(*input)[_columns.primitive]);
    const bool use_default = !res && !is_merge;
    int arc_radius = 4;

    if (is_selected) {
        cr->set_line_width(2.5);
        arc_radius = 6;
    }

    if(res == input || (use_default && is_first)) {
        // Draw straight connection to a standard input
        // Draw a lighter line for an implicit connection to a standard input
        const int tw = get_input_type_width();
        gint end_x = text_start_x + tw * src_id + 1;

        if(use_default && is_first) {
            Gdk::Cairo::set_source_rgba(cr, fg_color);
            cr->set_dash(std::vector<double> {1.0, 1.0}, 0);
        } else {
            Gdk::Cairo::set_source_rgba(cr, fg_color);
        }

        // draw a half-circle touching destination band
        cr->move_to(x1, y1);
        cr->line_to(end_x, y1);
        cr->stroke();
        cr->arc(end_x, y1, arc_radius, M_PI / 2, M_PI * 1.5);
        cr->fill();
    }
    else {
        // Draw an 'L'-shaped connection to another filter primitive
        // If no connection is specified, draw a light connection to the previous primitive
        if(use_default) {
                res = input;
                --res;
        }

        if(res) {
            Gdk::Rectangle rct;

            get_cell_area(get_model()->get_path(_model->children().begin()), *get_column(1), rct);
            static constexpr int fheight = CellRendererConnection::size_h;
            static constexpr int fwidth  = CellRendererConnection::size_w;

            get_cell_area(get_model()->get_path(res), *get_column(1), rct);
            const int row_index = find_index(res);
            const int x2 = rct.get_x() + fwidth * (row_count - row_index) - fwidth / 2;
            const int y2 = rct.get_y() + rct.get_height();

            // Draw a bevelled 'L'-shaped connection
            Gdk::Cairo::set_source_rgba(cr, fg_color);
            cr->move_to(x1, y1);
            cr->line_to(x2 - fwidth/4, y1);
            cr->line_to(x2, y1 - fheight/4);
            cr->line_to(x2, y2);
            cr->stroke();
        }
    }
    cr->restore();
}

// Draw the triangular outline of the connection node, and fill it
// if desired
void draw_connection_node(const Cairo::RefPtr<Cairo::Context>& cr,
                          std::vector<Geom::Point> const &points,
                          Gdk::RGBA const &fill, Gdk::RGBA const &stroke)
{
    cr->save();
    cr->move_to(points[0].x() + 0.5, points[0].y() + 0.5);
    cr->line_to(points[1].x() + 0.5, points[1].y() + 0.5);
    cr->line_to(points[2].x() + 0.5, points[2].y() + 0.5);
    cr->line_to(points[0].x() + 0.5, points[0].y() + 0.5);
    cr->close_path();

    Gdk::Cairo::set_source_rgba(cr, fill);
    cr->fill_preserve();
    cr->set_line_width(1);
    Gdk::Cairo::set_source_rgba(cr, stroke);
    cr->stroke();

    cr->restore();
}

// Creates a triangle outline of the connection node and returns true if (x,y) is inside the node
bool FilterEffectsDialog::PrimitiveList::do_connection_node(const Gtk::TreeModel::iterator& row, const int input,
                                                            std::vector<Geom::Point> &points,
                                                            const int ix, const int iy)
{
    Gdk::Rectangle rct;
    const int icnt = input_count((*row)[_columns.primitive]);

    get_cell_area(get_model()->get_path(_model->children().begin()), *get_column(1), rct);
    static constexpr int fheight = CellRendererConnection::size_h;
    static constexpr int fwidth  = CellRendererConnection::size_w;

    get_cell_area(_model->get_path(row), *get_column(1), rct);
    const float h = rct.get_height() / icnt;

    const int x = rct.get_x() + fwidth * (_model->children().size() - find_index(row));
    // this is how big arrowhead appears:
    const int con_w = (int)(fwidth * 0.70f);
    const int con_h = (int)(fheight * 0.35f);
    const int con_y = (int)(rct.get_y() + (h / 2) - con_h + (input * h));
    points.clear();
    points.emplace_back(x, con_y);
    points.emplace_back(x, con_y + con_h * 2);
    points.emplace_back(x - con_w, con_y + con_h);

    return ix >= x - h && iy >= con_y && ix <= x && iy <= points[1].y();
}

const Gtk::TreeModel::iterator FilterEffectsDialog::PrimitiveList::find_result(const Gtk::TreeModel::iterator& start,
                                                                    const SPAttr attr, int& src_id,
                                                                    const int pos)
{
    SPFilterPrimitive* prim = (*start)[_columns.primitive];
    Gtk::TreeModel::iterator target = _model->children().end();
    int image = 0;

    if(is<SPFeMerge>(prim)) {
        int c = 0;
        bool found = false;
        for (auto& o: prim->children) {
            if(c == pos && is<SPFeMergeNode>(&o)) {
                image = cast<SPFeMergeNode>(&o)->get_in();
                found = true;
            }
            ++c;
        }
        if(!found)
            return target;
    }
    else {
        if(attr == SPAttr::IN_)
            image = prim->get_in();
        else if(attr == SPAttr::IN2) {
            if(is<SPFeBlend>(prim))
                image = cast<SPFeBlend>(prim)->get_in2();
            else if(is<SPFeComposite>(prim))
                image = cast<SPFeComposite>(prim)->get_in2();
            else if(is<SPFeDisplacementMap>(prim))
                image = cast<SPFeDisplacementMap>(prim)->get_in2();
            else
                return target;
        }
        else
            return target;
    }

    if(image >= 0) {
        for(Gtk::TreeModel::iterator i = _model->children().begin();
            i != start; ++i) {
            if(((SPFilterPrimitive*)(*i)[_columns.primitive])->get_out() == image)
                target = i;
        }
        return target;
    }
    else if(image < -1) {
        src_id = -(image + 2);
        return start;
    }

    return target;
}

int FilterEffectsDialog::PrimitiveList::find_index(const Gtk::TreeModel::iterator& target)
{
    int i = 0;
    for (auto iter = _model->children().begin(); iter != target; ++iter, ++i) {}
    return i;
}

static std::pair<int, int> widget_to_bin_window(Gtk::TreeView const &tree_view, int const wx, int const wy)
{
    int bx, by;
    tree_view.convert_widget_to_bin_window_coords(wx, wy, bx, by);
    return {bx, by};
}

Gtk::EventSequenceState
FilterEffectsDialog::PrimitiveList::on_click_pressed(Gtk::GestureClick const & /*click*/,
                                                     int /*n_press*/,
                                                     double const wx, double const wy)
{
    Gtk::TreePath path;
    Gtk::TreeViewColumn* col;
    auto const [x, y] = widget_to_bin_window(*this, wx, wy);
    int cx, cy;

    _drag_prim = nullptr;

    if(get_path_at_pos(x, y, path, col, cx, cy)) {
        Gtk::TreeModel::iterator iter = _model->get_iter(path);
        std::vector<Geom::Point> points;

        _drag_prim = (*iter)[_columns.primitive];
        const int icnt = input_count(_drag_prim);

        for(int i = 0; i < icnt; ++i) {
            if(do_connection_node(_model->get_iter(path), i, points, x, y)) {
                _in_drag = i + 1;
                break;
            }
        }

        queue_draw();
    }

    if(_in_drag) {
        _scroll_connection = Glib::signal_timeout().connect(sigc::mem_fun(*this, &PrimitiveList::on_scroll_timeout), 150);
        _autoscroll_x = 0;
        _autoscroll_y = 0;
        get_selection()->select(path);
        return Gtk::EventSequenceState::CLAIMED;
    }

    return Gtk::EventSequenceState::NONE;
}

void FilterEffectsDialog::PrimitiveList::on_motion_motion(double wx, double wy)
{
    const int speed = 10;
    const int limit = 15;

    auto const [x, y] = widget_to_bin_window(*this, wx, wy);

    Gdk::Rectangle vis;
    get_visible_rect(vis);
    int vis_x, vis_y;
    int vis_x2, vis_y2;
    convert_widget_to_tree_coords(vis.get_x(), vis.get_y(), vis_x2, vis_y2);
    convert_tree_to_widget_coords(vis.get_x(), vis.get_y(), vis_x, vis_y);
    const int top = vis_y + vis.get_height();
    const int right_edge = vis_x + vis.get_width();

    // When autoscrolling during a connection drag, set the speed based on
    // where the mouse is in relation to the edges.
    if (y < vis_y)
        _autoscroll_y = -(int)(speed + (vis_y - y) / 5);
    else if (y < vis_y + limit)
        _autoscroll_y = -speed;
    else if (y > top)
        _autoscroll_y = (int)(speed + (y - top) / 5);
    else if (y > top - limit)
        _autoscroll_y = speed;
    else
        _autoscroll_y = 0;

    double const e2 = x - vis_x2 / 2;
    // horizontal scrolling
    if(e2 < vis_x)
        _autoscroll_x = -(int)(speed + (vis_x - e2) / 5);
    else if(e2 < vis_x + limit)
        _autoscroll_x = -speed;
    else if(e2 > right_edge)
        _autoscroll_x = (int)(speed + (e2 - right_edge) / 5);
    else if(e2 > right_edge - limit)
        _autoscroll_x = speed;
    else
        _autoscroll_x = 0;

    queue_draw();
}

Gtk::EventSequenceState
FilterEffectsDialog::PrimitiveList::on_click_released(Gtk::GestureClick const &click,
                                                      int /*n_press*/,
                                                      double const wx, double const wy)
{
    _scroll_connection.disconnect();

    auto const prim = get_selected();
    if(_in_drag && prim) {
        auto const [x, y] = widget_to_bin_window(*this, wx, wy);
        Gtk::TreePath path;
        Gtk::TreeViewColumn* col;
        int cx, cy;
        if (get_path_at_pos(x, y, path, col, cx, cy)) {
            auto const selected_iter = get_selection()->get_selected();
            g_assert(selected_iter);

            auto const target_iter = _model->get_iter(path);
            g_assert(target_iter);

            auto const target = target_iter->get_value(_columns.primitive);
            g_assert(target);

            col = get_column(1);
            g_assert(col);

            char const *in_val = nullptr;
            Glib::ustring result;

            Gdk::Rectangle rct;
            get_cell_area(path, *col, rct);
            const int twidth = get_input_type_width();
            const int sources_x = rct.get_width() - twidth * _inputs_count;
            if(cx > sources_x) {
                int src = (cx - sources_x) / twidth;
                if (src < 0) {
                    src = 0;
                } else if(src >= static_cast<int>(_inputs_count)) {
                    src = _inputs_count - 1;
                }
                result = FPInputConverter.get_key((FilterPrimitiveInput)src);
                in_val = result.c_str();
            } else {
                // Ensure that the target comes before the selected primitive
                for (auto iter = _model->children().begin(); iter != selected_iter; ++iter) {
                    if(iter == target_iter) {
                        Inkscape::XML::Node *repr = target->getRepr();
                        // Make sure the target has a result
                        const gchar *gres = repr->attribute("result");
                        if(!gres) {
                            result = cast<SPFilter>(prim->parent)->get_new_result_name();
                            repr->setAttributeOrRemoveIfEmpty("result", result);
                            in_val = result.c_str();
                        }
                        else
                            in_val = gres;
                        break;
                    }
                }
            }

            if(is<SPFeMerge>(prim)) {
                int c = 1;
                bool handled = false;
                for (auto& o: prim->children) {
                    if(c == _in_drag && is<SPFeMergeNode>(&o)) {
                        // If input is null, delete it
                        if(!in_val) {
                            //XML Tree being used directly here while it shouldn't be.
                            sp_repr_unparent(o.getRepr());
                            DocumentUndo::done(prim->document, _("Remove merge node"), INKSCAPE_ICON("dialog-filters"));
                            selected_iter->set_value(_columns.primitive, prim);
                        } else {
                            _dialog.set_attr(&o, SPAttr::IN_, in_val);
                        }
                        handled = true;
                        break;
                    }
                    ++c;
                }

                // Add new input?
                if(!handled && c == _in_drag && in_val) {
                    Inkscape::XML::Document *xml_doc = prim->document->getReprDoc();
                    Inkscape::XML::Node *repr = xml_doc->createElement("svg:feMergeNode");
                    repr->setAttribute("inkscape:collect", "always");

                    //XML Tree being used directly here while it shouldn't be.
                    prim->getRepr()->appendChild(repr);
                    auto node = cast<SPFeMergeNode>(prim->document->getObjectByRepr(repr));
                    Inkscape::GC::release(repr);
                    _dialog.set_attr(node, SPAttr::IN_, in_val);
                    selected_iter->set_value(_columns.primitive, prim);
                }
            }
            else {
                auto node_below = _dialog._filter_canvas.get_node_from_primitive(prim);
                auto node_above = _dialog._filter_canvas.get_node_from_primitive(target);
                _dialog._filter_canvas.create_connection(node_above, node_below);
                if(_in_drag == 1)
                    _dialog.set_attr(prim, SPAttr::IN_, in_val);
                else if(_in_drag == 2)
                    _dialog.set_attr(prim, SPAttr::IN2, in_val);
            }
        }

        _in_drag = 0;
        queue_draw();

        _dialog.update_settings_view();
    }

    if (click.get_current_button() == 3) {
        bool const sensitive = prim != nullptr;
        _primitive_menu->set_sensitive(sensitive);
        _dialog._popoverbin.setPopover(_primitive_menu.get());
        _primitive_menu->popup_at(*this, wx + 4, wy);
        return Gtk::EventSequenceState::CLAIMED;
    }

    return Gtk::EventSequenceState::NONE;
}

// Checks all of prim's inputs, removes any that use result
static void check_single_connection(SPFilterPrimitive* prim, const int result)
{
    if (prim && (result >= 0)) {
        if (prim->get_in() == result) {
            prim->removeAttribute("in");
        }

        if (auto blend = cast<SPFeBlend>(prim)) {
            if (blend->get_in2() == result) {
                prim->removeAttribute("in2");
            }
        } else if (auto comp = cast<SPFeComposite>(prim)) {
            if (comp->get_in2() == result) {
                prim->removeAttribute("in2");
            }
        } else if (auto disp = cast<SPFeDisplacementMap>(prim)) {
            if (disp->get_in2() == result) {
                prim->removeAttribute("in2");
            }
        }
    }
}

// Remove any connections going to/from prim_iter that forward-reference other primitives
void FilterEffectsDialog::PrimitiveList::sanitize_connections(const Gtk::TreeModel::iterator& prim_iter)
{
    SPFilterPrimitive *prim = (*prim_iter)[_columns.primitive];
    bool before = true;

    for (auto iter = _model->children().begin(); iter != _model->children().end(); ++iter) {
        if(iter == prim_iter)
            before = false;
        else {
            SPFilterPrimitive* cur_prim = (*iter)[_columns.primitive];
            if(before)
                check_single_connection(cur_prim, prim->get_out());
            else
                check_single_connection(prim, cur_prim->get_out());
        }
    }
}

// Reorder the filter primitives to match the list order
void FilterEffectsDialog::PrimitiveList::on_drag_end(Glib::RefPtr<Gdk::Drag> const &/*&drag*/,
                                                     bool /*delete_data*/)
{
    SPFilter* filter = _dialog._filter_modifier.get_selected_filter();
    g_assert(filter);

    int ndx = 0;
    for (auto iter = _model->children().begin(); iter != _model->children().end(); ++iter, ++ndx) {
        SPFilterPrimitive* prim = (*iter)[_columns.primitive];
        if (prim && prim == _drag_prim) {
            prim->getRepr()->setPosition(ndx);
            break;
        }
    }

    for (auto iter = _model->children().begin(); iter != _model->children().end(); ++iter) {
        SPFilterPrimitive* prim = (*iter)[_columns.primitive];
        if (prim && prim == _drag_prim) {
            sanitize_connections(iter);
            get_selection()->select(iter);
            break;
        }
    }

    filter->requestModified(SP_OBJECT_MODIFIED_FLAG);
    DocumentUndo::done(filter->document, _("Reorder filter primitive"), INKSCAPE_ICON("dialog-filters"));
}

static void autoscroll(Glib::RefPtr<Gtk::Adjustment> const &a, double const delta)
{
    auto v = a->get_value() + delta;
    v = std::clamp(v, 0.0, a->get_upper() - a->get_page_size());
    a->set_value(v);
}

// If a connection is dragged towards the top or bottom of the list, the list should scroll to follow.
bool FilterEffectsDialog::PrimitiveList::on_scroll_timeout()
{
    if (!(_autoscroll_y || _autoscroll_x)) return true;

    auto &scrolled_window = dynamic_cast<Gtk::ScrolledWindow &>(*get_parent());

    if(_autoscroll_y) {
        autoscroll(scrolled_window.get_vadjustment(), _autoscroll_y);
    }

    if(_autoscroll_x) {
        autoscroll(scrolled_window.get_hadjustment(), _autoscroll_x);
    }

    queue_draw();
    return true;
}

int FilterEffectsDialog::PrimitiveList::primitive_count() const
{
    return _model->children().size();
}

int FilterEffectsDialog::PrimitiveList::get_input_type_width() const
{
    // Maximum font height calculated in initText() and stored in _input_type_width.
    // Add 2 to font height to account for rectangle around text.
    return _input_type_width + 2;
}

int FilterEffectsDialog::PrimitiveList::get_inputs_count() const {
    return _inputs_count;
}

void FilterEffectsDialog::PrimitiveList::set_inputs_count(int count) {
    _inputs_count = count;
    queue_allocate();
    queue_draw();
}

enum class EffectCategory { Effect, Compose, Colors, Generation };

const Glib::ustring& get_category_name(EffectCategory category) {
    static const std::map<EffectCategory, Glib::ustring> category_names = {
        { EffectCategory::Effect,     _("Effect") },
        { EffectCategory::Compose,    _("Compositing") },
        { EffectCategory::Colors,     _("Color editing") },
        { EffectCategory::Generation, _("Generating") },
    };
    return category_names.at(category);
}

struct EffectMetadata {
    EffectCategory category;
    Glib::ustring icon_name;
    Glib::ustring tooltip;
};

static const std::map<Inkscape::Filters::FilterPrimitiveType, EffectMetadata>& get_effects() {
    static std::map<Inkscape::Filters::FilterPrimitiveType, EffectMetadata> effects = {
    { NR_FILTER_GAUSSIANBLUR,      { EffectCategory::Effect,     "feGaussianBlur-icon",
        _("Uniformly blurs its input. Commonly used together with Offset to create a drop shadow effect.") }},
    { NR_FILTER_MORPHOLOGY,        { EffectCategory::Effect,     "feMorphology-icon",
        _("Provides erode and dilate effects. For single-color objects erode makes the object thinner and dilate makes it thicker.") }},
    { NR_FILTER_OFFSET,            { EffectCategory::Effect,     "feOffset-icon",
        _("Offsets the input by an user-defined amount. Commonly used for drop shadow effects.") }},
    { NR_FILTER_CONVOLVEMATRIX,    { EffectCategory::Effect,     "feConvolveMatrix-icon",
        _("Performs a convolution on the input image enabling effects like blur, sharpening, embossing and edge detection.") }},
    { NR_FILTER_DISPLACEMENTMAP,   { EffectCategory::Effect,     "feDisplacementMap-icon",
        _("Displaces pixels from the first input using the second as a map of displacement intensity. Classical examples are whirl and pinch effects.") }},
    { NR_FILTER_TILE,              { EffectCategory::Effect,     "feTile-icon",
        _("Tiles a region with an input graphic. The source tile is defined by the filter primitive subregion of the input.") }},
    { NR_FILTER_COMPOSITE,         { EffectCategory::Compose,    "feComposite-icon",
        _("Composites two images using one of the Porter-Duff blending modes or the arithmetic mode described in SVG standard.") }},
    { NR_FILTER_BLEND,             { EffectCategory::Compose,    "feBlend-icon",
        _("Provides image blending modes, such as screen, multiply, darken and lighten.") }},
    { NR_FILTER_MERGE,             { EffectCategory::Compose,    "feMerge-icon",
        _("Merges multiple inputs using normal alpha compositing. Equivalent to using several Blend primitives in 'normal' mode or several Composite primitives in 'over' mode.") }},
    { NR_FILTER_COLORMATRIX,       { EffectCategory::Colors,     "feColorMatrix-icon",
        _("Modifies pixel colors based on a transformation matrix. Useful for adjusting color hue and saturation.") }},
    { NR_FILTER_COMPONENTTRANSFER, { EffectCategory::Colors,     "feComponentTransfer-icon",
        _("Manipulates color components according to particular transfer functions. Useful for brightness and contrast adjustment, color balance, and thresholding.") }},
    { NR_FILTER_DIFFUSELIGHTING,   { EffectCategory::Colors,     "feDiffuseLighting-icon",
        _("Creates \"embossed\" shadings.  The input's alpha channel is used to provide depth information: higher opacity areas are raised toward the viewer and lower opacity areas recede away from the viewer.") }},
    { NR_FILTER_SPECULARLIGHTING,  { EffectCategory::Colors,     "feSpecularLighting-icon",
        _("Creates \"embossed\" shadings.  The input's alpha channel is used to provide depth information: higher opacity areas are raised toward the viewer and lower opacity areas recede away from the viewer.") }},
    { NR_FILTER_FLOOD,             { EffectCategory::Generation, "feFlood-icon",
        _("Fills the region with a given color and opacity. Often used as input to other filters to apply color to a graphic.") }},
    { NR_FILTER_IMAGE,             { EffectCategory::Generation, "feImage-icon",
        _("Fills the region with graphics from an external file or from another portion of the document.") }},
    { NR_FILTER_TURBULENCE,        { EffectCategory::Generation, "feTurbulence-icon",
        _("Renders Perlin noise, which is useful to generate textures such as clouds, fire, smoke, marble or granite.") }},
    };
    return effects;
}

// populate popup with filter effects and completion list for a search box
void FilterEffectsDialog::add_effects(Inkscape::UI::Widget::CompletionPopup& popup, bool symbolic) {
    auto& menu = popup.get_menu();

    struct Effect {
        Inkscape::Filters::FilterPrimitiveType type;
        Glib::ustring label;
        EffectCategory category;
        Glib::ustring icon_name;
        Glib::ustring tooltip;
    };
    std::vector<Effect> effects;
    effects.reserve(get_effects().size());
    for (auto&& effect : get_effects()) {
        effects.push_back({
            effect.first,
            _(FPConverter.get_label(effect.first).c_str()),
            effect.second.category,
            effect.second.icon_name,
            effect.second.tooltip
        });
    }
    std::sort(begin(effects), end(effects), [=](auto&& a, auto&& b) {
        if (a.category != b.category) {
            return a.category < b.category;
        }
        return a.label < b.label;
    });

    popup.clear_completion_list();

    // 2-column menu
    Inkscape::UI::ColumnMenuBuilder<EffectCategory> builder{menu, 2, Gtk::IconSize::LARGE};
    for (auto const &effect : effects) {
        // build popup menu
        auto const &type = effect.type;
        auto const menuitem = builder.add_item(effect.label, effect.category, effect.tooltip,
                                               effect.icon_name, true, true,
                                               [=, this]{ add_filter_primitive(type); });
        auto const id = static_cast<int>(type);
        menuitem->signal_query_tooltip().connect([=, this] (int x, int y, bool kbd, const Glib::RefPtr<Gtk::Tooltip>& tooltipw) {
            return sp_query_custom_tooltip(this, x, y, kbd, tooltipw, id, effect.tooltip, effect.icon_name);
        }, false); // before
        if (builder.new_section()) {
            builder.set_section(get_category_name(effect.category));
        }
        // build completion list
        popup.add_to_completion_list(id, effect.label, effect.icon_name + (symbolic ? "-symbolic" : ""));
    }
    if (symbolic) {
        menu.add_css_class("symbolic");
    }
}

/*** FilterEffectsDialog ***/

FilterEffectsDialog::FilterEffectsDialog()
    : DialogBase("/dialogs/filtereffects", "FilterEffects"),
    _builder(create_builder("dialog-filter-editor.glade")),
    _paned(get_widget<Gtk::Paned>(_builder, "paned")),
    _main_grid(get_widget<Gtk::Grid>(_builder, "main")),
    _params_box(get_widget<Gtk::Box>(_builder, "params")),
    _search_box(get_widget<Gtk::Box>(_builder, "search")),
    _search_wide_box(get_widget<Gtk::Box>(_builder, "search-wide")),
    _filter_canvas(*this),
    testing_box(),
    _filter_wnd(get_widget<Gtk::ScrolledWindow>(_builder, "filter")),
    _cur_filter_btn(get_widget<Gtk::CheckButton>(_builder, "label"))
    , _add_primitive_type(FPConverter)
    , _add_primitive(_("Add Effect:"))
    , _empty_settings("", Gtk::Align::CENTER)
    , _no_filter_selected(_("No filter selected"), Gtk::Align::START)
    , _settings_initialized(false)
    , _locked(false)
    , _attr_lock(false)
    , _filter_modifier(*this, _builder)
    , _primitive_list(*this)
    , _settings_effect(Gtk::Orientation::VERTICAL)
    , _settings_filter(Gtk::Orientation::VERTICAL)
{
    _settings = std::make_unique<Settings>(*this, _settings_effect,
                                           [this](auto const a){ set_attr_direct(a); },
                                           NR_FILTER_ENDPRIMITIVETYPE);
    _cur_effect_name = &get_widget<Gtk::Label>(_builder, "cur-effect");
    _settings->_size_group->add_widget(*_cur_effect_name);
    _filter_general_settings = std::make_unique<Settings>(*this, _settings_filter,
                                                          [this](auto const a){ set_filternode_attr(a); }, 1);

    // testing_box.set_hexpand_set(true);
    // testing_box.set_hexpand(true);
    // testing_box.set_vexpand_set(true);
    // testing_box.set_vexpand(true);
    // testing_box.add_css_class("canvas");
    // testing_box.set_size_request(100, 100);
    // Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
    // // add_css_class("canvas");
    // // canvas.set_name("filter-canvas-fixed");

    // /*TODO: move the testing CSS file to the right place*/
    // Glib::ustring style = Inkscape::IO::Resource::get_filename(Inkscape::IO::Resource::UIS, "node-editor.css");
    // provider->load_from_path(style);
    // // provider->load_from_path("/home/phantomzback/Documents/GSOC_Projs/inkscape_final/testing.css");
    // testing_box.get_style_context()->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    // // get_style_context()->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    // Initialize widget hierarchy
    _primitive_box = &get_widget<Gtk::ScrolledWindow>(_builder, "filter");
    _primitive_list.set_enable_search(false);
    _primitive_box->set_child(_primitive_list);

    auto symbolic = Inkscape::Preferences::get()->getBool("/theme/symbolicIcons", true);
    add_effects(_effects_popup, symbolic);
    _effects_popup.get_entry().set_placeholder_text(_("Add effect"));
    _effects_popup.on_match_selected().connect(
        [this](int const id){ add_filter_primitive(static_cast<FilterPrimitiveType>(id)); });
    UI::pack_start(_search_box, _effects_popup);

    _settings_effect.set_valign(Gtk::Align::FILL);
    _params_box.append(_settings_effect);

    _settings_filter.set_margin(5);
    get_widget<Gtk::Popover>(_builder, "gen-settings").set_child(_settings_filter);

    get_widget<Gtk::Popover>(_builder, "info-popover").signal_show().connect([this]{
        if (auto prim = _filter_canvas.get_selected_primitive()) {
            if (prim->getRepr()) {
                auto id = FPConverter.get_id_from_label(prim->getRepr()->name());
                const auto& effect = get_effects().at(id);
                get_widget<Gtk::Image>(_builder, "effect-icon").set_from_icon_name(effect.icon_name);
                auto buffer = get_widget<Gtk::TextView>(_builder, "effect-info").get_buffer();
                buffer->set_text("");
                buffer->insert_markup(buffer->begin(), effect.tooltip);
                get_widget<Gtk::TextView>(_builder, "effect-desc").get_buffer()->set_text("");
            }
        }
    });

    _primitive_list.signal_primitive_changed().connect([this]{ update_settings_view(); });
    _filter_canvas.signal_primitive_changed().connect([this]{ update_settings_view(); });

    _cur_filter_toggle = _cur_filter_btn.signal_toggled().connect([this]{
        _filter_modifier.toggle_current_filter();
    });

    auto update_checkbox = [this]{
        auto active = _filter_modifier.is_selected_filter_active();
        _cur_filter_toggle.block();
        _cur_filter_btn.set_active(active);
        _cur_filter_toggle.unblock();
    };

    auto update_widgets = [=, this]{
        auto& opt = get_widget<Gtk::MenuButton>(_builder, "filter-opt");
        _primitive_list.update();
        Glib::ustring name;
        if (auto filter = _filter_modifier.get_selected_filter()) {
            name = get_filter_name(filter);
            _effects_popup.set_sensitive();
            _cur_filter_btn.set_sensitive(); // ideally this should also be selection-dependent
            opt.set_sensitive();
        } else {
            name = "-";
            _effects_popup.set_sensitive(false);
            _cur_filter_btn.set_sensitive(false);
            opt.set_sensitive(false);
        }
        get_widget<Gtk::Label>(_builder, "filter-name").set_label(name);
        update_checkbox();
        update_settings_view();
    };

    //TODO: adding animated GIFs to the info popup once they are ready:
    // auto a = Gdk::PixbufAnimation::create_from_file("/Users/mike/blur-effect.gif");
    // get_widget<Gtk::Image>(_builder, "effect-image").property_pixbuf_animation().set_value(a);

    init_settings_widgets();

    _filter_modifier.signal_filter_changed().connect([=](){

        auto filter = _filter_modifier.get_selected_filter();
        auto document = filter->document;
        
        int i = 0;
        if(filter != nullptr){
            for (auto &child : filter->children) {
                auto prim = cast<SPFilterPrimitive>(&child);
                // g_message("index and id: %d %s", i, prim->getAttribute("id"));
                i++;
            }
        }
        // _filter_canvas.update_canvas();
        _filter_canvas.update_canvas_new();
        // _filter_canvas.update_filter(_filter_modifier.get_selected_filter());
        update_widgets();
    });

    _filter_modifier.signal_filters_updated().connect([=](){
        update_checkbox();
    });

    _add_primitive.signal_clicked().connect(sigc::mem_fun(*this, &FilterEffectsDialog::add_primitive));
    _primitive_list.set_menu(sigc::mem_fun(*this, &FilterEffectsDialog::duplicate_primitive),
                             sigc::mem_fun(_primitive_list, &PrimitiveList::remove_selected));

    get_widget<Gtk::Button>(_builder, "new-filter").signal_clicked().connect([this]{ _filter_modifier.add_filter(); });
    append(_bin);
    _bin.set_expand(true);
    _bin.set_child(_popoverbin);
    _popoverbin.setChild(&_main_grid);

    get_widget<Gtk::Button>(_builder, "dup-btn").signal_clicked().connect([this]{ duplicate_primitive(); });
    get_widget<Gtk::Button>(_builder, "del-btn").signal_clicked().connect([this]{ _primitive_list.remove_selected(); });
    // get_widget<Gtk::Button>(_builder, "info-btn").signal_clicked().connect([this]{ /* todo */ });

    _show_sources = &get_widget<Gtk::ToggleButton>(_builder, "btn-connect");
    auto set_inputs = [this](bool const all){
        int count = all ? FPInputConverter._length : 2;
        _primitive_list.set_inputs_count(count);
        // full rebuild: this is what it takes to make cell renderer new min width into account to adjust scrollbar
        _primitive_list.update();
    };
    auto show_all_sources = Inkscape::Preferences::get()->getBool(prefs_path + "/dialogs/filters/showAllSources", false);
    _show_sources->set_active(show_all_sources);
    set_inputs(show_all_sources);
    _show_sources->signal_toggled().connect([=, this]{
        bool const show_all = _show_sources->get_active();
        set_inputs(show_all);
        Inkscape::Preferences::get()->setBool(prefs_path + "/dialogs/filters/showAllSources", show_all);
    });

    _paned.set_position(Inkscape::Preferences::get()->getIntLimited(prefs_path + "/handlePos", 200, 10, 9999));
    _paned.property_position().signal_changed().connect([this]{
        Inkscape::Preferences::get()->setInt(prefs_path + "/handlePos", _paned.get_position());
    });

    _primitive_list.update();

    // reading minimal width at this point should reflect space needed for fitting effect parameters panel
    Gtk::Requisition minimum_size, natural_size;
    get_preferred_size(minimum_size, natural_size);
    int min_width = minimum_size.get_width();
    _effects_popup.get_preferred_size(minimum_size, natural_size);
    auto const min_effects = minimum_size.get_width();
    // calculate threshold/minimum width of filters dialog in horizontal layout;
    // use this size to decide where transition from vertical to horizontal layout is;
    // if this size is too small dialog can get stuck in horizontal layout - users won't be able
    // to make it narrow again, due to min dialog size enforced by GTK
    int threshold_width = min_width + min_effects * 3;

    // two alternative layout arrangements depending on the dialog size;
    // one is tall and narrow with widgets in one column, while the other
    // is for wide dialogs with filter parameters and effects side by side
    _bin.connectBeforeResize([=, this] (int width, int height, int baseline) {
        if (width < 10 || height < 10) return;

        double const ratio = width / static_cast<double>(height);

        constexpr double hysteresis = 0.01;
        if (ratio < 1 - hysteresis || width <= threshold_width) {
            // make narrow/tall
            if (!_narrow_dialog) {
                // _main_grid.remove(_filter_wnd);
                _main_grid.remove(_filter_canvas);
                // _main_grid.remove(testing_box);
                _search_wide_box.remove(_effects_popup);
                _paned.set_start_child(_filter_wnd);
                // _paned.set_start_child(_filter_canvas);
                UI::pack_start(_search_box, _effects_popup);
                _paned.set_size_request();
                get_widget<Gtk::Box>(_builder, "connect-box-wide").remove(*_show_sources);
                get_widget<Gtk::Box>(_builder, "connect-box").append(*_show_sources);
                _narrow_dialog = true;
                ensure_size();
            }
        } else if (ratio > 1 + hysteresis && width > threshold_width) {
            // make wide/short
            if (_narrow_dialog) {
                _paned.property_start_child().set_value(nullptr);
                _search_box.remove(_effects_popup);
                // _main_grid.attach(_filter_wnd, 2, 1, 1, 2);
                _main_grid.attach(_filter_canvas, 2, 1, 1, 2);
                // _main_grid.attach(testing_box, 2, 1, 1, 2);
                UI::pack_start(_search_wide_box, _effects_popup);
                _paned.set_size_request(min_width);
                get_widget<Gtk::Box>(_builder, "connect-box").remove(*_show_sources);
                get_widget<Gtk::Box>(_builder, "connect-box-wide").append(*_show_sources);
                _narrow_dialog = false;
                ensure_size();
            }
        }
    });

    update_widgets();
    update();
    update_settings_view();
}

FilterEffectsDialog::~FilterEffectsDialog() = default;

void FilterEffectsDialog::documentReplaced()
{
    _resource_changed.disconnect();
    if (auto document = getDocument()) {
        auto const update_filters = [this]{ _filter_modifier.update_filters(); };
        _resource_changed = document->connectResourcesChanged("filter", update_filters);
        update_filters();
    }
}

void FilterEffectsDialog::selectionChanged(Inkscape::Selection *selection)
{
    if (selection) {
        _filter_modifier.update_selection(selection);
    }
}

void FilterEffectsDialog::selectionModified(Inkscape::Selection *selection, guint flags)
{
    if (flags & (SP_OBJECT_MODIFIED_FLAG |
                 SP_OBJECT_PARENT_MODIFIED_FLAG |
                 SP_OBJECT_STYLE_MODIFIED_FLAG)) {
        _filter_modifier.update_selection(selection);
    }
}

void FilterEffectsDialog::set_attrs_locked(const bool l)
{
    _locked = l;
}

void FilterEffectsDialog::init_settings_widgets()
{
    // TODO: Find better range/climb-rate/digits values for the SpinScales,
    //       most of the current values are complete guesses!

    _empty_settings.set_sensitive(false);
    UI::pack_start(_settings_effect, _empty_settings);

    _no_filter_selected.set_sensitive(false);
    UI::pack_start(_settings_filter, _no_filter_selected);
    _settings_initialized = true;

    _filter_general_settings->type(0);
    auto _region_auto = _filter_general_settings->add_checkbutton(true, SPAttr::AUTO_REGION, _("Automatic Region"), "true", "false", _("If unset, the coordinates and dimensions won't be updated automatically."));
    _region_pos = _filter_general_settings->add_multispinbutton(/*default x:*/ (double) -0.1, /*default y:*/ (double) -0.1, SPAttr::X, SPAttr::Y, _("Coordinates:"), -100, 100, 0.01, 0.1, 2, _("X coordinate of the left corners of filter effects region"), _("Y coordinate of the upper corners of filter effects region"));
    _region_size = _filter_general_settings->add_multispinbutton(/*default width:*/ (double) 1.2, /*default height:*/ (double) 1.2, SPAttr::WIDTH, SPAttr::HEIGHT, _("Dimensions:"), 0, 1000, 0.01, 0.1, 2, _("Width of filter effects region"), _("Height of filter effects region"));
    _region_auto->signal_attr_changed().connect( sigc::bind(sigc::mem_fun(*this, &FilterEffectsDialog::update_automatic_region), _region_auto));

    _settings->type(NR_FILTER_BLEND);
    _settings->add_combo(SP_CSS_BLEND_NORMAL, SPAttr::MODE, _("Mode:"), SPBlendModeConverter);

    _settings->type(NR_FILTER_COLORMATRIX);
    ComboBoxEnum<FilterColorMatrixType>* colmat = _settings->add_combo(COLORMATRIX_MATRIX, SPAttr::TYPE, _("Type:"), ColorMatrixTypeConverter, _("Indicates the type of matrix operation. The keyword 'matrix' indicates that a full 5x4 matrix of values will be provided. The other keywords represent convenience shortcuts to allow commonly used color operations to be performed without specifying a complete matrix."));
    _color_matrix_values = _settings->add_colormatrixvalues(_("Value(s):"));
    colmat->signal_attr_changed().connect(sigc::mem_fun(*this, &FilterEffectsDialog::update_color_matrix));

    _settings->type(NR_FILTER_COMPONENTTRANSFER);
    // TRANSLATORS: Abbreviation for red color channel in RGBA
    _settings->add_componenttransfervalues(C_("color", "R:"), SPFeFuncNode::R);
    // TRANSLATORS: Abbreviation for green color channel in RGBA
    _settings->add_componenttransfervalues(C_("color", "G:"), SPFeFuncNode::G);
    // TRANSLATORS: Abbreviation for blue color channel in RGBA
    _settings->add_componenttransfervalues(C_("color", "B:"), SPFeFuncNode::B);
    // TRANSLATORS: Abbreviation for alpha channel in RGBA
    _settings->add_componenttransfervalues(C_("color", "A:"), SPFeFuncNode::A);

    _settings->type(NR_FILTER_COMPOSITE);
    _settings->add_combo(COMPOSITE_OVER, SPAttr::OPERATOR, _("Operator:"), CompositeOperatorConverter);
    _k1 = _settings->add_spinscale(0, SPAttr::K1, _("K1:"), -10, 10, 0.1, 0.01, 2, _("If the arithmetic operation is chosen, each result pixel is computed using the formula k1*i1*i2 + k2*i1 + k3*i2 + k4 where i1 and i2 are the pixel values of the first and second inputs respectively."));
    _k2 = _settings->add_spinscale(0, SPAttr::K2, _("K2:"), -10, 10, 0.1, 0.01, 2, _("If the arithmetic operation is chosen, each result pixel is computed using the formula k1*i1*i2 + k2*i1 + k3*i2 + k4 where i1 and i2 are the pixel values of the first and second inputs respectively."));
    _k3 = _settings->add_spinscale(0, SPAttr::K3, _("K3:"), -10, 10, 0.1, 0.01, 2, _("If the arithmetic operation is chosen, each result pixel is computed using the formula k1*i1*i2 + k2*i1 + k3*i2 + k4 where i1 and i2 are the pixel values of the first and second inputs respectively."));
    _k4 = _settings->add_spinscale(0, SPAttr::K4, _("K4:"), -10, 10, 0.1, 0.01, 2, _("If the arithmetic operation is chosen, each result pixel is computed using the formula k1*i1*i2 + k2*i1 + k3*i2 + k4 where i1 and i2 are the pixel values of the first and second inputs respectively."));

    _settings->type(NR_FILTER_CONVOLVEMATRIX);
    _convolve_order = _settings->add_dualspinbutton((char*)"3", SPAttr::ORDER, _("Size:"), 1, max_convolution_kernel_size, 1, 1, 0, _("width of the convolve matrix"), _("height of the convolve matrix"));
    _convolve_target = _settings->add_multispinbutton(/*default x:*/ (double) 0, /*default y:*/ (double) 0, SPAttr::TARGETX, SPAttr::TARGETY, _("Target:"), 0, max_convolution_kernel_size - 1, 1, 1, 0, _("X coordinate of the target point in the convolve matrix. The convolution is applied to pixels around this point."), _("Y coordinate of the target point in the convolve matrix. The convolution is applied to pixels around this point."));
    //TRANSLATORS: for info on "Kernel", see http://en.wikipedia.org/wiki/Kernel_(matrix)
    _convolve_matrix = _settings->add_matrix(SPAttr::KERNELMATRIX, _("Kernel:"), _("This matrix describes the convolve operation that is applied to the input image in order to calculate the pixel colors at the output. Different arrangements of values in this matrix result in various possible visual effects. An identity matrix would lead to a motion blur effect (parallel to the matrix diagonal) while a matrix filled with a constant non-zero value would lead to a common blur effect."));
    _convolve_order->signal_attr_changed().connect(sigc::mem_fun(*this, &FilterEffectsDialog::convolve_order_changed));
    _settings->add_spinscale(0, SPAttr::DIVISOR, _("Divisor:"), 0, 1000, 1, 0.1, 2, _("After applying the kernelMatrix to the input image to yield a number, that number is divided by divisor to yield the final destination color value. A divisor that is the sum of all the matrix values tends to have an evening effect on the overall color intensity of the result."));
    _settings->add_spinscale(0, SPAttr::BIAS, _("Bias:"), -10, 10, 0.1, 0.5, 2, _("This value is added to each component. This is useful to define a constant value as the zero response of the filter."));
    _settings->add_combo(CONVOLVEMATRIX_EDGEMODE_NONE, SPAttr::EDGEMODE, _("Edge Mode:"), ConvolveMatrixEdgeModeConverter, _("Determines how to extend the input image as necessary with color values so that the matrix operations can be applied when the kernel is positioned at or near the edge of the input image."));
    _settings->add_checkbutton(false, SPAttr::PRESERVEALPHA, _("Preserve Alpha"), "true", "false", _("If set, the alpha channel won't be altered by this filter primitive."));

    _settings->type(NR_FILTER_DIFFUSELIGHTING);
    _settings->add_color(/*default: white*/ 0xffffffff, SPAttr::LIGHTING_COLOR, _("Diffuse Color:"), _("Defines the color of the light source"));
    _settings->add_spinscale(1, SPAttr::SURFACESCALE, _("Surface Scale:"), -5, 5, 0.01, 0.001, 3, _("This value amplifies the heights of the bump map defined by the input alpha channel"));
    _settings->add_spinscale(1, SPAttr::DIFFUSECONSTANT, _("Constant:"), 0, 5, 0.1, 0.01, 2, _("This constant affects the Phong lighting model."));
    // deprecated (https://developer.mozilla.org/en-US/docs/Web/SVG/Attribute/kernelUnitLength)
    // _settings->add_dualspinscale(SPAttr::KERNELUNITLENGTH, _("Kernel Unit Length:"), 0.01, 10, 1, 0.01, 1);
    _settings->add_lightsource();

    _settings->type(NR_FILTER_DISPLACEMENTMAP);
    _settings->add_spinscale(0, SPAttr::SCALE, _("Scale:"), 0, 100, 1, 0.01, 1, _("This defines the intensity of the displacement effect."));
    _settings->add_combo(DISPLACEMENTMAP_CHANNEL_ALPHA, SPAttr::XCHANNELSELECTOR, _("X displacement:"), DisplacementMapChannelConverter, _("Color component that controls the displacement in the X direction"));
    _settings->add_combo(DISPLACEMENTMAP_CHANNEL_ALPHA, SPAttr::YCHANNELSELECTOR, _("Y displacement:"), DisplacementMapChannelConverter, _("Color component that controls the displacement in the Y direction"));

    _settings->type(NR_FILTER_FLOOD);
    _settings->add_color(/*default: black*/ 0, SPAttr::FLOOD_COLOR, _("Color:"), _("The whole filter region will be filled with this color."));
    _settings->add_spinscale(1, SPAttr::FLOOD_OPACITY, _("Opacity:"), 0, 1, 0.1, 0.01, 2);

    _settings->type(NR_FILTER_GAUSSIANBLUR);
    _settings->add_dualspinscale(SPAttr::STDDEVIATION, _("Size:"), 0, 100, 1, 0.01, 2, _("The standard deviation for the blur operation."));

    _settings->type(NR_FILTER_MERGE);
    _settings->add_no_params();

    _settings->type(NR_FILTER_MORPHOLOGY);
    _settings->add_combo(MORPHOLOGY_OPERATOR_ERODE, SPAttr::OPERATOR, _("Operator:"), MorphologyOperatorConverter, _("Erode: performs \"thinning\" of input image.\nDilate: performs \"fattening\" of input image."));
    _settings->add_dualspinscale(SPAttr::RADIUS, _("Radius:"), 0, 100, 1, 0.01, 1);

    _settings->type(NR_FILTER_IMAGE);
    _settings->add_fileorelement(SPAttr::XLINK_HREF, _("Source of Image:"));
    _image_x = _settings->add_entry(SPAttr::X, _("Position X:"), _("Position X"));
    _image_x->signal_attr_changed().connect(sigc::mem_fun(*this, &FilterEffectsDialog::image_x_changed));
    //This is commented out because we want the default empty value of X or Y and couldn't get it from SpinButton
    //_image_y = _settings->add_spinbutton(0, SPAttr::Y, _("Y:"), -DBL_MAX, DBL_MAX, 1, 1, 5, _("Y"));
    _image_y = _settings->add_entry(SPAttr::Y, _("Position Y:"), _("Position Y"));
    _image_y->signal_attr_changed().connect(sigc::mem_fun(*this, &FilterEffectsDialog::image_y_changed));
    _settings->add_entry(SPAttr::WIDTH, _("Width:"), _("Width"));
    _settings->add_entry(SPAttr::HEIGHT, _("Height:"), _("Height"));

    _settings->type(NR_FILTER_OFFSET);
    _settings->add_checkbutton(false, SPAttr::PRESERVEALPHA, _("Preserve Alpha"), "true", "false", _("If set, the alpha channel won't be altered by this filter primitive."));
    _settings->add_spinscale(0, SPAttr::DX, _("Delta X:"), -100, 100, 1, 0.01, 2, _("This is how far the input image gets shifted to the right"));
    _settings->add_spinscale(0, SPAttr::DY, _("Delta Y:"), -100, 100, 1, 0.01, 2, _("This is how far the input image gets shifted downwards"));

    _settings->type(NR_FILTER_SPECULARLIGHTING);
    _settings->add_color(/*default: white*/ 0xffffffff, SPAttr::LIGHTING_COLOR, _("Specular Color:"), _("Defines the color of the light source"));
    _settings->add_spinscale(1, SPAttr::SURFACESCALE, _("Surface Scale:"), -5, 5, 0.1, 0.01, 2, _("This value amplifies the heights of the bump map defined by the input alpha channel"));
    _settings->add_spinscale(1, SPAttr::SPECULARCONSTANT, _("Constant:"), 0, 5, 0.1, 0.01, 2, _("This constant affects the Phong lighting model."));
    _settings->add_spinscale(1, SPAttr::SPECULAREXPONENT, _("Exponent:"), 1, 50, 1, 0.01, 1, _("Exponent for specular term, larger is more \"shiny\"."));
    // deprecated (https://developer.mozilla.org/en-US/docs/Web/SVG/Attribute/kernelUnitLength)
    // _settings->add_dualspinscale(SPAttr::KERNELUNITLENGTH, _("Kernel Unit Length:"), 0.01, 10, 1, 0.01, 1);
    _settings->add_lightsource();

    _settings->type(NR_FILTER_TILE);
    // add some filter primitive attributes: https://drafts.fxtf.org/filter-effects/#feTileElement
    // issue: https://gitlab.com/inkscape/inkscape/-/issues/1417
    _settings->add_entry(SPAttr::X, _("Position X:"), _("Position X"));
    _settings->add_entry(SPAttr::Y, _("Position Y:"), _("Position Y"));
    _settings->add_entry(SPAttr::WIDTH, _("Width:"), _("Width"));
    _settings->add_entry(SPAttr::HEIGHT, _("Height:"), _("Height"));

    _settings->type(NR_FILTER_TURBULENCE);
//    _settings->add_checkbutton(false, SPAttr::STITCHTILES, _("Stitch Tiles"), "stitch", "noStitch");
    _settings->add_combo(TURBULENCE_TURBULENCE, SPAttr::TYPE, _("Type:"), TurbulenceTypeConverter, _("Indicates whether the filter primitive should perform a noise or turbulence function."));
    _settings->add_dualspinscale(SPAttr::BASEFREQUENCY, _("Size:"), 0.001, 10, 0.001, 0.1, 3);
    _settings->add_spinscale(1, SPAttr::NUMOCTAVES, _("Detail:"), 1, 10, 1, 1, 0);
    _settings->add_spinscale(0, SPAttr::SEED, _("Seed:"), 0, 1000, 1, 1, 0, _("The starting number for the pseudo random number generator."));
}

void FilterEffectsDialog::add_filter_primitive(Filters::FilterPrimitiveType type) {
    if (auto filter = _filter_modifier.get_selected_filter()) {
        _filter_modifier._observer->set(nullptr);
        SPFilterPrimitive* prim = filter_add_primitive(filter, type);
        int num_sinks = input_count(prim);
        auto added_node = _filter_canvas.add_primitive_node(prim, 0, 0, type, FPConverter.get_label(type), num_sinks); 
        // prim->getRepr()->setPosition(0);
        _filter_canvas.update_document();
        _primitive_list.select(prim);
        DocumentUndo::done(filter->document, _("Add filter primitive"), INKSCAPE_ICON("dialog-filters"));
    }
}

void FilterEffectsDialog::add_primitive()
{
    add_filter_primitive(_add_primitive_type.get_active_data()->id);
}

void FilterEffectsDialog::duplicate_primitive()
{
    SPFilter* filter = _filter_modifier.get_selected_filter();
    SPFilterPrimitive* origprim = _primitive_list.get_selected();
    // SPFilterPrimitve* origprim = _filter_canvas.get_selected_primitive();

    if (filter && origprim) {
        Inkscape::XML::Node *repr;
        repr = origprim->getRepr()->duplicate(origprim->getRepr()->document());
        filter->getRepr()->appendChild(repr);

        DocumentUndo::done(filter->document, _("Duplicate filter primitive"), INKSCAPE_ICON("dialog-filters"));

        _primitive_list.update();
    }
}

void FilterEffectsDialog::convolve_order_changed()
{
    _convolve_matrix->set_from_attribute(_primitive_list.get_selected());
    // MultiSpinButtons orders widgets backwards: so use index 1 and 0
    _convolve_target->get_spinbuttons()[1]->get_adjustment()->set_upper(_convolve_order->get_spinbutton1().get_value() - 1);
    _convolve_target->get_spinbuttons()[0]->get_adjustment()->set_upper(_convolve_order->get_spinbutton2().get_value() - 1);
}

bool number_or_empy(const Glib::ustring& text) {
    if (text.empty()) {
        return true;
    }
    double n = g_strtod(text.c_str(), nullptr);
    if (n == 0.0 && strcmp(text.c_str(), "0") != 0 && strcmp(text.c_str(), "0.0") != 0) {
        return false;
    }
    else {
        return true;
    }
}

void FilterEffectsDialog::image_x_changed()
{
    if (number_or_empy(_image_x->get_text())) {
        _image_x->set_from_attribute(_primitive_list.get_selected());
    }
}

void FilterEffectsDialog::image_y_changed()
{
    if (number_or_empy(_image_y->get_text())) {
        _image_y->set_from_attribute(_primitive_list.get_selected());
    }
}

void FilterEffectsDialog::set_attr_direct(const AttrWidget* input)
{
    // set_attr(_primitive_list.get_selected(), input->get_attribute(), input->get_as_attribute().c_str());
    set_attr(_filter_canvas.get_selected_primitive(), input->get_attribute(), input->get_as_attribute().c_str());
}

void FilterEffectsDialog::set_filternode_attr(const AttrWidget* input)
{
    if(!_locked) {
        _attr_lock = true;
        SPFilter *filter = _filter_modifier.get_selected_filter();
        const gchar* name = (const gchar*)sp_attribute_name(input->get_attribute());
        if (filter && name && filter->getRepr()){
            filter->setAttributeOrRemoveIfEmpty(name, input->get_as_attribute());
            filter->requestModified(SP_OBJECT_MODIFIED_FLAG);
        }
        _attr_lock = false;
    }
}

void FilterEffectsDialog::set_child_attr_direct(const AttrWidget* input)
{
    // set_attr(_primitive_list.get_selected()->firstChild(), input->get_attribute(), input->get_as_attribute().c_str());
    set_attr(_filter_canvas.get_selected_primitive()->firstChild(), input->get_attribute(), input->get_as_attribute().c_str());
}

void FilterEffectsDialog::set_attr(SPObject* o, const SPAttr attr, const gchar* val)
{
    if(!_locked) {
        _attr_lock = true;

        SPFilter *filter = _filter_modifier.get_selected_filter();
        const gchar* name = (const gchar*)sp_attribute_name(attr);
        if(filter && name && o) {
            update_settings_sensitivity();

            o->setAttribute(name, val);
            filter->requestModified(SP_OBJECT_MODIFIED_FLAG);

            Glib::ustring undokey = "filtereffects:";
            undokey += name;
            DocumentUndo::maybeDone(filter->document, undokey.c_str(), _("Set filter primitive attribute"), INKSCAPE_ICON("dialog-filters"));
        }

        _attr_lock = false;
    }
}

void FilterEffectsDialog::update_filter_general_settings_view()
{
    if(_settings_initialized != true) return;

    if(!_locked) {
        _attr_lock = true;

        SPFilter* filter = _filter_modifier.get_selected_filter();

        if(filter) {
            _filter_general_settings->show_and_update(0, filter);
            _no_filter_selected.set_visible(false);
        } else {
            UI::get_children(_settings_filter).at(0)->set_visible(false);
            _no_filter_selected.set_visible(true);
        }

        _attr_lock = false;
    }
}

void FilterEffectsDialog::update_settings_view()
{
    update_settings_sensitivity();

    if (_attr_lock)
        return;

    // selected effect parameters

    for (auto const i : UI::get_children(_settings_effect)) {
        i->set_visible(false);
    }

    // SPFilterPrimitive* prim = _primitive_list.get_selected();
    SPFilterPrimitive* prim = _filter_canvas.get_selected_primitive();
    auto& header = get_widget<Gtk::Box>(_builder, "effect-header");
    SPFilter* filter = _filter_modifier.get_selected_filter();
    bool present = _filter_modifier.filters_present();

    if (prim && prim->getRepr()) {
        //XML Tree being used directly here while it shouldn't be.
        auto id = FPConverter.get_id_from_key(prim->getRepr()->name());
        _settings->show_and_update(id, prim);
        _empty_settings.set_visible(false);
        _cur_effect_name->set_text(_(FPConverter.get_label(id).c_str()));
        header.set_visible(true);
    }
    else {
        if (filter) {
            _empty_settings.set_text(_("Add effect from the search bar"));
        }
        else if (present) {
            _empty_settings.set_text(_("Select a filter"));
        }
        else {
            _empty_settings.set_text(_("No filters in the document"));
        }
        _empty_settings.set_visible(true);
        _cur_effect_name->set_text(Glib::ustring());
        header.set_visible(false);
    }

    // current filter parameters (area size)

    UI::get_children(_settings_filter).at(0)->set_visible(false);
    _no_filter_selected.set_visible(true);

    if (filter) {
        _filter_general_settings->show_and_update(0, filter);
        _no_filter_selected.set_visible(false);
    }

    ensure_size();
}

void FilterEffectsDialog::update_settings_sensitivity()
{
    // SPFilterPrimitive* prim = _primitive_list.get_selected();
    SPFilterPrimitive* prim = _filter_canvas.get_selected_primitive();
    const bool use_k = is<SPFeComposite>(prim) && cast<SPFeComposite>(prim)->get_composite_operator() == COMPOSITE_ARITHMETIC;
    _k1->set_sensitive(use_k);
    _k2->set_sensitive(use_k);
    _k3->set_sensitive(use_k);
    _k4->set_sensitive(use_k);
}

void FilterEffectsDialog::update_color_matrix()
{
    _color_matrix_values->set_from_attribute(_filter_canvas.get_selected_primitive());
}

void FilterEffectsDialog::update_automatic_region(Gtk::CheckButton *btn)
{
    bool automatic = btn->get_active();
    _region_pos->set_sensitive(!automatic);
    _region_size->set_sensitive(!automatic);
}

} // namespace Inkscape::UI::Dialog

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
