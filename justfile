os := `uname`
fife := if os == "Linux" { "pw-jack ./fife" } else { "./fife" }
indevice := if os == "Darwin" { "iac" } else { "ble" }
outdevice := if os == "Darwin" { "iac" } else { "ble" }

default: build run

build:
  mkdir -p build
  cd build && cmake .. && make

install:
  cd build && cmake --install

run:
  cd build && {{fife}} midi devices
  cd build && {{fife}} midi note-on --out-device={{outdevice}} --channel=1 48 64
  cd build && {{fife}} midi note-off --out-device={{outdevice}} --channel=1 48 0
  cd build && {{fife}} midi control-change --out-device={{outdevice}} --channel=1 7 64
  cd build && {{fife}} midi program-change --out-device={{outdevice}} --channel=1 16
  cd build && {{fife}} midi send --out-device={{outdevice}} 0xF0 0x7E 0x10 0x06 0x01 0xF7
  #cd build && {{fife}} midi listen --in-device=ble 1
  #cd build && {{fife}} midi open-in-port "Test Port"
  #cd build && {{fife}} midi open-out-port "Test Port"
  cd build && {{fife}} osc send --host=127.0.0.1 --port=9000 /hello siii foo 1 2 3
  #cd build && {{fife}} osc listen --host=127.0.0.1 --port=9000 /hello

