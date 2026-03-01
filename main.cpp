#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <string>
#include <vector>

#include <libproc.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

const char* kDefaultSockPath = "/tmp/agent-engine.sock";
const int kDefaultIntervalSec = 3;

const char* getSockPath() {
    const char* p = getenv("AGENT_SOCK");
    return p && p[0] ? p : kDefaultSockPath;
}

int getHeartbeatIntervalSec() {
    const char* p = getenv("HEARTBEAT_INTERVAL_SEC");
    if (!p || !p[0]) return kDefaultIntervalSec;
    int v = atoi(p);
    return v > 0 ? v : kDefaultIntervalSec;
}

// 采集当前进程列表，容错：失败返回空数组，不退出
std::vector<std::pair<pid_t, std::string>> collectProcesses() {
    std::vector<std::pair<pid_t, std::string>> out;
#ifdef __APPLE__
    int bytes = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (bytes <= 0) return out;

    std::vector<pid_t> pids(static_cast<size_t>(bytes) / sizeof(pid_t) + 1);
    int ret = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), bytes);
    if (ret <= 0) return out;

    int num_pids = ret / static_cast<int>(sizeof(pid_t));
    for (int i = 0; i < num_pids; i++) {
        pid_t pid = pids[i];
        if (pid == 0) continue;

        struct proc_taskallinfo info;
        int n = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &info, sizeof(info));
        if (n != sizeof(info)) continue;

        char name[sizeof(info.pbsd.pbi_comm) + 1];
        memcpy(name, info.pbsd.pbi_comm, sizeof(info.pbsd.pbi_comm));
        name[sizeof(info.pbsd.pbi_comm)] = '\0';
        out.push_back({pid, std::string(name)});
    }
#endif
    return out;
}

std::string serializeHeartbeat(const std::vector<std::pair<pid_t, std::string>>& processes) {
    json j;
    j["ts"] = static_cast<int64_t>(time(nullptr));
    json arr = json::array();
    for (const auto& p : processes) {
        arr.push_back({{"pid", static_cast<int>(p.first)}, {"name", p.second}});
    }
    j["processes"] = std::move(arr);
    return j.dump() + "\n";
}

int connectUDS(const char* path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool sendLine(int fd, const std::string& line) {
    const char* p = line.data();
    size_t len = line.size();
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n <= 0) return false;
        p += static_cast<size_t>(n);
        len -= static_cast<size_t>(n);
    }
    return true;
}

}  // namespace

int main() {
    const char* sockPath = getSockPath();
    int intervalSec = getHeartbeatIntervalSec();

    int fd = -1;
    for (;;) {
        if (fd < 0) {
            fd = connectUDS(sockPath);
            if (fd < 0) {
                sleep(static_cast<unsigned>(intervalSec));
                continue;
            }
        }

        auto processes = collectProcesses();
        std::string line = serializeHeartbeat(processes);

        if (!sendLine(fd, line)) {
            close(fd);
            fd = -1;
            continue;
        }

        sleep(static_cast<unsigned>(intervalSec));
    }
    return 0;
}
