#pragma once

#include <memory>
#include <thread>
#include <functional>
#include <cstdint>
#include <vector>

#include <boost/asio.hpp>
#include <boost/signals2.hpp>

#include <fftw3.h>

#include "AudioDevice.hpp"
#include "Spectrum.hpp"

class SpectrumAnalyzer
{
public:
	SpectrumAnalyzer(std::shared_ptr<AudioDevice>& audioDevice,
		double fStart, double fEnd,
		double binsPerOctave, unsigned int maxBlockSize, unsigned int threadCount = 4);
	~SpectrumAnalyzer();

	void addListener(std::function<void(SpectrumAnalyzer*,
		std::shared_ptr<Spectrum>, std::shared_ptr<Spectrum>)> cb);

	void removeListener(std::function<void(SpectrumAnalyzer*,
		std::shared_ptr<Spectrum>, std::shared_ptr<Spectrum>)>);

	std::shared_ptr<AudioDevice> getAudioDevice();

	std::shared_ptr<Spectrum> getLeftSpectrum();
	std::shared_ptr<Spectrum> getRightSpectrum();

private:
	void threadRoutine();
	void cbAudio(const int16_t* left, const int16_t* right);
	void fftRoutine(std::vector<int16_t>, std::vector<int16_t>);
	void generateWindow();

	static double sqr(const double x);

	//Thread stuff
	boost::asio::io_service ioService;
	std::unique_ptr<boost::asio::io_service::work> workUnit;
	std::vector<std::thread> asyncThreads;

	std::shared_ptr<Spectrum> leftSpectrum, rightSpectrum;

	//Audio sample buffers
	std::vector<int16_t> leftBuffer, rightBuffer;
//	std::mutex bufferMutex;

	//FFT stuff
	fftw_complex *fftIn, *fftOut;
	fftw_plan fftPlan;
	std::vector<double> fftWindow;

	//Signals
	boost::signals2::signal<void(SpectrumAnalyzer*,
		std::shared_ptr<Spectrum> left, std::shared_ptr<Spectrum> right)>
		sigSpectrumUpdate;

	//Audio device stuff
	std::shared_ptr<AudioDevice> audioDevice;
	unsigned int callbackID;
	unsigned int chunkSize; //Size of buffer from audio device
	unsigned int blockSize;	//Size of buffer sent through fft
};
