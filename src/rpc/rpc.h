//
// Copyright (C) 2014 OnlineCity
//

#pragma once

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <google/protobuf/message_lite.h>
#include <zmq.hpp>
#include <cstdint>
#include <cassert>
#include <memory>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

namespace oc {
namespace rpc {

typedef std::unique_ptr<zmq::socket_t> UniqueSocket;
typedef std::shared_ptr<zmq::context_t> SharedContext;

const int default_retries = 3;
const int default_timeout = 30000;  // miliseconds

UniqueSocket Socket(const SharedContext &context, int type, const char *addr);

class ClientConnection {
 public:
  std::unique_ptr<zmq::socket_t> socket_;
  std::shared_ptr<zmq::context_t> context_;
  std::string addr_;
  ClientConnection(const SharedContext &context, const char *addr) : context_(context), addr_(addr) {
    socket_ = Socket(context, ZMQ_REQ, addr);
  }
  ClientConnection(const SharedContext &context, const UniqueSocket &socket, const char *addr) :
    socket_(socket.get()),
    context_(context),
    addr_(addr) {

  }
  void reconnect() {
    socket_.reset(Socket(context_, ZMQ_REQ, addr_.c_str()).release());
  }
};

//
// SendBody* functions below format various input to zmq::message_t and sends it
// These functions are used for both replies and request arguments
//
void SendBody(const UniqueSocket &socket, const void *data, size_t length, bool send_more);
void SendBody(const UniqueSocket &socket, const google::protobuf::MessageLite &arg, bool send_more);

// Templated overloaded stub function for fundamental types
template <class Integral, class = typename std::enable_if<std::is_fundamental<Integral>::value >::type>
inline void SendBody(const UniqueSocket &socket, const Integral arg, bool send_more) {
  static_assert(std::is_fundamental<Integral>::value,
                "SendBody(socket, arg, send_more) arg parameter must be an fundamental type.");
  SendBody(socket, &arg, sizeof(Integral), send_more);
}

// Template overloaded stub for objects wrapped in unique_ptrs
template <class T>
inline void SendBody(const UniqueSocket &socket, const std::unique_ptr<T> &arg, bool send_more) {
  SendBody(socket, *arg, send_more);
}

template <class T>
void SendBody(const UniqueSocket &socket, const std::vector<T> &arg, bool send_more) {
  auto first = arg.cbegin();
  auto last = arg.cend();
  while (first != last) {
    bool has_more = std::next(first) != last;
    SendBody(socket, *first, has_more ? true : send_more);
    ++first;
  }
}

// Stringy bodies
template <class CharType>
inline void SendBody(const UniqueSocket &socket, const std::basic_string<CharType> &s, bool send_more) {
  SendBody(socket, s.c_str(), s.length(), send_more);
}
inline void SendBody(const UniqueSocket &socket, const char *s, bool send_more) {
  SendBody(socket, static_cast<std::string>(s), send_more);
}

// Never used but required by compilers
inline void SendBodyWithArgs(const UniqueSocket &socket) {
  return;
}

// Variadic template (compile time recursion) supporting variable number of arguments to SendBody
template <typename Argument, typename... Arguments>
void SendBodyWithArgs(const UniqueSocket &socket, const Argument &arg, const Arguments &... args) {
  bool has_more = (sizeof...(args) > 0);
  SendBody(socket, arg, has_more);
  if (has_more) {
    SendBodyWithArgs(socket, args...);
  }
}

//
// Request functions
//

// Send request stubs
void SendRequestMethodName(const UniqueSocket &socket, const char *method_name, bool send_more);
int32_t RecvReplyHead(const UniqueSocket &socket, bool multipart);

// Parse a zmq message as binary data
inline bool RecvReplyBody(const UniqueSocket &socket, void *out, int32_t code) {
  zmq::message_t body;
  socket->recv(&body);
  memcpy(out, body.data(), body.size());
  return body.more();
}

// Parse a zmq message as protobuf
inline bool RecvReplyBody(const UniqueSocket &socket, google::protobuf::MessageLite *out, int32_t code) {
  zmq::message_t body;
  socket->recv(&body);
  out->ParseFromArray(body.data(), body.size());
  return body.more();
}

// Parse a zmq message as a fundamental type
template <class Integral, class = typename std::enable_if<std::is_fundamental<Integral>::value >::type>
bool RecvReplyBody(const UniqueSocket &socket, Integral *out, int32_t code) {
  zmq::message_t body;
  socket->recv(&body);
  CHECK_EQ(sizeof(*out), body.size()) << "Size of output target does not match reply";
  memcpy(out, body.data(), body.size());
  return body.more();
}

// Parse multiple zmq messages, placing them in a vector
template <class ReplyType>
bool RecvReplyBody(const UniqueSocket &socket, std::vector<ReplyType> *out, int32_t code) {
  ReplyType element;
  bool has_more;
  do {
    has_more = RecvReplyBody(socket, &element, code);
    out->push_back(element);
  } while (has_more);
  CHECK_EQ(out->size(), code) << "Got less replies then header indicated";
  return false;
}

// Send a request
// Main request function, out and arguments can be replaced with nullptr if a void reply no arguments is expected.
// ie.
//  oc::pb::accounting::Account a;
//  SendRequest(con.get(), "authenticate", &a, "username", "password");
//
// supports various inputs, ie.
//  std::vector<int> v;
//  v.push_back(1);
//  v.push_back(2);
//  v.push_back(3);
//  SendRequest(con.get(), "methodName", nullptr, static_cast<int16_t>(1), 42, 6.02e23f, "hello world", a, v);
//
// arguments are optional via nullptr or overload, ie.
//  std::vector<oc::pb::accounting::Account> al;
//  SendRequest(con.get(), "getAllAccounts", &al);
//
// you can even do requests without arguments or replies, ie.
//  SendRequest(con.get(), "stopService");
template <class ReplyType, typename Argument, typename... Arguments>
int32_t SendRequestWithTimeout(ClientConnection *con, const char *method_name, ReplyType *out, int retries, int timeout,
                               const Argument &arg, const Arguments &... args) {
  int retries_left = retries;
  bool has_arguments = !std::is_same<std::nullptr_t, Argument>::value;
  do {
    // Send the request
    SendRequestMethodName(con->socket_, method_name, has_arguments);

    // Send the body
    if (has_arguments) {
      SendBodyWithArgs(con->socket_, arg, args...);
    }

    // Poll for a reply (with timeout)
    zmq::pollitem_t items[] = { { *(con->socket_), 0, ZMQ_POLLIN, 0 } };
    zmq::poll(&items[0], 1, timeout);
    if (items[0].revents & ZMQ_POLLIN) {
      auto code = RecvReplyHead(con->socket_, (out != nullptr));
      // TODO(duedal) handle exception replies
      if (out == nullptr && code == 0) {
        return code;
      }
      CHECK_GT(code, 0) << "Expected a non-empty reply";
      CHECK_EQ(RecvReplyBody(con->socket_, out, code), 0) << "Received multiple replies but expected 1";
      return code;
    } else if (--retries_left == 0) {
      LOG(FATAL) << "Gave up after " << retries << " retries";
    } else {
      LOG(WARNING) << "Timed out, retry";
      // Old socket will be confused; close it and open a new one
      con->reconnect();
    }
  } while (retries_left > 0);

  return 0;
}
template <class ReplyType, typename Argument, typename... Arguments>
inline int32_t SendRequest(ClientConnection *con, const char *method_name, ReplyType *out, const Argument &arg,
                           const Arguments &... args) {
  return SendRequestWithTimeout(con, method_name, out, default_retries, default_timeout, arg, args...);
}
template <class ReplyType>
inline int32_t SendRequest(ClientConnection *con, const char *method_name, ReplyType *out) {
  return SendRequestWithTimeout(con, method_name, out, default_retries, default_timeout, nullptr);
}
inline int32_t SendRequest(ClientConnection *con, const char *method_name) {
  return SendRequestWithTimeout<std::nullptr_t>(con, method_name, nullptr, default_retries, default_timeout, nullptr);
}

// Send a request with arguments expecting void reply
// ie.
//  SendRequestEmptyReply(con.get(), "setCredit", 1, 200);
template <typename Argument, typename... Arguments>
inline int32_t SendRequestWithTimeoutEmptyReply(ClientConnection *con, const char *method_name, int retries,
    int timeout, const Argument &arg, const Arguments &... args) {
  return SendRequestWithTimeout<std::nullptr_t>(con, method_name, nullptr, default_retries, default_timeout, arg,
         args...);
}
template <typename Argument, typename... Arguments>
inline int32_t SendRequestEmptyReply(ClientConnection *con, const char *method_name, const Argument &arg,
                                     const Arguments &... args) {
  return SendRequestWithTimeout<std::nullptr_t>(con, method_name, nullptr, default_retries, default_timeout, arg,
         args...);
}

// Send a request without arguments
// ie.
//  int64_t id;
//  SendEmptyRequest(con.get(), "getId", &id);
// or if you don't need timeout.
//  SendRequest(con.get(), "getId", &id);
template <class ReplyType>
inline int32_t SendEmptyRequest(ClientConnection *con, const char *method_name, ReplyType *out, int retries,
                                int timeout) {
  return SendRequestWithTimeout(con, method_name, out, retries, timeout, nullptr);
}
template <class ReplyType>
inline int32_t SendEmptyRequest(ClientConnection *con, const char *method_name, ReplyType *out) {
  return SendEmptyRequest(con, method_name, out, default_retries, default_timeout);
}

// Send a request without arguments expecting void reply
// ie. SendEmptyRequest(con.get(), "doStuff");
inline int32_t SendEmptyRequest(ClientConnection *con, const char *method_name, int retries, int timeout) {
  return SendEmptyRequest<std::nullptr_t>(con, method_name, nullptr, retries, timeout);
}
inline int32_t SendEmptyRequest(ClientConnection *con, const char *method_name) {
  return SendEmptyRequest<std::nullptr_t>(con, method_name, nullptr, default_retries, default_timeout);
}
//
// Reply functions
//

// Send reply stub
void SendReplyHead(const UniqueSocket &socket, int32_t code);

// Send a reply with some bytes
// ie. SendReply(socket, "OK", 2);
void SendReply(const UniqueSocket &socket, const void *data, size_t length);

// Send a reply with a (single) protobuf message
// ie. SendReply(socket, foo);
inline void SendReply(const UniqueSocket &socket, const google::protobuf::MessageLite &arg) {
  SendReplyHead(socket, 1l);
  SendBody(socket, arg, false);
}

// Send a reply with a list of elements, element must be fundamental or protobuf message
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
    SendBody(socket, *first, has_more);
    ++first;
  }
}

// Send a reply with a list of elements, element must be fundamental or protobuf message
template<class Iter>
inline void SendReply(const UniqueSocket &socket, Iter first, Iter last) {
  SendReply(socket, first, last, std::distance(first, last));
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

// Send a reply with a fundamental type (int, char, float etc)
// ie.
//  SendReply(socket, 1337ULL);
//  SendReply(socket, 1);
//  SendReply(socket, 0.1234);
//  SendReply(socket, 6.02e23f);
//  SendReply(socket, true);
template <class Integral, class = typename std::enable_if<std::is_fundamental<Integral>::value >::type>
inline void SendReply(const UniqueSocket &socket, const Integral arg) {
  static_assert(std::is_fundamental<Integral>::value,
                "SendReply(socket, arg) arg parameter must be an fundamental type.");
  SendReply(socket, &arg, sizeof(Integral));
}


}  // namespace rpc
}  // namespace oc
