#include "separator.hpp"

WayfireSeparator::WayfireSeparator(int pixels)
{
    int half = pixels / 2;
    separator.set_margin_start(half);
    separator.set_margin_end(half);
}

void WayfireSeparator::init(Gtk::HBox *container)
{
    separator.get_style_context()->add_class("separator");
    container->pack_start(separator);
    update_layout();
    separator.show_all();
}

void WayfireSeparator::update_layout(){
       std::string panel_position = WfOption<std::string> {"panel/position"};

       if (panel_position == PANEL_POSITION_LEFT or panel_position == PANEL_POSITION_RIGHT){
               separator.set_orientation(Gtk::ORIENTATION_VERTICAL);
       }
       else {
               separator.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
       }
}

void WayfireSeparator::handle_config_reload(){
       update_layout();
}

