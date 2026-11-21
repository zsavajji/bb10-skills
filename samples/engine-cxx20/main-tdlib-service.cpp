// tengramd: the engine of Tengram, a Telegram client for BlackBerry 10. The UI reads chats.db and talks to
// the engine over a loopback socket (docs/protocol.md). Usage: tengramd <data-dir>, with TG_API_ID and
// TG_API_HASH in the environment.
//
// This thread owns everything (the database, the sockets, all state) and sleeps in poll(). The TDLib thread
// (td.cpp) only receives and queues; a third thread waits for signals. The engine has no timer of its own:
// idle, it wakes only for TDLib or the UI. The exceptions are the 5 s limit of a stop, the flush, which
// comes 50 ms after something changed, and the pruning of downloaded media every 6 hours (data.cpp).

// [...] includes, say(), globals, the protocol dispatch: app logic left out

// The headless service runs under limits the launcher sets (RLIMIT_DATA from launcher.cfg's
// headless_heapsize_limit_mb, and QNX's RLIMIT_FREEMEM, a share of the whole phone's memory): the first sync
// after a login died with bad_alloc in 0.1.0.3. They are logged, and each soft limit is raised to its hard one,
// which a process may do for itself. RLIM_INFINITY prints as "inf".
void limits(bool raise) {
  static const struct { int id; const char *name; } kLimits[] = {
      {RLIMIT_DATA, "DATA"}, {RLIMIT_AS, "AS"}, {RLIMIT_STACK, "STACK"},
#ifdef RLIMIT_FREEMEM
      {RLIMIT_FREEMEM, "FREEMEM"},
#endif
  };
  auto show = [](rlim_t v) {
    return v == RLIM_INFINITY ? std::string("inf") : std::to_string(static_cast<unsigned long long>(v));
  };
  for (const auto &l : kLimits) {
    struct rlimit r;
    if (getrlimit(l.id, &r) != 0) {
      say("rlimit %s: %s", l.name, std::strerror(errno));
      continue;
    }
    std::string done;
    if (raise && r.rlim_cur != r.rlim_max) {
      struct rlimit up = {r.rlim_max, r.rlim_max};
      done = setrlimit(l.id, &up) == 0 ? ", raised to the hard limit" : std::string(", raise: ") + std::strerror(errno);
    }
    say("rlimit %s: soft %s, hard %s%s", l.name, show(r.rlim_cur).c_str(), show(r.rlim_max).c_str(), done.c_str());
    // The UI reads this (the link body's memMB): the limit the engine really runs under, after the raise.
    if (raise && l.id == RLIMIT_DATA) {
      struct rlimit now;
      mem_mb = getrlimit(RLIMIT_DATA, &now) == 0 && now.rlim_cur != RLIM_INFINITY ? static_cast<int>(now.rlim_cur >> 20) : 0;
    }
  }
}

// The first failed allocation says when, with the limits as they are then; the next one throws as usual.
void out_of_memory() {
  std::set_new_handler(nullptr);
  say("out of memory: an allocation failed");
  limits(false);
}

int main(int argc, char **argv) {
  if (argc == 2 && std::strcmp(argv[1], "-h") == 0) {
    usage(stdout);
    return 0;
  }
  // A test run names its data folder (--data). Without it this is the packaged service, started by the
  // navigator with no arguments we choose (docs/ui-fork.md §5): its folder is the sandbox's, HOME, where the UI
  // looks for engine.json (QDir::homePath()). Anything else on its command line is logged and ignored, as
  // HyperBerry's Go flags did: 0.1.0.2 took a stray argument for the folder and exited unseen in 60 ms.
  const char *dir = nullptr;
  std::string ignored;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--service") == 0) {
      service = true;
    } else if (std::strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
      dir = argv[++i];
    } else {
      ignored += std::string(" [") + argv[i] + "]";
    }
  }
  if (!dir) {
    dir = std::getenv("HOME");
    service = true;
    // A headless app's stderr goes nowhere: the log is a file in the data folder, the previous one kept once
    // it passes 1 MB. Before anything else, so an early failure is seen too.
    if (dir && *dir) {
      const std::string logs = std::string(dir) + "/logs";
      mkdir(logs.c_str(), 0700);
      const std::string log = logs + "/tengramd.log";
      struct stat st;
      if (stat(log.c_str(), &st) == 0 && st.st_size > (1 << 20)) {
        std::rename(log.c_str(), (log + ".old").c_str());
      }
      if (std::freopen(log.c_str(), "a", stderr)) {
        setvbuf(stderr, nullptr, _IOLBF, 0);
      }
    }
  }
  say("start: %s, data %s%s%s", service ? "service" : "test run", dir ? dir : "(no HOME)",
      ignored.empty() ? "" : ", ignored:", ignored.c_str());
  if (!dir || !*dir) {
    usage(stderr);
    return 2;
  }
  limits(true);
  std::set_new_handler(out_of_memory);
  // The API id and hash are compiled in from the build's environment (scripts/build-app.sh), never kept in a
  // file of the repository; TG_API_ID and TG_API_HASH in the environment override them (test runs under /root).
#ifdef TENGRAM_API_ID
  api_id = TENGRAM_API_ID;
  api_hash = TENGRAM_API_HASH;
#endif
  const char *id_env = std::getenv("TG_API_ID");
  const char *hash_env = std::getenv("TG_API_HASH");
  if (id_env && hash_env && std::atoi(id_env) > 0 && *hash_env) {
    api_id = std::atoi(id_env);
    api_hash = hash_env;
  }
  if (api_id <= 0 || api_hash.empty()) {
    std::fprintf(stderr, "set TG_API_ID (a number) and TG_API_HASH (from my.telegram.org)\n");
    return 2;
  }

  // Signals are blocked in every thread (TDLib's inherit the mask) and TERM, INT and HUP are taken by one
  // sigwait thread, because QNX has no usable sigaltstack or SA_RESTART. SIGPIPE is ignored and blocked.
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGHUP);
  sigaddset(&set, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &set, nullptr);
  signal(SIGPIPE, SIG_IGN);
  sigdelset(&set, SIGPIPE);

  umask(077);  // chats.db and everything else the engine makes: this user only
  make_dirs(data_dir = dir);
  char real[PATH_MAX];
  if (!realpath(data_dir.c_str(), real)) {
    std::fprintf(stderr, "cannot make the data directory %s: %s\n", data_dir.c_str(), std::strerror(errno));
    return 1;
  }
  data_dir = real;
  say("tengramd, data in %s", data_dir.c_str());

  // One engine per data folder: a second one would replace engine.json and then fail on TDLib's own lock.
  // The lock goes with the process, so nothing is left behind after a crash.
  const int lock_fd = open((data_dir + "/engine.lock").c_str(), O_WRONLY | O_CREAT, 0600);
  struct flock lock = {};
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  if (lock_fd < 0 || fcntl(lock_fd, F_SETLK, &lock) < 0) {
    std::fprintf(stderr, "another tengramd is using %s\n", data_dir.c_str());
    return 3;
  }

  bool fresh_key = false;
  if (!load_db_key(data_dir, fresh_key) || !db.open(data_dir, fresh_key) || !server.start(data_dir)) {
  // [...] app logic left out: the database key, the server start, the poll() loop, the stop
}
