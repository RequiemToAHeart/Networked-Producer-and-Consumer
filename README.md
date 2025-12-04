# Networked-Producer-and-Consumer
Here's a producer-consumer exercise involving file writes and network sockets. This will help you practice concurrent programming, file I/O, queueing, and network communication.  This is a simple simulation of a media upload service.


##How to Run

cd MediaSystem
mkdir build
cd build
cmake .. -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

build/
 ├── producer/Release/producer.exe
 └── consumer/Release/consumer.exe

 ^^^ find those
