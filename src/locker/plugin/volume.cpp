#include "volume.hpp"

LockerVolumes::LockerVolumes(const std::string& section) :
    volume(StreamRole::Sink, section),
    mic(StreamRole::Source, section)
{
    append(box);
    box.append(volume);
    // volume.set_orientation(Gtk::Orientation::VERTICAL);
    box.append(mic);
    box.set_orientation(Gtk::Orientation::VERTICAL);
    // mic.set_orientation(Gtk::Orientation::VERTICAL);
}

WayfireLockerVolumePlugin::WayfireLockerVolumePlugin() :
    WayfireLockerMultiOutputPlugin("locker/volume")
{}
