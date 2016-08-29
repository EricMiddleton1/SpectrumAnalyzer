#include "Exception.hpp"

Exception::Exception(uint16_t errorCode, const std::string& msg)
	: msg(msg) {
	this->errorCode = errorCode;
}

const char* Exception::what() const noexcept {
	return msg.c_str();
}

uint16_t Exception::getErrorCode() const noexcept {
	return errorCode;
}
