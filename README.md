# Erizo, a C/C++ Multipoint Control Unit (MCU) Library for WebRTC 

Erizo is a project that aims to implement a library able to communicate with WebRTC (http://www.webrtc.org) browser clients in order to provide advanced communication services. Currently it is tested on Ubuntu 11.10 and above but it should be able to be compiled on other distributions.

Updated code documentation can be found at http://ging.github.com/erizo 

## Directory structure

- /src -  The root source directory
- /src/erizo - The source of the main library
- /src/examples - Examples and tests

## Requirements

- CMake >= 2.8 
- libSRTP version >= 1.4.4
- Libnice version >= 1.10
- boost_threads >= 1.48
- boost_regex >= 1.48 (optional, only for examples)
- boost_asio >= 1.48 (optional, only for examples)
- boost_system >= 1.48 (optional, only for examples)
 
## Building Instructions

This project is built using CMake.

The easiest way to build it is to use the provided scripts:
- Run ./generateProject.sh to run cmake, test the dependencies and generate the Makefile.
- Run ./buildProyect.sh to build the project after generating the Makefile. It simply runs make in the build directory.
- Run ./generateEclipseProyect.sh to generate an Eclipse CDT project which can be imported and used to work with the code.

If doxygen is availabe a "doc" target is generated. HTML documentation can be built by running "make doc" in the build directory.

##Examples

As of now, the only application built using the library is a streaming application that connects via TCP to a server application built on top of node.js.

The node.js app is not released but the code should provide an example on how the SDP exchange is made.
The examples are unusable as of now, but they can help to understand the API.
In the future, we will include here tests and updated examples.

## License

The MIT License

Copyright (C) 2012 Universidad Politecnica de Madrid.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
