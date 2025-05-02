# development notes

this fork is for development of (hopefully) additions to wf-shell. Current changes are :

- added configuration `autohide_show/hide_delay`, which controlling the delay between the cursor entering and leaving the panel hotspots and it showing and hiding (if autohide is enabled)
- clarified `autohide_delay` description (a more suitable name would probably be autohide_time ?)
- the panel can be made to not span the whole side (like the gnome dock, or the similar kde option)
- the panel and dock can be attached to the left or right of the screen
- added src/panel/widget-utils for stuff that as usefull for widget stuff, including outside of descendants of wayfireWidget, most notably WayfireToplevel.

Current (new) problems are :
- getting the panel orientation to the widgets is quite horrible, would probably be better to get it directly from the panel ?
- not all panel widgets respond correctly to being put on the left or right (vertical panel) :
  - window-list looks and works quite bad horizontaly. i’d like some help on that, though you’d probably be better off starting over from the original version. as of f94f092cb5631f145b0db0d9b6f501aed4d9458e :
    - it’s inconsistent
    - looks terrible
    - drag and drop is broken. on an horizontal bar, it has no animation, and it just jumps it down to the bottom on a vertical bar.
  - command-output, as i am not entirely sure how to adapt it, and…
  - … it would probably be nice to have a setting to change how text is adapted for a vertical panel. currently, it is rotated 90 degrees to the left, but it should probalby support other orientations, wether the user wants it to stay horizontal (probably effectively exclusively in the case of autohide being on, because text takes a surprising lot of space), or it be rotated the other way.
- there’s probably some cleaning up to do in the code ! changes were quite hacked on.



# wf-shell

wf-shell is a repository which contains the various components needed to built a fully functional DE based around wayfire.
Currently it has only a GTK-based panel and background client.

# Dependencies

wf-shell needs the core wayland libraries and protocols (`wayland-devel` and `wayland-protocols-devel` for Fedora), gtkmm-3.0 and [wf-config](https://github.com/WayfireWM/wf-config)

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
