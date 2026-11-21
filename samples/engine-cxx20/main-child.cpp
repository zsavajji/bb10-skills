// phoenixd, the engine: phoenixd [--data DIR] | --version
// NDJSON on stdin/stdout (core/dispatcher.h). stdout is the protocol: logs go to the log file.
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "core/commands.h"
#include "core/dispatcher.h"
#include "core/media_commands.h"
#include "core/reddit_commands.h"
#include "net/http.h"
#include "util/log.h"

#ifndef PHX_VERSION
#define PHX_VERSION "0"
#endif

namespace {
bool make_dirs(const std::string &path) {
    for (size_t i = 1; i <= path.size(); i++)
        if (i == path.size() || path[i] == '/')
            if (::mkdir(path.substr(0, i).c_str(), 0700) != 0 && errno != EEXIST) return false;
    return true;
}
}  // namespace

int main(int argc, char **argv) {
    std::string data;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--version") {
            std::printf("phoenixd %s\n", PHX_VERSION);
            return 0;
        }
        if (a == "--data" && i + 1 < argc) data = argv[++i];
        else if (a.rfind("--", 0) == 0) {
            std::fprintf(stderr, "usage: phoenixd [--data DIR] | --version\n");
            return 2;
        }
        // other arguments (the launcher's own) are ignored
    }

    ::signal(SIGPIPE, SIG_IGN);
    // SIGINT/SIGTERM go to one thread that stops the dispatcher; blocked before any other thread.
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &sigs, nullptr);

    if (data.empty()) {
        const char *home = std::getenv("HOME");
        data = home && *home ? home : "data";
    }
    if (!make_dirs(data)) {
        std::fprintf(stderr, "phoenixd: cannot create %s: %s\n", data.c_str(), std::strerror(errno));
        return 1;
    }
    const std::string log_path = data + "/phoenixd.log";
    if (!phx::set_log_file(log_path)) std::fprintf(stderr, "phoenixd: cannot write %s\n", log_path.c_str());
    phx::log_info(std::string("phoenixd ") + PHX_VERSION + ", data in " + data);
#ifdef __QNX__
    if (::access("app/native/assets/cacert.pem", R_OK) == 0) phx::http_set_ca_file("app/native/assets/cacert.pem");
#endif

    phx::Dispatcher d(0, 1);
    phx::register_core_commands(d, data);
    // Each owner holds a module's state and threads: dropped, in reverse order, once run() returns.
    std::shared_ptr<void> reddit = phx::register_reddit_commands(d, data);
    std::shared_ptr<void> media = phx::register_media_commands(d, data);
    std::thread([&d, sigs] {
        int sig = 0;
        if (sigwait(&sigs, &sig) == 0) {
            phx::log_info("signal " + std::to_string(sig) + ": stopping");
            d.stop();
        }
    }).detach();
    d.run();
    media.reset();
    reddit.reset();
    phx::log_info("bye");
    return 0;
}
