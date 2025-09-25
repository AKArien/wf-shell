#include <gtkmm.h>
#include <iostream>
#include <glibmm.h>
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtkmm/enums.h"
#include "gtkmm/label.h"
#include "gtkmm/separator.h"
#include "launchers.hpp"
#include "gtk-utils.hpp"
#include "animated_scale.hpp"
#include "widget.hpp"
#include "wp/proxy-interfaces.h"

#include <pipewire/keys.h>

#include "wireplumber.hpp"

enum VolumeLevel
{
    VOLUME_LEVEL_MUTE = 0,
    VOLUME_LEVEL_LOW,
    VOLUME_LEVEL_MED,
    VOLUME_LEVEL_HIGH,
    VOLUME_LEVEL_OOR, /* Out of range */
};

static VolumeLevel volume_icon_for(double volume, double max)
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

bool WayfireWireplumber::on_popover_timeout(int timer)
{
    popover.popdown();
    return false;
}

void WayfireWireplumber::check_set_popover_timeout()
{
    popover_timeout.disconnect();

    popover_timeout = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(*this,
        &WayfireWireplumber::on_popover_timeout), 0), timeout * 1000);
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

    // boxes hierarchy and labeling
    Gtk::Orientation r1, r2;
    if (config::is_horizontal){ // todo : make this configurable

        r1 = Gtk::Orientation::HORIZONTAL;
        r2 = Gtk::Orientation::VERTICAL;
    }
    else {
        r1 = Gtk::Orientation::VERTICAL;
        r2 = Gtk::Orientation::HORIZONTAL;
    }
    Gtk::Box* master_box = new Gtk::Box(r1);
    // TODO : only show the boxes which have stuff in them
    master_box->append(sinks_box);
    master_box->append(sources_box);
    master_box->append(streams_box);
    sinks_box.set_orientation(r2);
    sinks_box.append(*new Gtk::Label("Output devices"));
    sinks_box.append(*new Gtk::Separator(r2));
    sources_box.set_orientation(r2);
    sources_box.append(*new Gtk::Label("Input devices"));
    sources_box.append(*new Gtk::Separator(r2));
    streams_box.set_orientation(r2);
    streams_box.append(*new Gtk::Label("Audio streams"));
    streams_box.append(*new Gtk::Separator(r2));

    /* Setup popover */
    popover.set_autohide(false);
    popover.set_child(*master_box);
    popover.get_style_context()->add_class("volume-popover");
    // popover.add_controller(scroll_gesture);

    /* Setup layout */
    container->append(button);
    button.set_child(main_image);

	WpCommon::init_wp(*this);
}


void WpCommon::init_wp(WayfireWireplumber& widget){
    // creates the core, object interests and connects signals

    /*
        if the core is already set, we are another widget, wether on another monitor
        or on the same wf-panel. We re-use the core, manager and all other objects
    */
    if (core != nullptr){
        std::cout << "wp core appears to already be up\n";
    	g_signal_connect(
    	    object_manager,
    	    "object_added",
    	    G_CALLBACK(on_object_added),
    	    &widget
    	);
    	// catch up to the objects already registered by the core
        WpIterator* reg_objs = wp_object_manager_new_iterator(object_manager);
        GValue item = G_VALUE_INIT;
        while (wp_iterator_next(reg_objs, &item)){
            on_object_added(object_manager, (gpointer)g_value_get_object(&item), &widget);
            g_value_unset(&item);
        }
        return;
    }
    std::cout << "Initialising wireplumber\n";
    wp_init(WP_INIT_PIPEWIRE);
	core = wp_core_new(NULL, NULL, NULL);
	object_manager = wp_object_manager_new();

	// sinks, sources and streams
    std::cout << "registering interests\n";
	WpObjectInterest* sink_interest = wp_object_interest_new_type(WP_TYPE_NODE);
	wp_object_interest_add_constraint(
	    sink_interest,
	    WP_CONSTRAINT_TYPE_PW_PROPERTY,
	    "media.class",
	    WP_CONSTRAINT_VERB_EQUALS,
	    g_variant_new_string("Audio/Sink")
	);

	wp_object_manager_add_interest_full(object_manager, sink_interest);

	WpObjectInterest* source_interest = wp_object_interest_new_type(WP_TYPE_NODE);
	wp_object_interest_add_constraint(
	    source_interest,
	    WP_CONSTRAINT_TYPE_PW_PROPERTY,
	    "media.class",
	    WP_CONSTRAINT_VERB_EQUALS,
	    g_variant_new_string("Audio/Source")
	);

	wp_object_manager_add_interest_full(object_manager, source_interest);

	WpObjectInterest* stream_interest = wp_object_interest_new_type(WP_TYPE_NODE);
	wp_object_interest_add_constraint(
	    stream_interest,
	    WP_CONSTRAINT_TYPE_PW_PROPERTY,
	    "media.class",
	    WP_CONSTRAINT_VERB_EQUALS,
	    g_variant_new_string("Stream/Output/Audio")
	);
	wp_object_manager_add_interest_full(object_manager, stream_interest);

    wp_core_load_component(
        core,
        "libwireplumber-module-mixer-api",
        "module",
        NULL,
        NULL,
        NULL,
        (GAsyncReadyCallback)on_plugin_loaded,
        &widget
    );

    wp_core_connect(core);
}

void WpCommon::on_plugin_loaded(WpCore* core, GAsyncResult* res, void* widget){
    mixer_api = wp_plugin_find(core, "mixer-api");

	wp_core_install_object_manager(core, object_manager);

	g_signal_connect(
	    object_manager,
	    "installed",
	    G_CALLBACK(WpCommon::on_om_installed),
	    widget
	);
}

void WpCommon::on_om_installed(WpObjectManager *manager, gpointer widget){
    g_signal_connect(
        manager,
        "object_added",
        G_CALLBACK(on_object_added),
        widget
    );
    g_signal_connect(
        manager,
        "object-removed",
        G_CALLBACK(on_object_removed),
        widget
    );
}

void WpCommon::on_object_added(WpObjectManager* manager, gpointer object, gpointer widget){
    // makes a new widget and handles signals and callbacks for the new object

    WpPipewireObject* obj = (WpPipewireObject*)object;

    /* TODO : add extra controls :
        - for sinks and sources, wether to set as default
        - for streams, which sink they go to
    */
    Gtk::Box* which_box;
    const gchar* type = wp_pipewire_object_get_property(obj, PW_KEY_MEDIA_CLASS);
    if (g_strcmp0(type, "Audio/Sink") == 0){
        which_box = &(((WayfireWireplumber*)widget)->sinks_box);
    }
    else if (g_strcmp0(type, "Audio/Source") == 0){
        which_box = &(((WayfireWireplumber*)widget)->sources_box);
    }
    else if (g_strcmp0(type, "Stream/Output/Audio") == 0){
        which_box = &(((WayfireWireplumber*)widget)->streams_box);
    }
    else {
        std::cout << "Could not match pipewire object media class, ignoring\n";
        return;
    }


    guint32 id = wp_proxy_get_bound_id(WP_PROXY(object));

    WayfireAnimatedScale* scale = new WayfireAnimatedScale();
    scale->set_range(0.0, 1.0);
	scale->set_target_value(0.5);
    scale->set_size_request(300, 0);

    const gchar* name;

    // try to find a name to display
    name = wp_pipewire_object_get_property(obj, PW_KEY_NODE_NICK);
    if (!name){
        name = wp_pipewire_object_get_property(obj, PW_KEY_NODE_NAME);
    }
    if (!name){
      name = wp_pipewire_object_get_property(obj, PW_KEY_NODE_DESCRIPTION);
    }
    if (!name){
        name = "Unnamed";
    }

    Gtk::Label* label = new Gtk::Label(Glib::ustring(name));

    Gtk::Button* mute = new Gtk::Button();

    Gtk::Grid* grid = new Gtk::Grid();
    grid->attach(*label, 0, 0, 2, 1);
    grid->attach(*scale, 0, 1, 1, 1);
    grid->attach(*mute, 1, 1, 1, 1);

    ((WayfireWireplumber*)widget)->objects_to_grids.insert({obj, grid});

	g_signal_connect(object, "params-changed", G_CALLBACK(on_params_changed), grid);

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
            ((WayfireWireplumber*)widget)->check_set_popover_timeout();
		}
	);

	which_box->append((Gtk::Widget&)*grid);
}

void WpCommon::on_params_changed(WpPipewireObject *object, gchar *id, gpointer grid){
    // updates the value of a scale when it’s volume or mute status changes from elsewhere
    if (g_strcmp0(id, "Props") == 0){
        GVariant* v = NULL;
        g_signal_emit_by_name(mixer_api, "get_volume", wp_proxy_get_bound_id(WP_PROXY(object)), &v);
        if (!v){
            std::cout << "Object doesn’t support volume, we really shouldn’t have gotten this far in the first place.";
            return;
        }
        gboolean mute;
        gdouble volume;
        g_variant_lookup(v, "volume", "d", &volume);
        g_variant_lookup(v, "mute", "b", &mute);
        g_clear_pointer(&v, g_variant_unref);

        WayfireAnimatedScale* animated_scale = (WayfireAnimatedScale*)(((Gtk::Grid*)grid)->get_child_at(0, 1));
        animated_scale->set_target_value(volume);
    }
}

void WpCommon::on_object_removed(WpObjectManager* manager, gpointer object, gpointer widget){
    std::cout << "object removed\n";
    WayfireWireplumber* wdg = (WayfireWireplumber*)widget;
    auto it = wdg->objects_to_grids.find((WpPipewireObject*)object);
    if (it == wdg->objects_to_grids.end()){
        // checking to avoid weird situations
        return;
    }
    Gtk::Grid* grid = it->second;
    Gtk::Box* box = (Gtk::Box*)grid->get_parent();
    box->remove(*grid);

    delete grid;
    wdg->objects_to_grids.erase((WpPipewireObject*)object);
}

WayfireWireplumber::~WayfireWireplumber(){
	gtk_widget_unparent(GTK_WIDGET(popover.gobj()));
	popover_timeout.disconnect();
}
