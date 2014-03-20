//
// Copyright (C) 2014 OnlineCity
//

#include "rpc/rpc.h"

namespace oc {
namespace rpc {

void SendRequestMethodName(const UniqueSocket &socket, const char *method_name, bool send_more) {
  size_t s = strlen(method_name);
  zmq::message_t head(s);
  memcpy(head.data(), method_name, s);
  CHECK(socket->send(head, send_more ? ZMQ_SNDMORE : 0));
}

int32_t RecvReplyHead(const UniqueSocket &socket, bool multipart) {
  zmq::message_t msg(sizeof(int32_t));
  CHECK(socket->recv(&msg));
  CHECK_EQ(msg.more(),multipart) << "Received mismatched multipart/singlepart";
  return *static_cast<int32_t *>(msg.data());
}

UniqueSocket Socket(const std::shared_ptr<zmq::context_t> &context, int type, const char *addr) {
  assert(type == ZMQ_REP || type == ZMQ_REQ);  // only support REQ,REP
  std::unique_ptr<zmq::socket_t> socket(new zmq::socket_t(*context, type));
  socket->connect(addr);

  if (type == ZMQ_REQ) {
    // Configure socket to not wait at close time
    int linger = 0;
    socket->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
  }
  return socket;
}

void SendReplyHead(const std::unique_ptr<zmq::socket_t> &socket, int32_t code) {
  zmq::message_t head(sizeof(int32_t));
  memcpy(head.data(), &code, sizeof(int32_t));
  auto did_send = socket->send(head, code != 0 ? ZMQ_SNDMORE : 0);
  assert(did_send == true);
}

void SendBody(const UniqueSocket &socket, const void *data, size_t length, bool send_more) {
  zmq::message_t body(length);
  memcpy(body.data(), data, length);
  auto did_send = socket->send(body, send_more ? ZMQ_SNDMORE : 0);
  assert(did_send);
}

void SendBody(const std::unique_ptr<zmq::socket_t> &socket,
                   const google::protobuf::MessageLite &arg,
                   bool send_more) {
  int s = arg.ByteSize();
  zmq::message_t body(s);
  arg.SerializeToArray(body.data(), s);
  auto did_send = socket->send(body, send_more ? ZMQ_SNDMORE : 0);
  assert(did_send);
}

void SendReply(const std::unique_ptr<zmq::socket_t> &socket, const void *data, size_t length) {
  // Send head with 1 argument
  SendReplyHead(socket, 1);
  // Send argument
  zmq::message_t reply_body(length);
  memcpy(reply_body.data(), data, length);
  auto did_send = socket->send(reply_body, 0);
  assert(did_send);
}

}  // namespace rpc
}  // namespace oc


