#include <gtkmm.h>
#include <iostream>
#include <glibmm.h>
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "launchers.hpp"
#include "gtk-utils.hpp"
#include "animated_scale.hpp"
#include "wp/wp.h"

#include "wireplumber.hpp"

enum VolumeLevel
{
    VOLUME_LEVEL_MUTE = 0,
    VOLUME_LEVEL_LOW,
    VOLUME_LEVEL_MED,
    VOLUME_LEVEL_HIGH,
    VOLUME_LEVEL_OOR, /* Out of range */
};

static VolumeLevel volume_icon_for(float volume, float max)
{
    auto third = max / 3;
    if (volume == 0)
    {
        return VOLUME_LEVEL_MUTE;
    } else if ((volume > 0) && (volume <= third))
    {
        return VOLUME_LEVEL_LOW;
    } else if ((volume > third) && (volume <= (third * 2)))
    {
        return VOLUME_LEVEL_MED;
    } else if ((volume > (third * 2)) && (volume <= max))
    {
        return VOLUME_LEVEL_HIGH;
    }

    return VOLUME_LEVEL_OOR;
}

void WayfireWireplumber::init(Gtk::Box *container){
    // sets up the « widget » part
    /* Setup button */
    button.signal_clicked().connect([=]
    {
        if (!popover.is_visible())
        {
            popover.popup();
        } else
        {
            popover.popdown();
        }
    });
    auto style = button.get_style_context();
    style->add_class("volume");
    style->add_class("flat");

    gtk_widget_set_parent(GTK_WIDGET(popover.gobj()), GTK_WIDGET(button.gobj()));

    // auto scroll_gesture = Gtk::EventControllerScroll::create();
    // scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    // {
    //     int change = 0;
    //     if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
    //     {
    //         // +- number of clicks.
    //         change = dy * max_norm * scroll_sensitivity;
    //     } else
    //     {
    //         // Number of pixels expected to have scrolled. usually in 100s
    //         change = (dy / 100.0) * max_norm * scroll_sensitivity;
    //     }

    //     set_volume(std::clamp(volume_scale.get_target_value() - change,
    //         0.0, max_norm));
    //     return true;
    // }, true);
    // scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    // button.add_controller(scroll_gesture);

    /* Setup popover */
    popover.set_autohide(false);
    popover.set_child(scales_box);
    popover.get_style_context()->add_class("volume-popover");
    // popover.add_controller(scroll_gesture);

    /* Setup layout */
    container->append(button);
    button.set_child(main_image);

	WpCommon::init_wp(*this);
}

void WpCommon::init_wp(WayfireWireplumber& widget){
    // creates the core, object interests and connects signals
    // this equates core being set to the status of wireplumber usage
    // it allows re-using the core, object manager, etc through multiple
    // instances of the widget, such as multiple monitors
    if (core != nullptr){
        std::cout << "wp core appears to already be up";
    	g_signal_connect(WpCommon::object_manager, "object_added", G_CALLBACK(WpCommon::on_object_added), &widget);
        return;
    }
    std::cout << "Initialising wireplumber";
    wp_init(WP_INIT_PIPEWIRE);
	core = wp_core_new(NULL, NULL, NULL);
	object_manager = wp_object_manager_new();

	// sinks, sources and streams
    std::cout << "registering interests";
	WpObjectInterest* sink_interest = wp_object_interest_new_type(WP_TYPE_NODE);
	wp_object_interest_add_constraint(sink_interest, WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class", WP_CONSTRAINT_VERB_EQUALS, g_variant_new_string("Audio/Sink"));
	wp_object_manager_add_interest_full(object_manager, sink_interest);

	WpObjectInterest* source_interest = wp_object_interest_new_type(WP_TYPE_NODE);
	wp_object_interest_add_constraint(source_interest, WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class", WP_CONSTRAINT_VERB_EQUALS, g_variant_new_string("Audio/Source"));
	wp_object_manager_add_interest_full(object_manager, source_interest);

	WpObjectInterest* stream_interest = wp_object_interest_new_type(WP_TYPE_NODE);
	wp_object_interest_add_constraint(stream_interest, WP_CONSTRAINT_TYPE_PW_PROPERTY, "media.class", WP_CONSTRAINT_VERB_EQUALS, g_variant_new_string("Stream/Output/Audio"));
	wp_object_manager_add_interest_full(object_manager, stream_interest);

    wp_core_load_component(core, "libwireplumber-module-mixer-api", "module", NULL, NULL, NULL, (GAsyncReadyCallback)on_plugin_loaded, &widget);
    
    wp_core_connect(core);
}

void WpCommon::on_plugin_loaded(WpCore* core, GAsyncResult* res, void* widget){
    mixer_api = wp_plugin_find(core, "mixer-api");

	wp_core_install_object_manager(core, object_manager);

	g_signal_connect(object_manager, "installed", G_CALLBACK(WpCommon::on_om_installed), widget);
}

void WpCommon::on_om_installed(WpObjectManager *manager, gpointer widget){
    g_signal_connect(WpCommon::object_manager, "object_added", G_CALLBACK(WpCommon::on_object_added), widget);
}

void WpCommon::on_object_added(WpObjectManager* manager, gpointer object, gpointer widget){
    // makes a new widget and handles signals and callbacks for the new object
    std::cout << "new object";

    guint32 id = wp_proxy_get_bound_id(WP_PROXY(object));

    auto scale = std::make_shared<WayfireAnimatedScale>();
    scale->set_range(0.0, 1.0);
	scale->set_target_value(0.5);
    scale->set_size_request(300, 0);

	// g_signal_connect(manager, "object-removed", G_CALLBACK(on_object_removed), scale);
	// g_signal_connect(object, "param-changed", G_CALLBACK(on_params_changed), scale);

	scale->set_user_changed_callback([scale, id](){

        GVariantBuilder gvb = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&gvb, "{sv}", "volume", g_variant_new_double(scale->get_target_value()));
        GVariant* v = g_variant_builder_end(&gvb);
        gboolean res FALSE;
        g_signal_emit_by_name(mixer_api, "set-volume", id, v, &res);
        if (!res){

        }
	});

	scale->signal_state_flags_changed().connect(
	    [=](Gtk::StateFlags){
            // check_set_popover_timeout();
		}
	);

	((WayfireWireplumber*)widget)->scales_box.append((Gtk::Widget&)*scale);
}

void WpCommon::on_params_changed(WpPipewireObject *object, gchar *id, gpointer scale){
    // updates the value of a scale when it’s volume or mute status changes from elsewhere
    std::cout << "param has changed";

    if (g_strcmp0(id, "Props") == 0){
        GVariant* v = NULL;
        g_signal_emit_by_name(&mixer_api, "get_volume", wp_proxy_get_bound_id(WP_PROXY(object)), v);

	    // WpIterator* iterator = wp_pipewire_object_enum_params_sync(object, "SPA_PROP_volume", NULL);
	    // GValue volume;
	    // if (!wp_iterator_next(iterator, &volume)){
	        // return;
	    // }
	    // g_assert(G_VALUE_HOLDS_FLOAT(&volume));

		// (WayfireAnimatedScale&)scale->set_target_value(g_value_get_float(&volume));
		WayfireAnimatedScale* animated_scale = static_cast<WayfireAnimatedScale*>(scale);
        animated_scale->set_target_value(g_variant_get_double(v));
    }
}

void WpCommon::on_object_removed(WpObjectManager* manager, gpointer object, gpointer scale){
	// cleanup on aisle 5 !
	std::cout << "object removed";
    WayfireAnimatedScale* animated_scale = static_cast<WayfireAnimatedScale*>(scale);
    Gtk::Box* box = (Gtk::Box*)(animated_scale->get_parent());
	box->remove((WayfireAnimatedScale&)scale);
	delete animated_scale;
}

WayfireWireplumber::~WayfireWireplumber(){
	gtk_widget_unparent(GTK_WIDGET(popover.gobj()));
	popover_timeout.disconnect();
}
