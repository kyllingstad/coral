/**
\file
\brief  Defines `coral::event::Future` and related functionality.
\copyright
    Copyright 2018-2018, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_EVENT_FUTURE
#define CORAL_EVENT_FUTURE

#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <utility>
#include <boost/optional.hpp>

#include <coral/config.h>
#include <coral/error.hpp>
#include <coral/event/reactor.hpp>



namespace coral
{
namespace event
{

namespace detail
{
    template<typename T> struct SharedState;

    template<typename T>
    struct ResultHandler
    {
        using Type = std::function<void(const T&)>;
    };

    template<>
    struct ResultHandler<void>
    {
        using Type = std::function<void()>;
    };
}


template<typename T>
class Future
{
public:
    using ResultType = T;

    Future();

    explicit Future(std::shared_ptr<detail::SharedState<T>> state);

    ~Future();

    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;

    Future(Future&& other) CORAL_NOEXCEPT;
    Future& operator=(Future&&) CORAL_NOEXCEPT;

    void OnCompletion(
        typename detail::ResultHandler<T>::Type resultHandler,
        std::function<void(std::exception_ptr)> exceptionHandler = std::rethrow_exception);

    /**
     *  \brief Checks if the future is valid.
     *
     *  This is true if and only if the following conditions hold:
     *
     *    - The future was not default-constructed.
     *    - The future has not been moved from.
     *    - No result/exception handler has been assigned yet.
     */
    bool Valid() CORAL_NOEXCEPT;

private:
    std::shared_ptr<detail::SharedState<T>> m_state;
};


template<typename T>
class Promise
{
public:
    Promise(Reactor& reactor);
    ~Promise();
    Future<T> GetFuture();
    void SetValue(const T& result);
    void SetException(std::exception_ptr ep);

private:
    std::shared_ptr<detail::SharedState<T>> m_state;
};


template<>
class Promise<void>
{
public:
    Promise(Reactor& reactor);
    ~Promise();
    Future<void> GetFuture();
    void SetValue();
    void SetException(std::exception_ptr ep);

private:
    std::shared_ptr<detail::SharedState<void>> m_state;
};


/* WIP:

namespace detail
{
    template<typename T>
    struct WhenAllState
    {
        using ResultVector = std::vector<ResultType>;

        Promise<ResultVector> promise;
        int remaining = 0;
        ResultVector results;
        std::vector<std::exception_ptr> exceptions;
    }
}


template<typename FutureIt>
Future<typename detail::WhenAllState<typename std::iterator_traits<FutureIt>::value_type::ResultType>::ResultVector>
    WhenAll(FutureIt first, FutureIt last)
{
    using T = typename std::iterator_traits<FutureIt>::value_type::ResultType
    const auto state = std::make_shared<WhenAllState<T>>();

    if (first == last) {
        state->promise.SetValue(state->results);
    } else {
        for (auto it = first; it != last; ++it) {
            ++(state->remaining)
            it->OnCompletion(
                [state] (const T& result) {
*/


// =============================================================================
// detail::SharedState<T> and helper functions
// =============================================================================


namespace detail
{

    template<typename T>
    struct ResultStorage { using Type = boost::optional<T>; };

    template<>
    struct ResultStorage<void> { using Type = bool; };

    template<typename T>
    struct SharedState
    {
        SharedState(Reactor& reactor_) : reactor(reactor_) { }

        Reactor& reactor;

        bool futureRetrieved = false;
        bool resultRetrieved = false;

        typename ResultHandler<T>::Type resultHandler = nullptr;
        std::function<void(std::exception_ptr)> exceptionHandler = nullptr;

        typename ResultStorage<T>::Type result = typename ResultStorage<T>::Type();
        std::exception_ptr exception = nullptr;
    };

    template<typename T>
    void CallResultHandler(const SharedState<T>& state)
    {
        assert(state.resultHandler);
        assert(state.result);
        state.resultHandler(*state.result);
    }

    inline void CallResultHandler(const SharedState<void>& state)
    {
        assert(state.resultHandler);
        assert(state.result);
        state.resultHandler();
    }

    template<typename T>
    void DelayCallResultHandler(std::shared_ptr<SharedState<T>> state)
    {
        assert(state);
        AddImmediateEvent(state->reactor, [state] (Reactor&) {
            state->resultRetrieved = true;
            detail::CallResultHandler(*state);
        });
    }

    template<typename T>
    void DelayCallExceptionHandler(std::shared_ptr<SharedState<T>> state)
    {
        assert(state);
        AddImmediateEvent(state->reactor, [state] (Reactor&) {
            state->resultRetrieved = true;
            state->exceptionHandler(state->exception);
        });
    }
}


// =============================================================================
// Future<T> function definitions
// =============================================================================


template<typename T>
Future<T>::Future() = default;


template<typename T>
Future<T>::Future(std::shared_ptr<detail::SharedState<T>> state)
    : m_state(state)
{
    assert(state);
}


template<typename T>
Future<T>::~Future() = default;


template<typename T>
Future<T>::Future(Future&& other) CORAL_NOEXCEPT
    : m_state(std::move(other.m_state))
{ }


template<typename T>
Future<T>& Future<T>::operator=(Future&& other) CORAL_NOEXCEPT
{
    m_state = std::move(other.m_state);
    return *this;
}


template<typename T>
void Future<T>::OnCompletion(
    typename detail::ResultHandler<T>::Type resultHandler,
    std::function<void(std::exception_ptr)> exceptionHandler)
{
    CORAL_PRECONDITION_CHECK(Valid());
    CORAL_INPUT_CHECK(resultHandler);
    CORAL_INPUT_CHECK(exceptionHandler);

    m_state->resultHandler = std::move(resultHandler);
    m_state->exceptionHandler = std::move(exceptionHandler);
    if (m_state->result) {
        DelayCallResultHandler(m_state);
    } else if (m_state->exception) {
        DelayCallExceptionHandler(m_state);
    }
}


template<typename T>
bool Future<T>::Valid() CORAL_NOEXCEPT
{
    return m_state && !m_state->resultHandler;
}


// =============================================================================
// Promise<T> function definitions
// =============================================================================


namespace detail
{
    template<typename T>
    Future<T> GetFuture(std::shared_ptr<SharedState<T>> state)
    {
        if (!state) {
            throw std::future_error(std::future_errc::no_state);
        }
        if (state->futureRetrieved) {
            throw std::future_error(std::future_errc::future_already_retrieved);
        }
        state->futureRetrieved = true;
        return Future<T>(state);
    }

    template<typename T>
    void EnforceUnsatisfied(std::shared_ptr<SharedState<T>> state)
    {
        if (!state) {
            throw std::future_error(std::future_errc::no_state);
        }
        if (state->result || state->exception) {
            throw std::future_error(std::future_errc::promise_already_satisfied);
        }
    }

    template<typename T>
    void SetValue(std::shared_ptr<SharedState<T>> state, const T& result)
    {
        EnforceUnsatisfied(state);
        state->result = result;
        if (state->resultHandler) {
            DelayCallResultHandler(state);
        }
    }

    void SetValue(std::shared_ptr<SharedState<void>> state)
    {
        EnforceUnsatisfied(state);
        state->result = true;
        if (state->resultHandler) {
            DelayCallResultHandler(state);
        }
    }

    template<typename T>
    void SetException(std::shared_ptr<SharedState<T>> state, std::exception_ptr ep)
    {
        EnforceUnsatisfied(state);
        state->exception = ep;
        if (state->exceptionHandler) {
            DelayCallExceptionHandler(state);
        }
    }

    template<typename T>
    void PromiseDtor(std::shared_ptr<SharedState<T>> state) CORAL_NOEXCEPT
    {
        if (state && !state->result && !state->exception) {
            SetException(
                state,
                std::make_exception_ptr(std::future_error(std::future_errc::broken_promise)));
        }
    }
}


template<typename T>
Promise<T>::Promise(Reactor& reactor)
    : m_state(std::make_shared<detail::SharedState<T>>(reactor))
{
}


template<typename T>
Promise<T>::~Promise()
{
    detail::PromiseDtor(m_state);
}


template<typename T>
Future<T> Promise<T>::GetFuture()
{
    return detail::GetFuture(m_state);
}


template<typename T>
void Promise<T>::SetValue(const T& result)
{
    detail::SetValue(m_state, result);
}


template<typename T>
void Promise<T>::SetException(std::exception_ptr ep)
{
    detail::SetException(m_state, ep);
}


// =============================================================================
// Promise<void> function definitions
// =============================================================================


inline Promise<void>::Promise(Reactor& reactor)
    : m_state(std::make_shared<detail::SharedState<void>>(reactor))
{
}


inline Promise<void>::~Promise()
{
    detail::PromiseDtor(m_state);
}


inline Future<void> Promise<void>::GetFuture()
{
    return detail::GetFuture(m_state);
}


inline void Promise<void>::SetValue()
{
    detail::SetValue(m_state);
}


inline void Promise<void>::SetException(std::exception_ptr ep)
{
    detail::SetException(m_state, ep);
}


}} // namespace
#endif // header guard
