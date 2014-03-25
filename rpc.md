# OC RPC 
Our RPC format is based on [Protobuf](https://code.google.com/p/protobuf/) + [ZeroMQ](http://zeromq.org/). It's not based on the advanced [majordomo ZMQ protocol](http://zguide.zeromq.org/page:all#Service-Oriented-Reliable-Queuing-Majordomo-Pattern), but instead keeps it's features, overhead and complexity as low as possible.

Also protobuf is optional, fundamental types such as int32 may be returned in their original binary form.

## Basic format
We use ZMQ multi-part messages, and optional protobuf encoding for data exchange between Client and Server.

All communication happens via ZMQ Request-Reply TCP-sockets. Servers must be bound on a specified port: <https://github.com/onlinecity/ocmg-lib/blob/master/docs/ports.md>

For reliability we stick to Client-Side Reliability, also known as "[Lazy Pirate Pattern](http://zguide.zeromq.org/page:all#Client-Side-Reliability-Lazy-Pirate-Pattern)" for now. This means that the client must enforce timeouts and re-attempt to send a message if it fails.

#### Request

Requests consists of a header, which is a UTF-8 string, containing a method name to call. The string is not NULL-terminated since ZMQ does it own termination.
A request may be followed by one or more argument parts.

#### Reply

Replies consist of a header, which is an int32, in [little-endian][endian] format. As an int32 it must be exactly 4 bytes long.
The header value has the following meanings:

Value | Description
----- | -----------
**-1** | Exception, body must be an protobuf-encoded exception
**0** | void, no reply data, but operation completed without exceptions
**[ 1, INT_MAX ]** | Number of message parts with reply data

[ -INT_MAX, -2 ] is unused, and reserved for future use.

#### Empty message heartbeat

In addition to the request and reply messages specified above the client may send an empty message containing no header or body, the server must then respond with an empty message in return (keeping with the lock-step Req-Rep pattern). This feature is to allow keeping TCP connections alive, since TCP connections can timeout after 30 minutes or so.

## [Encoding](id:encoding)

Following the header a request or a reply may contain one or more arguments / replies (hereafter parts). Each part can be a binary representation of a fundamental type, a string or a protobuf message. The part contains NO header, description or reference to it's type, the client and server must determine the type be refering to the external documentation of the method, ie. it must be known in advance.

##### Endianness

[endian]: http://en.wikipedia.org/wiki/Endianness "endianness, refer to how bytes of a data word are ordered within memory"

The network-order is big endian, so when transmitting data on a network we should convert from host-order (little endian on x86/arm), but as ZMQs guide [said](http://zguide.zeromq.org/page:all#Handwritten-Binary-Serialization) we should "be ready to break the rules". Thus all data is little endian ordered.

When using pack() in Python, we can use the default "native" byte order.

##### [Types](id:types)

The following [fundamental](http://www.cplusplus.com/reference/type_traits/is_fundamental/) types is supported. Everything is [little-endian][endian].

All of the [C data types](http://en.wikipedia.org/wiki/C_data_types) are supported, but we prefer the following definitions.

Type | Length | python [pack()](http://docs.python.org/3/library/struct.html) | php [pack()](http://www.php.net/manual/en/function.pack.php) | Description
--------------- | ------- | ------- | -------- | --------------------
bool            | 1 byte  | ```?``` | ```c```  |  encoded as 0 or 1
char            | 1 byte  | ```b``` | ```c```  | signed on most implementations
signed char     | 1 byte  | ```b``` | ```c```  | same as char but guaranteed to be sigend
unsigned char   | 1 byte  | ```B``` | ```C```  | 
int8            | 1 byte  | ```b``` | ```c```  |  
uint8           | 1 byte  | ```B``` | ```C```  | unsigned
int16           | 2 bytes | ```h``` | ```s```  |  
uint16          | 2 bytes | ```H``` | ```S```  | unsigned
int32           | 4 bytes | ```i``` | ```l```  | 
uint32          | 4 bytes | ```I``` | ```L```  | unsigned
int64           | 8 bytes | ```q``` | ```a4a4``` | php workaround
uint64          | 8 bytes | ```Q``` | ```LL``` | unsigned, php workaround
float           | 4 bytes | ```f``` | ```f```  | single precision floating-point type
double          | 8 bytes | ```d``` | ```d```  | double precision floating-point type
long double     | ~10 bytes | N/A, use ```d``` | N/A, use ```d``` | extended precision floating point type

Please use the [above table](#types) as reference, when writing service documentation.

In addition strings (without NULL-termination) and protobuf messages are supported.

If the service accepts/returns a list, the list must be expanded to multiple parts. 

## Language specifics

#### PHP (u)int64 encoding

(u)int64:

```
<?php
$p1 = pack('l',$input&0xFFFFFFFF);
$p2 = pack('l',($input>>32)&0xFFFFFFFF);
$e = pack('a4a4',strrev($p2),strrev($p1));
```

PHP can't use values larger than int64 max.
