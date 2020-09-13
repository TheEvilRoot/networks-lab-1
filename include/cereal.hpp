#ifndef _CEREAL_CEREAL_HPP
#define _CEREAL_CEREAL_HPP

#include <cstdint>
#include <string>
#include <stdexcept>

#include <Windows.h>

class CerealException : public std::exception {
private:
  std::uint32_t error;
  std::string message;

public:
  explicit CerealException(std::uint32_t error): error{error}, std::exception() {
    message = std::string("Error: ") + std::to_string(error);
  }

  char const* what() const override {
    return message.c_str();
  }
};

class Cereal {
private:
  std::string serial_name;
  std::uint32_t baud_rate;

  HANDLE serial_handle{ INVALID_HANDLE_VALUE };
  bool is_opened{ false };

public:
  Cereal(const char* serial_name, std::uint32_t baud_rate);
  ~Cereal();

  std::uint32_t open();

  std::uint32_t init();

  std::uint8_t read_byte() const;

  std::uint32_t write_byte(std::uint8_t byte) const;

  std::uint32_t write_int32(std::int32_t value) const;

  std::int32_t read_int32() const;

  bool is_serial_opened() const { return is_opened; }

  std::uint32_t get_baud_rate() const { return baud_rate; }

  void set_baud_rate(std::uint32_t rate) { baud_rate = rate; }

};

#endif 