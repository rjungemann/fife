#include <optional>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include "RtMidi.h"
/*#include "tinyosc.h"*/

#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>

#include <oscpp/client.hpp>
#include <oscpp/server.hpp>
#include <oscpp/print.hpp>
#include <iostream>

struct context;

void bail ();
void print_usage ();
void handle_arguments (context &ctx, int argc, char **argv);
unsigned char parse_byte (std::string str);
void handle_midi (context &ctx);
void handle_midi_listen (context &ctx);
std::string sluggerize (std::string str);
bool slug_match (std::string a, std::string b);
std::optional<int> get_midi_in_device_index (context &ctx, const std::string &device_name);
std::optional<int> get_midi_out_device_index (context &ctx, const std::string &device_name);
void handle_midi_devices (context &ctx);
void handle_osc (context &ctx);
static void sigint_handler(int x);
void handle_osc_packet (const OSCPP::Server::Packet& packet);
void handle_osc_send (context &ctx);
void handle_osc_listen (context &ctx);

struct context {
  std::optional<std::string> adapter;
  std::optional<std::string> command;

  std::unique_ptr<RtMidiIn> midiin;
  std::unique_ptr<RtMidiOut> midiout;

  bool verbose = false;
  std::optional<int> channel;
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
  fprintf(stderr, "  --channel=1         Channel to send messages to (default: 1)\n");
  fprintf(stderr, "  --in-device         Name of hardware device to use\n");
  fprintf(stderr, "  --out-device        Name of hardware device to use\n");
  fprintf(stderr, "  --host=127.0.0.1    Host to send OSC messages to (default: 127.0.0.1)\n");
  fprintf(stderr, "  --port=9000         Port to send OSC messages to (default: 9000)\n");
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
    { "channel", optional_argument, NULL, 'c' },
    { "in-device", optional_argument, NULL, 'i' },
    { "out-device", optional_argument, NULL, 'o' },
    { "host", optional_argument, NULL, 'H' },
    { "port", optional_argument, NULL, 'p' },
    { NULL, 0, NULL, 0 } // Required end-of-array marker
  };

  while ((opt = getopt_long(argc, argv, "vhi:o:H:p:", long_options, NULL)) != -1) {
    switch (opt) {
      case 'v':
        /*fprintf(stderr, "Verbose mode enabled\n");*/
        ctx.verbose = true;
        break;
      case 'c':
        /*fprintf(stderr, "Channel specified: %s\n", optarg);*/
        ctx.channel = std::atoi(optarg);
        break;
      case 'i':
        /*fprintf(stderr, "Requesting input device matching \"%s\"\n", optarg);*/
        ctx.in_device = std::string(optarg);
        break;
      case 'o':
        /*fprintf(stderr, "Requesting output device matching \"%s\"\n", optarg);*/
        ctx.out_device = std::string(optarg);
        break;
      case 'H':
        /*fprintf(stderr, "Host specified: %s\n", optarg);*/
        ctx.host = std::string(optarg);
        break;
      case 'p':
        /*fprintf(stderr, "Port specified: %s\n", optarg);*/
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
    ctx.extra_args.push_back(std::string(argv[i]));
  }
  /*// Inspect non-option arguments*/
  /*fprintf(stderr, "Non-option arguments: {");*/
  /*for (std::string n : ctx.extra_args) {*/
  /*  fprintf(stderr, "\"%s\",", n.c_str());*/
  /*}*/
  /*fprintf(stderr, "}\n");*/
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
    /*fprintf(stderr, "Opening MIDI input device with index %d\n", index.value());*/
    ctx.midiin->openPort(index.value());
  }
  if (ctx.out_device) {
    std::optional<int> index = get_midi_out_device_index(ctx, *ctx.out_device);
    if (!index.has_value()) {
      bail();
      return;
    }
    /*fprintf(stderr, "Opening MIDI output device with index %d\n", index.value());*/
    ctx.midiout->openPort(index.value());
  }

  // Handle command
  if (ctx.command == "devices") {
    fprintf(stderr, "Listing MIDI devices...\n");
    handle_midi_devices(ctx);
  }
  if (ctx.command == "note-on") {
    unsigned char note = parse_byte(ctx.extra_args[0]);
    unsigned char velocity = parse_byte(ctx.extra_args[1]);
    unsigned char status = 0x90 + (ctx.channel.value_or(1) - 1);
    fprintf(stderr, "Generating a note-on event (%d, %d) on channel %d\n", note, velocity, ctx.channel.value_or(1));
    std::vector<unsigned char> message = {status, note, velocity};
    ctx.midiout->sendMessage(&message);
  }
  if (ctx.command == "note-off") {
    unsigned char note = parse_byte(ctx.extra_args[0]);
    unsigned char velocity = parse_byte(ctx.extra_args[1]);
    unsigned char status = 0x80 + (ctx.channel.value_or(1) - 1);
    std::vector<unsigned char> message = {status, note, velocity};
    ctx.midiout->sendMessage(&message);
  }
  if (ctx.command == "control-change") {
    unsigned char controller = parse_byte(ctx.extra_args[0]);
    unsigned char value = parse_byte(ctx.extra_args[1]);
    unsigned char status = 0xB0 + (ctx.channel.value_or(1) - 1);
    std::vector<unsigned char> message = {status, controller, value};
    ctx.midiout->sendMessage(&message);
  }
  if (ctx.command == "program-change") {
    unsigned char program = parse_byte(ctx.extra_args[0]);
    unsigned char status = 0xC0 + (ctx.channel.value_or(1) - 1);
    std::vector<unsigned char> message = {status, program};
    ctx.midiout->sendMessage(&message);
  }
  if (ctx.command == "send") {
    std::vector<unsigned char> message;
    for (const std::string &arg : ctx.extra_args) {
      message.push_back(parse_byte(arg));
    }
    ctx.midiout->sendMessage(&message);
  }
  if (ctx.command == "listen") {
    handle_midi_listen(ctx);
  }
  if (ctx.command =="open-in-port") {
    fprintf(stderr, "Opening virtual MIDI input device\n");
    try {
      ctx.midiin->openVirtualPort("My Virtual Port");
    } catch (RtMidiError &error) {
      error.printMessage();
    }
    for (;;) {
      sleep(1);
    }
  }
  if (ctx.command == "open-out-port") {
    fprintf(stderr, "Opening virtual MIDI output device\n");
    try {
      ctx.midiout->openVirtualPort("My Virtual Port");
    } catch (RtMidiError &error) {
      error.printMessage();
    }
    for (;;) {
      sleep(1);
    }
  }
}

void handle_midi_callback (double deltatime, std::vector<unsigned char> *message, void *userData) {
  context *ctx = (context*)userData;
  unsigned int num_bytes = message->size();
  for (unsigned int i = 0; i < num_bytes; i++) {
    printf("%f %d %d\n, ", deltatime, i, message->at(i));
  }
}

void handle_midi_listen (context &ctx) {
  fprintf(stderr, "Listening for MIDI input...\n");
  ctx.midiin->setCallback( &handle_midi_callback, &ctx );
 
  // Don't ignore sysex, timing, or active sensing messages.
  ctx.midiin->ignoreTypes(false, false, false);
 
  // Wait for user input to quit.
  std::cout << "\nReading MIDI input ... press <enter> to quit.\n";
  char input;
  std::cin.get(input);
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
        fprintf(stderr, "Found MIDI input device \"%s\" matching \"%s\" at (index %d)\n", port_name.c_str(), device_name.c_str(), i);
        return i;
      }
    }
    catch (RtMidiError &error) {
      error.printMessage();
      return std::nullopt;
    }
  }
  fprintf(stderr, "MIDI input device not found matching \"%s\"\n", device_name.c_str());
  bail();
}

std::optional<int> get_midi_out_device_index (context &ctx, const std::string &device_name) {
  unsigned int num_ports = ctx.midiout->getPortCount();
  for (unsigned int i = 0; i < num_ports; i++) {
    try {
      std::string port_name = ctx.midiout->getPortName(i);
      if (slug_match(port_name, device_name)) {
        fprintf(stderr, "Found MIDI output device \"%s\" matching \"%s\" (at index %d)\n", port_name.c_str(), device_name.c_str(), i);
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
  std::string portName;
  for (unsigned int i = 0; i < nPorts; i++) {
    try {
      portName = ctx.midiin->getPortName(i);
    }
    catch (RtMidiError &error) {
      error.printMessage();
      return;
    }
    fprintf(stdout, "Input\t%d\t%s\n", i, portName.c_str());
  }
 
  // Check outputs.
  nPorts = ctx.midiout->getPortCount();
  for (unsigned int i=0; i<nPorts; i++) {
    try {
      portName = ctx.midiout->getPortName(i);
    }
    catch (RtMidiError &error) {
      error.printMessage();
      return;
    }
    fprintf(stdout, "Output\t%d\t%s\n", i, portName.c_str());
  }
}

void handle_osc (context &ctx) {
  if (ctx.command == "send") {
    handle_osc_send(ctx);
  }
  if (ctx.command == "listen") {
    handle_osc_listen(ctx);
  }
}

void handle_osc_send (context &ctx) {

  std::string address = ctx.extra_args[0];
  // TODO: Make sure types are converted properly
  std::string type_tags = ctx.extra_args[1];
  std::vector<std::string> arg_strings(ctx.extra_args.begin() + 2, ctx.extra_args.end());

  fprintf(stderr, "%s %s {", address.c_str(), type_tags.c_str());
  for (std::string n : arg_strings) {
    fprintf(stderr, "\"%s\",", n.c_str());
  }
  fprintf(stderr, "}\n");

  const int buffersize = 2048;
  void *buffer[buffersize];
  OSCPP::Client::Packet packet(buffer, buffersize);
  packet.openMessage(address.c_str(), 2 + OSCPP::Tags::array(type_tags.size()));
  for (auto i = 0; i < type_tags.size(); i++) {
    char tag = type_tags[i];
    switch (tag) {
      case 's':
        packet.string(arg_strings[i].c_str());
        break;
      case 'i':
        packet.int32(std::atoi(arg_strings[i].c_str()));
        break;
      case 'f':
        packet.float32(std::atof(arg_strings[i].c_str()));
        break;
      // TODO: Other types
      default:
        fprintf(stderr, "Unrecognized type: \"%c\"\n", tag);
        bail();
    }
  }
  packet.closeMessage();
  printf("Size: %d\n", packet.size());
  
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
  if (sendto(sockfd, &buffer, packet.size(), 0, (const struct sockaddr*)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Error in sendto()\n");
    exit(EXIT_FAILURE);
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;

  fprintf(stderr, "Time to send: %f[s]\n", delta_us / 1e6f);
  fprintf(stderr, "Finished...\n");
}

static volatile bool keep_running = true;

// handle Ctrl+C
static void sigint_handler(int x) {
  keep_running = false;
}

void handle_osc_packet (const OSCPP::Server::Packet& packet) {
  if (packet.isBundle()) {
    // Convert to bundle
    OSCPP::Server::Bundle bundle(packet);

    // Print the time
    std::cout << "#bundle " << bundle.time() << std::endl;

    // Get packet stream
    OSCPP::Server::PacketStream packets(bundle.packets());

    // Iterate over all the packets and call handle_packet recursively.
    // Cuidado: Might lead to stack overflow!
    while (!packets.atEnd()) {
      handle_osc_packet(packets.next());
    }
  } else {
    // Convert to message
    OSCPP::Server::Message msg(packet);

    // Get argument stream
    OSCPP::Server::ArgStream args(msg.args());

    // TODO
    fprintf(stdout, "TODO: Received message\n");

    /*if (msg == "/s_new") {*/
    /*  const char* name = args.string();*/
    /*  const int32_t id = args.int32();*/
    /*  std::cout << "/s_new" << " "*/
    /*    << name << " "*/
    /*    << id << " ";*/
    /*  // Get the params array as an ArgStream*/
    /*  OSCPP::Server::ArgStream params(args.array());*/
    /*  while (!params.atEnd()) {*/
    /*    const char* param = params.string();*/
    /*    const float value = params.float32();*/
    /*    std::cout << param << ":" << value << " ";*/
    /*  }*/
    /*  std::cout << std::endl;*/
    /*} else if (msg == "/n_set") {*/
    /*  const int32_t id = args.int32();*/
    /*  const char* key = args.string();*/
    /*  // Numeric arguments are converted automatically*/
    /*  // to float32 (e.g. from int32).*/
    /*  const float value = args.float32();*/
    /*  std::cout << "/n_set" << " "*/
    /*    << id << " "*/
    /*    << key << " "*/
    /*    << value << std::endl;*/
    /*} else {*/
    /*  // Simply print unknown messages*/
    /*  std::cout << "Unknown message: " << msg << std::endl;*/
    /*}*/
  }
}

void handle_osc_listen (context &ctx) {
  const int buffersize = 2048;
  std::array<char, buffersize> buffer; // declare a 2Kb buffer to read packet data into
  int port = ctx.port.value_or(9000);

  // register the SIGINT handler (Ctrl+C)
  signal(SIGINT, &sigint_handler);

  // open a socket to listen for datagrams (i.e. UDP packets) on port 9000
  const int fd = socket(AF_INET, SOCK_DGRAM, 0);
  fcntl(fd, F_SETFL, O_NONBLOCK); // set the socket to non-blocking
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = INADDR_ANY;
  bind(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in));
  printf("Listening for OSC messages on port %d.\n", port);
  printf("Press Ctrl+C to stop.\n");

  while (keep_running) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);
    struct timeval timeout = {1, 0}; // select times out after 1 second
    if (select(fd+1, &readSet, NULL, NULL, &timeout) > 0) {
      struct sockaddr sa; // can be safely cast to sockaddr_in
      socklen_t sa_len = sizeof(struct sockaddr_in);
      int len = 0;
      while ((len = (int) recvfrom(fd, buffer.data(), buffer.size(), 0, &sa, &sa_len)) > 0) {
        handle_osc_packet(OSCPP::Server::Packet(buffer.data(), len));
      }
    }
  }

  // close the UDP socket
  close(fd);
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
