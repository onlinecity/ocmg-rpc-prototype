/**
 * Copyright (C) 2012 OnlineCity
 */

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <zmq.hpp>
#include <string>
#include <vector>
#include <csignal>
#include <fstream>
#include <chrono>
#include <thread>
#include "daemon.h"

using std::string;

namespace {

std::shared_ptr<zmq::context_t> zmq_context;
std::atomic<bool> stop(false);

void signalCallback(int signum) {
    LOG(INFO) << "Shutting down";
    stop = true;
    if (zmq_context) zmq_context->close();
}

}  // unnamed namespace

DEFINE_bool(daemon, false, "Run as a daemon");
DEFINE_string(pidfile, "/var/run/ocmg-cpp-messagerouter.pid", "PID file");

int main(int argc, char **argv) {

    // Initialize gflags and glog
    string usage("Starts ocmg-cpp-messagerouter");
    google::SetUsageMessage(usage);
    google::SetVersionString("0.1");
    google::ParseCommandLineFlags(&argc, &argv, true);

    // Basic BSD daemon
    if (FLAGS_daemon) {
      oc_daemon(1,FLAGS_logtostderr || FLAGS_alsologtostderr);
      if (FLAGS_pidfile.length() > 0) {
        std::ofstream pidfile(FLAGS_pidfile, std::ofstream::out|std::ofstream::trunc);
        if (!pidfile || !pidfile.is_open()) {
          LOG(ERROR) << "Can't create pid file";
        } else {
          pidfile << getpid();
          pidfile.close();
        }
      }
    }

    google::InitGoogleLogging(argv[0]);

    zmq_context = std::shared_ptr<zmq::context_t>(new zmq::context_t(1));

    // add signal handler
    signal(SIGINT, signalCallback);

    // Main thread loop
    while(!stop) {
      std::this_thread::yield();
      std::this_thread::sleep_for(std::chrono::seconds(1));
#ifdef NDEBUG
      google::FlushLogFiles(google::WARNING);
#else
      google::FlushLogFiles(google::INFO);
#endif
    }
    return 0;
}
