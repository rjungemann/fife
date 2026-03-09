# Fife

By Roger Jungemann

## What is Fife?

Fife is a swiss-army knife utility for processing MIDI and OSC messages from the command line. Integrate your command-line scripts with MIDI and OSC.

## Instructions

```sh
mkdir -p build
cd build
cmake ..
make
pw-jack ./fife
```

## TODO

* MIDI
  * Listen for incoming MIDI messages
  * Create a virtual MIDI port
  * Clock events?
  * MPE?
  * TODO: What else?
* OSC
  * Send OSC messages
  * Listen for incoming OSC messages
* Allow changing of MIDI API (ALSA, JACK, etc.)

```sh
--help
--verbose
--in-device=DEVICE
--out-device=DEVICE
--host=HOST
--port=PORT

fife help

fife midi devices
fife midi note-on 1 48 64
fife midi note-off 1 48 0
fife midi control-change 7 16
fife midi program-change 1 16
fife midi send 0xF0 0x7E 0x10 0x06 0x01 0xF7

fife midi listen 1

fife osc send --host=127.0.0.1 --port=9000 /hello siii foo 1 2 3

fife osc listen --host=0.0.0.0 --port=9000 /hello siii
```

## References

* https://caml.music.mcgill.ca/~gary/rtmidi/
* https://github.com/thestk/rtmidi
* https://github.com/mhroth/tinyosc
