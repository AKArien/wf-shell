#include "single-notification.hpp"
#include "daemon.hpp"

#include <glibmm/main.h>
#include <gtk-utils.hpp>
#include <gtkmm/icontheme.h>

#include <ctime>
#include <string>

const static std::string FILE_URI_PREFIX = "file://";

static bool begins_with(const std::string & str, const std::string & prefix)
{
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

inline static bool is_file_uri(const std::string & str)
{
    return begins_with(str, FILE_URI_PREFIX);
}

inline static std::string path_from_uri(const std::string & str)
{
    return str.substr(FILE_URI_PREFIX.size());
}

static const int DAY_SEC = 24 * 60 * 60;

static std::string format_recv_time(const std::time_t & time)
{
    std::time_t delta = std::time(nullptr) - time;
    if (delta > 2 * DAY_SEC)
    {
        return std::to_string(delta / DAY_SEC) + "d ago";
    }

    if (delta > DAY_SEC)
    {
        return "Yesterday";
    }

    char c_str[] = "hh:mm";
    std::strftime(c_str, sizeof(c_str), "%R", std::localtime(&time));
    return std::string(c_str);
}

WfSingleNotification::WfSingleNotification(const Notification & notification)
{
    if (is_file_uri(notification.app_icon))
    {
        auto file_name = path_from_uri(notification.app_icon);
        int height     = Gtk::IconSize(Gtk::ICON_SIZE_LARGE_TOOLBAR);
        auto pixbuf    = load_icon_pixbuf_safe(file_name, height);
        app_icon.set(pixbuf);
    } else if (Gtk::IconTheme::get_default()->has_icon(notification.app_icon))
    {
        app_icon.set_from_icon_name(notification.app_icon, Gtk::ICON_SIZE_LARGE_TOOLBAR);
    } else
    {
        app_icon.set_from_icon_name("dialog-information", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    }

    add_css_class("notification");

    app_icon.add_css_class("app-icon");

    top_bar.add_css_class("top-bar");
    top_bar.append(app_icon);

    app_name.set_label(notification.app_name);
    app_name.set_halign(Gtk::Align::START);
    app_name.set_ellipsize(Pango::EllipsizeMode::END);
    app_name.add_css_class("app-name");
    top_bar.append(app_name);

    time_label.set_sensitive(false);
    time_label.set_label(format_recv_time(notification.additional_info.recv_time));
    time_label.add_css_class("time");
    signals.push_back(Glib::signal_timeout().connect(
        [=]
    {
        time_label.set_label(format_recv_time(notification.additional_info.recv_time));
        return true;
    },
        // updating once a day doesn't work with system suspending/hybernating
        10000, Glib::PRIORITY_LOW));
    top_bar.append(time_label);

    close_image.set_from_icon_name("window-close");
    close_button.set_child(close_image);
    close_button.add_css_class("flat");
    close_button.add_css_class("close");
    signals.push_back(close_button.signal_clicked().connect(
        [=] { Daemon::Instance()->closeNotification(notification.id, Daemon::CloseReason::Dismissed); }));
    top_bar.set_spacing(5);

    child.add(top_bar);

    Gtk::IconSize body_image_size = Gtk::ICON_SIZE_DIALOG;
    if (notification.hints.image_data)
    {
        auto image_pixbuf = notification.hints.image_data;
        if (image_pixbuf->get_width() > width)
        {
            image_pixbuf = image_pixbuf->scale_simple(width,
                width * image_pixbuf->get_height() / image_pixbuf->get_width(), Gdk::INTERP_BILINEAR);
        }

        image.set(image_pixbuf);
    } else if (!notification.hints.image_path.empty())
    {
        if (is_file_uri(notification.hints.image_path))
        {
            image.set(path_from_uri(notification.hints.image_path));
        } else
        {
            image.set_from_icon_name(notification.hints.image_path, body_image_size);
        }
    }

    content.add_css_class("notification-contents");
    content.append(image);

    text.set_halign(Gtk::ALIGN_START);
    text.set_line_wrap();
    text.set_line_wrap_mode(Pango::WRAP_CHAR);
    if (notification.body.empty())
    {
        text.set_markup(notification.summary);
    } else
    {
        // NOTE: that is not a really right way to implement FDN markup feature, but the easiest one.
        text.set_markup("<b>" + notification.summary + "</b>" + "\n" + notification.body);
    }

    content.pack_start(text);

    child.add(content);

    actions.add_css_class("actions");

    if (!notification.actions.empty())
    {
        for (uint i = 0; i + 1 < notification.actions.size(); ++++ i)
        {
            if (const auto action_key = notification.actions[i];action_key != "default")
            {
                auto action_button = Glib::RefPtr<Gtk::Button>(new Gtk::Button(notification.actions[i + 1]));
                signals.push_back(action_button->signal_clicked().connect(
                    [id = notification.id, action_key]
                {
                    Daemon::Instance()->invokeAction(id, action_key);
                }));
                actions.append(*action_button.get());
            } else
            {
                auto click_gesture = Gtk::GestureClick::create();
                signals.push_back(click_gesture->signal_pressed().connect(
                    [id = notification.id, action_key, click_gesture] (int count, double x, double y)
                {
                    click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
                }));
                signals.push_back(click_gesture->signal_released().connect(
                    [id = notification.id, action_key, click_gesture] (int count, double x, double y)
                {
                    Daemon::Instance()->invokeAction(id, action_key);
                }));
                default_action_ev_box.add_controller(click_gesture);
            }
        }

        if (!actions.get_children().empty())
        {
            actions.set_homogeneous();
            child.add(actions);
        }
    }

    default_action_ev_box.add(child);
    add(default_action_ev_box);
    set_transition_type(Gtk::REVEALER_TRANSITION_TYPE_SLIDE_UP);
}

WfSingleNotification::~WfSingleNotification()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}
