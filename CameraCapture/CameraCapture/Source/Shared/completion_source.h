#pragma once

#include <winrt/base.h>
#include <windows.h>

template <typename T>
struct completion_source
{
    completion_source()
    {
        m_signal.attach(CreateEvent(nullptr, true, false, nullptr));
    }

    void set(T const& value)
    {
        m_value = value;

        SetEvent(m_signal.get());
    }

    bool await_ready() const noexcept
    {
        return WaitForSingleObject(m_signal.get(), 0) == 0;
    }

    void await_suspend(std::experimental::coroutine_handle<void> resume)
    {
        m_wait.attach(winrt::check_pointer(CreateThreadpoolWait(callback, resume.address(), nullptr)));

        SetThreadpoolWait(m_wait.get(), m_signal.get(), nullptr);
    }

    bool reset() const noexcept
    {
        return ResetEvent(m_signal.get());
    }

    T await_resume() const noexcept
    {
        return m_value;
    }

private:
    static void __stdcall callback(PTP_CALLBACK_INSTANCE, void* context, PTP_WAIT, TP_WAIT_RESULT) noexcept
    {
        std::experimental::coroutine_handle<void>::from_address(context)();
    }

    struct wait_traits
    {
        using type = PTP_WAIT;

        static void close(type value) noexcept
        {
            CloseThreadpoolWait(value);
        }

        static constexpr type invalid() noexcept
        {
            return nullptr;
        }
    };

    winrt::handle m_signal;
    winrt::handle_type<wait_traits> m_wait;
    T m_value{};
};
