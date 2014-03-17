#include <gflags/gflags.h>
#include <glog/logging.h>
#include <zmq.hpp>
#include <ocmg/common.pb.h>
#include "gtest/gtest.h"
#include <string>
#include <thread>
#include <chrono>

namespace {

void rpc() {
  using oc::pb::common::BlacklistEntry;
  zmq::context_t context(1);
  zmq::socket_t req_socket(context, ZMQ_REQ);
  req_socket.connect("tcp://localhost:5507");

  while (1) {

    // Construct a request
    auto method_name = std::string("checkBlacklist");
    zmq::message_t head(method_name.length());
    memcpy(head.data(), method_name.c_str(), method_name.length());

    req_socket.send(head, ZMQ_SNDMORE);

    BlacklistEntry e;
    for (int i = 0; i < 5; i++) {
      e.set_hash("0800fc577294c34e0b28ad2839435945");
      e.set_msisdn(4526159917ULL);
      int s = e.ByteSize();
      zmq::message_t body(s);
      e.SerializeToArray(body.data(), s);
      req_socket.send(body, (i < 4) ? ZMQ_SNDMORE : 0);
    }

    zmq::message_t reply;
    req_socket.recv(&reply);
    LOG(INFO) << "Received: " << std::basic_string<char>(static_cast<const char *>(reply.data()),reply.size());

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
