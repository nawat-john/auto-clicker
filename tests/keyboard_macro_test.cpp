#include "../src/keyboard_macro.hpp"
#include "../src/engine.hpp"
#include "../src/hotkeys.hpp"
#include "../src/config.hpp"
#include "../src/app_ui.hpp"
#include "../src/registry.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <mutex>

struct SentKey {
    INPUT input;
    std::chrono::steady_clock::time_point time;
};
static std::mutex sentMutex;
static std::vector<SentKey> sent;
static bool failInput = false;
static UINT WINAPI captureInput(UINT count, LPINPUT inputs, int size) {
    assert(size == sizeof(INPUT));
    std::lock_guard lock(sentMutex);
    if (failInput) return 0;
    for (UINT i = 0; i < count; ++i) sent.push_back({inputs[i], std::chrono::steady_clock::now()});
    return count;
}

// Exercise the real replay worker without sending keys to the user's desktop.
#define SendInput captureInput
#include "../src/keyboard_macro.cpp"
#include "../src/engine.cpp"
#undef SendInput

struct KeyboardMacroTest {
    static bool key(KeyboardMacro& macro, DWORD vk, DWORD flags = 0, DWORD scan = 0) {
        KBDLLHOOKSTRUCT event{vk, scan, flags, 0, 0};
        return macro.onKey(event);
    }
    static void wait(KeyboardMacro& macro) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!macro.m_finished.load() && std::chrono::steady_clock::now() < deadline) Sleep(1);
        assert(macro.m_finished.load());
        macro.stop();
    }
    static void run(const std::filesystem::path& directory) {
        ClickerEngine engine; // Match the app's one-millisecond Windows timer resolution.
        KeyboardMacro macro;
        assert(key(macro, VK_F5));
        assert(macro.isRecording());
        assert(!macro.start(1));
        key(macro, VK_F5); // OS key repeat must not reset the recording.
        key(macro, 'X', LLKHF_INJECTED);
        key(macro, 'X', LLKHF_UP); // A key held before F5 has no matching down.
        assert(macro.m_capture.empty());
        key(macro, VK_LCONTROL, 0, 0x1d);
        Sleep(10);
        key(macro, 'A', 0, 0x1e);
        key(macro, 'A', 0, 0x1e); // Preserve auto-repeat downs.
        key(macro, 'A', LLKHF_UP, 0x1e);
        key(macro, VK_F6);
        key(macro, VK_F8);
        key(macro, VK_F8, LLKHF_UP);
        key(macro, VK_RIGHT, LLKHF_EXTENDED, 0x4d);
        key(macro, VK_RIGHT, LLKHF_EXTENDED | LLKHF_UP, 0x4d);
        key(macro, VK_F5, LLKHF_UP);
        assert(!macro.isRecording());
        assert(macro.m_events.size() == 11);
        assert(macro.m_events.front().timeUs == 0);
        assert(macro.m_events[1].timeUs >= 9000);
        assert(macro.m_events.back().flags & KEYEVENTF_KEYUP);
        assert(!key(macro, VK_F6, LLKHF_UP)); // Let the physical key-up through after recording.
        auto input = KeyboardMacro::inputFor(macro.m_events[7]);
        assert(input.ki.wScan == 0x4d);
        assert(input.ki.dwFlags == (KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY));

        auto path = directory / L"roundtrip.txt";
        assert(macro.save(path));
        KeyboardMacro loaded;
        assert(loaded.load(path));
        assert(loaded.m_durationUs == macro.m_durationUs);
        assert(loaded.m_events.size() == macro.m_events.size());
        for (size_t i = 0; i < macro.m_events.size(); ++i) {
            const auto& a = macro.m_events[i];
            const auto& b = loaded.m_events[i];
            assert(a.timeUs == b.timeUs && a.vk == b.vk && a.scan == b.scan && a.flags == b.flags);
        }
        key(macro, VK_F5);
        key(macro, VK_F5, LLKHF_UP);
        assert(macro.m_events.size() == loaded.m_events.size()); // Empty recordings keep the last macro.
        key(macro, VK_F5);
        key(macro, 'B');
        key(macro, VK_F7);
        key(macro, VK_F7, LLKHF_UP);
        key(macro, VK_F5, LLKHF_UP);
        assert(!macro.isRecording());
        assert(macro.m_events.size() == loaded.m_events.size());

        const char* invalid[] = {
            "garbage", "CLICKER_KEYS_V1 0 1\n0 65 30 0\n",
            "CLICKER_KEYS_V1 10 -18446744073709551614\n0 65 30 0\n10 65 30 2\n",
            "CLICKER_KEYS_V1 10 2\n0 -4294967231 30 0\n10 65 30 2\n",
            "CLICKER_KEYS_V1 10 2\n0 65 -4294967266 0\n10 65 30 2\n",
            "CLICKER_KEYS_V1 10 2\n0 65 30 0\n10 65 30 -4294967294\n",
            "CLICKER_KEYS_V1 10 1000001\n", "CLICKER_KEYS_V1 10 2\n0 65 30 0\n",
            "CLICKER_KEYS_V1 10 2\n0 65 30 0\n11 65 30 2\n",
            "CLICKER_KEYS_V1 10 2\n0 65 30 0\n-1 65 30 2\n",
            "CLICKER_KEYS_V1 10 2\n0 65 30 8\n10 65 30 2\n",
            "CLICKER_KEYS_V1 10 2\n0 256 30 0\n10 256 30 2\n",
            "CLICKER_KEYS_V1 10 2\n0 116 63 0\n10 116 63 2\n",
            "CLICKER_KEYS_V1 10 1\n0 65 30 2\n",
            "CLICKER_KEYS_V1 10 1\n0 65 30 0\n",
            "CLICKER_KEYS_V1 10 2\n0 65 30 0\n10 65 30 2\nextra"
        };
        for (const char* text : invalid) {
            std::ofstream file(directory / L"invalid.txt");
            file << text;
            file.close();
            assert(!loaded.load(directory / L"invalid.txt"));
            assert(loaded.m_events.size() == macro.m_events.size());
        }
        assert(!macro.save(directory / L"missing" / L"keys.txt"));
        assert(loaded.load(path)); // A failed save must preserve the existing file.

        macro.m_events = {{0, VK_F13, 0, 0}, {20000, VK_F13, 0, KEYEVENTF_KEYUP},
            {35000, VK_F14, 0, 0}, {50000, VK_F14, 0, KEYEVENTF_KEYUP}};
        macro.m_durationUs = 60000;
        int64_t maximumDriftUs = 0;
        for (int run = 0; run < 2; ++run) {
            sent.clear();
            assert(macro.start(3));
            wait(macro);
            assert(sent.size() == 12 && macro.count() == 6);
            for (size_t i = 0; i < sent.size(); ++i) {
                assert(sent[i].input.ki.wVk == macro.m_events[i % 4].vk);
                assert(sent[i].input.ki.dwFlags == macro.m_events[i % 4].flags);
                auto actual = std::chrono::duration_cast<std::chrono::microseconds>(sent[i].time - sent[0].time).count();
                auto expected = int64_t(i / 4) * 60000 + macro.m_events[i % 4].timeUs;
                auto drift = int64_t(std::abs(actual - expected));
                maximumDriftUs = (std::max)(maximumDriftUs, drift);
                assert(drift < 30000); // Allow scheduler preemption on a loaded Windows desktop.
            }
        }
        sent.clear();
        macro.m_events = {{0, VK_F13, 0, 0}, {1000000, VK_F13, 0, KEYEVENTF_KEYUP}};
        macro.m_durationUs = 1000000;
        assert(macro.start(0));
        while (macro.count() == 0) Sleep(1);
        auto stopped = std::chrono::steady_clock::now();
        macro.stop();
        assert(std::chrono::steady_clock::now() - stopped < std::chrono::milliseconds(100));
        assert(sent.size() == 2 && (sent.back().input.ki.dwFlags & KEYEVENTF_KEYUP));
        sent.clear();
        failInput = true;
        assert(macro.start(1));
        wait(macro);
        assert(sent.empty() && macro.count() == 0);
        failInput = false;

        auto executable = KeyboardMacro::recordingPath();
        assert(executable.filename() == L"recorded_keys.txt");
        assert(executable.parent_path() == std::filesystem::absolute(directory));
        AppConfig config = ConfigManager::getDefaultConfig();
        for (auto mode : {ExecutionMode::SYNC, ExecutionMode::ASYNC, ExecutionMode::RECORDED_KEYS}) {
            config.mode = mode;
            AppConfig restored;
            assert(ConfigManager::deserializeFromJson(restored, ConfigManager::serializeToJson(config)));
            assert(restored.mode == mode);
        }
        config.mode = ExecutionMode::RECORDED_KEYS;
        config.items.clear();
        AppConfig restored;
        assert(ConfigManager::deserializeFromJson(restored, ConfigManager::serializeToJson(config)));
        assert(restored.mode == config.mode && restored.items.empty());

        config = ConfigManager::getDefaultConfig();
        config.soundFeedback = false;
        config.repeatCount = 1;
        config.globalIntervalMs = 1;
        config.items.resize(1);
        config.items[0].actionType = static_cast<ActionType>(999); // No real desktop input.
        engine.setConfig(config);
        for (int run = 0; run < 2; ++run) {
            assert(engine.start());
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (engine.isRunning() && std::chrono::steady_clock::now() < deadline) Sleep(1);
            assert(!engine.isRunning());
        }
        engine.stop();
        assert(engine.getTotalCount() == 2);
        testOverflow(engine);
        testItems(engine, directory);
        testUI(directory);
        std::cout << "PASS: capture, held keys, persistence, validation, replay timing, restart, stop and config\n";
        std::cout << "Maximum replay timing deviation: " << maximumDriftUs << " us\n";
    }

    static void finish(ClickerEngine& engine) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (engine.isRunning() && std::chrono::steady_clock::now() < deadline) Sleep(1);
        assert(!engine.isRunning());
        engine.stop();
    }

    static ActionItem recorded(int id, WORD vk, int64_t durationUs) {
        ActionItem item;
        item.id = id;
        item.name = "Recording {" + std::to_string(id) + "} ] \\\"";
        item.actionType = ActionType::RECORDED_KEYS;
        item.recording = {durationUs, {{0, vk, 0, 0}, {durationUs / 2, vk, 0, KEYEVENTF_KEYUP}}};
        return item;
    }

    static void testOverflow(ClickerEngine& engine) {
        using Clock = KeyboardMacro::Clock;
        auto uintMax = (std::numeric_limits<uint32_t>::max)();
        auto intMax = (std::numeric_limits<int>::max)();
        AppConfig config;
        config.items = {recorded(intMax, VK_F13, 1000), recorded(1, VK_F14, 1000), recorded(1, VK_F14, 1000)};
        assert(config.nextItemId() == 2);
        config.items.erase(config.items.begin() + 1, config.items.end());
        assert(config.nextItemId() == 1);
        assert(ConfigManager::parseUnsigned("4294967295") == uintMax);
        assert(ConfigManager::parseUnsigned("0") == 0);
        for (const char* value : {"", "-1", "+1", "4294967296", "18446744073709551615", "1x", " 1"}) {
            bool rejected = false;
            try { ConfigManager::parseUnsigned(value); } catch (const std::out_of_range&) { rejected = true; }
            assert(rejected);
        }
        for (const char* json : {
            "{\"globalCpm\":-1}", "{\"randomJitterMs\":4294967296}",
            "{\"hotkeyStartStop\":4294967295}", "{\"items\":[{\"id\":2147483648}]}",
            "{\"items\":[{\"keyVk\":65536}]}", "{\"items\":[{\"actionType\":6}]}",
            "{\"items\":[{\"coordType\":4294967295}]}"}) {
            assert(!ConfigManager::deserializeFromJson(config, json));
            assert(config.items.front().id == intMax); // Rejected input keeps existing data.
        }

        std::atomic<bool> stopped{false};
        const auto original = recorded(1, VK_F13, 1000).recording;
        for (int invalid = 0; invalid < 8; ++invalid) {
            auto data = original;
            switch (invalid) {
                case 0: data.events[0].vk = 65535; break;
                case 1: data.events[0].scan = 65535; break;
                case 2: data.events[0].timeUs = -1; break;
                case 3: data.events[1].timeUs = (std::numeric_limits<int64_t>::max)(); break;
                case 4: data.durationUs = (std::numeric_limits<int64_t>::max)(); break;
                case 5: data.events[0].flags = 0xffffffff; break;
                case 6: data.events.pop_back(); break;
                case 7: data.events[1].vk = VK_F14; break;
            }
            assert(!KeyboardMacro::play(data, stopped));
        }
        auto excessive = original;
        excessive.events.resize(KeyboardMacro::MAX_EVENTS + 1);
        assert(!KeyboardMacro::play(excessive, stopped));
        Clock::time_point end;
        auto nearEnd = Clock::time_point::max() - std::chrono::microseconds(100);
        assert(!KeyboardMacro::cycleEnd(nearEnd, 101, end));
        assert(KeyboardMacro::cycleEnd(nearEnd, 100, end));
        assert(end == Clock::time_point::max());
        assert(!KeyboardMacro::cycleEnd(Clock::time_point{}, (std::numeric_limits<int64_t>::max)(), end));
        assert(!KeyboardMacro::cycleEnd(Clock::time_point{}, -1, end));

        std::atomic<uint64_t> count{(std::numeric_limits<uint64_t>::max)() - 1000};
        std::vector<std::thread> counters;
        for (int i = 0; i < 4; ++i) counters.emplace_back([&count] {
            for (int j = 0; j < 1000; ++j) ClickerEngine::incrementCount(count);
        });
        for (auto& thread : counters) thread.join();
        assert(count.load() == (std::numeric_limits<uint64_t>::max)());

        {
            KeyboardMacro capped;
            key(capped, VK_F5);
            key(capped, 'A');
            auto event = capped.m_capture.front();
            capped.m_capture.resize(KeyboardMacro::MAX_EVENTS - capped.m_down.size(), event);
            key(capped, 'A');
            assert(!capped.isRecording());
            assert(capped.m_events.size() <= KeyboardMacro::MAX_EVENTS);
            assert(capped.m_events.back().flags & KEYEVENTF_KEYUP);
            assert(KeyboardMacro::validRecording(capped.recording()));
            key(capped, VK_F5, LLKHF_UP);
            capped.m_pending.resize(64);
            key(capped, VK_F5);
            assert(!capped.isRecording());
            assert(!key(capped, 65535));
            assert(!key(capped, 'A', 0, 65536));
        }

        config = AppConfig{};
        config.soundFeedback = false;
        config.repeatCount = 0;
        config.globalIntervalMs = uintMax;
        ActionItem single;
        single.actionType = ActionType::KEY_PRESS;
        single.keyVk = VK_F13;
        config.items = {single, single};
        for (auto mode : {ExecutionMode::SYNC, ExecutionMode::ASYNC}) {
            for (uint32_t jitter : {uint32_t{2147483647}, uint32_t{2147483648}, uintMax}) {
                config.mode = mode;
                config.randomJitterMs = jitter;
                engine.setConfig(config);
                engine.resetCount();
                assert(engine.start());
                auto deadline = Clock::now() + std::chrono::seconds(2);
                while (engine.getTotalCount() == 0 && Clock::now() < deadline) Sleep(1);
                engine.stop();
                assert(engine.getTotalCount() != 0);
            }
        }
        std::cout << "PASS: integer limits, invalid array indices, clock overflow, saturating counters, capture caps and maximum jitter\n";
    }

    static void testItems(ClickerEngine& engine, const std::filesystem::path& directory) {
        AppConfig config;
        config.soundFeedback = false;
        config.repeatCount = 2;
        config.globalIntervalMs = 1;
        config.items = {recorded(1, VK_F13, 20000), recorded(2, VK_F14, 30000), recorded(3, VK_F16, 10000)};
        config.items[2].enabled = false;
        ActionItem single;
        single.actionType = ActionType::KEY_PRESS;
        single.keyVk = VK_F15;
        config.items.insert(config.items.begin() + 1, single);
        auto profile = directory / "multiple.json";
        assert(ConfigManager::saveToFile(config, profile.string()));
        AppConfig restored;
        assert(ConfigManager::loadFromFile(restored, profile.string()));
        assert(restored.version == 2 && restored.items.size() == 4);
        for (size_t i : {size_t{0}, size_t{2}, size_t{3}}) {
            assert(restored.items[i].name == config.items[i].name);
            assert(KeyboardMacro::serialize(restored.items[i].recording) == KeyboardMacro::serialize(config.items[i].recording));
        }
        assert(!restored.items[3].enabled);
        AppConfig unchanged = restored;
        assert(!ConfigManager::deserializeFromJson(restored, "{\"items\":[{\"actionType\":5}]}"));
        assert(restored.items.size() == unchanged.items.size());
        assert(!ConfigManager::deserializeFromJson(restored,
            "{\"items\":[{\"actionType\":5,\"recording\":\"CLICKER_KEYS_V1 10 1\\n0 65 30 0\\n\"}]}"));

        for (auto mode : {ExecutionMode::SYNC, ExecutionMode::RECORDED_KEYS}) {
            config.mode = mode;
            engine.setConfig(config);
            engine.resetCount();
            sent.clear();
            assert(engine.start());
            finish(engine);
            std::vector<WORD> expected = mode == ExecutionMode::SYNC ?
                std::vector<WORD>{VK_F13, VK_F13, VK_F15, VK_F15, VK_F14, VK_F14} :
                std::vector<WORD>{VK_F13, VK_F13, VK_F14, VK_F14};
            assert(sent.size() == expected.size() * 2);
            for (size_t i = 0; i < sent.size(); ++i) assert(sent[i].input.ki.wVk == expected[i % expected.size()]);
            assert(engine.getTotalCount() == expected.size());
        }
        config.mode = ExecutionMode::RECORDED_KEYS;
        config.repeatCount = 1;
        std::swap(config.items[0], config.items[2]);
        engine.setConfig(config);
        sent.clear();
        assert(engine.start());
        finish(engine);
        assert(sent.size() == 4 && sent.front().input.ki.wVk == VK_F14);
        config.items[0].enabled = false;
        engine.setConfig(config);
        sent.clear();
        assert(engine.start());
        finish(engine);
        assert(sent.size() == 2 && sent.front().input.ki.wVk == VK_F13);
        config.items[2].enabled = false;
        engine.setConfig(config);
        assert(!engine.start());

        config.items = {recorded(1, VK_F13, 1000000)};
        config.repeatCount = 0;
        engine.setConfig(config);
        sent.clear();
        assert(engine.start());
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline) {
            { std::lock_guard lock(sentMutex); if (!sent.empty()) break; }
            Sleep(1);
        }
        engine.stop();
        assert(sent.size() == 2 && (sent.back().input.ki.dwFlags & KEYEVENTF_KEYUP));

        config.items = {recorded(1, VK_F13, 20000), recorded(2, VK_F14, 30000)};
        config.mode = ExecutionMode::ASYNC;
        engine.setConfig(config);
        engine.resetCount();
        sent.clear();
        assert(engine.start());
        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (engine.getTotalCount() < 4 && std::chrono::steady_clock::now() < deadline) Sleep(1);
        engine.stop();
        assert(engine.getTotalCount() >= 4);
        std::array<bool, 256> down{};
        bool saw13 = false, saw14 = false;
        for (const auto& event : sent) {
            WORD vk = event.input.ki.wVk;
            assert(vk == VK_F13 || vk == VK_F14);
            saw13 |= vk == VK_F13;
            saw14 |= vk == VK_F14;
            down[vk] = !(event.input.ki.dwFlags & KEYEVENTF_KEYUP);
        }
        assert(saw13 && saw14 && !down[VK_F13] && !down[VK_F14]);
        config.repeatCount = 2;
        engine.setConfig(config);
        engine.resetCount();
        sent.clear();
        assert(engine.start());
        finish(engine);
        assert(engine.getTotalCount() == 4 && sent.size() == 8);
        failInput = true;
        config.mode = ExecutionMode::SYNC;
        engine.setConfig(config);
        assert(engine.start());
        finish(engine);
        assert(engine.inputFailed());
        failInput = false;
        std::cout << "PASS: multiple items, profile data, Sync/Async, recorded-only filtering, order, disable, cancel and input failures\n";
    }

    static void testUI(const std::filesystem::path& directory) {
        wchar_t registryPath[256];
        assert(GetEnvironmentVariableW(L"CLICKER_TEST_REGISTRY", registryPath, 256));
        const wchar_t* originalRegistry = RegistryManager::REG_SUBKEY;
        RegistryManager::REG_SUBKEY = registryPath;
        size_t expectedItems = 0;
        {
            AppUI app(GetModuleHandleW(nullptr));
            app.m_config.version = 1;
            {
                std::ofstream legacy(directory / "recorded_keys.txt");
                legacy << KeyboardMacro::serialize(recorded(1, VK_F17, 10000).recording);
            }
            assert(app.create(SW_HIDE));
            assert(app.m_config.version == 2);
            assert(app.m_config.items.back().recording.events.front().vk == VK_F17);
            assert(GetDlgItem(app.m_hwndMain, 2021));
            app.handleMainMessage(app.m_hwndMain, WM_COMMAND, 2021, 0);
            assert(app.m_recordedMode);
            app.m_macro.setRecordingBlocked(true);
            key(app.m_macro, VK_F5);
            key(app.m_macro, VK_F5, LLKHF_UP);
            assert(!app.m_macro.isRecording() && app.m_macro.takeRecordings().empty());
            app.m_macro.setRecordingBlocked(false);
            size_t initial = app.m_config.items.size();
            app.m_config.items.front().id = (std::numeric_limits<int>::max)();
            for (WORD vk : {WORD{VK_F13}, WORD{VK_F14}}) {
                key(app.m_macro, VK_F5);
                key(app.m_macro, vk);
                Sleep(2);
                key(app.m_macro, vk, LLKHF_UP);
                key(app.m_macro, VK_F5, LLKHF_UP);
            }
            app.handleMainMessage(app.m_hwndMain, KeyboardMacro::WM_RECORDED, 0, 0);
            assert(app.m_config.items.size() == initial + 2);
            assert(app.m_config.items[initial].recording.events[0].vk == VK_F13);
            assert(app.m_config.items[initial + 1].recording.events[0].vk == VK_F14);
            assert(app.m_config.items[initial].id != app.m_config.items[initial + 1].id);
            assert(ListView_GetItemCount(app.m_hListView) == int(initial + 2));
            ListView_SetItemState(app.m_hListView, int(initial + 1), LVIS_SELECTED, LVIS_SELECTED);
            app.moveSelectedItem(true);
            assert(app.m_config.items[initial].recording.events[0].vk == VK_F14);
            ListView_SetCheckState(app.m_hListView, int(initial), FALSE);
            assert(!app.m_config.items[initial].enabled);
            AppConfig loaded;
            assert(RegistryManager::loadConfig(loaded));
            assert(loaded.items.size() == initial + 2 && !loaded.items[initial].enabled);
            assert(loaded.items[initial + 1].recording.events[0].vk == VK_F13);
            app.m_config.mode = ExecutionMode::SYNC;
            app.updateUIFromConfig();
            assert(!app.m_recordedMode && app.m_macro.m_hook);
            assert(!key(app.m_macro, VK_F5)); // F5 remains available to the target outside recording mode.
            key(app.m_macro, VK_F5, LLKHF_UP);
            key(app.m_macro, VK_F6, LLKHF_INJECTED);
            assert(!app.m_macro.isRecording());
            assert(app.m_macro.takeRecordings().empty());
            ListView_SetItemState(app.m_hListView, -1, 0, LVIS_SELECTED);
            ListView_SetItemState(app.m_hListView, int(initial), LVIS_SELECTED, LVIS_SELECTED);
            app.deleteSelectedItem();
            assert(app.m_config.items.size() == initial + 1);
            assert(app.m_config.items.back().recording.events[0].vk == VK_F13);
            expectedItems = app.m_config.items.size();
            DestroyWindow(app.m_hwndMain);
        }
        {
            AppUI restarted(GetModuleHandleW(nullptr));
            assert(restarted.create(SW_HIDE));
            assert(restarted.m_config.items.size() == expectedItems); // Import legacy data only once.
            assert(restarted.m_config.items.back().recording.events[0].vk == VK_F13);
            DestroyWindow(restarted.m_hwndMain);
        }
        RegistryManager::REG_SUBKEY = originalRegistry;
        assert(std::filesystem::exists(directory / "recorded_keys.txt"));
        std::cout << "PASS: hidden GUI, separate recordings, move/delete/enable, Registry, restart and legacy migration\n";
    }
};

int main() {
    wchar_t executable[32768];
    assert(GetModuleFileNameW(nullptr, executable, 32768));
    KeyboardMacroTest::run(std::filesystem::path(executable).parent_path());
}
