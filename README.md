# Midcat

By Roger Jungemann

## Instructions

```sh
mkdir -p build
cd build
cmake ..
make
pw-jack ./midcat
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
  * TODO: What else?
* OSC
  * Send OSC messages
  * Listen for incoming OSC messages
  * TODO: What else?
* Scripting?

## References

* https://caml.music.mcgill.ca/~gary/rtmidi/
* https://github.com/thestk/rtmidi
* https://github.com/mhroth/tinyosc
