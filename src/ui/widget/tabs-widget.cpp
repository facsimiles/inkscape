// SPDX-License-Identifier: GPL-2.0-or-later
#include "tabs-widget.h"

#include <cassert>
#include <glibmm/i18n.h>
#include <giomm/menu.h>
#include <gtkmm/dragicon.h>
#include <gtkmm/dragsource.h>
#include <gtkmm/droptarget.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/label.h>
#include <gtkmm/picture.h>
#include <gtkmm/popovermenu.h>
#include <gtkmm/snapshot.h>
#include <gtkmm/tooltip.h>

#include "desktop.h"
#include "document.h"
#include "inkscape-application.h"
#include "ui/builder-utils.h"
#include "ui/popup-menu.h"
#include "ui/util.h"
#include "ui/widget/canvas.h"
#include "ui/widget/desktop-widget.h"
#include "util/value-utils.h"

#define BUILD(name) name{UI::get_widget<std::remove_reference_t<decltype(name)>>(builder, #name)}

using namespace Inkscape::Util;

namespace Inkscape::UI::Widget {
namespace {

struct TooltipUI
{
    TooltipUI()
        : TooltipUI{create_builder("document-tab-preview.ui")}
    {}

    TooltipUI(Glib::RefPtr<Gtk::Builder> const &builder)
        : BUILD(root)
        , BUILD(name)
        , BUILD(preview)
    {
        root.reference();

        // Clear preview on dismissal to save memory.
        root.signal_unmap().connect([this] {
            preview.set_paintable({});
            current_display_info = {};
        });
    }

    ~TooltipUI()
    {
        root.unreference();
    }

    Gtk::Box &root;
    Gtk::Label &name;
    Gtk::Picture &preview;

    struct CurrentDisplayInfo
    {
        SPDesktop *desktop;
        bool is_active_tab;
        auto operator<=>(CurrentDisplayInfo const &) const = default;
    };
    std::optional<CurrentDisplayInfo> current_display_info;
};

Gtk::PopoverMenu create_context_menu()
{
    auto menu = Gio::Menu::create();
    auto sec1 = Gio::Menu::create();
    sec1->append_item(Gio::MenuItem::create(_("Detach tab"), "tabs.detach"));
    sec1->append_item(Gio::MenuItem::create(_("Duplicate tab"), "tabs.duplicate"));
    menu->append_section(sec1);
    auto sec2 = Gio::Menu::create();
    sec2->append_item(Gio::MenuItem::create(_("Close tab"), "tabs.close"));
    menu->append_section(sec2);

    auto popovermenu = Gtk::PopoverMenu{menu};
    popovermenu.set_has_arrow(false);
    popovermenu.set_position(Gtk::PositionType::BOTTOM);

    return popovermenu;
}

Gtk::PopoverMenu &get_context_menu()
{
    static auto menu = create_context_menu();
    return menu;
}

struct TabDnD
{
    std::weak_ptr<Tab> tab;
    Geom::Point offset;
    int width;
};

// Attempt to extract a TabDnD from the content provider of a drag.
std::optional<TabDnD> get_tabdnd(Gdk::Drag &drag)
{
    Glib::ValueBase value;
    value.init(GlibValue::type<TabDnD>());
    try {
        drag.get_content()->get_value(value);
    } catch (Glib::Error const &) {
        return {};
    }
    return *GlibValue::get<TabDnD>(value);
}

struct DumbTab : Gtk::Box
{
    Gtk::Label &name;
    Gtk::Button &close;

    DumbTab()
        : DumbTab{create_builder("document-tab.ui")}
    {}

    DumbTab(Glib::RefPtr<Gtk::Builder> const &builder)
        : BUILD(name)
        , BUILD(close)
    {
        append(get_widget<Gtk::Box>(builder, "root"));
    }

    void set_active()
    {
        get_style_context()->add_class("tab_active");
    }

    void set_inactive()
    {
        get_style_context()->remove_class("tab_active");
    }
};

// Necessary since GTKmm vfunc is protected.
void snapshot_widget(Gtk::Widget &widget, Gtk::Snapshot &snapshot)
{
    auto widget_c = widget.gobj();
    GTK_WIDGET_GET_CLASS(widget_c)->snapshot(widget_c, snapshot.gobj());
}

// Only needed for a workaround; see below.
std::optional<Geom::Point> get_current_pointer_pos(Glib::RefPtr<Gdk::Device> const &pointer, Gtk::Widget &widget)
{
    double x, y;
    Gdk::ModifierType mask;
    auto root = widget.get_root();
    dynamic_cast<Gtk::Native &>(*root).get_surface()->get_device_position(pointer, x, y, mask);
    dynamic_cast<Gtk::Widget &>(*root).translate_coordinates(widget, x, y, x, y);
    return {{x, y}};
}

} // namespace

struct Tab : DumbTab
{
    SPDesktop *const desktop;
    TabsWidget *const parent;

    Tab(SPDesktop *desktop, TabsWidget *parent)
        : desktop{desktop}
        , parent{parent}
    {
        set_name("DocumentTab");
        set_has_tooltip(true);
    }
};

// This is necessary to ensure that TabsWidget::_tabs remains the *unique* owner of tabs.
// We want tabs to be deleted when it removes them - no zombies, please!
SPDesktop *consume_locked_tab_return_desktop(std::shared_ptr<Tab> &&tab)
{
    return tab ? tab->desktop : nullptr;
}

TabsWidget::TabsWidget(SPDesktopWidget *desktop_widget)
    : _desktop_widget{desktop_widget}
{
    set_name("DocumentTabsWidget");
    set_visible(false);

    auto click = Gtk::GestureClick::create();
    click->set_button(0);
    click->signal_pressed().connect([this, click = click.get()] (int, double x, double y) {
        // Find clicked tab.
        auto const [tab_weak, tab_pos] = _tabAtPoint({x, y});
        auto tab = tab_weak.lock();
        if (!tab) {
            return;
        }

        // Handle button actions.
        switch (click->get_current_button()) {
            case GDK_BUTTON_PRIMARY:
                double xc, yc;
                translate_coordinates(tab->close, x, y, xc, yc);
                if (!tab->close.contains(xc, yc)) {
                    _desktop_widget->switchDesktop(tab->desktop);
                }
                break;
            case GDK_BUTTON_SECONDARY: {
                auto &menu = get_context_menu();
                if (menu.get_parent()) {
                    menu.unparent();
                }
                menu.set_parent(*tab);
                UI::popup_at(menu, *tab, tab_pos);
                _right_clicked = tab_weak;
                break;
            }
            case GDK_BUTTON_MIDDLE:
                InkscapeApplication::instance()->destroy_window(consume_locked_tab_return_desktop(std::move(tab)));
                break;
            default:
                break;
        }
    });
    add_controller(click);

    auto dragsource = Gtk::DragSource::create();
    dragsource->signal_prepare().connect([this, dragsource = dragsource.get()] (double x, double y) -> Glib::RefPtr<Gdk::ContentProvider> {
        // Find tab where drag started.
        auto [tab_weak, tab_pos] = _tabAtPoint({x, y});
        auto const tab = tab_weak.lock();
        if (!tab) {
            return {};
        }

        // Store geometry and tab information in the content provider.
        auto const tabdnd = TabDnD{
            .tab = tab_weak,
            .offset = tab_pos,
            .width = tab->get_width()
        };

        return Gdk::ContentProvider::create(GlibValue::create<TabDnD>(std::move(tabdnd)));
    }, false);
    dragsource->signal_drag_begin().connect([this, dragsource = dragsource.get()] (auto const &drag) {
        auto const tabdnd = get_tabdnd(*drag);
        if (!tabdnd) {
            return;
        }

        auto const tab = tabdnd->tab.lock();
        if (!tab) {
            return;
        }

        // Set a replica of the tab as the visual dragged object.
        auto tabw = Gtk::make_managed<DumbTab>();
        tabw->name.set_text(_getTitle(tab->desktop));
        tabw->set_active();
        drag->set_hotspot(tabdnd->offset.x(), tabdnd->offset.y());
        auto dragicon = Gtk::DragIcon::get_for_drag(drag);
        dragicon->set_child(*tabw);

        // Hide the real tab while it's being dragged.
        tab->set_visible(false);

        // Adjust the layout. Wouldn't be necessary if GtkDropController worked perfectly.
        auto const [mx, my] = get_current_pointer_pos(dragsource->get_current_event_device(), *tab).value_or(tabdnd->offset);
        if (contains(mx, my)) {
            int const tab_x = mx - static_cast<int>(std::round(tabdnd->offset.x()));
            int const i = _computeDropLocation(tab_x);
            _adjustLayoutForDropLocation(i, tabdnd->width);
        }

        // Handle drag cancellation.
        drag->signal_cancel().connect([this, tab_weak = std::weak_ptr<Tab>(tab), drag = drag.get()] (auto reason) {
            auto tab = tab_weak.lock();
            if (!tab) {
                return;
            }

            // Check if tab was dropped on nothing, in which case we should detach it.
            if (reason == Gdk::DragCancelReason::NO_TARGET) {
                drag->drag_drop_done(true); // suppress drag-failed animation
            }

            // Reset widget modifications.
            tab->set_visible(true);
            _resetLayout();

            // Detach tab.
            auto const desktop = consume_locked_tab_return_desktop(std::move(tab));
            InkscapeApplication::instance()->detachTabToNewWindow(desktop);
        }, false);
    });
    add_controller(dragsource);

    auto droptarget = Gtk::DropTarget::create(GlibValue::type<TabDnD>(), Gdk::DragAction::COPY);
    auto handler = [this, droptarget = droptarget.get()] (double x, double y) -> Gdk::DragAction {
        // Determine whether an in-app tab is being dragged and get information about it.
        auto const drag = droptarget->get_drop()->get_drag();
        if (!drag) {
            return {}; // not in-app
        }
        auto const tabdnd = get_tabdnd(*drag);
        if (!tabdnd) {
            return {}; // not a tab
        }

        // Compute x coordinate of tab.
        int const tab_x = x - static_cast<int>(std::round(tabdnd->offset.x()));

        // Compute index of drop location.
        int const i = _computeDropLocation(tab_x);

        // Adjust tab positions to simulate drop.
        _adjustLayoutForDropLocation(i, tabdnd->width);

        return Gdk::DragAction::COPY;
    };
    droptarget->signal_enter().connect(handler, false);
    droptarget->signal_motion().connect(handler, false);
    droptarget->signal_leave().connect([this] {
        _resetLayout();
    });
    droptarget->signal_drop().connect([this] (Glib::ValueBase const &value, double x, double y) {
        auto const tabdnd = GlibValue::get<TabDnD>(value);
        if (!tabdnd) {
            return false; // not a tab
        }
        auto tab = tabdnd->tab.lock();
        if (!tab) {
            return false; // tab deleted while dragging
        }

        // Compute x coordinate of tab.
        int const tab_x = x - static_cast<int>(std::round(tabdnd->offset.x()));

        // Compute index of drop location.
        int const i = _computeDropLocation(tab_x);

        // Reset widget modifications.
        tab->set_visible(true);
        _resetLayout(); // reset destination
        if (tab->parent != this) {
            tab->parent->_resetLayout(); // reset source
        }

        // Reorder or migrate tab.
        if (tab->parent == this) { // Reorder.
            int const from = positionOfTab(tab->desktop);
            int const to = i - (i > from);
            _reorderTab(from, to);
        } else { // Migrate.
            auto const desktop = consume_locked_tab_return_desktop(std::move(tab));
            desktop->getDesktopWidget()->removeDesktop(desktop);
            _desktop_widget->addDesktop(desktop, i);
        }

        return true;
    }, false);
    add_controller(droptarget);

    auto actiongroup = Gio::SimpleActionGroup::create();
    actiongroup->add_action("detach", [this] {
        if (auto desktop = consume_locked_tab_return_desktop(_right_clicked.lock())) {
            InkscapeApplication::instance()->detachTabToNewWindow(desktop);
        }
    });
    actiongroup->add_action("duplicate", [this] {
        if (auto desktop = consume_locked_tab_return_desktop(_right_clicked.lock())) {
            // Fixme: After current tab.
            InkscapeApplication::instance()->window_open(desktop->getDocument());
        }
    });
    actiongroup->add_action("close", [this] {
        if (auto desktop = consume_locked_tab_return_desktop(_right_clicked.lock())) {
            InkscapeApplication::instance()->destroy_window(desktop);
        }
    });
    insert_action_group("tabs", actiongroup);
}

TabsWidget::~TabsWidget() = default;

void TabsWidget::addTab(SPDesktop *desktop, int pos)
{
    auto tab = std::make_shared<Tab>(desktop, this);
    tab->name.set_text(_getTitle(desktop));

    tab->close.signal_clicked().connect([this, desktop] { InkscapeApplication::instance()->destroy_window(desktop); });

    tab->signal_query_tooltip().connect([this, desktop] (int, int, bool, Glib::RefPtr<Gtk::Tooltip> const &tooltip) {
        _setTooltip(desktop, tooltip);
        return true; // show the tooltip
    }, true);

    assert(positionOfTab(desktop) == -1);

    if (pos == -1) {
        pos = _tabs.size();
    }
    assert(0 <= pos && pos <= _tabs.size());

    if (pos == 0) {
        prepend(*tab);
    } else {
        insert_child_after(*tab, *_tabs[pos - 1]);
    }
    _tabs.insert(_tabs.begin() + pos, std::move(tab));

    _updateVisibility();
}

void TabsWidget::removeTab(SPDesktop *desktop)
{
    int const i = positionOfTab(desktop);
    assert(i != -1);

    remove(*_tabs[i]);
    _tabs.erase(_tabs.begin() + i);

    _updateVisibility();
}

void TabsWidget::switchTab(SPDesktop *desktop)
{
    auto const active = _active.lock();

    if (active && active->desktop == desktop) {
        return;
    }

    if (active) {
        active->set_inactive();
        _active = {};
    }

    int const i = positionOfTab(desktop);
    if (i != -1) {
        _tabs[i]->set_active();
        _active = _tabs[i];
    }
}

void TabsWidget::refreshTitle(SPDesktop *desktop)
{
    int const i = positionOfTab(desktop);
    assert(i != -1);
    _tabs[i]->name.set_text(_getTitle(_tabs[i]->desktop));
}

void TabsWidget::_reorderTab(int from, int to)
{
    assert(0 <= from && from < _tabs.size());
    assert(0 <= to   && to   < _tabs.size());

    if (from == to) {
        return;
    }

    if (to == 0) {
        reorder_child_at_start(*_tabs[from]);
    } else {
        reorder_child_after(*_tabs[from], *_tabs[to - (from > to)]);
    }

    auto tab = std::move(_tabs[from]);
    _tabs.erase(_tabs.begin() + from);
    _tabs.insert(_tabs.begin() + to, std::move(tab));
}

int TabsWidget::positionOfTab(SPDesktop *desktop) const
{
    for (int i = 0; i < _tabs.size(); i++) {
        if (_tabs[i]->desktop == desktop) {
            return i;
        }
    }
    return -1;
}

SPDesktop *TabsWidget::tabAtPosition(int i) const
{
    return _tabs[i]->desktop;
}

void TabsWidget::_updateVisibility()
{
    set_visible(_tabs.size() > 1);
}

Glib::ustring TabsWidget::_getTitle(SPDesktop *desktop) const
{
    auto doc = desktop->doc();
    return Glib::ustring{doc->isModifiedSinceSave() ? "*" : ""} + doc->getDocumentName();
}

void TabsWidget::_setTooltip(SPDesktop *desktop, Glib::RefPtr<Gtk::Tooltip> const &tooltip)
{
    // Lazy-load tooltip ui file, shared among all instances.
    static auto const tooltip_ui = std::make_unique<TooltipUI>();

    auto const active = _active.lock();
    bool const is_active_tab = active && desktop == active->desktop;
    auto const display_info = TooltipUI::CurrentDisplayInfo{
        .desktop = desktop,
        .is_active_tab = is_active_tab
    };

    if (tooltip_ui->current_display_info != display_info) { // avoid pointless updates
        tooltip_ui->current_display_info = display_info;

        // Set name.
        tooltip_ui->name.set_label(desktop->doc()->getDocumentName());

        // Set preview.
        if (is_active_tab) {
            tooltip_ui->preview.set_paintable({}); // no preview for active tab
        } else {
            constexpr double scale = 0.2;
            auto snapshot = Gtk::Snapshot::create();
            snapshot->scale(scale, scale);
            snapshot_widget(*desktop->getCanvas(), *snapshot);
            tooltip_ui->preview.set_paintable(snapshot->to_paintable());
        }
    }

    // Embed custom widget into tooltip.
    tooltip->set_custom(tooltip_ui->root);
}

int TabsWidget::_computeDropLocation(int tab_x) const
{
    int x = 0;
    int i = 0;
    for (auto const &tab : _tabs) {
        if (tab->get_visible()) {
            int const tab_x_rel = tab_x - x;
            int const w = tab->get_width();
            if (tab_x_rel < w) {
                return i + (tab_x_rel > w / 2);
            }
            x += w;
        }
        i++;
    }

    return _tabs.size();
}

void TabsWidget::_resetLayout()
{
    for (auto const &tab : _tabs) {
        tab->set_margin_start(0);
    }
}

void TabsWidget::_adjustLayoutForDropLocation(int i, int width)
{
    _resetLayout();
    for (int j = i; j < _tabs.size(); j++) {
        if (_tabs[j]->get_visible()) {
            _tabs[j]->set_margin_start(width);
            break;
        }
    }
}

std::pair<std::weak_ptr<Tab>, Geom::Point> TabsWidget::_tabAtPoint(Geom::Point const &pos)
{
    double xt, yt;
    auto const it = std::find_if(_tabs.begin(), _tabs.end(), [&] (auto const &tab) {
        translate_coordinates(*tab, pos.x(), pos.y(), xt, yt);
        return tab->contains(xt, yt);
    });
    if (it == _tabs.end()) {
        return {{}, {}};
    }
    return {*it, {xt, yt}};
}

} // namespace Inkscape::UI::Widget
