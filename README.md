# development notes

this fork is for development of (hopefully) additions to wf-shell. Current changes are :

- added configuration `autohide_show/hide_delay`, which controlling the delay between the cursor entering and leaving the panel hotspots and it showing and hiding (if autohide is enabled)
- clarified `autohide_delay` description (a more suitable name would probably be autohide_time ?)
- the panel can be made to not span the whole side (like the gnome dock, or the similar kde option)
- the panel and dock can be attached to the left or right of the screen
- added structure config in WayfireWidget, containing static members for getting global widget configuration (probably mostly for orientation)

Current (new) problems are :
- not all panel widgets respond correctly to being put on the left or right (vertical panel) :
  - there is no vertical layout for window-list and network widgets
    - since they involve text, i plan on having text direction configurable independently from the panel position, which shouldn’t be all that hard, hopefully maybe
- there’s probably some cleaning up to do in the code ! changes were quite hacked on so far.

features i want to add :
- all the stuff that i’ve added to the config metadata files but not implemented
- multiple layers of widgets on wf-panel

# wf-shell

wf-shell is a repository which contains the various components needed to built a fully functional DE based around wayfire.
Currently it has only a GTK-based panel and background client.

# Dependencies

wf-shell needs the core wayland libraries and protocols (`wayland-devel` and `wayland-protocols-devel` for Fedora), gtkmm-4.0 and [wf-config](https://github.com/WayfireWM/wf-config)

# Build

Just like any meson project:
```
git clone https://github.com/WayfireWM/wf-shell && cd wf-shell
meson build --prefix=/usr --buildtype=release
ninja -C build && sudo ninja -C build install
```

# Configuration

To configure the panel and the dock, wf-shell uses a config file located (by default) in `~/.config/wf-shell.ini`
An example configuration can be found in the file `wf-shell.ini.example`, alongside with comments what each option does.

# Style & Theme

Style and theme can be altered with [CSS](/data/css/)

# Screenshots

![Panel & Background demo](/screenshot.png)
