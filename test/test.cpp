#include <iostream>
#include <cmath>
#include <cstdint>

#include <portaudio.h>
#include <fftw3.h>

#define SAMPLE_RATE		48000
#define BLOCK_SIZE		4096

//FFTW stuff
fftw_complex *fftIn, *fftOut;
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
	inputParams.suggestedLatency = 0.010;
	inputParams.hostApiSpecificStreamInfo = NULL;

	//Open stream
	retval = Pa_OpenStream(&inputStream, &inputParams, NULL, SAMPLE_RATE, BLOCK_SIZE,
		0, &paCallback, NULL);

	if(retval) {
		std::cout << "[Error] Pa_OpenStream: " << Pa_GetErrorText(retval)
			<< std::endl;

		Pa_Terminate();

		return -1;
	}

	//Initialize FFTW
	std::cout << "[Info] Initializing FFTW" << std::endl;
	
	//Allocate in/out buffers
	fftIn = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * BLOCK_SIZE);
	fftOut = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * BLOCK_SIZE);

	//Compute FFT plan
	fftPlan = fftw_plan_dft_1d(BLOCK_SIZE, fftIn, fftOut, FFTW_FORWARD, FFTW_MEASURE);

	std::cout << "[Info] Done initializing FFTW" << std::endl;

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

	//Stop audio stream
	Pa_StopStream(inputStream);

	//Close PortAudio
	Pa_Terminate();

	//Free FFT stuff
	fftw_destroy_plan(fftPlan);
	fftw_free(fftIn);
	fftw_free(fftOut);
}

double cpxMag(double real, double imag) {
	return std::sqrt(real*real + imag*imag);
}

int paCallback(const void* input, void* output, unsigned long frameCount,
	const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags,
	void* userData) {

/*
	double avg = 0;

	for(int i = 0; i < frameCount; i++) {
		int16_t sample = ((int16_t*)input)[i];

		avg += (double)sample * sample;
	}

	double rms = std::sqrt(avg / frameCount);

	double db = 20 * std::log10(rms / 65535);

	std::cout << db << '\n';
*/

	//Copy the audio samples into the FFT input buffer
	int16_t* samples = (int16_t*)input;
	for(int i = 0; i < frameCount; i++) {
		fftIn[i][0] = samples[i] / 32768. / frameCount;
		fftIn[i][1] = 0;
	}

	//Execute the FFT
	fftw_execute(fftPlan);
	
	//Find peak
	double peak = 0., peakIndex = 0;

	for(int i = 0; i < frameCount/2; i++) {
		double mag = cpxMag(fftOut[i][0], fftOut[i][1]);

		if(mag > peak) {
			peak = mag;
			peakIndex = i;
		}
	}

	
	std::cout << "[Info] Max value: " << 20*std::log10(peak) << " at "
		<< peakIndex*SAMPLE_RATE/frameCount << "hz" << std::endl;


	return paContinue;
}
