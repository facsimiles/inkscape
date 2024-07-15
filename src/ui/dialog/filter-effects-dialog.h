// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
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

#ifndef INKSCAPE_UI_DIALOG_FILTER_EFFECTS_H
#define INKSCAPE_UI_DIALOG_FILTER_EFFECTS_H

#include <memory>
#include <2geom/point.h>
#include <2geom/rect.h>
#include <sigc++/connection.h>
#include <sigc++/signal.h>
#include <glibmm/property.h>
#include <gtkmm/gesture.h> // Gtk::EventSequenceState
#include <gtkmm/label.h>
#include <gtkmm/treeview.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/fixed.h>
#include <gtkmm/stylecontext.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/textview.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/gesture.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/gesturedrag.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/snapshot.h>

#include "io/resource.h"
#include "attributes.h"
#include "display/nr-filter-types.h"
#include "ui/dialog/dialog-base.h"
#include "ui/widget/bin.h"
#include "ui/widget/combo-enums.h"
#include "ui/widget/completion-popup.h"
#include "ui/widget/popover-bin.h"
#include "ui/widget/widget-vfuncs-class-init.h"
#include "xml/helper-observer.h"

namespace Gdk {
class Drag;
} // namespace Gdk

namespace Gtk {
class Button;
class CheckButton;
class DragSource;
class GestureClick;
class Grid;
class Label;
class ListStore;
class Paned;
class ScrolledWindow;
class ToggleButton;
} // namespace Gtk

class SPFilter;
class SPFilterPrimitive;

#define NODE_TYPE FilterEditorNode
#define SCROLL_SENS (10.0)
#define SHIFT_DOWN ((modifier_state & Gdk::ModifierType::SHIFT_MASK)!=Gdk::ModifierType::SHIFT_MASK)
namespace Inkscape::UI {

namespace Widget {
class PopoverMenu;
} // namespace Widget

namespace Dialog {

class EntryAttr;
class FileOrElementChooser;
class DualSpinButton;
class MultiSpinButton;

class FilterEditorCanvas;
class FilterEditorNode;







class FilterEditorSource;
class FilterEditorSink;
class FilterEditorConnection;
class CanvasFixed;



class FilterEditorSource : public Gtk::Box{
    public:
        FilterEditorSource(FilterEditorNode* _node, std::string _label_string = "");
        FilterEditorNode* get_parent_node();

        std::vector<FilterEditorConnection*> get_connections();

        bool add_connection(FilterEditorConnection *connection);

        bool get_selected();

    protected:
        // int source_id;
        // int node_id;
        FilterEditorNode* node;        
        std::vector<FilterEditorConnection*> connections;
        std::string label_string; // The label corresponding to the string.
};



class FilterEditorSink : public Gtk::Box{
    public:
        FilterEditorSink(FilterEditorNode* _node, int _max_connections, std::string _label_string = "") : Gtk::Box(Gtk::Orientation::VERTICAL, 0), label_string(_label_string), node(_node), max_connections(_max_connections){
            this->set_name("filter-node-sink");
            Glib::RefPtr<Gtk::StyleContext> context = this->get_style_context();
            Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
            this->set_size_request(15, 15);
            Glib::ustring style = Inkscape::IO::Resource::get_filename(Inkscape::IO::Resource::UIS, "node-editor.css");
            provider->load_from_path(style);
            // provider->load_from_path("/home/phantomzback/Documents/GSOC_Projs/inkscape_final/testing.css");
            context->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            this->add_css_class("nodesink");
        };
        
        FilterEditorNode* get_parent_node(){
            return node;
        };

        std::vector<FilterEditorConnection*> get_connections(){
            return connections;
        };

        bool can_add_connection(){
            return connections.size() < max_connections;
        };

        bool add_connection(FilterEditorConnection* connection){
            if(connections.size() < max_connections){
                connections.push_back(connection);
                return true;
            }
            return false;
        };

        bool get_selected();

        

    protected:
        // int source_id;
        // int node_id;
        FilterEditorNode* node;       
        int max_connections = 1;
        std::vector<FilterEditorConnection*> connections;
        std::string label_string; // The label corresponding to the string.

};
class CanvasFixed : public Gtk::Fixed {
public:
    CanvasFixed(std::vector<FilterEditorConnection*>& _connections, FilterEditorCanvas* _canvas, double _x_offset = 0, double _y_offset = 0);

    void update_positions(double x_offset_new, double y_offset_new);

    double get_x_offset();
    double get_y_offset();

    void update_offset(double x, double y);

protected:
    double x_offset, y_offset;
    FilterEditorCanvas* canvas; 
    std::vector<FilterEditorConnection*>& connections;
    virtual void snapshot_vfunc(const std::shared_ptr<Gtk::Snapshot>& cr) override;
};

class FilterEditorConnection{
    public:
        FilterEditorConnection(FilterEditorSource* _source, FilterEditorSink* _sink, FilterEditorCanvas* _canvas) : source(_source), sink(_sink), canvas(_canvas){
            source_node = source->get_parent_node();      
            sink_node = sink->get_parent_node();
        }

        void get_position(double& x1, double& y1, double& x2, double& y2);
        FilterEditorNode* get_source_node();
        FilterEditorNode* get_sink_node();
        FilterEditorSource* get_source();
        FilterEditorSink* get_sink(); 
                
    private:

        FilterEditorCanvas* canvas;
        FilterEditorNode* source_node;
        FilterEditorNode* sink_node;
        FilterEditorSource* source;
        FilterEditorSink* sink;
};

class FilterEditorNode : public Gtk::Box{
    public:
        Gtk::Box node;
        Gtk::Box source_dock, sink_dock;
        std::vector<FilterEditorSink*> sinks;
        FilterEditorSource source;

        FilterEditorNode(int node_id, int x, int y, SPFilterPrimitive* primitive, std::string label_text, int num_outputs = 1, int num_inputs = 1);

        bool get_selected();
        bool toggle_selection(bool selected = true);
        void get_position(double& x, double& y);
        void update_position(double x, double y);

        FilterEditorSink* get_next_available_sink();
        FilterEditorSource* get_source();
        
        SPFilterPrimitive* get_primitive();
        bool is_selected = false;


    protected:
        SPFilterPrimitive* primitive;
        int node_id;
        double x, y;
        std::vector<FilterEditorNode*> connected_nodes;
};

// class FilterEditorPrimitiveNode : public FilterEditorNode{
//     public:
//         FilterEditorPrimitiveNode(int node_id, int x, int y, std::string label_text, SPFilterPrimitive* primitive, int num_outputs = 1, int num_inputs):



// }


class FilterEditorCanvas : public Gtk::ScrolledWindow{
    public:
        friend class CanvasFixed;
        FilterEditorCanvas(int w, int h);
        // ~FilterEditorCanvas() override;

        NODE_TYPE *add_node(SPFilterPrimitive *primitive, double x_click, double y_click, std::string label_text, int num_sources = 1, int num_sinks = 1);
        FilterEditorConnection *create_connection(FilterEditorSource *source, FilterEditorSink *sink);
        FilterEditorConnection *create_connection(FilterEditorNode *source_node, FilterEditorNode *sink_node);

        bool destroy_connection(FilterEditorConnection *connection);

        double get_zoom_factor();
        void update_offsets(double x, double y);
        void update_positions();
        void auto_arrange_nodes(bool selection_only = false);
        void delete_nodes();
        void select_nodes(std::vector<NODE_TYPE> nodes);
        void select_node(NODE_TYPE node);
        enum class FilterEditorEvent{
            SELECT,
            PAN_START,
            PAN_UPDATE,
            PAN_END,
            MOVE_START,
            MOVE_UPDATE,
            MOVE_END,
            CONNECTION_START,
            CONNECTION_UPDATE,
            CONNECTION_END,
            RUBBERBAND_START,
            RUBBERBAND_UPDATE,
            RUBBERBAND_END,
            NONE
        };
        SPFilterPrimitive* get_selected_primitive();
        sigc::signal<void ()>& signal_primitive_changed();
        FilterEditorNode* get_node(SPFilterPrimitive* prim);

    protected:
        std::vector<FilterEditorConnection*> connections;
         
    private:
        std::map<SPFilterPrimitive*, FilterEditorNode*> primitive_to_node;

        FilterEditorEvent current_event_type;
        double zoom_fac;
        FilterEditorSource* starting_source;
        std::pair<std::pair<double, double>, std::pair<double, double>> drag_global_coordinates;


        /*Rubberband Selection*/
        Glib::RefPtr<Gtk::Box> rubberband_rectangle;


        sigc::signal<void ()> _signal_primitive_changed;
        
        Gtk::Widget* active_widget = nullptr;
        Gtk::Widget* get_widget_under(double xl, double yl);

        template <typename T>
        T* resolve_to_type(Gtk::Widget* widget);

        
        /*Selection Based*/
        bool toggle_node_selection(NODE_TYPE* widget);

        void set_node_selection(NODE_TYPE* widget, bool selected = true);

        void clear_selection();
        void rubberband_select();
        double rubberband_x, rubberband_y, rubberband_size_x, rubberband_size_y;
        double drag_start_x, drag_start_y;
        void event_handler(double x, double y);
        

        /*Modifier related*/
        Gdk::ModifierType modifier_state = ~Gdk::ModifierType::NO_MODIFIER_MASK;
        bool in_click = false;
        bool in_drag = false;
        double click_start_x, click_start_y;



        /*Controllers and methods for gestures*/
        
        Glib::RefPtr<Gtk::GestureClick> gesture_click;
        Glib::RefPtr<Gtk::GestureDrag> gesture_drag;
        Glib::RefPtr<Gtk::GestureClick> gesture_right_click;
        Glib::RefPtr<Gtk::EventControllerKey> key_controller;
        Glib::RefPtr<Gtk::EventControllerScroll> scroll_controller;

        
        
        void on_scroll(const Gtk::EventControllerScroll& scroll);
        

        void initialize_gestures();


        CanvasFixed canvas; 

        std::vector<std::shared_ptr<FilterEditorNode>> nodes;
        std::vector<NODE_TYPE*> selected_nodes;
        // std::vector<std::pair<NODE_TYPE*, NODE_TYPE*>> connections;
        
        NODE_TYPE* create_node(SPFilterPrimitive* primitive);
        void remove_node(int node_id);
        void connect_nodes(int node1, int node2);
        void disconnect_nodes(int node1, int node2);
        void set_node_position(int node_id, int x, int y);


        /*Geometry related*/
        void global_to_local(double xg, double yg, double& xl, double& yl);
        void local_to_global(double xl, double yl, double& xg, double& yg);
        void place_node(NODE_TYPE* node, double x,  double y, bool local = false);

        



};

    /*Overall class for the node editor canvas*/


class FilterEffectsDialog : public DialogBase
{
    using parent_type = DialogBase;

public:
    FilterEffectsDialog();
    ~FilterEffectsDialog() override;

    void set_attrs_locked(const bool);

private:
    void documentReplaced() override;
    void selectionChanged(Inkscape::Selection *selection) override;
    void selectionModified(Inkscape::Selection *selection, guint flags) override;

    sigc::scoped_connection _resource_changed;

    friend class FileOrElementChooser;

    class FilterModifier : public Gtk::Box
    {
    public:
        FilterModifier(FilterEffectsDialog& d, Glib::RefPtr<Gtk::Builder> builder);

        void update_filters();
        void update_selection(Selection *);

        SPFilter* get_selected_filter();
        void select_filter(const SPFilter*);
        void add_filter();
        bool is_selected_filter_active();
        void toggle_current_filter();
        bool filters_present() const;

        sigc::signal<void ()>& signal_filter_changed()
        {
            return _signal_filter_changed;
        }
        sigc::signal<void ()>& signal_filters_updated() {
            return _signal_filters_updated;
        }

    private:
        class Columns : public Gtk::TreeModel::ColumnRecord
        {
        public:
            Columns()
            {
                add(filter);
                add(label);
                add(sel);
                add(count);
            }

            Gtk::TreeModelColumn<SPFilter*> filter;
            Gtk::TreeModelColumn<Glib::ustring> label;
            Gtk::TreeModelColumn<int> sel;
            Gtk::TreeModelColumn<int> count;
        };

        std::unique_ptr<UI::Widget::PopoverMenu> create_menu();
        void on_filter_selection_changed();
        void on_name_edited(const Glib::ustring&, const Glib::ustring&);
        void on_selection_toggled(const Glib::ustring&);
        void selection_toggled(Gtk::TreeModel::iterator iter, bool toggle);

        void update_counts();
        Gtk::EventSequenceState filter_list_click_released(Gtk::GestureClick const &click,
                                                           int n_press, double x, double y);
        void remove_filter();
        void duplicate_filter();
        void rename_filter();
        void select_filter_elements();

        Glib::RefPtr<Gtk::Builder> _builder;
        FilterEffectsDialog& _dialog;
        Gtk::TreeView& _list;
        Glib::RefPtr<Gtk::ListStore> _filters_model;
        Columns _columns;
        Gtk::CellRendererToggle _cell_toggle;
        Gtk::Button& _add;
        Gtk::Button& _dup;
        Gtk::Button& _del;
        Gtk::Button& _select;
        std::unique_ptr<UI::Widget::PopoverMenu> _menu;
        sigc::signal<void ()> _signal_filter_changed;
        std::unique_ptr<Inkscape::XML::SignalObserver> _observer;
        sigc::signal<void ()> _signal_filters_updated;
    };

    class PrimitiveColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        PrimitiveColumns()
        {
            add(primitive);
            add(type_id);
            add(type);
            add(id);
        }

        Gtk::TreeModelColumn<SPFilterPrimitive*> primitive;
        Gtk::TreeModelColumn<Inkscape::Filters::FilterPrimitiveType> type_id;
        Gtk::TreeModelColumn<Glib::ustring> type;
        Gtk::TreeModelColumn<Glib::ustring> id;
    };

    class CellRendererConnection : public Gtk::CellRenderer
    {
    public:
        CellRendererConnection();
        Glib::PropertyProxy<void*> property_primitive();

        static constexpr int size_w = 16;
        static constexpr int size_h = 21;

    private:
        void get_preferred_width_vfunc(Gtk::Widget& widget,
                                       int& minimum_width,
                                       int& natural_width) const override;

        void get_preferred_width_for_height_vfunc(Gtk::Widget& widget,
                                                  int height,
                                                  int& minimum_width,
                                                  int& natural_width) const override;

        void get_preferred_height_vfunc(Gtk::Widget& widget,
                                        int& minimum_height,
                                        int& natural_height) const override;

        void get_preferred_height_for_width_vfunc(Gtk::Widget& widget,
                                                  int width,
                                                  int& minimum_height,
                                                  int& natural_height) const override;

        // void* should be SPFilterPrimitive*, some weirdness with properties prevents this
        Glib::Property<void*> _primitive;
    };

    class PrimitiveList
        : public UI::Widget::WidgetVfuncsClassInit
        , public Gtk::TreeView
    {
        using parent_type = Gtk::TreeView;

    public:
        PrimitiveList(FilterEffectsDialog&);

        sigc::signal<void ()>& signal_primitive_changed();

        void update();
        void set_menu(sigc::slot<void ()> dup, sigc::slot<void ()> rem);

        SPFilterPrimitive* get_selected();
        void select(SPFilterPrimitive *prim);
        void remove_selected();
        int primitive_count() const;
        int get_input_type_width() const;
        void set_inputs_count(int count);
        int get_inputs_count() const;

    private:
        void snapshot_vfunc(Glib::RefPtr<Gtk::Snapshot> const &snapshot) override;
        void css_changed(GtkCssStyleChange *change) override;

        void on_drag_end(Glib::RefPtr<Gdk::Drag> const &drag, bool delete_data);

        Gtk::EventSequenceState on_click_pressed (Gtk::GestureClick const &click,
                                                  int n_press, double x, double y);
        Gtk::EventSequenceState on_click_released(Gtk::GestureClick const &click,
                                                  int n_press, double x, double y);
        void on_motion_motion(double x, double y);

        void init_text();

        bool do_connection_node(const Gtk::TreeModel::iterator& row, const int input,
                                std::vector<Geom::Point> &points,
                                const int ix, const int iy);

        const Gtk::TreeModel::iterator find_result(const Gtk::TreeModel::iterator& start, const SPAttr attr, int& src_id, const int pos);
        int find_index(const Gtk::TreeModel::iterator& target);
        void draw_connection(const Cairo::RefPtr<Cairo::Context>& cr,
                             const Gtk::TreeModel::iterator&, const SPAttr attr, const int text_start_x,
                             const int x1, const int y1, const int row_count, const int pos,
                             const Gdk::RGBA fg_color, const Gdk::RGBA mid_color);
        void sanitize_connections(const Gtk::TreeModel::iterator& prim_iter);
        void on_primitive_selection_changed();
        bool on_scroll_timeout();

        FilterEffectsDialog& _dialog;
        Glib::RefPtr<Gtk::ListStore> _model;
        PrimitiveColumns _columns;
        CellRendererConnection _connection_cell;
        std::unique_ptr<UI::Widget::PopoverMenu> _primitive_menu;
        Glib::RefPtr<Pango::Layout> _vertical_layout;
        int _in_drag = 0;
        SPFilterPrimitive *_drag_prim = nullptr;
        sigc::signal<void ()> _signal_primitive_changed;
        sigc::connection _scroll_connection;
        int _autoscroll_y{};
        int _autoscroll_x{};
        std::unique_ptr<Inkscape::XML::SignalObserver> _observer;
        int _input_type_width {};
        int _input_type_height{};
        int _inputs_count{};
        Gdk::RGBA bg_color{};
    };

    void init_settings_widgets();

    // Handlers
    void add_primitive();
    void remove_primitive();
    void duplicate_primitive();
    void convolve_order_changed();
    void image_x_changed();
    void image_y_changed();
    void add_filter_primitive(Filters::FilterPrimitiveType type);

    void set_attr_direct(const UI::Widget::AttrWidget*);
    void set_child_attr_direct(const UI::Widget::AttrWidget*);
    void set_filternode_attr(const UI::Widget::AttrWidget*);
    void set_attr(SPObject*, const SPAttr, const gchar* val);
    void update_settings_view();
    void update_filter_general_settings_view();
    void update_settings_sensitivity();
    void update_color_matrix();
    void update_automatic_region(Gtk::CheckButton *btn);
    void add_effects(Inkscape::UI::Widget::CompletionPopup& popup, bool symbolic);

    Glib::RefPtr<Gtk::Builder> _builder;
    UI::Widget::Bin _bin;
    UI::Widget::PopoverBin _popoverbin;
    Gtk::Paned& _paned;
    Gtk::Grid& _main_grid;
    Gtk::Box& _params_box;
    Gtk::Box& _search_box;
    Gtk::Box& _search_wide_box;
    Gtk::ScrolledWindow& _filter_wnd;
    FilterEditorCanvas _filter_canvas;
    bool _narrow_dialog = true;
    Gtk::ToggleButton *_show_sources = nullptr;
    Gtk::CheckButton& _cur_filter_btn;
    sigc::connection _cur_filter_toggle;
    // View/add primitives
    Gtk::ScrolledWindow* _primitive_box;

    UI::Widget::ComboBoxEnum<Inkscape::Filters::FilterPrimitiveType> _add_primitive_type;
    Gtk::Button _add_primitive;

    // Bottom pane (filter effect primitive settings)
    Gtk::Box _settings_filter;
    Gtk::Box _settings_effect;
    Gtk::Label _empty_settings;
    Gtk::Label _no_filter_selected;
    Gtk::Label* _cur_effect_name;
    bool _settings_initialized;

    class Settings;
    class MatrixAttr;
    class ColorMatrixValues;
    class ComponentTransferValues;
    class LightSourceControl;

    std::unique_ptr<Settings> _settings;
    std::unique_ptr<Settings> _filter_general_settings;

    // General settings
    MultiSpinButton *_region_pos, *_region_size;

    // Color Matrix
    ColorMatrixValues* _color_matrix_values;

    // Component Transfer
    ComponentTransferValues* _component_transfer_values;

    // Convolve Matrix
    MatrixAttr* _convolve_matrix;
    DualSpinButton* _convolve_order;
    MultiSpinButton* _convolve_target;

    // Image
    EntryAttr* _image_x;
    EntryAttr* _image_y;

    // For controlling setting sensitivity
    Gtk::Widget* _k1, *_k2, *_k3, *_k4;

    // To prevent unwanted signals
    bool _locked;
    bool _attr_lock;

    // These go last since they depend on the prior initialization of
    // other FilterEffectsDialog members
    FilterModifier _filter_modifier;
    PrimitiveList _primitive_list;
    Inkscape::UI::Widget::CompletionPopup _effects_popup;
};

} // namespace Dialog

} // namespace Inkscape::UI

#endif // INKSCAPE_UI_DIALOG_FILTER_EFFECTS_H

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


