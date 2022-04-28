# traffic_relay

This repo used as an example to demonstrate how to use `epoll` in a single thread, served both as client and server.

## TODO

- Eliminate the use of hard-coded address and port.
- Add example of `kqueue`, and maybe `iocp`.
- Exploring `io_uring`.
- Refactor, especially add more comments.

## Usage

1.  Compile `main.cpp` using
    ```
    g++  main.cpp -lpthread -o main
    ```
2.  Running `main`
    ```
    ./main
    ```
3.  In another machine, use a `tcp server` listen to the port `50007`
    ```
    nc -l 50007
    ```
4.  In another machine, use a `tcp client` connect to the port `50007`
    ```
    nc <ip> 50007
    ```
5.  Try to send some data to the server or send data back from server to the client, and see the result. You could also use multiple clients to connect to the server.
