#include "warp.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <sstream>

namespace {

// Run a command (argv-style, no shell) and capture stdout. Mirrors the
// `warp-cli ... 2>/dev/null` usage in the original script.
std::string runCapture(const std::vector<std::string>& args) {
    std::string cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) cmd += ' ';
        // naive quoting; our args are fixed (warp-cli + subcommands + UUIDs)
        cmd += '\'' + args[i] + '\'';
    }
    cmd += " 2>/dev/null";

    std::array<char, 256> buf{};
    std::string out;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return out;
    while (fgets(buf.data(), buf.size(), pipe.get())) out += buf.data();
    return out;
}

// Run a command for its exit code only (used by actions).
int runStatusOnly(const std::vector<std::string>& args) {
    std::string cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) cmd += ' ';
        cmd += '\'' + args[i] + '\'';
    }
    cmd += " >/dev/null 2>&1";
    int rc = system(cmd.c_str());
    return rc;  // 0 == success
}

// Extract the substring after the first occurrence of `key: `, up to EOL.
// Replicates the awk `-F': '` parse keyed on a marker line in warp.sh.
std::string fieldAfter(const std::string& text, const std::string& key) {
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        auto pos = line.find(key);
        if (pos == std::string::npos) continue;
        auto colon = line.find(": ", pos);
        if (colon == std::string::npos) continue;
        std::string val = line.substr(colon + 2);
        // trim trailing whitespace/CR
        while (!val.empty() && (val.back() == '\r' || val.back() == '\n' ||
                                val.back() == ' '))
            val.pop_back();
        return val;
    }
    return "";
}

}  // namespace

WarpController::WarpController() {
    // UUIDs copied verbatim from warp.sh:7-11.
    vnets_ = {
        {"default", "51d02f9b-41ec-4b0a-8691-072aa331acff"},
        {"TESP2", "a61c9382-38d9-488d-b735-a927532bb006"},
        {"TECE1", "55f51d19-3faf-43d9-9ddc-646a557edd1f"},
    };
    refreshStatus();
}

WarpController::~WarpController() {
    if (worker_.joinable()) worker_.join();
    if (pollWorker_.joinable()) pollWorker_.join();
}

std::string WarpController::status() const {
    std::lock_guard<std::mutex> lk(statusMtx_);
    return status_;
}

std::string WarpController::vnetName() const {
    std::lock_guard<std::mutex> lk(statusMtx_);
    return vnetName_;
}

bool WarpController::isConnected() const { return status() == "Connected"; }

void WarpController::refreshStatus() {
    // warp.sh:14 — parse "Status update: <X>" line.
    std::string raw = runCapture({"warp-cli", "status"});
    std::string st = fieldAfter(raw, "Status update");
    if (st.empty()) st = "Unknown";

    // warp.sh:15 — parse "Currently selected: <UUID>".
    std::string rawV = runCapture({"warp-cli", "vnet"});
    std::string uuid = fieldAfter(rawV, "Currently selected");

    // reverse lookup UUID -> name (warp.sh:24-26)
    std::string name = "—";
    for (const auto& v : vnets_)
        if (v.uuid == uuid) { name = v.name; break; }

    std::lock_guard<std::mutex> lk(statusMtx_);
    status_ = st;
    vnetName_ = name;
}

void WarpController::poll(double nowSec, double intervalSec) {
    // Don't poll while an action is mid-flight (status is in flux).
    if (action_.load() == ActionState::Running) return;
    if (nowSec - lastPoll_ < intervalSec) return;
    if (polling_.load()) return;  // previous refresh still running
    lastPoll_ = nowSec;
    if (pollWorker_.joinable()) pollWorker_.join();  // finished; reap it
    polling_.store(true);
    pollWorker_ = std::thread([this] {
        refreshStatus();
        polling_.store(false);
    });
}

std::string WarpController::actionLabel() const {
    std::lock_guard<std::mutex> lk(actionLblMtx_);
    return actionLabel_;
}

void WarpController::clearAction() {
    auto s = action_.load();
    if (s == ActionState::Ok || s == ActionState::Fail) {
        if (worker_.joinable()) worker_.join();
        action_.store(ActionState::Idle);
    }
}

bool WarpController::runAsync(const std::string& label,
                             std::vector<std::string> args) {
    if (action_.load() == ActionState::Running) return false;
    if (worker_.joinable()) worker_.join();  // reap a finished prior worker

    {
        std::lock_guard<std::mutex> lk(actionLblMtx_);
        actionLabel_ = label;
    }
    action_.store(ActionState::Running);

    worker_ = std::thread([this, args = std::move(args)]() {
        int rc = runStatusOnly(args);
        refreshStatus();
        action_.store(rc == 0 ? ActionState::Ok : ActionState::Fail);
    });
    return true;
}

bool WarpController::connect() {
    return runAsync("Cloud Connect", {"warp-cli", "connect"});
}

bool WarpController::disconnect() {
    return runAsync("Cloud Disconnect", {"warp-cli", "disconnect"});
}

bool WarpController::setVnet(const std::string& uuid) {
    std::string label = "Switch vnet";
    for (const auto& v : vnets_)
        if (v.uuid == uuid) { label = "Warp " + v.name; break; }
    return runAsync(label, {"warp-cli", "vnet", uuid});
}
