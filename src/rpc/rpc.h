//
// Copyright (C) 2014 OnlineCity
//

#pragma once

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <google/protobuf/message_lite.h>
#include <zmq.hpp>
#include <memory>
#include <iterator>
#include <string>
#include <cstdint>
#include <type_traits>

namespace oc {
namespace rpc {

typedef std::unique_ptr<zmq::socket_t> UniqueSocket;

// Stub functions used below
void SendReplyHead(const UniqueSocket &socket, int32_t code);
void SendReplyBody(const UniqueSocket &socket, const void *data, size_t length, bool send_more);
void SendReplyBody(const UniqueSocket &socket, const google::protobuf::MessageLite &arg, bool send_more);

// Templated overloaded stub function for integral types
template <class Integral, class = typename std::enable_if<std::is_integral<Integral>::value >::type>
inline void SendReplyBody(const UniqueSocket &socket, const Integral arg, bool send_more) {
  static_assert(std::is_integral<Integral>::value,
                "SendReplyBody(socket, arg, send_more) arg parameter must be an integral type.");
  SendReplyBody(socket, &arg, sizeof(Integral), send_more);
}

// Template overloaded stub for objects wrapped in unique_ptrs
template <class T>
inline void SendReplyBody(const UniqueSocket &socket, const std::unique_ptr<T> &arg, bool send_more) {
  SendReplyBody(socket, *arg, send_more);
}

// Send a reply with some bytes
// ie. SendReply(socket, "OK", 2);
void SendReply(const UniqueSocket &socket, const void *data, size_t length);

// Send a reply with a (single) protobuf message
// ie. SendReply(socket, foo);
inline void SendReply(const UniqueSocket &socket, const google::protobuf::MessageLite &arg) {
  SendReplyHead(socket, 1l);
  SendReplyBody(socket, arg, false);
}

// Send a reply with a list of elements, element must be integral or protobuf message
// ie.
//   std::vector<int> vec;
//   vec.push_back(1);
//   vec.push_back(2);
//   vec.push_back(3);
//   SendReply(socket, vec.cbegin(), vec.cend(), vec.size());
template<class Iter>
inline void SendReply(const UniqueSocket &socket, Iter first, Iter last, size_t num_args) {
  SendReplyHead(socket, num_args);
  while (first != last) {
    bool has_more = std::next(first) != last;
    SendReplyBody(socket, *first, has_more);
    ++first;
  }
}

// Send a reply with a list of elements, element must be integral or protobuf message
template<class Iter>
inline void SendReply(const UniqueSocket &socket, Iter first, Iter last) {
  int32_t num_args = static_cast<int32_t>(std::distance(first, last));
  SendReply(socket, first, last, num_args);
}

// Send a stringy reply
// ie SendReply(socket, std::string("OK"));
template <class CharType>
inline void SendReply(const UniqueSocket &socket, const std::basic_string<CharType> &s) {
  SendReply(socket, s.c_str(), s.length());
}
inline void SendReply(const UniqueSocket &socket, const char *s) {
  SendReply(socket, static_cast<std::string>(s));
}

// Send a reply with a integral type (int, char, float etc)
// ie.
//  SendReply(socket, 1337l);
//  SendReply(socket, 1);
//  SendReply(socket, 0.1234);
//  SendReply(socket, true);
template <class Integral, class = typename std::enable_if<std::is_integral<Integral>::value >::type>
inline void SendReply(const UniqueSocket &socket, const Integral arg) {
  static_assert(std::is_integral<Integral>::value, "SendReply(socket, arg) arg parameter must be an integral type.");
  SendReply(socket, &arg, sizeof(Integral));
}


}  // namespace rpc
}  // namespace oc
