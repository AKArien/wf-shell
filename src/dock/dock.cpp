#include <gtkmm/window.h>
#include <gdkmm/frameclock.h>
#include <glibmm/main.h>
#include <gdk/wayland/gdkwayland.h>

#include <iostream>
#include <map>

#include <gtk-utils.hpp>
#include <wf-shell-app.hpp>
#include <gtk4-layer-shell.h>
#include <wf-autohide-window.hpp>

#include "dock.hpp"
#include "../util/gtk-utils.hpp"
#include <css-config.hpp>


class WfDock::impl
{
    WayfireOutput *output;
    std::unique_ptr<WayfireAutohidingWindow> window;
    wl_surface *_wl_surface;
    Gtk::Box out_box;
    // flowbox doesn’t really cut it unfortunately. can’t center the inner widgets and can’t complete the down/right row first
    // listbox neither, since it can’t even be oriented

    WfOption<std::string> css_path{"dock/css_path"};
    WfOption<int> dock_width{"dock/dock_width"};
    WfOption<int> dock_height{"dock/dock_height"};

	WfOption<std::string> position{"dock/position"};
	WfOption<int> entries_per_line{"dock/max_per_line"};

	WfOption<std::string> orientation{"dock/orientation"};

	void (Gtk::Box::*ap_or_pre_pend)(Gtk::Widget&); // pointer to Gtk::Box::prepend or Gtk::Box::append, updated by update_layout
	Gtk::Widget* (Gtk::Widget::*first_or_last_child)(); // same, for get_first_child and get_last_child
	bool reverse_iteration;


  public:
    impl(WayfireOutput *output)
    {
        this->output = output;
        window = std::unique_ptr<WayfireAutohidingWindow>(
            new WayfireAutohidingWindow(output, "dock"));
        gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_TOP);
        out_box.get_style_context()->add_class("out-box");

		gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_TOP, 0);
		gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, 0);
		gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, 0);
		gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, 0);

        window->set_child(out_box);
        window->set_default_size(dock_width, dock_height);
        update_layout();

        window->get_style_context()->add_class("wf-dock");

        out_box.set_halign(Gtk::Align::CENTER);

        if ((std::string)css_path != "")
        {
            auto css = load_css_from_path(css_path);
            if (css)
            {
                auto display = Gdk::Display::get_default();
                Gtk::StyleContext::add_provider_for_display(display, css, GTK_STYLE_PROVIDER_PRIORITY_USER);
            }
        }

		// this is temporary for testing purposes, should be placed somewhere else that can be seen by all the stuff that needs to have it
        auto orientation_css = R"(
			.left {
				transform: rotate(270deg);
			}
			.right {
				transform: rotate(90deg);
			}

		)";
		auto orientation_provider = Gtk::CssProvider::create();
		orientation_provider->load_from_data(orientation_css);
        Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(), orientation_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);


        window->present();
        new CssFromConfigInt("dock/icon_height", ".toplevel-icon {-gtk-icon-size:", "px;}");
        _wl_surface = gdk_wayland_surface_get_wl_surface(
            window->get_surface()->gobj());
    }

    void prepare_new_layer(Gtk::Box& box){
        box.get_style_context()->add_class("box");
        box.set_homogeneous(true);

		if ((std::string)position == "left" or (std::string)position == "right"){
			box.set_orientation(Gtk::Orientation::VERTICAL);
		}
		else {
			box.set_orientation(Gtk::Orientation::HORIZONTAL);
		}

        box.add_tick_callback([=] (Glib::RefPtr<Gdk::FrameClock> fc)
        {
            set_clickable_region();
            return true;
        });
    }

	void apply_orientation(Gtk::Widget& widget){
		if (widget.get_style_context()->has_class("custom")){
			widget.get_style_context()->remove_class("custom");
		}
		else if (widget.get_style_context()->has_class("left")){
			widget.get_style_context()->remove_class("left");
		}
		else if (widget.get_style_context()->has_class("right")){
			widget.get_style_context()->remove_class("right");
		}

		if ((std::string)orientation == "custom"){
			widget.get_style_context()->add_class("custom-orientation");
		}
		else if ((std::string)orientation == "left" || ((std::string)orientation == "match" && (std::string)position == "right")){
			widget.get_style_context()->add_class("left");
		}
		else if ((std::string)orientation == "right" || ((std::string)orientation == "match" && (std::string)position == "left")){
			widget.get_style_context()->add_class("right");
		}
	}

	void update_layout(){

		Gtk::Orientation orientation;
		if ((std::string)position == "left" or (std::string)position == "right"){
			orientation = Gtk::Orientation::VERTICAL;
			out_box.set_orientation(Gtk::Orientation::HORIZONTAL);
		}
		else {
			orientation = Gtk::Orientation::HORIZONTAL;
			out_box.set_orientation(Gtk::Orientation::VERTICAL);
		}

		if ((std::string)position == "bottom" or (std::string)position == "right"){
			ap_or_pre_pend = &Gtk::Box::prepend;
			first_or_last_child = &Gtk::Widget::get_first_child;
			reverse_iteration = true;
		}
		else {
			ap_or_pre_pend = &Gtk::Box::append;
			first_or_last_child = &Gtk::Widget::get_last_child;
			reverse_iteration = false;
		}

		for (auto layer : out_box.get_children()){
			((Gtk::Box*)layer)->set_orientation(orientation);

			for (auto child : layer->get_children()){
				apply_orientation(*child);
			}
		}
    }

    void add_child(Gtk::Widget& widget)
    {
		if ((int)(out_box.get_children().size()) == 0 || (int)((out_box.*first_or_last_child)()->get_children().size()) == entries_per_line){ // create a new box if the last one is full, or there is none
		    Gtk::Box new_box;
		    prepare_new_layer(new_box);
			(out_box.*ap_or_pre_pend)(new_box);
		}

		Gtk::Box& last_child = *(Gtk::Box*)(out_box.*first_or_last_child)();

		widget.set_halign(Gtk::Align::CENTER);
		widget.set_valign(Gtk::Align::CENTER);
		widget.get_style_context()->add_class("re-orient");

 	    last_child.append(widget);
    }

    void rem_child(Gtk::Widget& widget)
    {
	    Gtk::Box* prev_row = nullptr;
	    bool found = false;

		auto check_row = [&](Gtk::Widget* row){
			if (!found){
				for (Gtk::Widget* item : row->get_children()){
				    if (&widget == item){
				        found = true;
				        ((Gtk::Box*)row)->remove(*item);
				        break;
				    }
				}
				if (!found){
					return;
				}
			}

		    // move the first widget of every next line and append it to the previous line
			if (prev_row != nullptr){
				Gtk::Widget* to_move = ((Gtk::Box*)row)->get_last_child();
		        prev_row->append(*to_move);
				((Gtk::Box*)row)->remove(*to_move);
			}

			prev_row = ((Gtk::Box*)row);

		};
	    
	    auto children = out_box.get_children();

		if (reverse_iteration){
			for (auto it = children.rbegin(); it != children.rend(); ++it){
				check_row(*it);
			}
		}
		else{
			for (auto it = children.begin(); it != children.end(); ++it){
				check_row(*it);
			}
		}

	    // for (auto row : out_box.get_children()){ // find the widget
	    // }

		// if we emptied a row, delete it
        if ((out_box.*first_or_last_child)()->get_children().size() == 0){
	        out_box.remove(*(out_box.*first_or_last_child)());
        }
    }

    wl_surface *get_wl_surface()
    {
        return this->_wl_surface;
    }

    /* Sets the central section as clickable and transparent edges as click-through
     *  Gets called regularly to ensure css size changes all register */
    void set_clickable_region()
    {
        auto surface = window->get_surface();
        auto widget_bounds = out_box.compute_bounds(*window);

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

void WfDock::handle_config_reload(){
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
