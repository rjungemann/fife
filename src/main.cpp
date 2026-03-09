#include <optional>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include "RtMidi.h"
#include "tinyosc.h"

#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>

struct context;

void bail ();
void print_usage ();
void handle_arguments (context &ctx, int argc, char **argv);
unsigned char parse_byte (std::string str);
void handle_midi (context &ctx);
std::string sluggerize (std::string str);
bool slug_match (std::string a, std::string b);
std::optional<int> get_midi_in_device_index (context &ctx, const std::string &device_name);
std::optional<int> get_midi_out_device_index (context &ctx, const std::string &device_name);
void handle_midi_devices (context &ctx);
void handle_osc (context &ctx);
void handle_osc_send (context &ctx);

struct context {
  std::optional<std::string> adapter;
  std::optional<std::string> command;

  std::unique_ptr<RtMidiIn> midiin;
  std::unique_ptr<RtMidiOut> midiout;

  bool verbose = false;
  std::optional<std::string> in_device;
  std::optional<std::string> out_device;
  std::optional<std::string> host;
  std::optional<int> port;

  std::vector<std::string> extra_args;
};

void bail () {
  exit(EXIT_FAILURE);
}

void print_usage () {
  fprintf(stderr, "Usage: fife <command> [options]\n");
  fprintf(stderr, "Commands:\n");
  fprintf(stderr, "  --verbose           Enable verbose mode\n");
  fprintf(stderr, "  --help              Show this help message\n");
  fprintf(stderr, "  --in-device         Name of hardware device to use\n");
  fprintf(stderr, "  --out-device        Name of hardware device to use\n");
  fprintf(stderr, "  --host              Host to send OSC messages to (default: 127.0.0.1)\n");
  fprintf(stderr, "  --port              Port to send OSC messages to (default: 9000)\n");
}

void handle_arguments (context &ctx, int argc, char **argv) {
  if (argc < 3) {
    print_usage();
    bail();
  }
  ctx.adapter = std::string(argv[1]);
  ctx.command = std::string(argv[2]);

  int opt;
  // External variable set by getopt_long to the option's argument
  extern char *optarg; 
  // External variable for the index of the next argument to be processed
  extern int optind;
  optind = 3;

  static struct option long_options[] = {
    // {name, has_arg, flag, val}
    { "verbose", no_argument, NULL, 'v' },
    { "help", no_argument, NULL, 'h' },
    { "in-device", optional_argument, NULL, 'i' },
    { "out-device", optional_argument, NULL, 'o' },
    { "host", optional_argument, NULL, 'H' },
    { "port", optional_argument, NULL, 'p' },
    { NULL, 0, NULL, 0 } // Required end-of-array marker
  };

  while ((opt = getopt_long(argc, argv, "vhi:o:H:p:", long_options, NULL)) != -1) {
    switch (opt) {
      case 'v':
        fprintf(stderr, "Verbose mode enabled\n");
        ctx.verbose = true;
        break;
      case 'i':
        fprintf(stderr, "Hardware device specified: %s\n", optarg);
        ctx.in_device = std::string(optarg);
        break;
      case 'o':
        fprintf(stderr, "Hardware device specified: %s\n", optarg);
        ctx.out_device = std::string(optarg);
        break;
      case 'H':
        fprintf(stderr, "Host specified: %s\n", optarg);
        ctx.host = std::string(optarg);
        break;
      case 'p':
        fprintf(stderr, "Port specified: %s\n", optarg);
        ctx.port = std::atoi(optarg);
        break;
      case 'h':
      case '?':
        print_usage();
        break;
      default:
        bail();
    }
  }
  // Process any remaining non-option arguments
  for (int i = optind; i < argc; i++) {
    fprintf(stderr, "Non-option argument: %s\n", argv[i]);
    ctx.extra_args.push_back(std::string(argv[i]));
  }
}

// TODO: Size checking (4 for hex, 1-3 for decimal)
unsigned char parse_byte (std::string str) {
  try {
    if (str.find("0x") == 0) {
      if (str.length() != 4) {
        throw std::invalid_argument("Hex byte string too long");
      }
      return std::stoi(str, nullptr, 16);
    }
    if (str.length() > 3) {
      throw std::invalid_argument("Decimal byte string too long");
    }
    return std::stoi(str);
  }
  catch (std::exception &e) {
    fprintf(stderr, "Error parsing integer: %s\n", str.c_str());
    bail();
  }
}

void handle_midi (context &ctx) {
  // Open devices
  try {
    ctx.midiin.reset(new RtMidiIn());
    ctx.midiout.reset(new RtMidiOut());
  }
  catch (RtMidiError &error) {
    error.printMessage();
    bail();
  }
  if (ctx.in_device) {
    std::optional<int> index = get_midi_in_device_index(ctx, *ctx.in_device);
    if (!index.has_value()) {
      bail();
      return;
    }
    fprintf(stderr, "Opening MIDI input device with index %d\n", index.value());
    ctx.midiin->openPort(index.value());
  }
  if (ctx.out_device) {
    std::optional<int> index = get_midi_out_device_index(ctx, *ctx.out_device);
    if (!index.has_value()) {
      bail();
      return;
    }
    fprintf(stderr, "Opening MIDI output device with index %d\n", index.value());
    ctx.midiout->openPort(index.value());
  }

  // Handle command
  if (ctx.command == "devices") {
    handle_midi_devices(ctx);
  }
  if (ctx.command == "note-on") {
    unsigned char note = parse_byte(ctx.extra_args[0]);
    unsigned char velocity = parse_byte(ctx.extra_args[1]);
    std::vector<unsigned char> message = {0x90, note, velocity};
    ctx.midiout->sendMessage(&message);
  }
  if (ctx.command == "note-off") {
    unsigned char note = parse_byte(ctx.extra_args[0]);
    unsigned char velocity = parse_byte(ctx.extra_args[1]);
    std::vector<unsigned char> message = {0x80, note, velocity};
    ctx.midiout->sendMessage(&message);
  }
  if (ctx.command == "control-change") {
    unsigned char controller = parse_byte(ctx.extra_args[0]);
    unsigned char value = parse_byte(ctx.extra_args[1]);
    std::vector<unsigned char> message = {0xB0, controller, value};
    ctx.midiout->sendMessage(&message);
  }
  if (ctx.command == "program-change") {
    unsigned char program = parse_byte(ctx.extra_args[0]);
    std::vector<unsigned char> message = {0xC0, program};
    ctx.midiout->sendMessage(&message);
  }
  if (ctx.command == "raw") {
    std::vector<unsigned char> message;
    for (const std::string &arg : ctx.extra_args) {
      message.push_back(parse_byte(arg));
    }
    ctx.midiout->sendMessage(&message);
  }
}

std::string sluggerize (std::string str /* by copy */) {
  for (auto i = 0; i < str.length(); i++) {
    char &c = str[i];
    if (std::isalnum(c)) {
      c = std::tolower(c);
    }
  }
  return str;
}

bool slug_match (std::string a, std::string b) {
  return sluggerize(a).find(sluggerize(b)) != std::string::npos;
}

std::optional<int> get_midi_in_device_index (context &ctx, const std::string &device_name) {
  unsigned int num_ports = ctx.midiin->getPortCount();
  for (unsigned int i = 0; i < num_ports; i++) {
    try {
      std::string port_name = ctx.midiin->getPortName(i);
      if (slug_match(port_name, device_name)) {
        fprintf(stderr, "Found MIDI input device '%s' matching '%s' at index %d\n", port_name.c_str(), device_name.c_str(), i);
        return i;
      }
    }
    catch (RtMidiError &error) {
      error.printMessage();
      return std::nullopt;
    }
  }
  fprintf(stderr, "MIDI input device not found: %s\n", device_name.c_str());
  bail();
}

std::optional<int> get_midi_out_device_index (context &ctx, const std::string &device_name) {
  unsigned int num_ports = ctx.midiout->getPortCount();
  for (unsigned int i = 0; i < num_ports; i++) {
    try {
      std::string port_name = ctx.midiout->getPortName(i);
      if (slug_match(port_name, device_name)) {
        fprintf(stderr, "Found MIDI output device '%s' matching '%s' at index %d\n", port_name.c_str(), device_name.c_str(), i);
        return i;
      }
    }
    catch (RtMidiError &error) {
      error.printMessage();
      return std::nullopt;
    }
  }
  fprintf(stderr, "MIDI output device not found: %s\n", device_name.c_str());
  bail();
}

void handle_midi_devices (context &ctx) {
  // Check inputs.
  unsigned int nPorts = ctx.midiin->getPortCount();
  fprintf(stderr, "MIDI input sources\n", nPorts);
  std::string portName;
  for (unsigned int i = 0; i < nPorts; i++) {
    try {
      portName = ctx.midiin->getPortName(i);
    }
    catch (RtMidiError &error) {
      error.printMessage();
      return;
    }
    fprintf(stderr, "  [%d] %s\n", i, portName.c_str());
  }
 
  // Check outputs.
  nPorts = ctx.midiout->getPortCount();
  fprintf(stderr, "MIDI output sources\n", nPorts);
  for (unsigned int i=0; i<nPorts; i++) {
    try {
      portName = ctx.midiout->getPortName(i);
    }
    catch (RtMidiError &error) {
      error.printMessage();
      return;
    }
    fprintf(stderr, "  [%d] %s\n", i, portName.c_str());
  }
}

void handle_osc (context &ctx) {
  if (ctx.command == "send") {
    handle_osc_send(ctx);
  }
}

void handle_osc_send (context &ctx) {
  std::string address = ctx.extra_args[0];
  // TODO: Make sure types are converted properly
  std::string type_tags = ctx.extra_args[1];
  std::vector<std::string> arg_strings(ctx.extra_args.begin() + 2, ctx.extra_args.end());

  const int buffersize = 2048;
  char buffer[buffersize];
  int len = tosc_writeMessage(buffer, sizeof(buffer), address.c_str(), type_tags.c_str(), arg_strings.size(), arg_strings.data());
  tosc_printOscBuffer(buffer, len);
  
  struct timespec start, end;
  int sockfd;
  struct sockaddr_in server;
  fprintf(stderr, "Configure socket...\n");
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    fprintf(stderr, "Error opening socket");
    exit(EXIT_FAILURE);
  }

  bzero((char*)&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(ctx.host.value().c_str());
  server.sin_port = htons(ctx.port.value());

  fprintf(stderr, "Send UDP data...\n");
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  if (sendto(sockfd, &buffer, buffersize, 0, (const struct sockaddr*)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Error in sendto()\n");
    exit(EXIT_FAILURE);
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;

  fprintf(stderr, "Time to send: %f[s]\n", delta_us / 1e6f);
  fprintf(stderr, "Finished...\n");
}

int main (int argc, char **argv) {
  context ctx;
  handle_arguments(ctx, argc, argv);

  if (ctx.adapter == "midi") {
    handle_midi(ctx);
  }
  if (ctx.adapter == "osc") {
    handle_osc(ctx);
  }

  return 0;
}
