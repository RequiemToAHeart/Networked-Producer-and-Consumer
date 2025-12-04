# Networked-Producer-and-Consumer
Here's a producer-consumer exercise involving file writes and network sockets. This will help you practice concurrent programming, file I/O, queueing, and network communication.  This is a simple simulation of a media upload service.


##Dependencies, Software, & Libraries
###vcpkg<br>
git clone https://github.com/microsoft/vcpkg "C:\vcpkg"<br>
C:\vcpkg\bootstrap-vcpkg.bat<br>

<br>
integrate:<br>
C:\vcpkg\vcpkg integrate install

###Dependencies for vcpkg<br>
C:\vcpkg\vcpkg install grpc:x64-windows<br>
C:\vcpkg\vcpkg install openssl:x64-windows<br>
C:\vcpkg\vcpkg install sqlite3:x64-windows<br>
C:\vcpkg\vcpkg install nlohmann-json:x64-windows<br>
C:\vcpkg\vcpkg install cpp-httplib:x64-windows<br>

###ffmpeg<br>
https://www.gyan.dev/ffmpeg/builds/<br>

###CMake<br>
https://cmake.org/download/<br>

##How to Run<br>
(use cmd)<br>

cd MediaSystem<br>
mkdir build<br>
cd build<br>
cmake .. -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake<br>
cmake --build . --config Release<br>

build/<br>
<br>
 ├── producer/Release/producer.exe<br>
 └── consumer/Release/consumer.exe<br>
<br>
 ^^^ find those
