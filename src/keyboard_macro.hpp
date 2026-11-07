#pragma once

#include "models.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>

class KeyboardMacro {
public:
    static constexpr UINT WM_RECORDED = WM_APP + 10;
    static constexpr UINT WM_PLAYBACK_ERROR = WM_APP + 11;
    static constexpr UINT WM_RECORDING_LIMIT = WM_APP + 12;
    static constexpr size_t MAX_EVENTS = 1000000;
    ~KeyboardMacro();
    bool enable(HWND hwnd, UINT startKey, UINT panicKey, UINT pickKey);
    void disable();
    bool start(uint32_t repeats);
    void stop();
    void cancelRecording();
    void setRecordingEnabled(bool enabled) { m_recordingEnabled = enabled; if (!enabled) cancelRecording(); }
    void setRecordingBlocked(bool blocked) { m_recordingBlocked = blocked; }
    KeyRecording recording() const { return {m_durationUs, m_events}; }
    std::vector<KeyRecording> takeRecordings() { std::vector<KeyRecording> ready; ready.swap(m_pending); return ready; }
    static std::string serialize(const KeyRecording& recording);
    static bool deserialize(KeyRecording& recording, const std::string& data);
    static bool play(const KeyRecording& recording, const std::atomic<bool>& running,
        uint32_t repeats = 1, std::atomic<uint64_t>* count = nullptr);
    bool isRecording() const { return m_recording; }
    bool isRunning() const { return m_running.load(); }
    uint64_t count() const { return m_count.load(); }
    bool save(const std::filesystem::path& path) const;
    bool load(const std::filesystem::path& path);
    static std::filesystem::path recordingPath();

private:
    friend struct KeyboardMacroTest;
    using Clock = std::chrono::steady_clock;
    static LRESULT CALLBACK hookProc(int code, WPARAM msg, LPARAM data);
    bool onKey(const KBDLLHOOKSTRUCT& key);
    void finishRecording();
    void replay(uint32_t repeats);
    static INPUT inputFor(const RecordedKey& key);
    static size_t keyIndex(const RecordedKey& key);
    static bool readRecording(std::istream& input, KeyRecording& recording);
    static bool validRecording(const KeyRecording& recording);
    static bool cycleEnd(Clock::time_point origin, int64_t durationUs, Clock::time_point& end);
    static KeyboardMacro* s_instance;
    HHOOK m_hook = nullptr;
    HWND m_hwnd = nullptr;
    UINT m_startKey = VK_F6, m_panicKey = VK_F7, m_pickKey = VK_F8;
    std::array<bool, 256> m_controlHeld{};
    std::array<bool, 256> m_suppressed{};
    std::array<RecordedKey, 512> m_held{};
    std::array<bool, 512> m_down{};
    std::vector<RecordedKey> m_capture, m_events;
    std::vector<KeyRecording> m_pending;
    Clock::time_point m_origin;
    int64_t m_durationUs = 0;
    bool m_recording = false;
    bool m_recordingEnabled = true;
    bool m_recordingBlocked = false;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_finished{true};
    std::atomic<uint64_t> m_count{0};
    std::thread m_worker;
};
