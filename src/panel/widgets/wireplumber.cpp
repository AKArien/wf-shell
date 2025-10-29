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
#include "animated_scale.hpp"
#include "widget.hpp"
#include "wp/defs.h"
#include "wp/proxy-interfaces.h"
#include "wp/proxy.h"

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

static const gchar *DEFAULT_NODE_MEDIA_CLASSES[] = {
  "Audio/Sink",
  "Audio/Source",
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

WfWpControl::WfWpControl(WpPipewireObject* obj, WayfireWireplumber* parent_widget){

    object = obj;
    parent = parent_widget;

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

    name = g_strdup_printf("%s %d", name, id);

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
        }
    );

	scale.set_user_changed_callback(
	    [this, id](){
            GVariantBuilder gvb = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
            g_variant_builder_add(&gvb, "{sv}", "volume", g_variant_new_double(std::pow(scale.get_target_value(), 3))); // see line x
            GVariant* v = g_variant_builder_end(&gvb);
            gboolean res FALSE;
            g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, v, &res);
        }
	);

	scale.signal_state_flags_changed().connect(
	    [=](Gtk::StateFlags){
            parent->check_set_popover_timeout();
		}
	);

    // initialise the values
    GVariant* v = NULL;
    g_signal_emit_by_name(WpCommon::mixer_api, "get-volume", id, &v);
    if (!v) {
        return;
    }
    gboolean mute = FALSE;
    gdouble volume = 0.0;
    g_variant_lookup(v, "volume", "d", &volume);
    g_variant_lookup(v, "mute", "b", &mute);
    g_clear_pointer(&v, g_variant_unref);
    set_btn_status_no_callbk(mute);
    set_scale_target_value(std::cbrt(volume)); // see line x
}

Glib::ustring WfWpControl::get_name(){
    return label.get_text();
}

void WfWpControl::set_btn_status_no_callbk(bool state){
    mute_conn.block(true);
    button.set_active(state);
    mute_conn.block(false);
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
    WfWpControl* copy = new WfWpControl(object, parent);
    return copy;
}

WfWpControlStream::WfWpControlStream(WpPipewireObject* obj, WayfireWireplumber* parent_widget) : WfWpControl(obj, parent_widget){
    // for streams, we determine what sink they go to

    // register all sinks
    potential_outputs_names = Gtk::StringList::create();
    for (const auto& [obj, name] : parent->sinks_to_names){
        register_potential_output(obj);
    }

    parent->streams_controls.push_back(this);

    // force selection of the correct output
    verify_current_output();
}

void WfWpControlStream::register_potential_output(guint32 id){
    potential_outputs_names->append(parent->sinks_to_names.find(id)->second);
    verify_current_output();
}

void WfWpControlStream::remove_potential_output(guint32 id){
    potential_outputs_names->remove(potential_outputs_names->find(parent->sinks_to_names.find(id)->second));
    verify_current_output();
}

void WfWpControlStream::verify_current_output(){
    const gchar* target = wp_pipewire_object_get_property(object, "target.object");
    for (const auto& [obj, name] : parent->sinks_to_names){
        if (g_strcmp0(name.c_str(), target) == 0){
            output_selector.set_selected(potential_outputs_names->find(name));
        }
    }
}

WfWpControlStream* WfWpControlStream::copy(){
    WfWpControlStream* copy = new WfWpControlStream(object, parent);
    return copy;
}

WfWpControlStream::~WfWpControlStream(){
    parent->streams_controls.erase(
        std::find(
            parent->streams_controls.begin(),
            parent->streams_controls.end(),
            this
        )
    );
}

WfWpControlDevice::WfWpControlDevice(WpPipewireObject* obj, WayfireWireplumber* parent_widget) : WfWpControl(obj, parent_widget){
    // for devices (sinks and sources), we determine if they are the default
    attach(default_btn, 1, 0, 1, 1);

    WpProxy* proxy = WP_PROXY(object);

    def_conn = default_btn.signal_clicked().connect(
        [proxy](){
            const gchar* media_class = wp_pipewire_object_get_property(
                WP_PIPEWIRE_OBJECT(proxy),
                PW_KEY_MEDIA_CLASS
            );
            for (guint i = 0; i < G_N_ELEMENTS(DEFAULT_NODE_MEDIA_CLASSES); i++) {
                if (g_strcmp0(media_class, DEFAULT_NODE_MEDIA_CLASSES[i])){
                    continue;
                }
                gboolean res = FALSE;
                const gchar *name = wp_pipewire_object_get_property(
                    WP_PIPEWIRE_OBJECT(proxy),
                    PW_KEY_NODE_NAME
                );
                if (!name) continue;

                g_signal_emit_by_name (WpCommon::default_nodes_api, "set-default-configured-node-name",
                DEFAULT_NODE_MEDIA_CLASSES[i], name, &res);
                if (!res) continue;

                wp_core_sync(WpCommon::core, NULL, NULL, NULL);
            }
        }
    );

}

void WfWpControlDevice::set_def_status_no_callbk(bool state){
    def_conn.block(true);
    default_btn.set_active(state);
    def_conn.block(false);
}

WfWpControlDevice* WfWpControlDevice::copy(){
    WfWpControlDevice* copy = new WfWpControlDevice(object, parent);
    return copy;
}

bool WayfireWireplumber::on_popover_timeout(int timer)
{
    popover_timeout.disconnect();
    // if the popover child is the master box and there is a popover timeut timer,
    // it means that the popover was closed manually, and thus replaced by the mixer
    if (popover->get_child() == (Gtk::Widget*)&master_box) return true;
    popover->set_child(master_box);
    popover->popdown();
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

    button = std::make_unique<WayfireMenuButton>("panel");
    button->get_style_context()->add_class("volume");
    button->get_style_context()->add_class("flat");
    button->set_child(main_image);
    button->show();
    button->get_popover()->set_child(master_box);
    popover = button->get_popover();
    popover->signal_closed().connect(
        [=]{
            // when the widget is clicked during the « small » popup, replace by full mixer
            // if the popover child is the master box, it was already closed and we don’t interfere
            if (popover->get_child() != (Gtk::Widget*)&master_box){
                popover->set_child(master_box);
                // this causes a small delay, but is the simplest way. could be made to look better
                button->activate();
            }
        }
    );

    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        if (!face) return false; // no face means we have nothing to change by scrolling
        dy = dy * -1; // for the same scrolling as volume widget, which we will agree it is more intuitive for more people. TODO : make this configurable
        double change = 0;
        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = (dy * scroll_sensitivity) / 10;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy * scroll_sensitivity) / 100;
        }
        guint32 id = wp_proxy_get_bound_id(WP_PROXY(face->object));
        GVariantBuilder gvb = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&gvb, "{sv}", "volume", g_variant_new_double(std::pow(face->get_scale_target_value() + change, 3))); // see line x
        GVariant* v = g_variant_builder_end(&gvb);
        gboolean res FALSE;
        g_signal_emit_by_name(WpCommon::mixer_api, "set-volume", id, v, &res);
        return true;
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    button->add_controller(scroll_gesture);

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
    popover->set_child(master_box);
    popover->get_style_context()->add_class("volume-popover");
    popover->add_controller(scroll_gesture);

    /* Setup layout */
    container->append(*button);
    button->set_child(main_image);

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
            mixer_api,
            "changed",
            G_CALLBACK(WpCommon::on_mixer_changed),
            &widget
        );

        g_signal_connect(
            default_nodes_api,
            "changed",
            G_CALLBACK(WpCommon::on_default_nodes_changed),
            &widget
        );

        g_signal_connect(
            object_manager,
            "object_added",
            G_CALLBACK(on_object_added),
            &widget
        );

        g_signal_connect(
            object_manager,
            "object-removed",
            G_CALLBACK(on_object_removed),
            &widget
        );

        // catch up to objects already registered by the manager
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

	// register interests in  sinks, sources and streams
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
        (GAsyncReadyCallback)on_mixer_plugin_loaded,
        &widget
    );

    wp_core_connect(core);
}

void WpCommon::on_mixer_plugin_loaded(WpCore* core, GAsyncResult* res, void* widget){
    mixer_api = wp_plugin_find(core, "mixer-api");
    // as far as i understand, this should set the mixer api to use linear scale
    // however, it doesn’t and i can’t find what is wrong.
    // for now, we calculate manually
    // g_object_set(mixer_api, "scale", 0, NULL); // set to linear

    wp_core_load_component(
        core,
        "libwireplumber-module-default-nodes-api",
        "module",
        NULL,
        NULL,
        NULL,
        (GAsyncReadyCallback)on_default_nodes_plugin_loaded,
        widget
    );

}

void WpCommon::on_default_nodes_plugin_loaded(WpCore* core, GAsyncResult* res, void* widget){
    default_nodes_api = wp_plugin_find(core, "default-nodes-api");

    on_all_plugins_loaded(core, widget);
}

void WpCommon::on_all_plugins_loaded(WpCore* core, void* widget){
    g_signal_connect(
        mixer_api,
        "changed",
        G_CALLBACK(WpCommon::on_mixer_changed),
        widget
    );

    g_signal_connect(
        default_nodes_api,
        "changed",
        G_CALLBACK(WpCommon::on_default_nodes_changed),
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
    // adds a new widget to the appropriate section

    WpPipewireObject* obj = (WpPipewireObject*)object;
    guint32 id = wp_proxy_get_bound_id(WP_PROXY(obj));

    WayfireWireplumber* wdg = (WayfireWireplumber*)widget;

    WfWpControl* control;
    Gtk::Box* which_box;
    const gchar* type = wp_pipewire_object_get_property(obj, PW_KEY_MEDIA_CLASS);
    if (g_strcmp0(type, "Audio/Sink") == 0){
        which_box = &(wdg->sinks_box);
        control = new WfWpControlDevice(obj, wdg);
        wdg->sinks_to_names.insert({id, control->get_name()});
        for (auto stream : wdg->streams_controls){
            stream->register_potential_output(id);
        }
    }
    else if (g_strcmp0(type, "Audio/Source") == 0){
        which_box = &(wdg->sources_box);
        control = new WfWpControlDevice(obj, wdg);
        wdg->sources_to_names.insert({id, control->get_name()});
    }
    else if (g_strcmp0(type, "Stream/Output/Audio") == 0){
        which_box = &(wdg->streams_box);
        control = new WfWpControlStream(obj, (WayfireWireplumber*)widget);
    }
    else {
        std::cout << "Could not match pipewire object media class, ignoring\n";
        return;
    }

    wdg->objects_to_controls.insert({obj, control});
    which_box->append((Gtk::Widget&)*control);
}

void WpCommon::on_mixer_changed(gpointer mixer_api, guint id, gpointer widget){
    // update the visual of the appropriate WfWpControl according to external changes

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

    for (auto &it : wdg->objects_to_controls) {
        WpPipewireObject* obj = it.first;
        control = it.second;
        if (wp_proxy_get_bound_id(WP_PROXY(obj)) == id){
            break;
        }
    }

    if (!control){
        return;
    }

    control->set_btn_status_no_callbk(mute);
    control->set_scale_target_value(std::cbrt(volume)); // see line x

    // only if the popover is not already visible and showing something else
    if (!wdg->popover->is_visible() && control->object
        !=
        ((WfWpControl*)(wdg->popover->get_child()))->object
    ){
        wdg->face = control->copy();
        wdg->popover->set_child(*wdg->face);
    }

    wdg->face->set_btn_status_no_callbk(mute);
    wdg->face->set_scale_target_value(std::cbrt(volume)); // see line x

    if (!wdg->popover->is_visible()){
        wdg->popover->popup();
    }
    wdg->check_set_popover_timeout();

    wdg->update_icon();
}

void WpCommon::on_default_nodes_changed(gpointer default_nodes_api, gpointer widget){
    std::cout << "default nodes changed\n";
    WayfireWireplumber* wdg = (WayfireWireplumber*) widget;
    std::vector<guint32> defaults;
    guint32 id = SPA_ID_INVALID;

    for (guint32 i = 0; i < G_N_ELEMENTS(DEFAULT_NODE_MEDIA_CLASSES); i++) {
        g_signal_emit_by_name(default_nodes_api, "get-default-node", DEFAULT_NODE_MEDIA_CLASSES[i], &id);
        if (id != SPA_ID_INVALID) {
            defaults.push_back(id);
        }
    }

    for (const auto &entry : wdg->objects_to_controls) {
        auto proxy = WP_PROXY(entry.first);
        auto ctrl = (WfWpControlDevice*) entry.second;

        guint32 bound_id = wp_proxy_get_bound_id(proxy);
        if (bound_id == SPA_ID_INVALID) {
            continue;
        }

        bool is_default = std::any_of(defaults.begin(), defaults.end(), [bound_id](guint32 def_id){ return def_id == bound_id; });
        if (is_default) {
            ctrl->set_def_status_no_callbk(true);
            continue;
        }

        // if the control is not for a sink or source (non WfWpControlDevice), don’t try to set status
        bool in_sinks = wdg->sinks_to_names.find(bound_id) != wdg->sinks_to_names.end();
        bool in_sources = wdg->sources_to_names.find(bound_id) != wdg->sources_to_names.end();

        if (in_sinks || in_sources) {
            ctrl->set_def_status_no_callbk(false);
        }
    }
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

    guint32 id = wp_proxy_get_bound_id(WP_PROXY((WpPipewireObject*)object));
    for (auto stream : wdg->streams_controls){
        stream->remove_potential_output(id);
    }

    if (wdg->sinks_to_names.find(id) == wdg->sinks_to_names.end()){
        wdg->sinks_to_names.erase(id);
    }
    if (wdg->sources_to_names.find(id) == wdg->sources_to_names.end()){
        wdg->sources_to_names.erase(id);
    }
}

WayfireWireplumber::~WayfireWireplumber(){
	gtk_widget_unparent(GTK_WIDGET(popover->gobj()));
	popover_timeout.disconnect();
}
