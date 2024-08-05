sudo apt install make -y
sudo apt install g++ -y
make static_lib -j9
sudo apt install cmake

# Installing gRPC and Protobuf
sudo apt-get install build-essential autoconf libtool pkg-config
sudo apt-get install libudev-dev libsystemd-dev

cd ~/
git clone https://github.com/grpc/grpc
cd grpc
git submodule update --init
mkdir -p cmake/build
cd cmake/build
cmake ../..
make -j4
sudo make install
