// ABOUTME: Decides whether the person at the device confirmed an OTA install by holding WAKE for 2 seconds.
// ABOUTME: Pure logic fed with button samples and timestamps, so host tests cover every confirmation path.
#ifndef UPDATE_CONFIRMATION_H
#define UPDATE_CONFIRMATION_H

class UpdateConfirmation {
   public:
    enum Result { WAITING, CONFIRMED, TIMED_OUT };

    static const unsigned long HOLD_MS = 2000;
    static const unsigned long WINDOW_MS = 30000;

    // Feed one button sample. The 30 s window starts at the first sample. A hold only counts once the button
    // has been seen released, so a stuck button or one still held from waking the device can't confirm.
    Result sample(bool pressed, unsigned long nowMs);

   private:
    bool m_started = false;
    bool m_seenRelease = false;
    bool m_holding = false;
    unsigned long m_windowStartMs = 0;
    unsigned long m_holdStartMs = 0;
};

#endif  // UPDATE_CONFIRMATION_H
