#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// WarpController — wraps `warp-cli` (mirrors the original warp.sh logic).
// Actions run on a detached worker thread so the render loop never blocks.
class WarpController {
public:
    // Named vnet (human label -> UUID). Order matches the original menu.
    struct Vnet {
        std::string name;
        std::string uuid;
    };

    enum class ActionState { Idle, Running, Ok, Fail };

    WarpController();
    ~WarpController();

    const std::vector<Vnet>& vnets() const { return vnets_; }

    // --- live status (updated by refreshStatus / background poll) ----------
    std::string status() const;        // "Connected" / "Disconnected" / "Unknown"
    std::string vnetName() const;      // human label of selected vnet, or "—"
    bool isConnected() const;

    // Re-query warp-cli status + vnet (blocking; cheap, call off the hot path
    // or via the throttled poll() below).
    void refreshStatus();
    // Throttled poll: starts a background refresh at most every `intervalSec`
    // (warp-cli takes ~0.3s; never run it on the render thread). Call each frame.
    void poll(double nowSec, double intervalSec = 2.0);

    // --- async actions -----------------------------------------------------
    // Kick off a warp-cli command on a worker thread. Returns false if an
    // action is already running. Result readable via actionState().
    bool connect();
    bool disconnect();
    bool setVnet(const std::string& uuid);

    ActionState actionState() const { return action_.load(); }
    std::string actionLabel() const;   // e.g. "Cloud Connect"
    // Move Ok/Fail back to Idle once the UI has shown the result.
    void clearAction();

private:
    bool runAsync(const std::string& label, std::vector<std::string> args);

    std::vector<Vnet> vnets_;

    mutable std::mutex statusMtx_;
    std::string status_{"Unknown"};
    std::string vnetName_{"—"};
    double lastPoll_{-1e9};

    std::atomic<ActionState> action_{ActionState::Idle};
    std::string actionLabel_;          // guarded by actionLblMtx_
    mutable std::mutex actionLblMtx_;
    std::thread worker_;
    std::thread pollWorker_;
    std::atomic<bool> polling_{false};
};
