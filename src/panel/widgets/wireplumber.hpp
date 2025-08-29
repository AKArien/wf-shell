#ifndef WIDGETS_PIPEWIRE_HPP
#define WIDGETS_PIPEWIRE_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"
#include <gtkmm/image.h>
#include <gtkmm/scale.h>
extern "C" {
	#include <wp/wp.h>
}
#include <wayfire/util/duration.hpp>

class WayfireWireplumber : public WayfireWidget{
	Gtk::Image main_image;
	Gtk::Button button;
	Gtk::Popover popover;

	WfOption<double> timeout{"panel/volume_display_timeout"};
	WfOption<double> scroll_sensitivity{"panel/volume_scroll_sensitivity"};

	// static WpCore* core;
	// static WpObjectManager* object_manager;

	// void init_wp();

	// static void on_object_added(WpObjectManager* manager, gpointer object, gpointer data);

	// static void on_params_changed(WpPipewireObject* object, gchar* id, gpointer scale);

	// static void on_object_removed(WpObjectManager* manager, gpointer node, gpointer scale);

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

		Gtk::Box scales_box;

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

	static void on_object_added(WpObjectManager* manager, gpointer object, gpointer data);

	static void on_params_changed(WpPipewireObject* object, gchar* id, gpointer scale);

	static void on_object_removed(WpObjectManager* manager, gpointer node, gpointer scale);
}

#endif // WIDGETS_PIPEWIRE_HPP
