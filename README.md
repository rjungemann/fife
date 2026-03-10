# Fife

By Roger Jungemann

## What is Fife?

Fife is a tool for processing MIDI and OSC messages from the command line.
Integrate your shell scripts with MIDI and OSC.

## Build instructions

Requires your OS's build tools and `cmake`. `just` is optional, for my
convenience.

```sh
# Using just
just --list
just

# Without just
mkdir -p build
cd build
cmake ..
make

# To run,
build/fife

# If you are using PipeWire on Linux (`sudo apt install pw-jack`)
pw-jack ./fife
```

## Usage

To get usage info:

```sh
fife help
```

Named arguments:

```sh
# Output usage message
--help

# Enable verbose output on stderr
--verbose

# Set MIDI hardware device(s)
--in-device=DEVICE
--out-device=DEVICE

# Set MIDI channel
--channel=CHANNEL

# Set OSC host and port
--host=HOST
--port=PORT
```

Sending and receiving MIDI messages:

```sh
# List MIDI input and output devices
fife midi devices

# Send a MIDI note
fife midi note-on --channel=1 48 64
fife midi note-off --channel=1 48 0

# Send a CC message
fife midi control-change --channel=1 7 16

# Send a program change message
fife midi program-change --channel=1 16

# Send sysex messages
fife midi send 0xF0 0x7E 0x10 0x06 0x01 0xF7
fife midi send 240 126 16 6 1 247

# Open a virtual MIDI port
fife midi open-in-port "Test Device"
fife midi open-out-port "Test Device"

# Listen for incoming MIDI messages
fife midi listen --channel=1
```

Send and receive OSC messages:

```sh
fife osc send --host=127.0.0.1 --port=9000 /hello siii foo 1 2 3

fife osc listen --host=0.0.0.0 --port=9000 /hello siii
```

OSC types supported currently are `int32`, `float32`, and `string`.

## Roadmap

* MIDI
  * Clock events?
  * MPE?
  * RPNs and NRPNs?
* Virtual MIDI ports should tee output
* Allow changing of MIDI API (ALSA, JACK, etc.)
* Use `has_value` or similar to verify optionals
* Make sure we allow hex or decimal parsing everywhere
* OSC float64
* OSC blobs
* OSC arrays
* OSC bundles

## References

* https://caml.music.mcgill.ca/~gary/rtmidi/
* https://github.com/thestk/rtmidi
* https://github.com/kaoskorobase/oscpp
* https://web.archive.org/web/20150407060239/http://www.philrees.co.uk/nrpnq.htm
