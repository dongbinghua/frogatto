#ifndef PREFERENCES_HPP_INCLUDED
#define PREFERENCES_HPP_INCLUDED

namespace preferences {

bool parse_arg(const char* arg);
bool no_sound();
bool show_debug_hitboxes();
bool use_pretty_scaling();
void set_use_pretty_scaling(bool value);
bool fullscreen();
void set_fullscreen(bool value);
}

#endif
