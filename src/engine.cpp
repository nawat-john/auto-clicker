#include "engine.hpp"
#include "config.hpp"
#include "keyboard_macro.hpp"
#include <windows.h>
#include <mmsystem.h>
#include <random>
#include <thread>
#include <iostream>

static thread_local std::mt19937 g_rng(1337);

ClickerEngine::ClickerEngine() {
    timeBeginPeriod(1);
    m_config = ConfigManager::getDefaultConfig();
}

ClickerEngine::~ClickerEngine() {
    stop();
    timeEndPeriod(1);
}

void ClickerEngine::setConfig(const AppConfig& config) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
}

AppConfig ClickerEngine::getConfig() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

void ClickerEngine::incrementCount(std::atomic<uint64_t>& count) {
    auto value = count.load(std::memory_order_relaxed);
    while (value != (std::numeric_limits<uint64_t>::max)() &&
        !count.compare_exchange_weak(value, value + 1, std::memory_order_relaxed)) {}
}

void ClickerEngine::preciseSleep(double milliseconds, const std::atomic<bool>& runningFlag) {
    if (milliseconds <= 0.0) return;

    auto start = std::chrono::high_resolution_clock::now();
    double targetMicroseconds = milliseconds * 1000.0;

    // Coarse sleep if target > 3ms to avoid 100% CPU utilization
    if (milliseconds > 3.0) {
        while (runningFlag.load(std::memory_order_relaxed)) {
            auto elapsed = std::chrono::duration<double, std::micro>(
                std::chrono::high_resolution_clock::now() - start
            ).count();
            double remaining = targetMicroseconds - elapsed;
            if (remaining <= 2000.0) break; // Last 2ms will be fine-grained
            
            // Sleep in 1ms increments
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Busy spin for sub-millisecond precision
    while (runningFlag.load(std::memory_order_relaxed)) {
        auto elapsed = std::chrono::duration<double, std::micro>(
            std::chrono::high_resolution_clock::now() - start
        ).count();
        if (elapsed >= targetMicroseconds) break;
        YieldProcessor();
    }
}

void ClickerEngine::executeAction(const ActionItem& item, uint32_t jitterMs) {
    if (item.actionType == ActionType::RECORDED_KEYS) return;
    (void)jitterMs;
    POINT originalPos = {0, 0};
    if (item.coordType == CoordType::FIXED_COORDS) {
        if (item.restoreMouse) {
            GetCursorPos(&originalPos);
        }
        SetCursorPos(item.x, item.y);
    }

    if (item.actionType == ActionType::KEY_PRESS) {
        // Keyboard simulation with scan code for game compatibility
        WORD vk = item.keyVk;
        UINT scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);

        INPUT inputs[2] = {};
        // Key Down
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = vk;
        inputs[0].ki.wScan = static_cast<WORD>(scanCode);
        inputs[0].ki.dwFlags = 0;

        // Key Up
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = vk;
        inputs[1].ki.wScan = static_cast<WORD>(scanCode);
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(1, &inputs[0], sizeof(INPUT));
        // Short press duration
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        SendInput(1, &inputs[1], sizeof(INPUT));
    } else {
        // Mouse simulation
        DWORD downFlag = 0;
        DWORD upFlag = 0;

        switch (item.actionType) {
            case ActionType::MOUSE_LEFT_CLICK:
            case ActionType::MOUSE_DOUBLE_LEFT:
                downFlag = MOUSEEVENTF_LEFTDOWN;
                upFlag = MOUSEEVENTF_LEFTUP;
                break;
            case ActionType::MOUSE_RIGHT_CLICK:
                downFlag = MOUSEEVENTF_RIGHTDOWN;
                upFlag = MOUSEEVENTF_RIGHTUP;
                break;
            case ActionType::MOUSE_MIDDLE_CLICK:
                downFlag = MOUSEEVENTF_MIDDLEDOWN;
                upFlag = MOUSEEVENTF_MIDDLEUP;
                break;
            default:
                break;
        }

        if (downFlag != 0 && upFlag != 0) {
            INPUT input = {};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = downFlag;
            SendInput(1, &input, sizeof(INPUT));

            std::this_thread::sleep_for(std::chrono::milliseconds(8));

            input.mi.dwFlags = upFlag;
            SendInput(1, &input, sizeof(INPUT));

            if (item.actionType == ActionType::MOUSE_DOUBLE_LEFT) {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));

                input.mi.dwFlags = downFlag;
                SendInput(1, &input, sizeof(INPUT));
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
                input.mi.dwFlags = upFlag;
                SendInput(1, &input, sizeof(INPUT));
            }
        }
    }

    if (item.coordType == CoordType::FIXED_COORDS && item.restoreMouse) {
        SetCursorPos(originalPos.x, originalPos.y);
    }
}

void ClickerEngine::playSound(bool starting) {
    if (!m_config.soundFeedback) return;
    std::thread([starting]() {
        if (starting) {
            Beep(988, 70);  // B5
            Beep(1318, 90); // E6
        } else {
            Beep(1318, 70); // E6
            Beep(880, 90);  // A5
        }
    }).detach();
}

bool ClickerEngine::executeItem(const ActionItem& item, uint32_t jitterMs) {
    if (item.actionType == ActionType::RECORDED_KEYS) {
        if (!KeyboardMacro::play(item.recording, m_running)) {
            m_inputFailed.store(true);
            m_running.store(false);
            if (m_statusCallback) m_statusCallback(false, m_totalCount.load());
            return false;
        }
    } else {
        executeAction(item, jitterMs);
    }
    if (m_running.load()) incrementCount(m_totalCount);
    return true;
}

bool ClickerEngine::start() {
    if (m_running.load()) return false;
    stop();

    // Check if there are active items
    m_inputFailed.store(false);
    std::vector<ActionItem> activeItems;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        for (const auto& item : m_config.items) {
            if (item.enabled && (m_config.mode != ExecutionMode::RECORDED_KEYS ||
                item.actionType == ActionType::RECORDED_KEYS)) {
                activeItems.push_back(item);
            }
        }
    }

    if (activeItems.empty()) {
        return false;
    }

    m_running.store(true);
    playSound(true);

    if (m_statusCallback) {
        m_statusCallback(true, m_totalCount.load());
    }

    m_workerThreads.clear();

    if (m_config.mode != ExecutionMode::ASYNC) {
        m_activeWorkers.store(1);
        m_workerThreads.emplace_back([this] { syncWorker(); m_activeWorkers.fetch_sub(1); });
    } else {
        m_activeWorkers.store(activeItems.size());
        // Async: spawn a thread for each enabled item
        for (size_t i = 0; i < activeItems.size(); ++i) {
            m_workerThreads.emplace_back([this, item = activeItems[i], i] {
                asyncItemWorker(item, i);
                if (m_activeWorkers.fetch_sub(1) == 1) {
                    m_running.store(false);
                    if (m_statusCallback) m_statusCallback(false, m_totalCount.load());
                }
            });
        }
    }

    return true;
}

void ClickerEngine::stop() {
    bool wasRunning = m_running.exchange(false);

    // Keep keyboard hook callbacks flowing while SendInput workers release their keys.
    while (m_activeWorkers.load() != 0) {
        MSG message;
        PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
        Sleep(1);
    }

    for (auto& t : m_workerThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_workerThreads.clear();

    if (!wasRunning) return;

    playSound(false);

    if (m_statusCallback) {
        m_statusCallback(false, m_totalCount.load());
    }
}

void ClickerEngine::syncWorker() {
    uint32_t cycleCount = 0;

    while (m_running.load(std::memory_order_relaxed)) {
        AppConfig cfg;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            cfg = m_config;
        }

        std::vector<ActionItem> enabledItems;
        for (const auto& it : cfg.items) {
            if (it.enabled && (cfg.mode != ExecutionMode::RECORDED_KEYS ||
                it.actionType == ActionType::RECORDED_KEYS)) enabledItems.push_back(it);
        }

        if (enabledItems.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        for (const auto& item : enabledItems) {
            if (!m_running.load(std::memory_order_relaxed)) break;

            // Delay before
            if (item.delayBeforeMs > 0) {
                preciseSleep(item.delayBeforeMs, m_running);
                if (!m_running.load(std::memory_order_relaxed)) break;
            }

            // Calculate Jitter
            int64_t jitter = 0;
            if (cfg.randomJitterMs > 0 && item.actionType != ActionType::RECORDED_KEYS) {
                std::uniform_int_distribution<int64_t> dist(-static_cast<int64_t>(cfg.randomJitterMs), cfg.randomJitterMs);
                jitter = dist(g_rng);
            }

            // Execute
            if (!executeItem(item, cfg.randomJitterMs)) break;

            // Delay after
            if (item.delayAfterMs > 0) {
                preciseSleep(item.delayAfterMs, m_running);
                if (!m_running.load(std::memory_order_relaxed)) break;
            }

            // Item interval wait
            if (item.actionType == ActionType::RECORDED_KEYS) continue;
            double baseInterval = item.useCustomRate ? item.intervalMs : cfg.globalIntervalMs;
            double interval = baseInterval + jitter;
            if (interval < 1.0) interval = 1.0;

            preciseSleep(interval, m_running);
        }

        if (cfg.repeatCount > 0 && ++cycleCount >= cfg.repeatCount) {
            // Reached repeat limit
            m_running.store(false);
            if (m_statusCallback) {
                m_statusCallback(false, m_totalCount.load());
            }
            break;
        }
    }
}

void ClickerEngine::asyncItemWorker(ActionItem item, size_t threadIndex) {
    (void)threadIndex;
    uint32_t cycleCount = 0;

    while (m_running.load(std::memory_order_relaxed)) {
        AppConfig cfg;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            cfg = m_config;
        }

        // Delay before
        if (item.delayBeforeMs > 0) {
            preciseSleep(item.delayBeforeMs, m_running);
            if (!m_running.load(std::memory_order_relaxed)) break;
        }

        // Calculate jitter
        int64_t jitter = 0;
        if (cfg.randomJitterMs > 0 && item.actionType != ActionType::RECORDED_KEYS) {
            std::uniform_int_distribution<int64_t> dist(-static_cast<int64_t>(cfg.randomJitterMs), cfg.randomJitterMs);
            jitter = dist(g_rng);
        }

        // Execute
        if (!executeItem(item, cfg.randomJitterMs)) break;

        // Delay after
        if (item.delayAfterMs > 0) {
            preciseSleep(item.delayAfterMs, m_running);
            if (!m_running.load(std::memory_order_relaxed)) break;
        }

        if (cfg.repeatCount > 0 && ++cycleCount >= cfg.repeatCount) break;
        if (item.actionType == ActionType::RECORDED_KEYS) continue;
        double baseInterval = item.useCustomRate ? item.intervalMs : cfg.globalIntervalMs;
        double interval = baseInterval + jitter;
        if (interval < 1.0) interval = 1.0;

        preciseSleep(interval, m_running);
    }
}
