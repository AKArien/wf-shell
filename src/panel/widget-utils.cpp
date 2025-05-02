#include "widget-utils.hpp"

namespace widget_utils{

	bool is_panel_vertical(){
		std::string panel_position = WfOption<std::string> {"panel/position"};
	    if (panel_position == PANEL_POSITION_TOP or panel_position == PANEL_POSITION_BOTTOM){
	        return true;
	    }
	    return false;
	}
}
