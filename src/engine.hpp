#pragma once

#include "models.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <chrono>

class ClickerEngine {
public:
    using StatusCallback = std::function<void(bool isRunning, uint64_t totalCount)>;

    ClickerEngine();
    ~ClickerEngine();

    void setConfig(const AppConfig& config);
    AppConfig getConfig() const;

    bool start();
    void stop();
    bool isRunning() const { return m_running.load(); }
    uint64_t getTotalCount() const { return m_totalCount.load(); }
    void resetCount() { m_totalCount.store(0); }

    void setStatusCallback(StatusCallback callback) { m_statusCallback = callback; }

    static void executeAction(const ActionItem& item, uint32_t jitterMs);
    static void preciseSleep(double milliseconds, const std::atomic<bool>& runningFlag);

private:
    void syncWorker();
    void asyncItemWorker(ActionItem item, size_t threadIndex);
    void playSound(bool starting);

    AppConfig m_config;
    mutable std::mutex m_configMutex;

    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_totalCount{0};

    std::vector<std::thread> m_workerThreads;
    StatusCallback m_statusCallback;
};
