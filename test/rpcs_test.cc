#include <gflags/gflags.h>
#include <glog/logging.h>
#include <zmq.hpp>
#include <ocmg/common.pb.h>
#include "gtest/gtest.h"
#include <string>

namespace {

void rpc() {
  using oc::pb::common::BlacklistEntry;
  zmq::context_t context(1);
  zmq::socket_t rep_socket(context, ZMQ_REP);
  rep_socket.bind("tcp://*:5507");

  while (1) {

    zmq::message_t head;
    rep_socket.recv(&head);

    LOG(INFO) << "Received head: " << std::basic_string<char>(static_cast<const char *>(head.data()),head.size());

    ASSERT_TRUE(head.more());

    zmq::message_t arg;
    BlacklistEntry e;
    do {
      arg.rebuild();
      rep_socket.recv(&arg);
      e.Clear();
      e.ParseFromArray(arg.data(),arg.size());
      LOG(INFO) << "Received argument: " << std::basic_string<char>(static_cast<const char *>(arg.data()),arg.size());
      LOG(INFO) << "Received: " << e.ShortDebugString();
    } while(arg.more());

    zmq::message_t reply(2);
    memcpy(reply.data(), "OK", 2);
    rep_socket.send(reply, 0);
  }
}

}

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  ::rpc();
  return 0;
}
