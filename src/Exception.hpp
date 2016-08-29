#pragma once

#include <exception>
#include <string>
#include <cstdint>

class Exception : public std::exception {
public:
	//constructor
	Exception(uint16_t errorCode, const std::string& msg);

	//returns the error message
	virtual const char* what() const noexcept;

	uint16_t getErrorCode() const noexcept;

private:
	uint16_t errorCode;
	std::string msg;
};
