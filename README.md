## ZeroMQ+Protobuf RPC prototype

#### Status

Although it says prototype, it's almost ready for use. Experienced programmers can quickly modify it to fit their use case, and it works. 

##### Protocol

See [`rpc.md`](rpc.md) for details.

#### How to run

Install deps: CMake, protobuf, gflags, glog and ZMQ using your favorite package manager. C++11 compiler required.

Compile:

```
cmake .
make
```

Run server: ```./bin/rpcs -alsologtostderr```

Run client: ```./bin/rpcc -alsologtostderr```

#### License

Licensed under MIT-license.