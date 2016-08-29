#include "AudioDevice.hpp"

#include <iostream>

const int AudioDevice::DEFAULT_DEVICE;

AudioDevice::AudioDevice(int _deviceID, unsigned int _sampleRate,
	unsigned int _blockSize) 
	:	sampleRate{_sampleRate}
	,	blockSize{_blockSize}
	,	running{false} {
	
	//Initialize PortAudio
	PaError retval;

	retval = Pa_Initialize();
	if(retval) {
		//Failed to initialize
		throw Exception(ERROR_PORTAUDIO_INITIALIZE, "AudioDevice::AudioDevice: "
			"Failed to initialize PortAudio: " +
			std::string(Pa_GetErrorText(retval)));
	}

	if(_deviceID == DEFAULT_DEVICE) {
		//Get default device ID
		_deviceID = Pa_GetDefaultInputDevice();
	}

	//For debugging
	std::cout << "[Info] AudioDevice::AudioDevice: Using input device: "
		<< Pa_GetDeviceInfo(_deviceID)->name << std::endl;

	//Set stream parameters
	PaStreamParameters inputParams;

	inputParams.device = _deviceID;
	inputParams.channelCount = 2; //Stereo
	inputParams.sampleFormat = paInt16; //16bit audio
	inputParams.suggestedLatency = STREAM_LATENCY;
	inputParams.hostApiSpecificStreamInfo = NULL;

	//Open stream
	retval = Pa_OpenStream(&inputStream, &inputParams, NULL, sampleRate,
		blockSize, 0, &AudioDevice::paCallback, this);

	if(retval) {
		//Failed to open stream

		//Terminate PortAudio
		Pa_Terminate();

		throw Exception(ERROR_PORTAUDIO_STREAM_OPEN, "AudioDevice::AudioDevice: "
			"Failed to open stream: " + std::string(Pa_GetErrorText(retval)));
	}

	//Allocate memory for left/right audio samples
	leftSamples = new int16_t[blockSize];
	rightSamples = new int16_t[blockSize];
}

AudioDevice::~AudioDevice() {
	if(running) {
		//Stop the stream
		Pa_StopStream(&inputStream);
	}

	//Terminate PortAudio
	Pa_Terminate();

	//Free left/right audio sample buffers
	delete[] leftSamples;
	delete[] rightSamples;
}

unsigned int AudioDevice::addCallback(std::function<void(const int16_t*,
	const int16_t*)> cb) {
	static unsigned int id = 0;
	
	//Insert the callback into the map
	auto cbPair = std::pair<unsigned int,
		std::function<void(const int16_t*, const int16_t*)>>(id, cb);
	callbacks.insert(cbPair);

	return id++;
}


void AudioDevice::removeCallback(unsigned int id) {
	//Lock the callback vector mutex
	std::unique_lock<std::mutex> callbackLock(callbackMutex);

	//Find callback with given id
	auto cbItr = callbacks.find(id);

	if(cbItr == callbacks.end()) {
		throw Exception(ERROR_CALLBACK_INVALID_ID, "AudioDevice::removeCallback: "
			"Invalid callback ID");
	}

	//Remove callback
	callbacks.erase(cbItr);

//mutex is released here
}

int AudioDevice::startStream() {
	PaError retval = Pa_StartStream(inputStream);

	return (int)retval;
}

int AudioDevice::stopStream() {
	PaError retval = Pa_StopStream(inputStream);

	return (int)retval;
}

unsigned int AudioDevice::getSampleRate() {
	return sampleRate;
}

unsigned int AudioDevice::getBlockSize() {
	return blockSize;
}

int AudioDevice::paCallback(const void* input, void*,
	unsigned long frameCount, const PaStreamCallbackTimeInfo*,
	PaStreamCallbackFlags, void* userData) {

	AudioDevice *pDev = (AudioDevice*)userData;

	//Unstrip the left/right audio samples
	//By default, they come packed l0/r0/l1/r1...
	const int16_t* samples = (const int16_t*)input;
	for(unsigned int i = 0; i < frameCount; i++) {
		int sampleIndex = 2*i;

		pDev->leftSamples[i] = samples[sampleIndex];
		pDev->rightSamples[i] = samples[sampleIndex + 1];
	}

	//Lock the callback vector mutex
	std::unique_lock<std::mutex> callbackLock(pDev->callbackMutex);

	//Call each callback
	for(auto& cb : pDev->callbacks) {
		cb.second(pDev->leftSamples, pDev->rightSamples);
	}

	return paContinue; //mutex is released here
}
