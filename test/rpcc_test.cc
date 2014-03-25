#include <gflags/gflags.h>
#include <glog/logging.h>
#include <zmq.hpp>
#include "gtest/gtest.h"
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <forward_list>
#include "rpc/rpc.h"

namespace {

void rpc() {
  auto context = std::make_shared<zmq::context_t>(1);

  std::unique_ptr<oc::rpc::ClientConnection> con(new oc::rpc::ClientConnection(context, "tcp://localhost:5507"));

  // Main loop for this simple demo
  while (1) {

    std::vector<int> reply;
    std::vector<int> v = { 1, 2, 3 };
    oc::rpc::SendRequest(con.get(), "TestArgs", &reply, 1,  0.1234, "hello world", v);
    for (auto const& e : reply) {
      LOG(INFO) << "Received: " << e;
    }

    // For this demo simply sleep and do it again
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

}

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  ::rpc();
  return 0;
}
