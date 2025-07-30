#include "ztimer.h"

ZTimer::ZTimer()
{
}

ZTimer::ZTimer(Duration duration_ms) : m_timerDuration(duration_ms)
{
}

ZTimer::ZTimer(Duration duration_ms, CallbackFunc callback)
            : m_timerDuration(duration_ms), m_callback(callback)
{
}

ZTimer::~ZTimer()
{
    Stop();
}

void ZTimer::Start()
{
    m_startTime = Clock::now();
    m_currentTime = m_startTime;
    m_timerStarted = true;
    m_timerFinished = false;
}

void ZTimer::Stop()
{
    m_timerStarted = false;
}

void ZTimer::SetDuration(Duration duration_ms)
{
    if (m_timerStarted)
        return;
    m_timerDuration = duration_ms;
}

void ZTimer::SetCallback(std::function<void()> callback)
{
    if (m_timerStarted)
        return;
    m_callback = callback;
}

void ZTimer::SetRepeating(bool value)
{
    if (m_timerStarted)
        return;
    m_repeatingTimer = value;
}

void ZTimer::Update(Duration deltaTime)
{
    if (!m_timerStarted)
        return;

    m_currentTime += deltaTime;

    if (m_currentTime >= m_startTime + m_timerDuration)
    {
        m_callback();
        if (!m_repeatingTimer)
        {
            Stop();
            m_timerFinished = true;
        }
        else
            Start();
    }
}
