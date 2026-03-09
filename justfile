default: build run

build:
  mkdir -p build
  cd build && cmake .. && make

run:
  cd build && pw-jack ./fife midi devices
  #cd build && pw-jack ./fife midi note-on --out-device=ble 1 48 64
  #cd build && pw-jack ./fife midi note-off --out-device=ble 1 48 0
  #cd build && pw-jack ./fife midi control-change --out-device=ble 1 7 64
  #cd build && pw-jack ./fife midi program-change --out-device=ble 1 16
  #cd build && pw-jack ./fife midi send --out-device=ble 0xF0 0x7E 0x10 0x06 0x01 0xF7
  #cd build && pw-jack ./fife midi listen --in-device=ble 1
  #cd build && pw-jack ./fife midi open-in-port "Test Port"
  #cd build && pw-jack ./fife midi open-out-port "Test Port"
  cd build && pw-jack ./fife osc send --host=127.0.0.1 --port=9000 /hello siii foo 1 2 3
  #cd build && pw-jack ./fife osc listen --host=127.0.0.1 --port=9000 /hello siii

