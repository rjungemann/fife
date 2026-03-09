default: build run

build:
  mkdir -p build
  cd build && cmake .. && make

run:
  cd build && pw-jack ./fife midi devices
  cd build && pw-jack ./fife midi note-on --out-device=ble 1 48 64

