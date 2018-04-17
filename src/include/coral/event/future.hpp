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
    struct ResultHandlerType_s { using Type = std::function<void(T)>; };

    template<>
    struct ResultHandlerType_s<void> { using Type = std::function<void()>; };

    template<typename T>
    using ResultHandlerType = typename ResultHandlerType_s<T>::Type;
}


/**
 *  \brief  Represents the eventual completion (or failure) of an asynchronous
 *          operation, and its resulting value (or exception).
 *
 *  The primary use case for `Future` is as a return value from a function
 *  whose result is not immediately available, but will become available later
 *  (typically as a result of a `Reactor` event).  Hence, it is an alternative
 *  to taking a completion handler callback as a function parameter.
 *
 *  The user of a `Future` object calls `OnCompletion()` to register a result
 *  handler and an error handler.  These are callback functions that will be
 *  called when a result is ready or an error occurs, respectively.
 *
 *  An object of this class works in tandem with a corresponding `Promise`
 *  object that is used to set the value (or exception), triggering a call
 *  to the completion handler.  The `Promise` must be created first, and
 *  then the `Future` is obtained by calling `Promise::GetFuture()`.
 *
 *  A `Promise` and its corresponding `Future` have a "shared state" which
 *  contains either the result/exception, stored by the `Promise`, or the
 *  result and exception handlers, stored by the `Future`. Once the shared state
 *  contains both a result/exception and a set of handlers, the appropriate
 *  handler will be called.
 *
 *  A `Promise`, and by extension its `Future`, are associated with a `Reactor`
 *  which is used to dispatch the event that triggers the handler call.
 *
 *  **Comparison with `std::future`**
 *
 *  `Future` is similar to `std::future`, except that it uses a "push" style
 *  mechanism to transfer the result rather than the "pull" style API of
 *  `std::future`.  That is, setting a result with `Promise` causes the
 *  completion handler of the corresponding `Future` to be called automatically.
 *  This is in contrast to `std::future`, where one calls the `get_value()`
 *  function which blocks until a result is ready.
 *
 *  Furthermore, while `std::future` is designed for use in multi-threaded
 *  code, `Future` is designed for use in single-threaded `Reactor`-based
 *  code (and is in fact not thread-safe at all).
 *
 *  \tparam T
 *      The result type, which may be `void`.  If it is not `void`, it must
 *      be movable and copyable, and its move constructor and move assignment
 *      operator must be `noexcept`.
 */
template<typename T>
class Future
{
public:
    static_assert(
        std::is_nothrow_move_constructible<T>::value || std::is_void<T>::value,
        "Future<T>: Move-constructing T may throw");
    static_assert(
        std::is_nothrow_move_assignable<T>::value || std::is_void<T>::value,
        "Future<T>: Move-assigning T may throw");

    /// The type of the future result.
    using ResultType = T;

    /**
     *  \brief  Default constructor; creates an empty `Future`.
     *
     *  This constructs an empty `Future`, i.e., one which does not share
     *  state with any `Promise`.  The only functions which can safely be
     *  called on such an object are its destructor, its assignment operator
     *  and `Valid()`.
     *
     *  The only way to obtain a non-empty `Future` is to call
     *  `Promise::GetFuture()`.
     */
    Future();

    // For internal use by `Promise::GetFuture()`.
    explicit Future(std::shared_ptr<detail::SharedState<T>> state);

    /// Destructor.
    ~Future();

    /// Copying is disabled, as there may only be one `Future` per `Promise`.
    Future(const Future&) = delete;

    /// Copying is disabled, as there may only be one `Future` per `Promise`.
    Future& operator=(const Future&) = delete;

    /// Move constructor. Leaves `other` in the empty state.
    Future(Future&& other) CORAL_NOEXCEPT;

    /// Move assignment operator. Leaves `other` in the empty state.
    Future& operator=(Future&&) CORAL_NOEXCEPT;

    /**
     *  \brief  Specifies the callback functions that will be called when a
     *          result is ready or an error occurs.
     *
     *  If the shared state contains a result or an exception at the time this
     *  function is called, it will register an event with the associated
     *  `Reactor` (using `AddImmediateEvent()`), causing the appropriate handler
     *  to be called at the next iteration of the event loop.
     *
     *  If the shared state does *not* contain a result or exception, the
     *  handlers will be stored in the shared state and invoked whenever a
     *  result or exception becomes ready.  This means that the handlers
     *  may be called even after the `Future` object has been destroyed.
     *
     *  This function may only be called once, and it may not be called on
     *  an object for which `Valid()` returns `false`.
     *
     *  \param [in] resultHandler
     *      The result handler, which must be a callable object with signature
     *      `void(T)`, or `void()` if `T` is `void`.
     *
     *  \param [in] exceptionHandler
     *      The exception handler, which must be a callable object with
     *      signature `void(std::exception_ptr)`.  This may be omitted, in
     *      which case it defaults to `std::rethrow_exception`, which simply
     *      throws the exception.
     *
     *  \throws std::invalid_argument
     *      if `resultHandler` or `exceptionHandler` is empty.
     *
     *  \pre `Valid()` returns `true`.
     *  \post `Valid()` returns `false`.
     */
    void OnCompletion(
        detail::ResultHandlerType<T> resultHandler,
        std::function<void(std::exception_ptr)> exceptionHandler = std::rethrow_exception);

    /**
     *  \brief  Checks if this `Future` is valid.
     *
     *  This is true if and only if the following conditions hold:
     *
     *    - The `Future` was not default-constructed.
     *    - It has not been moved from.
     *    - No result/exception handler has been assigned yet.
     */
    bool Valid() CORAL_NOEXCEPT;

    /**
     *  \brief  Returns the `Reactor` associated with this `Future`.
     *
     *  \pre This object is not empty (i.e., it was not default-constructed
     *       or moved from).
     */
    Reactor& GetReactor() const CORAL_NOEXCEPT;

private:
    std::shared_ptr<detail::SharedState<T>> m_state;
};


/**
 *  \brief  Provides a facility to store the result of an asynchronous
 *          operation so it can be retrieved via a `Future`.
 *
 *  A `Promise` and its corresponding `Future` have a "shared state" which
 *  contains either the result/exception, stored by the `Promise`, or the
 *  result and exception handlers, stored by the `Future`. Once the shared state
 *  contains both a result/exception and a set of handlers, the appropriate
 *  handler will be called.
 *
 *  A `Promise`, and by extension its `Future`, are associated with a `Reactor`
 *  which is used to dispatch the event that triggers the handler call.
 *
 *  **Comparison with `std::promise`**
 *
 *  `Promise` is similar to `std::promise`, except that it uses a "push" style
 *  mechanism to transfer the result rather than the "pull" style API of
 *  `std::promise` and `std::future`.  That is, setting a result with `Promise`
 *  causes the completion handler of the corresponding `Future` to be called
 *  automatically.  This is in contrast to `std::future`, where one calls the
 *  `get_value()` function which blocks until a result is ready.
 *
 *  Furthermore, while `std::promise` is designed for use in multi-threaded
 *  code, `Promise` is designed for use in single-threaded `Reactor`-based
 *  code (and is in fact not thread-safe at all).
 */
template<typename T>
class Promise
{
public:
    /**
     *  \brief  Constructor.
     *
     *  This creates a `Promise` which is associated with the given `Reactor`.
     *  The `Reactor` object must always outlive the `Promise` object.
     */
    Promise(Reactor& reactor);

    /**
     *  \brief  Destructor.
     *
     *  If this `Promise` has a shared state, and no result or exception
     *  has been stored in it yet, an exception of type `std::future_error`
     *  with code `std::future_errc::broken_promise` will be stored in the
     *  shared state.
     */
    ~Promise();

    /**
     *  \brief  Returns a `Future` which shares state with this `Promise`.
     *
     *  The returned `Future` object is typically passed to the code which
     *  is supposed to deal with the result/exception from the operation.
     *
     *  This function may only be called once for a given `Promise`, as
     *  there may only be one `Future` with which it shares state.
     *
     *  \throws std::future_error
     *      The exception contains the code `std::future_errc::no_state` if
     *      the object is not associated with any shared state (e.g. if it has
     *      been moved from), or `std::future_errc::future_already_retrieved`
     *      if the function has been called before for this object.
     */
    Future<T> GetFuture();

    /**
     *  \brief  Stores a value in the shared state.
     *
     *  If the shared state contains a result handler at the time this
     *  function is called, it will register an event with the associated
     *  `Reactor` (using `AddImmediateEvent()`), causing the handler
     *  to be called at the next iteration of the event loop.
     *
     *  If the shared state does *not* contain a result handler, the
     *  value will be stored in the shared state and the handler will be
     *  invoked whenever one is registered.  This means that the handlers
     *  may be called even after the `Promise` object has been destroyed.
     *
     *  This function may only be called once for a given `Promise`.
     *
     *  \note
     *      If `T` is `void`, this function does not take a parameter,
     *      i.e., its signature is `void()`.
     *
     *  \throws std::future_error
     *      The exception contains the code `std::future_errc::no_state` if
     *      the object is not associated with any shared state (e.g. if it has
     *      been moved from), or `std::future_errc::future_already_retrieved`
     *      if a result has already been stored in the shared state.
     */
    void SetValue(const T& result);

    /**
     *  \brief  Stores an exception in the shared state.
     *
     *  If the shared state contains an exception handler at the time this
     *  function is called, it will register an event with the associated
     *  `Reactor` (using `AddImmediateEvent()`), causing the handler
     *  to be called at the next iteration of the event loop.
     *
     *  If the shared state does *not* contain an exception handler, the
     *  exception will be stored in the shared state and the handler will be
     *  invoked whenever one is registered.  This means that the handlers
     *  may be called even after the `Promise` object has been destroyed.
     *
     *  This function may only be called once for a given `Promise`.
     *
     *  \throws std::future_error
     *      The exception contains the code `std::future_errc::no_state` if
     *      the object is not associated with any shared state (e.g. if it has
     *      been moved from), or `std::future_errc::future_already_retrieved`
     *      if a result has already been stored in the shared state.
     */
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


namespace detail
{
    template<typename F>
    struct IsFuture { static constexpr bool value = false; };

    template<typename T>
    struct IsFuture<Future<T>> { static constexpr bool value = true; };

    class EndChainedFuture;

    template<typename T>
    class ChainedFuture
    {
    public:
        ChainedFuture(Future<T> future);

        template<typename H>
        auto Then(H&& handler)
            ->  std::enable_if_t<
                    IsFuture<std::result_of_t<H(T)>>::value,
                    ChainedFuture<typename std::result_of_t<H(T)>::ResultType>>;

        template<typename H>
        auto Then(H&& handler)
            ->  std::enable_if_t<
                    std::is_void<std::result_of_t<H(T)>>::value,
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
                    IsFuture<std::result_of_t<H()>>::value,
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
}


/**
 *  \brief  Convenience function that simplifies chaining of asynchronous
 *          operations.
 *
 *  Often, the completion of an asynchronous operation will trigger the
 *  initiation of a new one, or even a series of subsequent operations.
 *  This function provides a nice syntax for such chained operations,
 *  hiding the intermediate `Future` objects and allowing for easy and
 *  robust error handling.
 *
 *  For example, assume that we have two asynchronous functions and one
 *  "normal" function:
 *
 *      Future<int> DoStuff();
 *      Future<Foo> DoMoreStuff();
 *      void DoFinalStuff();
 *
 *  We now want to call `DoMoreStuff()` when `DoStuff()`'s operation completes,
 *  and then `DoFinalStuff()` when `DoMoreStuff()`'s operation completes.
 *  Normally, we'd write this as follows:
 *
 *      DoStuff().OnCompletion(
 *          [] (int i) {
 *              // use i for something
 *              DoMoreStuff().OnCompletion(
 *                  [] (Foo foo) {
 *                      // use foo for something
 *                      DoFinalStuff();
 *                  },
 *                  [] (std::exception_ptr e) {
 *                      // handle error
 *                  });
 *          },
 *          [] (std::exception_ptr e) {
 *              // handle error
 *          });
 *
 *  It is plain to see that each subsequent operation creates a new level of
 *  indentation, and here we haven't even included all the *other* code and
 *  error handling machinery you'd likely want to put in the handlers.
 *
 *  Now, here's the same example using `Chain()`:
 *
 *      Chain(DoStuff(), [] (int i) {
 *          // use i for something
 *          return DoMoreStuff();
 *      }).Then([] (Foo foo) {
 *          // use foo for something
 *          DoFinalStuff();
 *      }).Catch([] (std::exception_ptr e) {
 *          // handle *all* errors
 *      });
 *
 *  This is much clearer, and error handling is way simpler: All errors,
 *  whether from the asynchronous operations or exceptions thrown by the
 *  handlers themselves, are forwarded to the `Catch` clause and handled
 *  in one place.
 *
 *  More generally, the syntax is as follows:
 *
 *      Chain(future, handler1)
 *          .Then(handler2)
 *          .Then(handler3)
 *          ...
 *          .Then(handlerN)
 *          .Catch(errorHandler);
 *
 *  Each handler, except the last one, must have the following signature:
 *
 *      Future<T_I> handlerI(T_Iminus1)     // if T_Iminus1 is not void
 *      Future<T_I> handlerI()              // if T_Iminus1 is void
 *
 *  Here, `I=1,2,…,N` and `T_0` is defined such that the type of
 *  `future` is `Future<T_0>`.  The last handler may (and generally should)
 *  have the following signature:
 *
 *      void handlerN(T_Nminus1)    // if T_Nminus1 is not void
 *      void handlerN()             // if T_Nminus1 is void
 *
 *  Each handler will be invoked when the `Future` returned by the previous one
 *  in the chain is resolved.
 *
 *  The signature of `errorHandler` must have the following form:
 *
 *      void errorHandler(std::exception_ptr)
 *
 *  The chain should always be terminated by `Catch()`, or all errors will be
 *  silently ignored since no handler will be registered for the last `Future`.
 */
template<typename T, typename H>
auto Chain(Future<T> original, H&& handler)
    ->  decltype(detail::ChainedFuture<T>(std::move(original)).Then(std::forward<H>(handler)))
{
    return detail::ChainedFuture<T>(std::move(original))
        .Then(std::forward<H>(handler));
}


/// The result of one of the input operations of `WhenAll().`
template<typename T>
struct AnyResult
{
    /// Contains the result of the operation if it succeeded, otherwise empty.
    boost::optional<T> value;

    /// Contains an exception pointer if the operation failed, otherwise null.
    std::exception_ptr exception;
};


namespace detail
{
    template<typename FutureIt>
    using FutureItResultType =
        typename std::iterator_traits<FutureIt>::value_type::ResultType;

    template<typename T>
    struct WhenAllState
    {
        unsigned completed = 0;
        std::vector<AnyResult<T>> results;
        Promise<std::vector<AnyResult<T>>> promise;

        WhenAllState(Reactor& reactor) : promise(reactor) { }
    };
}


/**
 *  \brief  Creates a `Future` whose completion is tied to the completion
 *          of a number of other futures.
 *
 *  This function takes a sequence of `Future` objects and returns a single
 *  one whose result is ready when the results of *all* the input futures
 *  are ready.
 *
 *  The returned future will never yield an exception, regardless of the
 *  results of the input futures.  Instead, its result value will be a vector
 *  of type `std::vector<AnyResult>` whose size is exactly equal to the
 *  length of the input sequence.  Its elements will be in the same order
 *  as their corresponding input `Future`s.  Each element will contain either
 *  a result value or an exception, never both.
 *
 *  The function will register completion handlers for all the input futures.
 *  It is therefore required that `Future::Valid()` return `true` for each of
 *  them at the time the call is made.  On return, it will be `false` for
 *  all of them.
 *
 *  \param [in] first
 *      A forward iterator to the beginning of a sequence of objects of type
 *      `Future`.
 *  \param [in] last
 *      An iterator that points one element past the end of the sequence
 *      started by `first`.
 *
 *  \returns
 *      A `Future` whose result represents the collected results of the
 *      input futures.  It never yields an exception.
 *
 *  \throws std::invalid_argument
 *      The input sequence is empty (i.e., `first == last`).
 *  \throws std::future_error
 *      `Future::Valid()` returns `false` for any object in the input
 *      sequence (error code `std::future_errc::no_state`).
 */
template<typename ForwardIt>
Future<std::vector<AnyResult<detail::FutureItResultType<ForwardIt>>>>
    WhenAll(ForwardIt first, ForwardIt last)
{
    using T = detail::FutureItResultType<ForwardIt>;
    CORAL_INPUT_CHECK(first != last);
    for (auto it = first; it != last; ++it) {
        if (!it->Valid()) {
            throw std::future_error(std::future_errc::no_state);
        }
    }

    auto state = std::make_shared<detail::WhenAllState<T>>(first->GetReactor());
    for (auto it = first; it != last; ++it) {
        state->results.emplace_back();
        it->OnCompletion(
            [state, index = state->results.size()-1] (T result) {
                state->results[index].value = std::move(result); // noexcept per static_assert above
                ++(state->completed);
                if (state->completed == state->results.size()) {
                    state->promise.SetValue(state->results);
                }
            },
            [state, index = state->results.size()-1] (std::exception_ptr ex) {
                state->results[index].exception = ex;
                ++(state->completed);
                if (state->completed == state->results.size()) {
                    state->promise.SetValue(state->results);
                }
            });
    }
    return state->promise.GetFuture();
}


/// Range version of `WhenAll()` which simply forwards to the iterator version.
template<typename ForwardRange>
auto WhenAll(ForwardRange&& range)
{
    using std::begin;
    using std::end;
    return WhenAll(begin(range), end(range));
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

        ResultHandlerType<T> resultHandler = nullptr;
        std::function<void(std::exception_ptr)> exceptionHandler = nullptr;

        typename ResultStorage<T>::Type result = typename ResultStorage<T>::Type();
        std::exception_ptr exception = nullptr;
    };

    template<typename T>
    void CallResultHandler(SharedState<T>& state)
    {
        assert(state.resultHandler);
        assert(state.result);
        state.resultHandler(std::move(*state.result));
    }

    inline void CallResultHandler(SharedState<void>& state)
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
    detail::ResultHandlerType<T> resultHandler,
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

    inline
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


inline
Promise<void>::Promise(Reactor& reactor)
    : m_state(std::make_shared<detail::SharedState<void>>(reactor))
{
}


inline
Promise<void>::~Promise()
{
    detail::PromiseDtor(m_state);
}


inline
Future<void> Promise<void>::GetFuture()
{
    return detail::GetFuture(m_state);
}


inline
void Promise<void>::SetValue()
{
    detail::SetValue(m_state);
}


inline
void Promise<void>::SetException(std::exception_ptr ep)
{
    detail::SetException(m_state, ep);
}


// =============================================================================
// ChainedFuture<T> function definitions
// =============================================================================

namespace detail
{
    template<typename T>
    void Chain_(Future<T> future, std::shared_ptr<Promise<T>> promise)
    {
        future.OnCompletion(
            [promise] (const T& r) { promise->SetValue(r); },
            [promise] (std::exception_ptr ep) { promise->SetException(ep); });
    }

    inline
    void Chain_(Future<void> future, std::shared_ptr<Promise<void>> promise)
    {
        future.OnCompletion(
            [promise] () { promise->SetValue(); },
            [promise] (std::exception_ptr ep) { promise->SetException(ep); });
    }


    template<typename T>
    ChainedFuture<T>::ChainedFuture(Future<T> future)
        : future_(std::move(future))
    { }


    template<typename T>
    template<typename H>
    auto ChainedFuture<T>::Then(H&& handler)
        ->  std::enable_if_t<
                IsFuture<std::result_of_t<H(T)>>::value,
                ChainedFuture<typename std::result_of_t<H(T)>::ResultType>>
    {
        using R = typename std::result_of_t<H(T)>::ResultType;
        auto promise = std::make_shared<Promise<R>>(future_.GetReactor());
        future_.OnCompletion(
            [handler = std::forward<H>(handler), promise] (T result) {
                Future<R> f;
                try {
                    f = handler(std::move(result));
                } catch (...) {
                    promise->SetException(std::current_exception());
                    return;
                }
                Chain_(std::move(f), promise);
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
                std::is_void<std::result_of_t<H(T)>>::value,
                EndChainedFuture>
    {
        auto promise = std::make_shared<Promise<void>>(future_.GetReactor());
        future_.OnCompletion(
            [handler = std::forward<H>(handler), promise] (T result) {
                try {
                    handler(std::move(result));
                } catch (...) {
                    promise->SetException(std::current_exception());
                    return;
                }
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
        future_.OnCompletion([] (T) { }, std::move(handler));
        future_ = Future<T>();
    }

}

// =============================================================================
// ChainedFuture<void> function definitions
// =============================================================================

namespace detail
{
    inline
    ChainedFuture<void>::ChainedFuture(Future<void> future)
        : future_(std::move(future))
    { }


    template<typename H>
    auto ChainedFuture<void>::Then(H&& handler)
        ->  std::enable_if_t<
                IsFuture<std::result_of_t<H()>>::value,
                ChainedFuture<typename std::result_of_t<H()>::ResultType>>
    {
        using R = typename std::result_of_t<H()>::ResultType;
        auto promise = std::make_shared<Promise<R>>(future_.GetReactor());
        future_.OnCompletion(
            [handler = std::forward<H>(handler), promise] () {
                Future<R> f;
                try {
                    f = handler();
                } catch (...) {
                    promise->SetException(std::current_exception());
                    return;
                }
                Chain_(std::move(f), promise);
            },
            [promise] (std::exception_ptr ep) {
                promise->SetException(ep);
            });
        future_ = Future<void>();
        return ChainedFuture<R>(promise->GetFuture());
    }


    template<typename H>
    auto ChainedFuture<void>::Then(H&& handler)
        ->  std::enable_if_t<
                std::is_void<std::result_of_t<H()>>::value,
                EndChainedFuture>
    {
        auto promise = std::make_shared<Promise<void>>(future_.GetReactor());
        future_.OnCompletion(
            [handler = std::forward<H>(handler), promise] () {
                try {
                    handler();
                } catch (...) {
                    promise->SetException(std::current_exception());
                    return;
                }
                promise->SetValue();
            },
            [promise] (std::exception_ptr ep) {
                promise->SetException(ep);
            });
        future_ = Future<void>();
        return EndChainedFuture(promise->GetFuture());
    }


    inline
    void ChainedFuture<void>::Catch(
        std::function<void(std::exception_ptr)> handler)
    {
        future_.OnCompletion([] () { }, std::move(handler));
        future_ = Future<void>();
    }
}


// =============================================================================
// EndChainedFuture function definitions
// =============================================================================

namespace detail
{
    inline
    EndChainedFuture::EndChainedFuture(Future<void> future)
        : future_(std::move(future))
    { }


    inline
    void EndChainedFuture::Catch(
        std::function<void(std::exception_ptr)> handler)
    {
        future_.OnCompletion([] () { }, std::move(handler));
        future_ = Future<void>();
    }
}


}} // namespace
#endif // header guard
