cd MediaSystem
mkdir build
cd build
cmake .. -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

build/
producer/Release/producer.exe
consumer/Release/consumer.exe

RUN CONSUMER.EXE FIRST

Running Producer:
producer.exe <server:port> <producer_id> <input_folder>
producer.exe localhost:50051 producer1 C:\Users\requi\Desktop\MediaSystem\MediaInput

Producer uploads the file manually from MediaInput Folder

WHATEVER YOU PUT IN THERE WILL BE UPLOADED

server: port to gRPC server of consumer
producer_id: producer1
and cd\MediaInput