#ifndef SOFTWARE_EVENTS_H
#define SOFTWARE_EVENTS_H

#include "boost/sml.hpp"
namespace sml = boost::sml;
using namespace sml;

struct ButtonPress{};
struct LongPress{};
struct Done{};
struct Error{};


// Definitions to make the table easier to read.
static auto buttonPress = sml::event<ButtonPress>;
static auto longPress = sml::event<LongPress>;
static auto done = sml::event<Done>;
static auto error = sml::event<Error>;

#endif // SOFTWARE_EVENTS_H
