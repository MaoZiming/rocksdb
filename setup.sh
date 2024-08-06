export MY_INSTALL_DIR=$HOME/.local
mkdir -p $MY_INSTALL_DIR
export PATH="$MY_INSTALL_DIR/bin:$PATH"
sudo apt install -y build-essential autoconf libtool pkg-config

git clone --recurse-submodules --depth 1 --shallow-submodules https://github.com/grpc/grpc
cd grpc
mkdir -p cmake/build
pushd cmake/build
cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      ../..
make -j 4
sudo make install
popd


sudo apt install make -y
sudo apt install g++ -y
make static_lib -j9
sudo apt install cmake

# Installing gRPC and Protobuf
sudo apt-get install build-essential autoconf libtool pkg-config
sudo apt-get install libudev-dev libsystemd-dev


cd rocksdb
make static_lib