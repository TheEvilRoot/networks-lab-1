#include "cereal.hpp"

Cereal::Cereal(const char* serial_name, std::uint32_t baud_rate) 
  : serial_name{ serial_name }, baud_rate{ baud_rate } { }

Cereal::~Cereal() {
  if (serial_handle != INVALID_HANDLE_VALUE && is_serial_opened()) {
    CloseHandle(serial_handle);
  }
}

std::uint32_t Cereal::open() {
	if (is_serial_opened()) {
		return ERROR_SUCCESS;
	}

	auto handle = CreateFileA(serial_name.c_str(), 
		GENERIC_READ | GENERIC_WRITE,
		0, nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		return GetLastError();
	}
	serial_handle = handle;
	is_opened = true;
	return ERROR_SUCCESS;
}

std::uint32_t Cereal::init() {
	if (!is_serial_opened() || serial_handle == INVALID_HANDLE_VALUE) {
		return ERROR_INVALID_HANDLE;
	}

	DCB params{ 0 };
	if (GetCommState(serial_handle, &params)) {
		params.BaudRate = baud_rate;
		params.ByteSize = 8;
		params.StopBits = ONESTOPBIT;
		params.Parity = NOPARITY;
		params.fDtrControl = DTR_CONTROL_ENABLE;

		if (SetCommState(serial_handle, &params)) {
			PurgeComm(serial_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
			Sleep(1000); // don't know why, but... whatever
			return ERROR_SUCCESS;
		}
	}
	return GetLastError();
}

std::uint8_t Cereal::read_byte() const {
	COMSTAT serialStatus;
	DWORD serialErrors;

	ClearCommError(serial_handle, &serialErrors, &serialStatus);

	DWORD actualRead = 0;
	uint8_t b = 0;
	if (ReadFile(serial_handle, &b, 1, &actualRead, nullptr)) {
		return b;
	}
	throw CerealException(GetLastError());
}

std::uint32_t Cereal::write_byte(std::uint8_t byte) const {
	DWORD actualSend = 0;

	auto result = WriteFile(serial_handle, &byte, 1, &actualSend, nullptr);
	if (actualSend != 1) {
		throw CerealException(GetLastError());
	}

	COMSTAT serialStatus;
	DWORD serialErrors;
	ClearCommError(serial_handle, &serialErrors, &serialStatus);

	return actualSend;
}

std::uint32_t Cereal::write_int32(std::int32_t value) const {
	std::uint8_t bytes[4] = { 
		value & 0xff,
		(value & 0xff00) >> 8,
		(value & 0xff0000) >> 16,
		(value & 0xff000000) >> 24 };

	DWORD actualSend = 0;

	auto result = WriteFile(serial_handle, bytes, 4, &actualSend, nullptr);
	if (actualSend != 4) {
		throw CerealException(GetLastError());
	}

	COMSTAT serialStatus;
	DWORD serialErrors;
	ClearCommError(serial_handle, &serialErrors, &serialStatus);

	return actualSend;
}

std::int32_t Cereal::read_int32() const {
	COMSTAT serialStatus;
	DWORD serialErrors;

	ClearCommError(serial_handle, &serialErrors, &serialStatus);

	DWORD actualRead = 0;
	uint8_t bytes[4] = { 0x0 };
	if (ReadFile(serial_handle, bytes, 4, &actualRead, nullptr) && actualRead == 4) {
		std::uint32_t value = 0;
		std::memcpy(reinterpret_cast<std::uint8_t*>(&value), bytes, 4);
		return value;
	}
	throw CerealException(GetLastError());
}