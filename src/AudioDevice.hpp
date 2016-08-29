#pragma once

#include <functional>
#include <map>
#include <mutex>

#include <portaudio.h>

#include "Exception.hpp"


#define STREAM_LATENCY	0.010

//Stereo audio input class

class AudioDevice
{
public:
	static const int DEFAULT_DEVICE = -1;

	//Error codes
	static const int ERROR_PORTAUDIO_INITIALIZE = 0x00002000;
	static const int ERROR_PORTAUDIO_STREAM_OPEN = 0x00002001;
	static const int ERROR_CALLBACK_INVALID_ID = 0x00002002;

	
	AudioDevice(int deviceID, unsigned int sampleRate, unsigned int blockSize);
	~AudioDevice();

	unsigned int addCallback(std::function<void(const int16_t*,
		const int16_t*)> cb);
	void removeCallback(unsigned int id);

	int startStream();
	int stopStream();

	unsigned int getSampleRate();
	unsigned int getBlockSize();

	bool isRunning();

private:
	//PortAudio callback
	static int paCallback(const void* input, void* output,
		unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags, void* userData);

	//PortAudio stuff
	PaStream *inputStream;

	//Callbacks
	std::map<unsigned int,
		std::function<void(const int16_t* left, const int16_t* right)>> callbacks;
	std::mutex callbackMutex;

	//Audio stuff
	int16_t *leftSamples, *rightSamples;
	unsigned int sampleRate, blockSize;

	bool running;
};
