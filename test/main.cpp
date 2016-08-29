#include <iostream>
#include <cmath>
#include <cstdint>

#include <portaudio.h>
#include <fftw3.h>

#define SAMPLE_RATE		48000
#define 

//FFTW stuff
fftw_complex *in, *out;
fftw_plan fftPlan;

int paCallback(const void* input, void* output, unsigned long frameCount,
	const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
	void* userData);

int main() {
	std::cout << "[Info] Initializing PortAudio" << std::endl;

	//Initialize PortAudio
	PaError retval;
	if((retval = Pa_Initialize())) {
		std::cout << "[Error] Unable to intialize PortAudio "
			"(error " << (int)retval << ")" << std::endl;

		return -1;
	}

	std::cout << "PortAudio Devices: " << std::endl;

	//Loop through audio devices
	for(int i = 0; i < Pa_GetDeviceCount(); i++) {
		const PaDeviceInfo *device = Pa_GetDeviceInfo(i);

		std::cout << "[" << i << "]: " << device->name << std::endl;

		std::cout << "\tMax input channels: " << device->maxInputChannels
			<< std::endl;
		std::cout << "\tMax output channels: " << device->maxOutputChannels
			<< std::endl;
	}

	int inputDevice = Pa_GetDefaultInputDevice();

	std::cout << "[Info] Using default input device [" << inputDevice << "]"
		<< std::endl;

	PaStream *inputStream;
	PaStreamParameters inputParams;

	inputParams.device = inputDevice;
	inputParams.channelCount = 1;
	inputParams.sampleFormat = paInt16;
	inputParams.suggestedLatency = 0.050;
	inputParams.hostApiSpecificStreamInfo = NULL;

	//Open stream
	retval = Pa_OpenStream(&inputStream, &inputParams, NULL, 48000, 4096,
		paNoFlag, &paCallback, NULL);

	if(retval) {
		std::cout << "[Error] Pa_OpenStream: " << Pa_GetErrorText(retval)
			<< std::endl;

		Pa_Terminate();

		return -1;
	}

	//Start stream
	retval = Pa_StartStream(inputStream);

	if(retval) {
		std::cout << "[Error] Pa_StartStream: " << Pa_GetErrorText(retval)
			<< std::endl;

		Pa_Terminate();

		return -1;
	}

	//Wait
	std::cin.get();

	Pa_StopStream(&inputStream);

	//Close PortAudio
	Pa_Terminate();
}

int paCallback(const void* input, void* output, unsigned long frameCount,
	const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
	void* userData) {
	
	double avg = 0;

	for(int i = 0; i < frameCount; i++) {
		int16_t sample = ((int16_t*)input)[i];

		avg += (double)sample * sample;
	}

	double rms = std::sqrt(avg / frameCount);

	double db = 20 * std::log10(rms / 65535);

	std::cout << db << '\n';

	return paContinue;
}
