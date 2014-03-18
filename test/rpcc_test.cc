#include <gflags/gflags.h>
#include <glog/logging.h>
#include <zmq.hpp>
#include <ocmg/common.pb.h>
#include "gtest/gtest.h"
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <forward_list>

namespace {

std::unique_ptr<zmq::socket_t> connect(const std::unique_ptr<zmq::context_t> &context) {
  auto host = "tcp://localhost:5507";
  //auto host = "tcp://10.99.0.62:7246";
  LOG(INFO) << "Connecting to " << host;
  std::unique_ptr<zmq::socket_t> socket(new zmq::socket_t(*context, ZMQ_REQ));
  socket->connect(host);

  // Configure socket to not wait at close time
  int linger = 0;
  socket->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
  return socket;
}

void rpc() {
  using oc::pb::common::BlacklistEntry;
  std::unique_ptr<zmq::context_t> context(new zmq::context_t(1));
  auto socket = ::connect(context);

  // Define by some other means
  const int timeout = 1000; // milliseconds
  const int retries = 3;

  // Main loop for this simple demo
  while (1) {

    // Construct a request head
    auto method_name = std::string("checkBlacklist");
    zmq::message_t head(method_name.length());
    memcpy(head.data(), method_name.c_str(), method_name.length());

    // Construct a list of arguments
    std::forward_list<BlacklistEntry> args;
    BlacklistEntry e;
    for (int i = 0; i < 5; i++) {
      e.set_hash("0800fc577294c34e0b28ad2839435945");
      e.set_msisdn(4526159917ULL);
      args.push_front(e);
    }

    // Lazy pirate pattern: (http://zguide.zeromq.org/page:all#Client-Side-Reliability-Lazy-Pirate-Pattern)
    int retries_left = retries;
    do {
      // Send the request
      zmq::message_t head(method_name.length());
      memcpy(head.data(), method_name.c_str(), method_name.length());
      ASSERT_TRUE(socket->send(head, ZMQ_SNDMORE));

      for (auto arg = args.cbegin(); arg != args.cend();) {
        int s = arg->ByteSize();
        zmq::message_t body(s);
        arg->SerializeToArray(body.data(), s);

        bool has_more = ++arg != args.cend();  // Advance iterator and check

        ASSERT_TRUE(socket->send(body, has_more ? ZMQ_SNDMORE : 0));
      }

      // Poll for a reply (with timeout)
      zmq::pollitem_t items[] = { { *socket, 0, ZMQ_POLLIN, 0 } };
      zmq::poll(&items[0], 1, timeout);
      if (items[0].revents & ZMQ_POLLIN) {
        zmq::message_t reply_head;
        socket->recv(&reply_head);

        ASSERT_TRUE(reply_head.more());

        int32_t reply_code = *static_cast<int32_t *>(reply_head.data());
        LOG(INFO) << "Reply code: " << reply_code;

        zmq::message_t reply;
        do {
          socket->recv(&reply);
          LOG(INFO) << "Received (" << reply.size() << "): " << std::string(static_cast<const char *>(reply.data()),reply.size());
        } while (reply.more());
        break; // break retry loop

      } else if (--retries_left == 0) {
        LOG(FATAL) << "Gave up after " << retries << " retries";
      } else {
        LOG(WARNING) << "Timed out, retry";
        // Old socket will be confused; close it and open a new one
        socket = ::connect(context);
      }
    } while (retries_left > 0);

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

