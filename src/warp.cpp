#include "warp.hpp"

#include <cstdio>
#include <cstdlib>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

// ---- locating warp-cli -----------------------------------------------------
// GUI apps started from a desktop menu / Finder get a minimal PATH, so besides
// searching PATH we try the places Cloudflare's installers put warp-cli.
// WARP_CLI=/path/to/warp-cli overrides everything.

bool isExecutable(const std::string& p) {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    return access(p.c_str(), X_OK) == 0;
#endif
}

std::string findWarpCli() {
    if (const char* env = std::getenv("WARP_CLI"); env && *env) return env;
#ifdef _WIN32
    const char sep = ';';
    const char* exe = "warp-cli.exe";
#else
    const char sep = ':';
    const char* exe = "warp-cli";
#endif
    if (const char* path = std::getenv("PATH")) {
        std::stringstream ss(path);
        std::string dir;
        while (std::getline(ss, dir, sep))
            if (!dir.empty() && isExecutable(dir + "/" + exe)) return dir + "/" + exe;
    }
#ifdef _WIN32
    std::string pf = std::getenv("ProgramFiles") ? std::getenv("ProgramFiles") : "C:\\Program Files";
    const std::string fallbacks[] = {pf + "\\Cloudflare\\Cloudflare WARP\\warp-cli.exe"};
#else
    const std::string fallbacks[] = {
        "/usr/local/bin/warp-cli", "/usr/bin/warp-cli", "/opt/homebrew/bin/warp-cli",
        "/Applications/Cloudflare WARP.app/Contents/Resources/warp-cli"};
#endif
    for (const auto& f : fallbacks)
        if (isExecutable(f)) return f;
    return exe;  // let the OS try; failures surface as Unknown / FAILED
}

const std::string& warpCli() {
    static const std::string path = findWarpCli();
    return path;
}

// ---- running it ---------------------------------------------------------------
// Runs args (args[0] == "warp-cli" is replaced by the resolved path) with no
// shell-visible window, stderr discarded. Captures stdout into *out if given.
// Returns the exit code (0 == success), or -1 if it could not be started.

#ifdef _WIN32
std::string quoteArg(const std::string& a) {
    if (!a.empty() && a.find_first_of(" \t\"") == std::string::npos) return a;
    std::string q = "\"";
    for (char c : a) {
        if (c == '"') q += '\\';
        q += c;
    }
    return q + "\"";
}

int runProcess(std::vector<std::string> args, std::string* out) {
    args[0] = warpCli();
    std::string cmd;
    for (size_t i = 0; i < args.size(); ++i) cmd += (i ? " " : "") + quoteArg(args[i]);

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE nul = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    HANDLE rd = nullptr, wr = nullptr;
    if (out) {
        CreatePipe(&rd, &wr, &sa, 0);
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    }
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = out ? wr : nul;
    si.hStdError = nul;
    PROCESS_INFORMATION pi{};
    std::vector<char> line(cmd.begin(), cmd.end());
    line.push_back('\0');
    // CREATE_NO_WINDOW: this is a GUI app; don't flash a console per call.
    BOOL ok = CreateProcessA(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                             nullptr, nullptr, &si, &pi);
    if (wr) CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) {
        if (rd) CloseHandle(rd);
        return -1;
    }
    if (out) {
        char buf[256];
        DWORD n = 0;
        while (ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n) out->append(buf, n);
        CloseHandle(rd);
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(code);
}
#else
int runProcess(std::vector<std::string> args, std::string* out) {
    args[0] = warpCli();
    std::string cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) cmd += ' ';
        cmd += '\'';
        for (char c : args[i]) cmd += (c == '\'') ? std::string("'\\''") : std::string(1, c);
        cmd += '\'';
    }
    cmd += out ? " 2>/dev/null" : " >/dev/null 2>&1";
    if (!out) {
        int rc = std::system(cmd.c_str());
        return (rc != -1 && WIFEXITED(rc)) ? WEXITSTATUS(rc) : -1;
    }
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return -1;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) *out += buf;
    int rc = pclose(pipe);
    return (rc != -1 && WIFEXITED(rc)) ? WEXITSTATUS(rc) : -1;
}
#endif

std::string runCapture(const std::vector<std::string>& args) {
    std::string out;
    runProcess(args, &out);
    return out;
}

int runStatusOnly(const std::vector<std::string>& args) { return runProcess(args, nullptr); }

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
    std::string name = "-";
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
