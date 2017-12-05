#include <coral/net/rfsm.hpp>


namespace coral
{
namespace net
{
namespace rfsm
{

namespace
{
    const std::uint16_t INDETERMINATE_STATE = 0;
}


// =============================================================================
// Master
// =============================================================================

class Master::Private
{
public:
    Private(
        coral::net::Reactor& reactor,
        const coral::net::Endpoint& serverEndpoint)
        : m_reactor{reactor}
        , m_socket{coral::net::zmqx::GlobalContext(), ZMQ_REQ} 
        , m_state{INDETERMINATE_STATE}
        , m_busy{false}
    {
        m_socket.connect(serverEndpoint.URL());
        m_reactor.AddSocket(
            m_socket.Socket(),
            [this] (coral::net::Reactor&, zmq::socket_t&) {
                ReceiveReply();
            });
    }

    ~Private() = default;

    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;

    Private(Private&&) = delete;
    Private& operator=(Private&&) = delete;

    void SendEvent(
        const char* eventID, std::size_t eventIDSize,
        const char* eventData, std::size_t eventDataSize,
        std::chrono::milliseconds timeout,
        ResponseHandler onComplete)
    {
        if (m_busy) {
            throw std::logic_error{"Slave is busy"};
        }
        m_socket.send("EVENT",   5,           ZMQ_SNDMORE);
        m_socket.send(eventID,   eventIDSize, ZMQ_SNDMORE);
        m_socket.send(eventData, eventDataSize);
        SetTimeout(timeout);
        m_busy = true;
        m_responseHandler = onComplete;
    }

private:
    void ReceiveReply()
    {
    }
    void SetTimeout(std::chrono::milliseconds timeout);

    coral::net::Reactor& m_reactor;
    zmq::socket_t m_socket;
    std::uint16_t m_state;
    bool m_busy;
    ResponseHandler m_responseHandler;
};


// =============================================================================
// Slave
// =============================================================================

class Slave::Private
{
}

}}} // namespace
