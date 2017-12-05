/**
\file
\brief  Module header for coral::net::rfsm.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_NET_RFSM_HPP
#define CORAL_NET_RFSM_HPP

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>

#include <zmq.hpp>

#include <coral/config.h>
#include <coral/net/reactor.hpp>
#include <coral/net/zmqx.hpp>
#include <coral/net.hpp>


namespace coral
{
namespace net
{

/// Classes that implement the "Remote Finite State Machine" (RFSM) protocol.
namespace rfsm
{


class Master
{
public:
    /**
    \brief  Constructs a new master instance connected to the given endpoint,
            and registers it with the given reactor to be notified of replies
            to the commands that are sent.

    `protocolIdentifier` must contain the identifier for the protocol with
    which requests will be made.
    */
    Master(
        coral::net::Reactor& reactor,
        const coral::net::Endpoint& serverEndpoint);

    ~Master() CORAL_NOEXCEPT;

    Master(const Master&) = delete;
    Master& operator=(const Master&) = delete;

    Master(Master&&) CORAL_NOEXCEPT;
    Master& operator=(Master&&) CORAL_NOEXCEPT;

    typedef std::function<void(
            const std::error_code& ec,
            std::uint16_t state,
            const char* responseID, std::size_t responseIDSize,
            const char* responseData, std::size_t responseDataSize)>
        ResponseHandler;

    void SendEvent(
        const char* eventID, std::size_t eventIDSize,
        const char* eventData, std::size_t eventDataSize,
        std::chrono::milliseconds timeout,
        ResponseHandler onComplete);

private:
    class Private;
    std::unique_ptr<Private> m_private;
};


class StateMachine
{
public:
    // Handles event and returns the (possibly new) state
    virtual std::uint16_t HandleEvent(
        const char* eventID, std::size_t eventIDSize,
        const char* eventData, std::size_t eventDataSize,
        const char*& responseID, std::size_t& responseIDSize,
        const char*& responseData, std::size_t& responseDataSize) = 0;

    virtual ~SlaveProtocolHandler() = default;
};


class Slave
{
public:
    Slave(
        coral::net::Reactor& reactor,
        const coral::net::Endpoint& endpoint,
        std::shared_ptr<SlaveProtocolHandler> handler);

    ~Slave() CORAL_NOEXCEPT;

    Slave(const Slave&) = delete;
    Slave& operator=(const Slave&) = delete;

    Slave(Slave&&) CORAL_NOEXCEPT;
    Slave& operator=(Slave&&) CORAL_NOEXCEPT;

    /**
    \brief  Returns the endpoint to which the server is bound.

    This is generally the one that was specified in the constructor, unless
    the server is bound to a local endpoint (not a proxy), in which case
    there are two special cases:

      - If the address was specified as `*` (i.e., bind on all interfaces),
        then the returned address will be `0.0.0.0`.
      - If the port was specified as `*` (i.e., ask the OS for an available
        emphemeral port), then the actual port will be returned.
    */
    coral::net::Endpoint BoundEndpoint() const;

private:
    class Private;
    std::unique_ptr<Private> m_private;
};


}}} // namespace
#endif
