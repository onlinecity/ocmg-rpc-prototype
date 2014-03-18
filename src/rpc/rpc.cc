//
// Copyright (C) 2014 OnlineCity
//

#include "rpc/rpc.h"

namespace oc {
namespace rpc {

void SendReplyHead(const std::unique_ptr<zmq::socket_t> &socket, int32_t code) {
  zmq::message_t reply_head(sizeof(int32_t));
  memcpy(reply_head.data(), &code, sizeof(int32_t));
  socket->send(reply_head, ZMQ_SNDMORE);
}

void SendReplyBody(const UniqueSocket &socket, const void *data, size_t length, bool send_more) {
  zmq::message_t body(length);
  memcpy(body.data(), data, length);
  socket->send(body, send_more ? ZMQ_SNDMORE : 0);
}

void SendReplyBody(const std::unique_ptr<zmq::socket_t> &socket,
                   const google::protobuf::MessageLite &arg,
                   bool send_more) {
  int s = arg.ByteSize();
  zmq::message_t body(s);
  arg.SerializeToArray(body.data(), s);
  socket->send(body, send_more ? ZMQ_SNDMORE : 0);
}

void SendReply(const std::unique_ptr<zmq::socket_t> &socket, const void *data, size_t length) {
  // Send head with 1 argument
  SendReplyHead(socket, 1l);
  // Send argument
  zmq::message_t reply_body(length);
  memcpy(reply_body.data(), data, length);
  socket->send(reply_body, 0);
}

}  // namespace rpc
}  // namespace oc
