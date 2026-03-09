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
  * Send note-on
  * Send note-off
  * Send CC's
  * Send program changes
  * Send sysex
  * Listen for incoming MIDI messages
  * Create a virtual MIDI port
  * Clock events?
  * MPE?
  * TODO: What else?
* OSC
  * Send OSC messages
  * Listen for incoming OSC messages
  * TODO: What else?
* Scripting?

```sh
--help
--verbose
--device=DEVICE


fire help

fife midi devices
fife midi note-on 1 48 64
fife midi note-off 1 48 0
fife midi control-change 1 16
fife midi raw 0xF0 0x7E 0x10 0x06 0x01 0xF7

fife midi listen 1
# TODO: Filtering?
# TODO: Format?

fife osc message 127.0.0.1:8000/hello,siii foo 1 2 3

fife osc listen 127.0.0.1:8000/hello,siii
# TODO: Wildcards or less constrained routes?
# TODO: Filtering?
# TODO: Format?
```

## References

* https://caml.music.mcgill.ca/~gary/rtmidi/
* https://github.com/thestk/rtmidi
* https://github.com/mhroth/tinyosc
