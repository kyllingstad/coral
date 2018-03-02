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
#include <type_traits>
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

    Reactor& GetReactor() const CORAL_NOEXCEPT;

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

/*
template<typename T, typename H>
auto Chain(Future<T> future, H&& handler)
    ->  std::enable_if_t<
            detail::IsFuture<std::result_of_t<H(const T&)>>::value,
            typename std::result_of_t<H(const T&)>>
{
    using R = typename std::result_of_t<H()>::ResultType;
    auto promise = std::make_shared<Promise<R>>(future.GetReactor());
    future.OnCompletion(
        [handler = std::forward<H>(handler), promise] () {
            handler().OnCompletion(
                [promise] (const R& nextResult) {
                    promise->SetValue(nextResult);
                },
                [promise] (std::exception_ptr ep) {
                    promise->SetException(ep);
                });
        },
        [promise] (std::exception_ptr ep) {
            promise->SetException(ep);
        });
    future_ = Future<void>();
    return ChainedFuture<R>(promise->GetFuture());
}
*/

template<typename T>
class ChainedFuture;

class EndChainedFuture;


namespace detail
{
    template<typename F>
    struct IsFuture { static constexpr bool value = false; };

    template<typename T>
    struct IsFuture<Future<T>> { static constexpr bool value = true; };
}


template<typename T>
class ChainedFuture
{
public:
    ChainedFuture(Future<T> future);

    template<typename H>
    auto Then(H&& handler)
        ->  std::enable_if_t<
                detail::IsFuture<std::result_of_t<H(const T&)>>::value,
                ChainedFuture<typename std::result_of_t<H(const T&)>::ResultType>>;

    template<typename H>
    auto Then(H&& handler)
        ->  std::enable_if_t<
                std::is_void<std::result_of_t<H(const T&)>>::value,
                EndChainedFuture>;

    void Catch(std::function<void(std::exception_ptr)> handler);

private:
    Future<T> future_;
};


template<>
class ChainedFuture<void>
{
public:
    ChainedFuture(Future<void> future);

    template<typename H>
    auto Then(H&& handler)
        ->  std::enable_if_t<
                detail::IsFuture<std::result_of_t<H()>>::value,
                ChainedFuture<typename std::result_of_t<H()>::ResultType>>;

    template<typename H>
    auto Then(H&& handler)
        ->  std::enable_if_t<
                std::is_void<std::result_of_t<H()>>::value,
                EndChainedFuture>;

    void Catch(std::function<void(std::exception_ptr)> handler);

private:
    Future<void> future_;
};


class EndChainedFuture
{
public:
    EndChainedFuture(Future<void> future);

    void Catch(std::function<void(std::exception_ptr)> handler);

private:
    Future<void> future_;
};


template<typename T, typename H>
auto Chain(Future<T> original, H&& handler)
    ->  decltype(ChainedFuture<T>(std::move(original)).Then(std::forward<H>(handler)))
{
    return ChainedFuture<T>(std::move(original)).Then(std::forward<H>(handler));
}


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


template<typename T>
Reactor& Future<T>::GetReactor() const CORAL_NOEXCEPT
{
    return m_state->reactor;
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


// =============================================================================
// ChainedFuture<T> function definitions
// =============================================================================


namespace detail
{
    template<typename T>
    void Chain(Future<T> future, std::shared_ptr<Promise<T>> promise)
    {
        future.OnCompletion(
            [promise] (const T& r) { promise->SetValue(r); },
            [promise] (std::exception_ptr ep) { promise->SetException(ep); });
    }

    void Chain(Future<void> future, std::shared_ptr<Promise<void>> promise)
    {
        future.OnCompletion(
            [promise] () { promise->SetValue(); },
            [promise] (std::exception_ptr ep) { promise->SetException(ep); });
    }
}


template<typename T>
ChainedFuture<T>::ChainedFuture(Future<T> future)
    : future_(std::move(future))
{ }


template<typename T>
template<typename H>
auto ChainedFuture<T>::Then(H&& handler)
    ->  std::enable_if_t<
            detail::IsFuture<std::result_of_t<H(const T&)>>::value,
            ChainedFuture<typename std::result_of_t<H(const T&)>::ResultType>>
{
    using R = typename std::result_of_t<H(const T&)>::ResultType;
    auto promise = std::make_shared<Promise<R>>(future_.GetReactor());
    future_.OnCompletion(
        [handler = std::forward<H>(handler), promise] (const T& result) {
            detail::Chain(handler(result), promise);
        },
        [promise] (std::exception_ptr ep) {
            promise->SetException(ep);
        });
    future_ = Future<T>();
    return ChainedFuture<R>(promise->GetFuture());
}


template<typename T>
template<typename H>
auto ChainedFuture<T>::Then(H&& handler)
    ->  std::enable_if_t<
            std::is_void<std::result_of_t<H(const T&)>>::value,
            EndChainedFuture>
{
    auto promise = std::make_shared<Promise<void>>(future_.GetReactor());
    future_.OnCompletion(
        [handler = std::forward<H>(handler), promise] (const T& result) {
            handler(result);
            promise->SetValue();
        },
        [promise] (std::exception_ptr ep) {
            promise->SetException(ep);
        });
    future_ = Future<T>();
    return EndChainedFuture(promise->GetFuture());
}


template<typename T>
void ChainedFuture<T>::Catch(std::function<void(std::exception_ptr)> handler)
{
    future_.OnCompletion([] (const T&) { }, std::move(handler));
    future_ = Future<T>();
}


// =============================================================================
// ChainedFuture<void> function definitions
// =============================================================================


inline ChainedFuture<void>::ChainedFuture(Future<void> future)
    : future_(std::move(future))
{ }


template<typename H>
inline auto ChainedFuture<void>::Then(H&& handler)
    ->  std::enable_if_t<
            detail::IsFuture<std::result_of_t<H()>>::value,
            ChainedFuture<typename std::result_of_t<H()>::ResultType>>
{
    using R = typename std::result_of_t<H()>::ResultType;
    auto promise = std::make_shared<Promise<R>>(future_.GetReactor());
    future_.OnCompletion(
        [handler = std::forward<H>(handler), promise] () {
            detail::Chain(handler(), promise);
        },
        [promise] (std::exception_ptr ep) {
            promise->SetException(ep);
        });
    future_ = Future<void>();
    return ChainedFuture<R>(promise->GetFuture());
}


template<typename H>
inline auto ChainedFuture<void>::Then(H&& handler)
    ->  std::enable_if_t<
            std::is_void<std::result_of_t<H()>>::value,
            EndChainedFuture>
{
    auto promise = std::make_shared<Promise<void>>(future_.GetReactor());
    future_.OnCompletion(
        [handler = std::forward<H>(handler), promise] () {
            handler();
            promise->SetValue();
        },
        [promise] (std::exception_ptr ep) {
            promise->SetException(ep);
        });
    future_ = Future<void>();
    return EndChainedFuture(promise->GetFuture());
}


inline void ChainedFuture<void>::Catch(
    std::function<void(std::exception_ptr)> handler)
{
    future_.OnCompletion([] () { }, std::move(handler));
    future_ = Future<void>();
}


// =============================================================================
// EndChainedFuture function definitions
// =============================================================================


inline EndChainedFuture::EndChainedFuture(Future<void> future)
    : future_(std::move(future))
{ }


inline void EndChainedFuture::Catch(
    std::function<void(std::exception_ptr)> handler)
{
    future_.OnCompletion([] () { }, std::move(handler));
    future_ = Future<void>();
}


}} // namespace
#endif // header guard
