RFC 867 daytime implementation
===========================================

This is a simple server implementation of RFC 867 Daytime Protocol (https://tools.ietf.org/html/rfc867).

# Basics
The implementation is currently based on TCP. A UDP version (RFC 867 is allowing both transport protocols) will be added later.

# Build
Build with the following prequisites is tested on Ubuntu 20.04.3 LTS. Code should be build- and runable out of the box on any modern Linux distribution but not on Windows (the Socket API is slightly different there).

Use the following command to install the prequisites for building:
```
sudo apt install build-essential git
```
Now checkout the Github repository for the project to a local directory on your system:
```
git clone https://github.com/mark-hgb/daytime2.git
```
To build the code for server and client just type:
```
make
```
# Usage
```
./daytime -h
Implementation of the RFC 867 Daytime protocol
Usage: daytime [OPTIONS] 

Options:
  -i, --ip IP     listening IP (default all)
  -t, --tcp       TCP service (default)
  -p, --port NUM  port number (default 13)
  -h, --help      display this help message

```
Without any parameter the server listen on port 13 on any local IP address using TCP. Keep in mind that you need superuser rights to start the server on port 13 (well-known port). You can select any local listen IP with option -i or --ip and any local listen port with option -p or --port

# Credits
t.b.d.

