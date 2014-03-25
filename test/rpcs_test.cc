#include <gflags/gflags.h>
#include <glog/logging.h>
#include <zmq.hpp>
#include "gtest/gtest.h"
#include <string>
#include <chrono>
#include <thread>
#include <memory>
#include "rpc/rpc.h"

namespace {

void rpc() {
  zmq::context_t context(1);
  std::unique_ptr<zmq::socket_t> rep_socket(new zmq::socket_t(context, ZMQ_REP));
  rep_socket->bind("tcp://*:5507");

  while (1) {

    zmq::message_t head;
    rep_socket->recv(&head);

    LOG(INFO) << "Received head (" << head.size() << "): " << std::basic_string<char>(static_cast<const char *>(head.data()),head.size());

    if (head.more()) {
      zmq::message_t arg;
      do {
        arg.rebuild();
        rep_socket->recv(&arg);
        LOG(INFO) << "Received arg (" << arg.size() << "): " << std::basic_string<char>(static_cast<const char *>(arg.data()),arg.size());
      } while(arg.more());
    }

    //std::this_thread::sleep_for(std::chrono::seconds(2));

    std::vector<int> vec = { 1, 2, 3 };
    oc::rpc::SendReply(rep_socket, vec.cbegin(), vec.cend(), vec.size());
  }
}

}

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  ::rpc();
  return 0;
}
