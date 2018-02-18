/**
\file
\brief Contains the coral::event::Reactor class and related functionality.
\copyright
    Copyright 2013-2018, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_EVENT_REACTOR_HPP
#define CORAL_EVENT_REACTOR_HPP

#include <coral/net/reactor.hpp>


namespace coral
{
namespace event
{


// Reactor will be moved here eventually. For now, we merely alias it.
using coral::net::Reactor;
using coral::net::AddImmediateEvent;


}} // namespace
#endif // header guard
