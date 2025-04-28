#include "tray.hpp"

void WayfireStatusNotifier::init(Gtk::HBox *container)
{
    icons_hbox.get_style_context()->add_class("tray");
    container->add(icons_hbox);
}

void WayfireStatusNotifier::add_item(const Glib::ustring & service)
{
    if (items.count(service) != 0)
    {
        return;
    }

    items.emplace(service, service);
    icons_hbox.pack_start(items.at(service));
    icons_hbox.show_all();
}

void WayfireStatusNotifier::remove_item(const Glib::ustring & service)
{
    items.erase(service);
}

void WayfireStatusNotifier::update_layout(){
       std::string panel_position = WfOption<std::string> {"panel/position"};

       if (panel_position == PANEL_POSITION_LEFT or panel_position == PANEL_POSITION_RIGHT){
               icons_hbox.set_orientation(Gtk::ORIENTATION_VERTICAL);
       }
       else {
               icons_hbox.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
       }
}

void WayfireStatusNotifier::handle_config_reload(){
       update_layout();
}
