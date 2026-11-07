#include "keyboard_macro.hpp"
#include "engine.hpp"
#include "hotkeys.hpp"
#include <fstream>

KeyboardMacro* KeyboardMacro::s_instance = nullptr;

KeyboardMacro::~KeyboardMacro() { disable(); }

bool KeyboardMacro::enable(HWND hwnd, UINT startKey, UINT panicKey, UINT pickKey) {
    m_hwnd = hwnd;
    m_startKey = startKey;
    m_panicKey = panicKey;
    m_pickKey = pickKey;
    if (m_hook) return true;
    m_controlHeld.fill(false);
    m_suppressed.fill(false);
    s_instance = this;
    m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, GetModuleHandleW(nullptr), 0);
    if (!m_hook) s_instance = nullptr;
    return m_hook != nullptr;
}

void KeyboardMacro::disable() {
    if (m_hook) UnhookWindowsHookEx(m_hook);
    m_hook = nullptr;
    if (s_instance == this) s_instance = nullptr;
    cancelRecording();
    stop();
}

LRESULT CALLBACK KeyboardMacro::hookProc(int code, WPARAM msg, LPARAM data) {
    if (code == HC_ACTION && s_instance &&
        s_instance->onKey(*reinterpret_cast<const KBDLLHOOKSTRUCT*>(data))) return 1;
    return CallNextHookEx(nullptr, code, msg, data);
}

size_t KeyboardMacro::keyIndex(const RecordedKey& key) {
    return key.vk + ((key.flags & KEYEVENTF_EXTENDEDKEY) ? 256 : 0);
}

bool KeyboardMacro::onKey(const KBDLLHOOKSTRUCT& key) {
    if ((key.flags & LLKHF_INJECTED) || key.vkCode == 0 || key.vkCode >= 255 || key.scanCode > 0xff) return false;
    bool up = (key.flags & LLKHF_UP) != 0;
    bool wasHeld = m_controlHeld[key.vkCode];
    m_controlHeld[key.vkCode] = !up;

    if (key.vkCode == VK_F5) {
        if (!m_recordingEnabled) return false;
        if (!up && !wasHeld && !isRunning() && m_finished.load() && !m_recordingBlocked) {
            // Bound completed captures while the UI saves the previous recording.
            if (m_pending.size() >= 64) {
                PostMessageW(m_hwnd, WM_RECORDING_LIMIT, 1, 0);
                return true;
            }
            m_capture.clear();
            m_down.fill(false);
            m_recording = true;
        } else if (up && m_recording) {
            finishRecording();
        }
        return true;
    }
    if (key.vkCode == m_panicKey || (!m_recording &&
        (key.vkCode == m_startKey || key.vkCode == m_pickKey))) {
        if (up) {
            bool suppressed = m_suppressed[key.vkCode];
            m_suppressed[key.vkCode] = false;
            return suppressed;
        }
        if (!up && !wasHeld) {
            m_suppressed[key.vkCode] = true;
            HotkeyId id = key.vkCode == m_panicKey ? HotkeyId::PANIC_STOP :
                (key.vkCode == m_startKey ? HotkeyId::START_STOP : HotkeyId::PICK_POS);
            if (id == HotkeyId::PANIC_STOP) {
                m_running.store(false);
                cancelRecording();
            }
            PostMessageW(m_hwnd, WM_HOTKEY, static_cast<WPARAM>(id), 0);
        }
        return m_suppressed[key.vkCode];
    }
    if (m_recording) {
        if (m_capture.size() >= MAX_EVENTS - m_down.size()) {
            finishRecording();
            PostMessageW(m_hwnd, WM_RECORDING_LIMIT, 0, 0);
            return false;
        }
        auto now = Clock::now();
        if (m_capture.empty()) m_origin = now;
        RecordedKey event{std::chrono::duration_cast<std::chrono::microseconds>(now - m_origin).count(),
            static_cast<WORD>(key.vkCode), static_cast<WORD>(key.scanCode),
            ((key.flags & LLKHF_EXTENDED) ? KEYEVENTF_EXTENDEDKEY : 0u) |
            (up ? KEYEVENTF_KEYUP : 0u)};
        size_t index = keyIndex(event);
        if (!up || m_down[index]) {
            m_capture.push_back(event);
            m_down[index] = !up;
            m_held[index] = event;
        }
    }
    return false;
}

void KeyboardMacro::cancelRecording() {
    m_recording = false;
    m_capture.clear();
    m_down.fill(false);
}

void KeyboardMacro::finishRecording() {
    m_recording = false;
    if (m_capture.empty()) return;
    m_durationUs = (std::max)(int64_t{1},
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - m_origin).count());
    for (size_t i = 0; i < m_down.size(); ++i) {
        if (!m_down[i]) continue;
        auto event = m_held[i];
        event.timeUs = m_durationUs;
        event.flags |= KEYEVENTF_KEYUP;
        m_capture.push_back(event);
    }
    m_down.fill(false);
    m_events.swap(m_capture);
    m_pending.push_back(recording());
    PostMessageW(m_hwnd, WM_RECORDED, 0, 0);
}

INPUT KeyboardMacro::inputFor(const RecordedKey& key) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key.scan ? 0 : key.vk;
    input.ki.wScan = key.scan;
    input.ki.dwFlags = key.flags | (key.scan ? KEYEVENTF_SCANCODE : 0u);
    return input;
}

bool KeyboardMacro::start(uint32_t repeats) {
    if (m_recording || isRunning() || m_events.empty()) return false;
    stop();
    m_count.store(0);
    m_finished.store(false);
    m_running.store(true);
    m_worker = std::thread(&KeyboardMacro::replay, this, repeats);
    return true;
}

void KeyboardMacro::stop() {
    m_running.store(false);
    if (!m_worker.joinable()) return;
    // SendInput can await this thread's keyboard hook, even while we are stopping.
    while (!m_finished.load()) {
        MSG message;
        PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
        Sleep(1);
    }
    m_worker.join();
}

void KeyboardMacro::replay(uint32_t repeats) {
    bool ok = play(recording(), m_running, repeats, &m_count);
    m_running.store(false);
    if (!ok) PostMessageW(m_hwnd, WM_PLAYBACK_ERROR, 0, 0);
    m_finished.store(true);
}

bool KeyboardMacro::play(const KeyRecording& recording, const std::atomic<bool>& running,
    uint32_t repeats, std::atomic<uint64_t>* count) {
    if (!validRecording(recording)) return false;
    std::array<RecordedKey, 512> held{};
    std::array<bool, 512> down{};
    auto origin = Clock::now();
    bool failed = false;
    auto waitUntil = [&running](Clock::time_point deadline) {
        double remaining = std::chrono::duration<double, std::milli>(deadline - Clock::now()).count();
        ClickerEngine::preciseSleep(remaining, running);
    };
    for (uint32_t cycle = 0; running.load() && (repeats == 0 || cycle < repeats);) {
        Clock::time_point end;
        if (!cycleEnd(origin, recording.durationUs, end)) { failed = true; break; }
        for (const auto& event : recording.events) {
            waitUntil(origin + std::chrono::microseconds(event.timeUs));
            if (!running.load()) break;
            INPUT input = inputFor(event);
            if (SendInput(1, &input, sizeof(INPUT)) != 1) {
                failed = true;
                break;
            }
            size_t index = keyIndex(event);
            down[index] = !(event.flags & KEYEVENTF_KEYUP);
            held[index] = event;
            if (down[index] && count) ClickerEngine::incrementCount(*count);
        }
        if (failed) break;
        waitUntil(end);
        origin = end;
        if (repeats != 0) ++cycle;
    }
    // Release only keys this replay pressed, including when stopped mid-chord.
    for (size_t i = 0; i < down.size(); ++i) {
        if (!down[i]) continue;
        held[i].flags |= KEYEVENTF_KEYUP;
        INPUT input = inputFor(held[i]);
        if (SendInput(1, &input, sizeof(INPUT)) != 1) failed = true;
    }
    return !failed;
}

bool KeyboardMacro::cycleEnd(Clock::time_point origin, int64_t durationUs, Clock::time_point& end) {
    if (durationUs <= 0 || durationUs > std::chrono::duration_cast<std::chrono::microseconds>(Clock::duration::max()).count() / 2)
        return false;
    auto duration = std::chrono::duration_cast<Clock::duration>(std::chrono::microseconds(durationUs));
    if (origin > Clock::time_point::max() - duration) return false;
    end = origin + duration;
    return true;
}

bool KeyboardMacro::validRecording(const KeyRecording& recording) {
    Clock::time_point end;
    if (recording.events.empty() || recording.events.size() > MAX_EVENTS ||
        !cycleEnd(Clock::time_point{}, recording.durationUs, end)) return false;
    std::array<bool, 512> down{};
    int64_t previous = 0;
    if (recording.events.front().timeUs != 0) return false;
    for (const auto& event : recording.events) {
        if (event.timeUs < previous || event.timeUs > recording.durationUs || event.vk == 0 ||
            event.vk >= 255 || event.vk == VK_F5 || event.scan > 0xff ||
            (event.flags & ~(KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY))) return false;
        size_t index = keyIndex(event);
        if ((event.flags & KEYEVENTF_KEYUP) && !down[index]) return false;
        down[index] = !(event.flags & KEYEVENTF_KEYUP);
        previous = event.timeUs;
    }
    return std::none_of(down.begin(), down.end(), [](bool held) { return held; });
}

std::filesystem::path KeyboardMacro::recordingPath() {
    std::wstring path(32768, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return {};
    path.resize(length);
    return std::filesystem::path(path).parent_path() / L"recorded_keys.txt";
}

bool KeyboardMacro::save(const std::filesystem::path& path) const {
    if (path.empty() || m_events.empty()) return false;
    auto temporary = path;
    temporary += L".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    if (!file) return false;
    file << serialize(recording());
    file.close();
    if (file && MoveFileExW(temporary.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileW(temporary.c_str());
    return false;
}

bool KeyboardMacro::load(const std::filesystem::path& path) {
    if (isRunning() || m_recording) return false;
    std::ifstream file(path);
    KeyRecording loaded;
    if (!readRecording(file, loaded)) return false;
    m_events = std::move(loaded.events);
    m_durationUs = loaded.durationUs;
    return true;
}

std::string KeyboardMacro::serialize(const KeyRecording& recording) {
    std::ostringstream output;
    output << "CLICKER_KEYS_V1 " << recording.durationUs << ' ' << recording.events.size() << '\n';
    for (const auto& event : recording.events)
        output << event.timeUs << ' ' << event.vk << ' ' << event.scan << ' ' << event.flags << '\n';
    return output.str();
}

bool KeyboardMacro::deserialize(KeyRecording& recording, const std::string& data) {
    std::istringstream input(data);
    return readRecording(input, recording);
}

bool KeyboardMacro::readRecording(std::istream& file, KeyRecording& recording) {
    std::string magic;
    int64_t duration;
    int64_t count;
    if (!(file >> magic >> duration >> count) || magic != "CLICKER_KEYS_V1" ||
        duration <= 0 || duration > std::chrono::duration_cast<std::chrono::microseconds>(Clock::duration::max()).count() / 2 ||
        count <= 0 || count > static_cast<int64_t>(MAX_EVENTS)) return false;
    std::vector<RecordedKey> events;
    for (int64_t i = 0; i < count; ++i) {
        int64_t time, vk, scan, flags;
        if (!(file >> time >> vk >> scan >> flags) || vk <= 0 || vk >= 255 ||
            scan < 0 || scan > 0xff || flags < 0 || flags > (KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY)) return false;
        RecordedKey event{time, static_cast<WORD>(vk), static_cast<WORD>(scan), static_cast<DWORD>(flags)};
        events.push_back(event);
    }
    file >> std::ws;
    if (!file.eof()) return false;
    KeyRecording loaded{duration, std::move(events)};
    if (!validRecording(loaded)) return false;
    recording = std::move(loaded);
    return true;
}
