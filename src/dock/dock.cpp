#include <gtkmm/window.h>
#include <gdkmm/frameclock.h>
#include <glibmm/main.h>
#include <gdk/wayland/gdkwayland.h>
#include <glibmm/refptr.h>
#include <gtkmm/constraint.h>
#include <gtkmm/constraintlayout.h>
#include <gtk4-layer-shell.h>
#include <gtkmm/enums.h>

#include "dock.hpp"
#include "wf-shell-app.hpp"
#include "wf-autohide-window.hpp"
#include "../util/gtk-utils.hpp"

class WfDock::impl
{
    WayfireOutput *output;
    std::unique_ptr<WayfireAutohidingWindow> window;
    wl_surface *_wl_surface;
    Gtk::Box out_box;
    Gtk::Box box;
    // std::map<Gtk::Widget&, Glib::RefPtr<Gtk::Constraint>> widget_to_constraints;
    Glib::RefPtr<Gtk::ConstraintLayout> layout;
    Gtk::Constraint::Attribute new_row_targ_attr, new_row_source_attr, new_itm_targ_attr, new_itm_source_attr;

    WfOption<std::string> css_path{"dock/css_path"};
    WfOption<int> dock_width{"dock/dock_width"};
    WfOption<int> dock_height{"dock/dock_height"};

    WfOption<std::string> position{"dock/position"};
    WfOption<int> entries_per_line{"dock/max_per_line"};

  public:
    impl(WayfireOutput *output)
    {
        this->output = output;
        window = std::unique_ptr<WayfireAutohidingWindow>(
            new WayfireAutohidingWindow(output, "dock"));
        window->set_auto_exclusive_zone(false);
        gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_TOP);

        gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_TOP, 0);
        gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, 0);
        gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, 0);
        gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, 0);

        layout = Gtk::ConstraintLayout::create();
        box.set_layout_manager(layout);

        out_box.append(box);
        out_box.get_style_context()->add_class("out-box");
        box.add_css_class("box");

        window->set_child(out_box);
        update_layout();

        window->add_css_class("wf-dock");

        out_box.set_halign(Gtk::Align::CENTER);

        if (css_path.value() != "")
        {
            auto css = load_css_from_path(css_path);
            if (css)
            {
                auto display = Gdk::Display::get_default();
                Gtk::StyleContext::add_provider_for_display(display, css, GTK_STYLE_PROVIDER_PRIORITY_USER);
            }
        }

        window->present();
        _wl_surface = gdk_wayland_surface_get_wl_surface(
            window->get_surface()->gobj());
    }

    void update_layout(){
        window->set_default_size(dock_width, dock_height);

        if (position.value() == "top")
        {
            new_row_targ_attr = Gtk::Constraint::Attribute::TOP;
            new_row_source_attr = Gtk::Constraint::Attribute::BOTTOM;
            new_itm_targ_attr = Gtk::Constraint::Attribute::LEFT;
            new_itm_source_attr = Gtk::Constraint::Attribute::RIGHT;
        }
        else if (position.value() == "bottom")
        {
            new_row_targ_attr = Gtk::Constraint::Attribute::BOTTOM;
            new_row_source_attr = Gtk::Constraint::Attribute::TOP;
            new_itm_targ_attr = Gtk::Constraint::Attribute::LEFT;
            new_itm_source_attr = Gtk::Constraint::Attribute::RIGHT;
        }
        else if (position.value() == "left")
        {
            new_row_targ_attr = Gtk::Constraint::Attribute::LEFT;
            new_row_source_attr = Gtk::Constraint::Attribute::RIGHT;
            new_itm_targ_attr = Gtk::Constraint::Attribute::TOP;
            new_itm_source_attr = Gtk::Constraint::Attribute::BOTTOM;
        }
        else if (position.value() == "right")
        {
            new_row_targ_attr = Gtk::Constraint::Attribute::RIGHT;
            new_row_source_attr = Gtk::Constraint::Attribute::LEFT;
            new_itm_targ_attr = Gtk::Constraint::Attribute::TOP;
            new_itm_source_attr = Gtk::Constraint::Attribute::BOTTOM;
        }
    }

    void add_child(Gtk::Widget& widget)
    {
        if (box.get_children().size() == 0)
        {
            box.append(widget);
            return;
        }
        if (box.get_children().size() % entries_per_line == 0){
            auto constraint = Gtk::Constraint::create(
                widget.make_refptr_constrainttarget(),
                new_row_targ_attr,
                Gtk::Constraint::Relation::EQ,
                (box.get_children().at(box.get_children().size() - entries_per_line))->make_refptr_constrainttarget(),
                new_row_source_attr,
                1.0,
                0.0,
                1
            );
            layout->add_constraint(constraint);
        } else
        {
            auto constraint = Gtk::Constraint::create(
                widget.make_refptr_constrainttarget(),
                new_itm_targ_attr,
                Gtk::Constraint::Relation::EQ,
                (box.get_children().at(box.get_children().size()-1))->make_refptr_constrainttarget(),
                new_itm_source_attr,
                1.0,
                0.0,
                1
            );
            layout->add_constraint(constraint);
        }
        box.append(widget);
    }

    void rem_child(Gtk::Widget& widget)
    {
        this->box.remove(widget);
    }

    wl_surface *get_wl_surface()
    {
        return this->_wl_surface;
    }

    /* Sets the central section as clickable and transparent edges as click-through
     * Gets called regularly to ensure css size changes all register */
    void set_clickable_region()
    {
        auto surface = window->get_surface();
        auto widget_bounds = box.compute_bounds(*window);

        auto rect = Cairo::RectangleInt{
            (int)widget_bounds->get_x(),
            (int)widget_bounds->get_y(),
            (int)widget_bounds->get_width(),
            (int)widget_bounds->get_height()
        };

        auto region = Cairo::Region::create(rect);

        surface->set_input_region(region);
    }
};

WfDock::WfDock(WayfireOutput *output) :
    pimpl(new impl(output))
{}
WfDock::~WfDock() = default;

void WfDock::handle_config_reload()
{
    pimpl->update_layout();
}

void WfDock::add_child(Gtk::Widget& w)
{
    return pimpl->add_child(w);
}

void WfDock::rem_child(Gtk::Widget& w)
{
    return pimpl->rem_child(w);
}

wl_surface*WfDock::get_wl_surface()
{
    return pimpl->get_wl_surface();
}
