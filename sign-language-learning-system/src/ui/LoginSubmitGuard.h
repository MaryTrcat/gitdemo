#pragma once

class LoginSubmitGuard {
public:
    bool tryBegin()
    {
        if (m_locked) {
            return false;
        }
        m_locked = true;
        return true;
    }

    void unlock()
    {
        m_locked = false;
    }

    bool isLocked() const
    {
        return m_locked;
    }

private:
    bool m_locked = false;
};
