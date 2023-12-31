/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include "platforms/unix/libpandabase/signal.h"

namespace panda::os::unix {

bool operator==(const ::panda::os::unix::SignalCtl &l, const ::panda::os::unix::SignalCtl &r)
{
    return 0U == memcmp(&l, &r, sizeof(l));
}

bool operator!=(const ::panda::os::unix::SignalCtl &l, const ::panda::os::unix::SignalCtl &r)
{
    return !(l == r);
}

}  // namespace panda::os::unix

namespace panda::test {

class UnixSignal : public ::testing::Test {
protected:
    static void SetUpTestSuite() {}

    void SetUp() override
    {
        os::unix::SignalCtl::GetCurrent(signal_ctl_);
        sig_action_count_ = 0;
    }

    void TearDown() override
    {
        os::unix::SignalCtl signal_ctl;
        os::unix::SignalCtl::GetCurrent(signal_ctl);
    }

    static void SigAction(int sig, UnixSignal *self)
    {
        self->sig_action_count_ += sig;
    }

    static void Delay()
    {
        usleep(TIME_TO_WAIT);
    }

    void CheckSignalsInsideCycle(int signals)
    {
        uint32_t timeout_counter = 0;
        while (true) {
            if (sig_action_count_ == signals) {
                break;
            }

            ++timeout_counter;
            Delay();

            ASSERT_NE(timeout_counter, max_timeout_counter_wait_) << "Timeout error: Signals not got in time";
        }
    }

    static const uint32_t TIME_TO_WAIT = 100U * 1000U;  // 0.1 second
                                                        // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    const uint32_t max_timeout_counter_wait_ = 5U * 60U * 1000U * 1000U / TIME_TO_WAIT;  // 5 minutes

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    os::unix::SignalCtl signal_ctl_ {};
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    std::atomic_int sig_action_count_ {0U};
};

TEST_F(UnixSignal, CheckRestoringSigSet)
{
    // This check is performed by UnixSignal.TearDown()
}

TEST_F(UnixSignal, CheckRestoringSigSet2)
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

    os::unix::SignalCtl signal_ctl;
    os::unix::SignalCtl::GetCurrent(signal_ctl);
    ASSERT_NE(signal_ctl_, signal_ctl);

    pthread_sigmask(SIG_UNBLOCK, &sigset, nullptr);
}

TEST_F(UnixSignal, StartStopThread)
{
    os::unix::SignalCatcherThread catcher_thread;

    catcher_thread.StartThread(&UnixSignal::SigAction, this);
    catcher_thread.StopThread();
}

TEST_F(UnixSignal, StartStopThreadCatchOnlyCatcherThread)
{
    os::unix::SignalCatcherThread catcher_thread;
    catcher_thread.CatchOnlyCatcherThread();

    catcher_thread.StartThread(&UnixSignal::SigAction, this);
    catcher_thread.StopThread();
}

TEST_F(UnixSignal, SendSignalsToMainThread)
{
    os::unix::SignalCatcherThread catcher_thread({SIGUSR1, SIGUSR2});
    catcher_thread.StartThread(&UnixSignal::SigAction, this);

    // Send signals to main thread
    kill(getpid(), SIGUSR1);
    kill(getpid(), SIGUSR2);

    // Wait for the signals inside the cycle
    CheckSignalsInsideCycle(SIGUSR1 + SIGUSR2);
    catcher_thread.StopThread();

    ASSERT_EQ(sig_action_count_, SIGUSR1 + SIGUSR2);
}

// The test fails on WSL
#ifndef PANDA_TARGET_WSL
TEST_F(UnixSignal, SendSignalsToMainThread2)
{
    EXPECT_DEATH(
        {
            os::unix::SignalCatcherThread catcher_thread({SIGUSR1});
            catcher_thread.CatchOnlyCatcherThread();

            catcher_thread.StartThread(&UnixSignal::SigAction, this);

            // Send signals to main thread
            catcher_thread.SendSignal(SIGUSR1);
            CheckSignalsInsideCycle(SIGUSR1);

            // After kill process must die
            kill(getpid(), SIGUSR1);
            CheckSignalsInsideCycle(SIGUSR1 + SIGUSR1);
            catcher_thread.StopThread();

            ASSERT_EQ(true, false) << "Error: Thread must die before this assert";
        },
        "");
}
#endif  // PANDA_TARGET_WSL

TEST_F(UnixSignal, SendSignalsToCatcherThread)
{
    os::unix::SignalCatcherThread catcher_thread({SIGUSR1, SIGUSR2});
    catcher_thread.CatchOnlyCatcherThread();

    catcher_thread.StartThread(&UnixSignal::SigAction, this);

    // Send signals to catcher thread
    catcher_thread.SendSignal(SIGUSR1);
    catcher_thread.SendSignal(SIGUSR2);

    // Wait for the signals inside the cycle
    CheckSignalsInsideCycle(SIGUSR1 + SIGUSR2);
    catcher_thread.StopThread();

    ASSERT_EQ(sig_action_count_, SIGUSR1 + SIGUSR2);
}

TEST_F(UnixSignal, SendUnhandledSignal)
{
    EXPECT_DEATH(
        {
            os::unix::SignalCatcherThread catcher_thread({SIGUSR1});
            catcher_thread.StartThread(&UnixSignal::SigAction, this);

            // Send signals to catcher thread
            catcher_thread.SendSignal(SIGUSR1);
            catcher_thread.SendSignal(SIGUSR2);

            // Wait for the signals inside the cycle
            CheckSignalsInsideCycle(SIGUSR1 + SIGUSR2);
            catcher_thread.StopThread();

            ASSERT_EQ(sig_action_count_, SIGUSR1 + SIGUSR2);
        },
        "");
}

TEST_F(UnixSignal, SendSomeSignalsToCatcherThread)
{
    os::unix::SignalCatcherThread catcher_thread({SIGQUIT, SIGUSR1, SIGUSR2});
    catcher_thread.CatchOnlyCatcherThread();

    catcher_thread.StartThread(&UnixSignal::SigAction, this);

    // Send signals to catcher thread
    catcher_thread.SendSignal(SIGQUIT);
    catcher_thread.SendSignal(SIGUSR1);
    catcher_thread.SendSignal(SIGUSR2);

    // Wait for the signals inside the cycle
    CheckSignalsInsideCycle(SIGQUIT + SIGUSR1 + SIGUSR2);
    catcher_thread.StopThread();

    ASSERT_EQ(sig_action_count_, SIGQUIT + SIGUSR1 + SIGUSR2);
}

TEST_F(UnixSignal, AfterThreadStartCallback)
{
    os::unix::SignalCatcherThread catcher_thread({SIGQUIT});

    // NOLINTNEXTLINE(readability-magic-numbers)
    catcher_thread.SetupCallbacks([this]() { sig_action_count_ += 1000; }, nullptr);

    catcher_thread.StartThread(&UnixSignal::SigAction, this);

    // Send signals to catcher thread
    catcher_thread.SendSignal(SIGQUIT);

    // Wait for the signals inside the cycle
    // NOLINTNEXTLINE(readability-magic-numbers)
    CheckSignalsInsideCycle(SIGQUIT + 1000U);
    catcher_thread.StopThread();

    ASSERT_EQ(sig_action_count_, SIGQUIT + 1000U);
}

TEST_F(UnixSignal, BeforeThreadStopCallback)
{
    os::unix::SignalCatcherThread catcher_thread({SIGQUIT});

    // NOLINTNEXTLINE(readability-magic-numbers)
    catcher_thread.SetupCallbacks(nullptr, [this]() { sig_action_count_ += 2000; });

    catcher_thread.StartThread(&UnixSignal::SigAction, this);

    // Send signals to catcher thread
    catcher_thread.SendSignal(SIGQUIT);

    // Wait for the signals inside the cycle
    CheckSignalsInsideCycle(SIGQUIT);
    catcher_thread.StopThread();

    ASSERT_EQ(sig_action_count_, SIGQUIT + 2000U);
}

TEST_F(UnixSignal, ThreadCallbacks)
{
    os::unix::SignalCatcherThread catcher_thread({SIGQUIT});

    // NOLINTNEXTLINE(readability-magic-numbers)
    catcher_thread.SetupCallbacks([this]() { sig_action_count_ += 1000; }, [this]() { sig_action_count_ += 2000; });

    catcher_thread.StartThread(&UnixSignal::SigAction, this);

    // Send signals to catcher thread
    catcher_thread.SendSignal(SIGQUIT);

    // Wait for the signals inside the cycle
    // NOLINTNEXTLINE(readability-magic-numbers)
    CheckSignalsInsideCycle(1000U + SIGQUIT);
    catcher_thread.StopThread();

    ASSERT_EQ(sig_action_count_, 1000U + SIGQUIT + 2000U);
}

}  // namespace panda::test
