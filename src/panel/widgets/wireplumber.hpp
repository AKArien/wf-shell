#ifndef WIDGETS_PIPEWIRE_HPP
#define WIDGETS_PIPEWIRE_HPP

#include "../widget.hpp"
#include "gtkmm/togglebutton.h"
#include "wf-popover.hpp"
#include "wp/proxy-interfaces.h"
#include <gtkmm/image.h>
#include <gtkmm/scale.h>
extern "C" {
	#include <wp/wp.h>
}
#include <wayfire/util/duration.hpp>
#include <map>

class WfWpControl : public Gtk::Grid{
	// Custom grid with extra methods and data to facilitate handling
	private:
		WayfireAnimatedScale scale;
		Gtk::Label label;
		Gtk::ToggleButton button;
		WpPipewireObject* object;
		sigc::connection mute_conn;

	public:
		WfWpControl(WpPipewireObject* obj);
		void set_btn_status_no_callbk(bool state);
		void set_scale_target_value(double volume);
};

class wayfire_config;
class WayfireWireplumber : public WayfireWidget{
	Gtk::Image main_image;
	Gtk::Button button;
	Gtk::Popover popover;

	/*
		the « face » is the representation of the audio channel that shows it’s
		volume level on the widget icon and is concerned by the quick actions.
		currently, it is the last channel to have been updated.
		we have the whole grid to show all the info in the popup upon updates
		TODO : make this configurable, other ideas : default source/sink
	*/
	WfWpControl* face;

	WfOption<double> timeout{"panel/volume_display_timeout"};
	WfOption<double> scroll_sensitivity{"panel/volume_scroll_sensitivity"};

	// void on_volume_scroll(GdkEventScroll *event);
	// void on_volume_button_press(GdkEventButton *event);
	void on_volume_value_changed();
	bool on_popover_timeout(int timer);

	gulong notify_volume_signal   = 0;
	gulong notify_is_muted_signal = 0;
	gulong notify_default_sink_changed = 0;
	sigc::connection popover_timeout;
	sigc::connection volume_changed_signal;

	enum set_volume_flags_t
	{
		/* Neither show popover nor update volume */
		VOLUME_FLAG_NO_ACTION    = 0,
		/* Show volume popover */
		VOLUME_FLAG_SHOW_POPOVER = 1,
		/* Update real volume with GVC */
		VOLUME_FLAG_UPDATE_GVC   = 2,
		/* Both of the above */
		VOLUME_FLAG_FULL         = 3,
	};

	public:
		void init(Gtk::Box *container) override;
		virtual ~WayfireWireplumber();

		Gtk::Box sinks_box;
		Gtk::Box sources_box;
		Gtk::Box streams_box;
		// TODO : add a category for stuff that listens to an audio source

		// to keep track of which control is associated with which node
		std::map<WpPipewireObject*, WfWpControl*> objects_to_controls;

		/** Update the icon based on volume and muted state */
		void update_icon();

		/** Called when the volume changed from outside of the widget */
		void on_volume_changed_external();

		/** Called when the default sink changes */
		void on_default_sink_changed();

		/**
		* Check whether the popover should be auto-hidden, and if yes, start
		* a timer to hide it
		*/
		void check_set_popover_timeout();
};

namespace WpCommon{
	static WpCore* core = nullptr;
	static WpObjectManager* object_manager;
	static WpPlugin* mixer_api;
	static WpPlugin* default_nodes_api;

	static void init_wp(WayfireWireplumber& widget);
	static void on_plugin_loaded(WpCore* core, GAsyncResult* res, void* data);
	static void on_om_installed(WpObjectManager* manager, gpointer widget);
	static void on_object_added(WpObjectManager* manager, gpointer object, gpointer widget);
	static void on_params_changed(WpPipewireObject* object, gchar* id, gpointer grid);
	static void on_object_removed(WpObjectManager* manager, gpointer node, gpointer widget);
}

#endif // WIDGETS_PIPEWIRE_HPP
