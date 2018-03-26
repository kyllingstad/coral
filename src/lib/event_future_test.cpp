#include <stdexcept>
#include <gtest/gtest.h>
#include <coral/event/future.hpp>


using namespace coral::event;


TEST(coral_event, Future_int1)
{
    Reactor reactor;
    Promise<int> promise(reactor);
    auto future = promise.GetFuture();
    EXPECT_TRUE(future.Valid());
    int value = 0;
    future.OnCompletion([&] (const int& i) {
        value = i;
    });
    EXPECT_FALSE(future.Valid());
    EXPECT_EQ(0, value);
    promise.SetValue(123);
    EXPECT_EQ(0, value);
    reactor.Run();
    EXPECT_EQ(123, value);
}


TEST(coral_event, Future_int2)
{
    Reactor reactor;
    Promise<int> promise(reactor);
    auto future = promise.GetFuture();
    EXPECT_TRUE(future.Valid());
    promise.SetValue(123);
    int value = 0;
    future.OnCompletion([&] (const int& i) {
        value = i;
    });
    EXPECT_FALSE(future.Valid());
    EXPECT_EQ(0, value);
    reactor.Run();
    EXPECT_EQ(123, value);
}

TEST(coral_event, Future_int3)
{
    Reactor reactor;
    Promise<int> promise(reactor);
    promise.SetValue(123);
    auto future = promise.GetFuture();
    EXPECT_TRUE(future.Valid());
    int value = 0;
    future.OnCompletion([&] (const int& i) {
        value = i;
    });
    EXPECT_FALSE(future.Valid());
    EXPECT_EQ(0, value);
    reactor.Run();
    EXPECT_EQ(123, value);
}


TEST(coral_event, Future_void1)
{
    Reactor reactor;
    Promise<void> promise(reactor);
    auto future = promise.GetFuture();
    bool value = false;
    future.OnCompletion([&] () {
        value = true;
    });
    EXPECT_FALSE(value);
    promise.SetValue();
    EXPECT_FALSE(value);
    reactor.Run();
    EXPECT_TRUE(value);
}


TEST(coral_event, Future_void2)
{
    Reactor reactor;
    Promise<void> promise(reactor);
    auto future = promise.GetFuture();
    promise.SetValue();
    bool value = false;
    future.OnCompletion([&] () {
        value = true;
    });
    EXPECT_FALSE(value);
    reactor.Run();
    EXPECT_TRUE(value);
}


TEST(coral_event, Future_void3)
{
    Reactor reactor;
    Promise<void> promise(reactor);
    promise.SetValue();
    auto future = promise.GetFuture();
    bool value = false;
    future.OnCompletion([&] () {
        value = true;
    });
    EXPECT_FALSE(value);
    reactor.Run();
    EXPECT_TRUE(value);
}


TEST(coral_event, Future_int_err1)
{
    Reactor reactor;
    Promise<int> promise(reactor);
    auto future = promise.GetFuture();
    EXPECT_TRUE(future.Valid());
    future.OnCompletion([] (const int&) { });
    EXPECT_FALSE(future.Valid());
    promise.SetException(std::make_exception_ptr(std::length_error("")));
    EXPECT_THROW(reactor.Run(), std::length_error);
}


TEST(coral_event, Future_int_err2)
{
    Reactor reactor;
    Promise<int> promise(reactor);
    auto future = promise.GetFuture();
    EXPECT_TRUE(future.Valid());
    promise.SetException(std::make_exception_ptr(std::length_error("")));
    future.OnCompletion([] (const int&) { });
    EXPECT_FALSE(future.Valid());
    EXPECT_THROW(reactor.Run(), std::length_error);
}


TEST(coral_event, Future_int_err3)
{
    Reactor reactor;
    Promise<int> promise(reactor);
    promise.SetException(std::make_exception_ptr(std::length_error("")));
    auto future = promise.GetFuture();
    EXPECT_TRUE(future.Valid());
    future.OnCompletion([] (const int&) { });
    EXPECT_FALSE(future.Valid());
    EXPECT_THROW(reactor.Run(), std::length_error);
}


TEST(coral_event, Future_void_err1)
{
    Reactor reactor;
    Promise<void> promise(reactor);
    auto future = promise.GetFuture();
    EXPECT_TRUE(future.Valid());
    future.OnCompletion([] () { });
    EXPECT_FALSE(future.Valid());
    promise.SetException(std::make_exception_ptr(std::length_error("")));
    EXPECT_THROW(reactor.Run(), std::length_error);
}


TEST(coral_event, Future_void_err2)
{
    Reactor reactor;
    Promise<void> promise(reactor);
    auto future = promise.GetFuture();
    EXPECT_TRUE(future.Valid());
    promise.SetException(std::make_exception_ptr(std::length_error("")));
    future.OnCompletion([] () { });
    EXPECT_FALSE(future.Valid());
    EXPECT_THROW(reactor.Run(), std::length_error);
}


TEST(coral_event, Future_void_err3)
{
    Reactor reactor;
    Promise<void> promise(reactor);
    promise.SetException(std::make_exception_ptr(std::length_error("")));
    auto future = promise.GetFuture();
    EXPECT_TRUE(future.Valid());
    future.OnCompletion([] () { });
    EXPECT_FALSE(future.Valid());
    EXPECT_THROW(reactor.Run(), std::length_error);
}


TEST(coral_event, Future_int_broken)
{
    Reactor reactor;
    Future<int> future;
    EXPECT_TRUE(!future.Valid());

    {
        Promise<int> promise(reactor);
        future = promise.GetFuture();
        EXPECT_TRUE(future.Valid());
    }

    future.OnCompletion([] (const int&) { });
    try {
        reactor.Run();
        ADD_FAILURE();
    } catch (const std::future_error& e) {
        EXPECT_TRUE(e.code() == std::future_errc::broken_promise);
    }
}


TEST(coral_event, Future_void_broken)
{
    Reactor reactor;
    Future<void> future;
    EXPECT_TRUE(!future.Valid());

    {
        Promise<void> promise(reactor);
        future = promise.GetFuture();
        EXPECT_TRUE(future.Valid());
    }

    future.OnCompletion([] () { });
    try {
        reactor.Run();
        ADD_FAILURE();
    } catch (const std::future_error& e) {
        EXPECT_TRUE(e.code() == std::future_errc::broken_promise);
    }
}

namespace
{
    class coral_event_Chain : public testing::Test
    {
    public:
        Reactor reactor;
        Promise<int> promise1;
        Promise<void> promise2;
        Promise<double> promise3;

        int value1 = 0;
        bool value2 = false;
        double value3 = 0.0;
        bool exception = false;

        coral_event_Chain()
            : promise1(reactor)
            , promise2(reactor)
            , promise3(reactor)
        {
        }
    };
}

TEST_F(coral_event_Chain, normal)
{
    Chain(promise1.GetFuture(), [&] (const int& i) {
        value1 = i;
        return promise2.GetFuture();
    }).Then([&] () {
        value2 = true;
        return promise3.GetFuture();
    }).Then([&] (const double& d) {
        value3 = d;
    }).Catch([&] (std::exception_ptr ep) {
        exception = true;
    });
    promise1.SetValue(123);
    promise2.SetValue();
    promise3.SetValue(2.0);
    reactor.Run();
    EXPECT_EQ(123, value1);
    EXPECT_TRUE(value2);
    EXPECT_EQ(2.0, value3);
    EXPECT_FALSE(exception);
}

TEST_F(coral_event_Chain, futureException1)
{
    Chain(promise1.GetFuture(), [&] (const int& i) {
        value1 = i;
        return promise2.GetFuture();
    }).Then([&] () {
        value2 = true;
        return promise3.GetFuture();
    }).Then([&] (const double& d) {
        value3 = d;
    }).Catch([&] (std::exception_ptr ep) {
        exception = true;
    });
    promise1.SetException(std::make_exception_ptr(std::runtime_error("")));
    promise2.SetValue();
    promise3.SetValue(2.0);
    reactor.Run();
    EXPECT_EQ(0, value1);
    EXPECT_FALSE(value2);
    EXPECT_EQ(0.0, value3);
    EXPECT_TRUE(exception);
}

TEST_F(coral_event_Chain, futureException2)
{
    Chain(promise1.GetFuture(), [&] (const int& i) {
        value1 = i;
        return promise2.GetFuture();
    }).Then([&] () {
        value2 = true;
        return promise3.GetFuture();
    }).Then([&] (const double& d) {
        value3 = d;
    }).Catch([&] (std::exception_ptr ep) {
        exception = true;
    });
    promise1.SetValue(123);
    promise2.SetException(std::make_exception_ptr(std::runtime_error("")));
    promise3.SetValue(2.0);
    reactor.Run();
    EXPECT_EQ(123, value1);
    EXPECT_FALSE(value2);
    EXPECT_EQ(0.0, value3);
    EXPECT_TRUE(exception);
}

TEST_F(coral_event_Chain, futureException3)
{
    Chain(promise1.GetFuture(), [&] (const int& i) {
        value1 = i;
        return promise2.GetFuture();
    }).Then([&] () {
        value2 = true;
        return promise3.GetFuture();
    }).Then([&] (const double& d) {
        value3 = d;
    }).Catch([&] (std::exception_ptr ep) {
        exception = true;
    });
    promise1.SetValue(123);
    promise2.SetValue();
    promise3.SetException(std::make_exception_ptr(std::length_error("")));
    reactor.Run();
    EXPECT_EQ(123, value1);
    EXPECT_TRUE(value2);
    EXPECT_EQ(0.0, value3);
    EXPECT_TRUE(exception);
}

TEST_F(coral_event_Chain, handlerException1)
{
    Chain(promise1.GetFuture(), [&] (const int& i) {
        if (value1 == 0) throw std::runtime_error("");
        value1 = i;
        return promise2.GetFuture();
    }).Then([&] () {
        value2 = true;
        return promise3.GetFuture();
    }).Then([&] (const double& d) {
        value3 = d;
    }).Catch([&] (std::exception_ptr ep) {
        exception = true;
    });
    promise1.SetValue(123);
    promise2.SetValue();
    promise3.SetValue(2.0);
    reactor.Run();
    EXPECT_EQ(0, value1);
    EXPECT_FALSE(value2);
    EXPECT_EQ(0.0, value3);
    EXPECT_TRUE(exception);
}

TEST_F(coral_event_Chain, handlerException2)
{
    Chain(promise1.GetFuture(), [&] (const int& i) {
        value1 = i;
        return promise2.GetFuture();
    }).Then([&] () {
        if (!value2) throw std::runtime_error("");
        value2 = true;
        return promise3.GetFuture();
    }).Then([&] (const double& d) {
        value3 = d;
    }).Catch([&] (std::exception_ptr ep) {
        exception = true;
    });
    promise1.SetValue(123);
    promise2.SetValue();
    promise3.SetValue(2.0);
    reactor.Run();
    EXPECT_EQ(123, value1);
    EXPECT_FALSE(value2);
    EXPECT_EQ(0.0, value3);
    EXPECT_TRUE(exception);
}

TEST_F(coral_event_Chain, handlerException3)
{
    Chain(promise1.GetFuture(), [&] (const int& i) {
        value1 = i;
        return promise2.GetFuture();
    }).Then([&] () {
        value2 = true;
        return promise3.GetFuture();
    }).Then([&] (const double& d) {
        if (value3 == 0.0) throw std::runtime_error("");
        value3 = d;
    }).Catch([&] (std::exception_ptr ep) {
        exception = true;
    });
    promise1.SetValue(123);
    promise2.SetValue();
    promise3.SetValue(2.0);
    reactor.Run();
    EXPECT_EQ(123, value1);
    EXPECT_TRUE(value2);
    EXPECT_EQ(0.0, value3);
    EXPECT_TRUE(exception);
}
