/*
 * COM-based one-sided client-server communication protocol.
 */

#include "cereal.hpp"

#include <string>
#include <iostream>


#define DEBUG true

#define D(a, ...) if (DEBUG) fprintf(stderr, a, __VA_ARGS__)

/*
 * Protocol call: send string (0x1)
 * - send command byte 0x1
 * - send 4 bytes of string length
 * - send N bytes where N is length of string (no null-termination)
 * - send pairity byte
 */
void sendString(Cereal& cereal, const std::string& value) {
  D("Sending command byte 0x1\n");
  cereal.write_byte(0x01);
  D("Sending length %llu\n", value.size());
  cereal.write_int32(value.size());
  D("Sending string...\n");
  std::uint8_t pairity = 0;
  for (auto i = 0; i < value.size(); i++) {
    cereal.write_byte(value[i]);
    D("Progress: %08lld/%08lld\r", i, value.size());
    pairity |= (value[i] & 0x1);
  }
  D("\nSending pairity...\n");
  cereal.write_byte(pairity);
  D("String sending is done\n");
}

/*
 * Protocol call: set baud rate (0x2)
 * - send command byte 0x2
 * - send 4 bytes int 
 */
void sendBaud(Cereal& cereal, std::uint32_t baud) {
  D("Sending command 0x2\n");
  cereal.write_byte(0x02);
  D("Sending baud %d\n", baud);
  cereal.write_int32(baud);
  D("Sending baud is done\n");
}

/*
 * Protocol call: shutdown (0x3)
 * - send command byte 0x3
 */
void sendShutdown(Cereal& cereal) {
  D("Sending command 0x3\n");
  cereal.write_byte(0x03);
  D("Sending shutdown is done");
}

void server() {
  fprintf(stderr, "Server\n");
  Cereal server("COM2", 115200);
  server.open();
  fprintf(stderr, "init: %d\n", server.init());
  while (true) {
    auto ctrl = server.read_byte();
    switch (ctrl) {
    case 0x01: {
      auto length = server.read_int32();
      std::string buffer;
      std::uint8_t pairity = 0;
      for (int i = 0; i < length; i++) {
        auto byte = server.read_byte();
        buffer += (char)byte;
        pairity |= (byte & 0x1);
      }
      auto chk = server.read_byte();
      if (chk != pairity) {
        fprintf(stderr, "Pairity failed %d != %d\n", pairity, chk);
      } else {
        fprintf(stderr, "String: %s\n", buffer.c_str());
      }
      break;
    }
    case 0x02: {
      auto new_baud = server.read_int32();
      server.set_baud_rate(new_baud);
      fprintf(stderr, "re-init: %d\n", server.init());
      fprintf(stderr, "Baud: set to %d\n", new_baud);
      break;
    }
    case 0x03: {
      fprintf(stderr, "Got shutdown request");
      return;
    }
    }
  }
}

void client() {
  fprintf(stderr, "Client\n");
  Cereal client("COM3", 1115200);
  client.open();
  fprintf(stderr, "init: %d\n", client.init());
  while (true) {
    std::cout << ">> ";
    std::string cmd;
    std::getline(std::cin, cmd);
    switch (cmd[0]) {
    case 's': case 'S': {
      std::cout << "S >> ";
      std::string string;
      std::getline(std::cin, string);
      sendString(client, string);
      break;
    }
    case 'b': case 'B': {
      std::cout << "B >> ";
      std::string buffer;
      std::getline(std::cin, buffer);
      int baud = std::stoi(buffer);
      sendBaud(client, baud);
      client.set_baud_rate(baud);
      fprintf(stderr, "re-init: %d\n", client.init());
      break;
    }
    case 'q': case 'Q': {
      sendShutdown(client);
      break;
    }
    }
  }
}

void createClientProcess(const std::string& exec) {
  std::string cli = exec + " client";
  std::unique_ptr<char[]> args = std::make_unique<char[]>(cli.size() + 1);
  std::memcpy(args.get(), cli.c_str(), cli.size());
  args[cli.size()] = 0;

  STARTUPINFO suInfo{};
  PROCESS_INFORMATION psInfo{};
  CreateProcessA(nullptr, args.get(), nullptr, nullptr, false, CREATE_NEW_CONSOLE, nullptr, nullptr, &suInfo, &psInfo);

  CloseHandle(psInfo.hProcess);
  CloseHandle(psInfo.hThread);
}

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    createClientProcess(std::string(argv[0]));
    server();
  } else {
    std::string type(argv[1]);
    if (type == "server") {
      server();
    } else if (type == "client"){
      client();
    } else {
      fprintf(stderr, "Unknown type: %s\n", argv[1]);
    }
  }
}