#include <gtkmm.h>
#include <iostream>
#include <glibmm.h>
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtkmm/enums.h"
#include "gtkmm/label.h"
#include "gtkmm/separator.h"
#include "gtkmm/togglebutton.h"
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

static VolumeLevel volume_icon_for(double volume){
    double max = 1.0;
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

WfWpControl::WfWpControl(WpPipewireObject* obj){

    object = obj;

    guint32 id = wp_proxy_get_bound_id(WP_PROXY(object));

    scale.set_range(0.0, 1.0);
	scale.set_target_value(0.5);
    scale.set_size_request(300, 0);

    const gchar* name;

    // try to find a name to display
    name = wp_pipewire_object_get_property(object, PW_KEY_NODE_NICK);
    if (!name){
        name = wp_pipewire_object_get_property(object, PW_KEY_NODE_NAME);
    }
    if (!name){
      name = wp_pipewire_object_get_property(object, PW_KEY_NODE_DESCRIPTION);
    }
    if (!name){
        name = "Unnamed";
    }

    label.set_text(Glib::ustring(name));

    attach(label, 0, 0, 2, 1);
    attach(button, 1, 1, 1, 1);
    attach(scale, 0, 1, 1, 1);

    mute_conn = button.signal_toggled().connect(
        [this, id](){
            GVariantBuilder gvb = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
            g_variant_builder_add(&gvb, "{sv}", "mute", g_variant_new_boolean(button.get_active()));
            GVariant* v = g_variant_builder_end(&gvb);
            gboolean res FALSE;
            g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, v, &res);
            if (!res){

            }
        }
    );

	scale.set_user_changed_callback(
	    [this, id](){
            GVariantBuilder gvb = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
            g_variant_builder_add(&gvb, "{sv}", "volume", g_variant_new_double(std::pow(scale.get_target_value(), 3))); // see line x
            GVariant* v = g_variant_builder_end(&gvb);
            gboolean res FALSE;
            g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, v, &res);
            if (!res){

            }
        }
	);

	scale.signal_state_flags_changed().connect(
	    [=](Gtk::StateFlags){
            // ((WayfireWireplumber*)widget)->check_set_popover_timeout();
		}
	);

    // ugly copy-paste of on_mixer_changed code, clean this
    GVariant* v = NULL;
    g_signal_emit_by_name(WpCommon::mixer_api, "get_volume", wp_proxy_get_bound_id(WP_PROXY(object)), &v);
    if (!v){
        // Object doesn’t support volume, we really shouldn’t have gotten this far in the first place
        return;
    }
    gboolean mute;
    gdouble volume;
    g_variant_lookup(v, "volume", "d", &volume);
    g_variant_lookup(v, "mute", "b", &mute);
    g_clear_pointer(&v, g_variant_unref);
    std::cout << "volume : " << volume << ", mute : " << mute << "\n";
    set_btn_status_no_callbk(mute);
    set_scale_target_value(std::cbrt(volume)); // see line x
}

void WfWpControl::set_btn_status_no_callbk(bool state){
    mute_conn.block();
    button.set_active(state);
    mute_conn.unblock();
}

void WfWpControl::set_scale_target_value(double volume){
    scale.set_target_value(volume);
}

double WfWpControl::get_scale_target_value(){
    return scale.get_target_value();
}

bool WfWpControl::is_muted(){
    return button.get_active();
}

WfWpControl* WfWpControl::copy(){
    WfWpControl* copy = new WfWpControl(object);
    return copy;
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
    // sets up the « widget part »

    /* Setup button */
    button.signal_clicked().connect([=]
    {
        if (!popover.is_visible())
        {
            popover.popup();
            popover.set_child(master_box);
            return;
        }
        if (popover.get_child() != (Gtk::Widget*)(&master_box)){
            // if the popover was a single control, switch it to the full mixer and stay deployed
            popover.set_child(master_box);
            return;
        }
        popover.popdown();
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
    // Gtk::Box* master_box = new eeeGtk::Box(r1);
    master_box.set_orientation(r1);
    // TODO : only show the boxes which have stuff in them
    master_box.append(sinks_box);
    master_box.append(*new Gtk::Separator(r1));
    master_box.append(sources_box);
    master_box.append(*new Gtk::Separator(r1));
    master_box.append(streams_box);
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
    popover.set_child(master_box);
    popover.get_style_context()->add_class("volume-popover");
    // popover.add_controller(scroll_gesture);

    /* Setup layout */
    container->append(button);
    button.set_child(main_image);

	WpCommon::init_wp(*this);
}

void WayfireWireplumber::update_icon()
{
    VolumeLevel current = volume_icon_for(face->get_scale_target_value());

    if (!face || face->is_muted()){
        main_image.set_from_icon_name("audio-volume-muted");
        return;
    }

    std::map<VolumeLevel, std::string> icon_name_from_state = {
        {VOLUME_LEVEL_MUTE, "audio-volume-muted"},
        {VOLUME_LEVEL_LOW, "audio-volume-low"},
        {VOLUME_LEVEL_MED, "audio-volume-medium"},
        {VOLUME_LEVEL_HIGH, "audio-volume-high"},
        {VOLUME_LEVEL_OOR, "audio-volume-muted"},
    };

    main_image.set_from_icon_name(icon_name_from_state.at(current));
}

void WpCommon::init_wp(WayfireWireplumber& widget){
    // creates the core, object interests and connects signals

    /*
        if the core is already set, we are another widget, wether on another monitor
        or on the same wf-panel. We re-use the core, manager and all other objects
    */
    if (core != nullptr){
    	g_signal_connect(
    	    object_manager,
    	    "object_added",
    	    G_CALLBACK(on_object_added),
    	    &widget
    	);
        // catch up to object already registered by the manager
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
        wp_spa_json_new_from_string("{\"scale\": 0}"),
        NULL,
        NULL,
        (GAsyncReadyCallback)on_plugin_loaded,
        &widget
    );

    wp_core_connect(core);
}

void WpCommon::on_plugin_loaded(WpCore* core, GAsyncResult* res, void* widget){
    mixer_api = wp_plugin_find(core, "mixer-api");
    // as far as i understand, this should set the mixer api to use linear scale
    // however, it doesn’t and i can’t find what is wrong.
    // until somesone figures it out, we calculate ourselves on lines 97 and 369
    // g_object_set(mixer_api, "scale", 0, NULL); // set to linear
    g_signal_connect(
        mixer_api,
        "changed",
        G_CALLBACK(WpCommon::on_mixer_changed),
        widget
    );

    g_signal_connect(
        object_manager,
        "object_added",
        G_CALLBACK(on_object_added),
        widget
    );

    g_signal_connect(
        object_manager,
        "object-removed",
        G_CALLBACK(on_object_removed),
        widget
    );

	wp_core_install_object_manager(core, object_manager);
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

    WfWpControl* control = new WfWpControl(obj);

    ((WayfireWireplumber*)widget)->objects_to_controls.insert({obj, control});

	which_box->append((Gtk::Widget&)*control);
}

void WpCommon::on_mixer_changed(gpointer mixer, guint id, gpointer widget)
{
    GVariant* v = NULL;
    // ask the mixer-api for the up-to-date data
    g_signal_emit_by_name(WpCommon::mixer_api, "get-volume", id, &v);
    if (!v) {
        return;
    }

    gboolean mute = FALSE;
    gdouble volume = 0.0;
    g_variant_lookup(v, "volume", "d", &volume);
    g_variant_lookup(v, "mute", "b", &mute);
    g_clear_pointer(&v, g_variant_unref);

    WayfireWireplumber* wdg = (WayfireWireplumber*)widget;
    // find the control that corresponds to this id (match bound id)
    WfWpControl* control;

    for (const auto &it : wdg->objects_to_controls) {
        WpPipewireObject* obj = it.first;
        control = it.second;
        if (wp_proxy_get_bound_id(WP_PROXY(obj)) == id){
            break;
        }
    }

    control->set_btn_status_no_callbk(mute);
    control->set_scale_target_value(std::cbrt(volume)); // see line x

    if (control->object != ((WfWpControl*)(wdg->popover.get_child()))->object){
        wdg->face = control->copy();
        wdg->popover.set_child(*wdg->face);
    }

    // this feels a bit ugly, but it’s probably alright
    wdg->face->set_btn_status_no_callbk(mute);
    wdg->face->set_scale_target_value(std::cbrt(volume)); // see line x

    if (!wdg->popover.is_visible()){
        wdg->popover.popup();
    }
    else {
        wdg->check_set_popover_timeout();
    }

    wdg->update_icon();
}

void WpCommon::on_object_removed(WpObjectManager* manager, gpointer object, gpointer widget){

    WayfireWireplumber* wdg = (WayfireWireplumber*)widget;
    auto it = wdg->objects_to_controls.find((WpPipewireObject*)object);
    if (it == wdg->objects_to_controls.end()){
        // shouldn’t happen, but checking to avoid weird situations
        return;
    }
    WfWpControl* control = it->second;
    Gtk::Box* box = (Gtk::Box*)control->get_parent();
    box->remove(*control);

    delete control;
    wdg->objects_to_controls.erase((WpPipewireObject*)object);
}

WayfireWireplumber::~WayfireWireplumber(){
	gtk_widget_unparent(GTK_WIDGET(popover.gobj()));
	popover_timeout.disconnect();
}
