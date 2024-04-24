#include <boost/sml.hpp>

#include "ossmi.h"
#pragma once
namespace sml = boost::sml;

struct event_trigger {};

class SM {
    OSSMI& osmi;

  public:
    explicit SM(OSSMI& osmi) : osmi(osmi) {}

    auto operator()() const {
        using namespace sml;
        return make_transition_table(*"initial_state"_s +
                                         event<event_trigger> / [this] {
                                             osmi.action();
                                         } = "final_state"_s);
    }
};