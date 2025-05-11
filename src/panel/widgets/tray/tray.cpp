#include "tray.hpp"

void WayfireStatusNotifier::init(Gtk::Box *container)
{
	update_layout();
    icons_hbox.get_style_context()->add_class("tray");
    icons_hbox.set_spacing(5);
    container->append(icons_hbox);
}

void WayfireStatusNotifier::add_item(const Glib::ustring & service)
{
    if (items.count(service) != 0)
    {
        return;
    }

    items.emplace(service, service);
    icons_hbox.append(items.at(service));
}

void WayfireStatusNotifier::remove_item(const Glib::ustring & service)
{
    if (items.count(service) == 0)
    {
        return;
    }

    icons_hbox.remove(items.at(service));
    items.erase(service);
}

void WayfireStatusNotifier::update_layout(){
       std::string panel_position = WfOption<std::string> {"panel/position"};

       if (panel_position == PANEL_POSITION_LEFT or panel_position == PANEL_POSITION_RIGHT){
               icons_hbox.set_orientation(Gtk::Orientation::VERTICAL);
       }
       else {
               icons_hbox.set_orientation(Gtk::Orientation::HORIZONTAL);
       }
}

void WayfireStatusNotifier::handle_config_reload(){
       update_layout();
}
