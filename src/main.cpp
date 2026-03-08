#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include "RtMidi.h"

int main (int argc, char **argv) {
  int opt;
  // External variable set by getopt_long to the option's argument
  extern char *optarg; 
  // External variable for the index of the next argument to be processed
  extern int optind; 

  static struct option long_options[] = {
    // {name, has_arg, flag, val}
    { "verbose", no_argument, NULL, 'v' },
    { "file", required_argument, NULL, 'f' },
    { "help", no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 } // Required end-of-array marker
  };

  while ((opt = getopt_long(argc, argv, "vf:h", long_options, NULL)) != -1) {
    switch (opt) {
      case 'v':
        printf("Verbose mode enabled\n");
        break;
      case 'f':
        printf("File specified: %s\n", optarg);
        break;
      case 'h':
        printf("Usage: %s [--verbose] [--file=<file>] [--help]\n", argv[0]);
        break;
      case '?':
        // Handle unknown options or missing arguments
        fprintf(stderr, "Unknown option or missing argument\n");
        break;
      default:
        abort();
    }
  }

  // Process any remaining non-option arguments
  for (int i = optind; i < argc; i++) {
    printf("Non-option argument: %s\n", argv[i]);
  }

  RtMidiIn *midiin = 0;
  RtMidiOut *midiout = 0;

  try {
    midiin = new RtMidiIn();
  }
  catch (RtMidiError &error) {
    error.printMessage();
    exit(EXIT_FAILURE);
  }

  // Check inputs.
  unsigned int nPorts = midiin->getPortCount();
  printf("There are %d MIDI input sources available\n", nPorts);
  std::string portName;
  for (unsigned int i = 0; i < nPorts; i++) {
    try {
      portName = midiin->getPortName(i);
    }
    catch (RtMidiError &error) {
      error.printMessage();
      goto cleanup;
    }
    printf("  Input Port #%d: %s\n", i + 1, portName.c_str());
  }
 
  // RtMidiOut constructor
  try {
    midiout = new RtMidiOut();
  }
  catch ( RtMidiError &error ) {
    error.printMessage();
    goto cleanup;
  }
 
  // Check outputs.
  nPorts = midiout->getPortCount();
  printf("There are %d MIDI output ports available.\n", nPorts);
  for (unsigned int i=0; i<nPorts; i++) {
    try {
      portName = midiout->getPortName(i);
    }
    catch (RtMidiError &error) {
      error.printMessage();
      goto cleanup;
    }
    printf("  Output Port #%d: %s\n", i + 1, portName.c_str());
  }

  cleanup:
  delete midiin;
  delete midiout;

  return 0;
}
