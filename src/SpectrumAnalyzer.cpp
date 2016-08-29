#include "SpectrumAnalyzer.hpp"

SpectrumAnalyzer::SpectrumAnalyzer(std::shared_ptr<AudioDevice>& _audioDevice,
	unsigned int chunksPerBlock, double fStart, double fEnd,
	double binsPerOctave, unsigned int threadCount)
	:	workUnit(std::make_unique<boost::asio::io_service::work>(ioService))
	,	leftSpectrum(std::make_shared<Spectrum>(fStart, fEnd, binsPerOctave))
	,	rightSpectrum(std::make_shared<Spectrum>(fStart, fEnd, binsPerOctave))
	,	audioDevice(_audioDevice)
	,	chunkSize{audioDevice->getBlockSize()}
	,	blockSize{chunkSize * chunksPerBlock} {
	
	//Initialize FFTW stuff
	//fftIn, fftOut, fftPlan
	fftIn = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * blockSize);
	fftOut = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * blockSize);

	//Compute FFT plan
	fftPlan = fftw_plan_dft_1d(blockSize, fftIn, fftOut, FFTW_FORWARD,
		FFTW_MEASURE);

	//Generate FFT window function
	generateWindow();

	//Initialize audio buffers
	//leftBuffer = new int16_t[blockSize];
	//rightBuffer = new int16_t[blockSize];
	leftBuffer.resize(blockSize);
	rightBuffer.resize(blockSize);

	//Launch threads
	for(unsigned int i = 0; i < threadCount; ++i) {
		asyncThreads.emplace_back(std::bind(&SpectrumAnalyzer::threadRoutine,
			this));
	}

	//Register audio callback
	auto cb = [this](const int16_t* left, const int16_t* right) {
			cbAudio(left, right);
		};

	callbackID = audioDevice->addCallback(cb);
}

SpectrumAnalyzer::~SpectrumAnalyzer() {
	//Remove audio callback
	audioDevice->removeCallback(callbackID);

	//Shutdown threads
	workUnit.reset();

	for(auto& thread : asyncThreads) {
		thread.join();
	}

	//Cleanup fftw stuff
	fftw_destroy_plan(fftPlan);
	fftw_free(fftIn);
	fftw_free(fftOut);
}

void SpectrumAnalyzer::addListener(std::function<void(SpectrumAnalyzer*,
	std::shared_ptr<Spectrum>, std::shared_ptr<Spectrum>)> cb) {

	sigSpectrumUpdate.connect(cb);
}

void SpectrumAnalyzer::removeListener(std::function<void(SpectrumAnalyzer*,
	std::shared_ptr<Spectrum>, std::shared_ptr<Spectrum>)> /*cb*/) {

	//sigSpectrumUpdate.disconnect(cb);
}

std::shared_ptr<Spectrum> SpectrumAnalyzer::getLeftSpectrum() {
	return leftSpectrum;
}

std::shared_ptr<Spectrum> SpectrumAnalyzer::getRightSpectrum() {
	return rightSpectrum;
}

void SpectrumAnalyzer::cbAudio(const int16_t* left, const int16_t* right) {
	//Lock the buffer mutex
	//std::unique_lock<std::mutex> bufferLock(bufferMutex);

	//Shift the samples forward by 1 chunk size
	std::memcpy(leftBuffer.data(), &leftBuffer[chunkSize],
		sizeof(int16_t) * (blockSize - chunkSize));
	std::memcpy(rightBuffer.data(), &rightBuffer[chunkSize],
		sizeof(int16_t) * (blockSize - chunkSize));

	//Copy the new chunk samples into the end of the block
	std::memcpy(&leftBuffer[blockSize - chunkSize], left,
		sizeof(int16_t) * chunkSize);
	std::memcpy(&rightBuffer[blockSize - chunkSize], right,
		sizeof(int16_t) * chunkSize);

	//Post the fft routine to the async thread pool
	ioService.post(std::bind(&SpectrumAnalyzer::fftRoutine, this,
		leftBuffer, rightBuffer));
}

void SpectrumAnalyzer::threadRoutine() {
	//Run work from ioService
	ioService.run();

	std::cout << "[Info] SpectrumAnalyzer::threadRoutine: thread returning"
		<< std::endl;
}

void SpectrumAnalyzer::fftRoutine(std::vector<int16_t> left,
	std::vector<int16_t> right) {
	//Lock the buffer mutex
	//std::unique_lock<std::mutex> bufferLock(bufferMutex);

	//Get pointers to the back buffers
	//std::vector<int16_t> left, right;

	//left.resize(blockSize);
	//right.resize(blockSize);

	//Copy the buffers into the vectors
	//std::memcpy((void*)left.data(), leftBuffer, sizeof(int16_t) * blockSize);
	//std::memcpy((void*)right.data(), rightBuffer, sizeof(int16_t) * blockSize);

	//Unlock the buffer mutex
	//bufferLock.unlock();

	//This value will be used often
	double sampleRate = audioDevice->getSampleRate();
	
	//Do left FFT first
	//Copy real audio data into complex fft input array and scale to [-1., 1.]
/*
	fftw_complex* cpxPtr = fftIn;
	std::for_each(left, left + blockSize, [&cpxPtr](auto sample) {
		(*cpxPtr)[0] = ((double)sample / INT16_MAX); //Real
		(*cpxPtr++)[1] = 0.; //Imaginary
	});
*/
	for(unsigned int i = 0; i < blockSize; i++) {
		fftIn[i][0] = window[i] * ((double)left[i] / INT16_MAX); //Real
		fftIn[i][1] = 0.; //Imaginary
	}

	//Do FFT on left samples
	fftw_execute(fftPlan);

	//Fill left spectrum with new FFT data
	leftSpectrum->clear();

	for(unsigned int i = 0; i <= (blockSize / 2); ++i) {
		double f = sampleRate * i / blockSize; //Frequency of fft bin

		try {
			//Put the energy from this bin into the appropriate location
			leftSpectrum->get(f).addEnergy(sqr(fftOut[i][0]) + sqr(fftOut[i][1]));
		}
		catch(const Exception& e) {
			if(e.getErrorCode() != Spectrum::ERROR_BIN_NOT_FOUND) {
				std::cout << "[Error] SpectrumAnalyzer::cbAudio Exception caught: "
					<< e.what() << std::endl;
			}
			else {
				//This is not an error
				//The frequency is not in the range of interest for the spectrum
			}
		}
	}

	//Now do right FFT
	//Copy real audio data into complex fft input array and scale to [-1., 1.]
	auto cpxPtr = fftIn;
	std::for_each(right.begin(), right.end(), [&cpxPtr](auto sample) {
		(*cpxPtr)[0] = (double)sample / INT16_MAX; //Real
		(*cpxPtr++)[1] = 0.; //Imaginary
	});

	//Do FFT on right samples
	fftw_execute(fftPlan);

	//Fill right spectrum with new FFT data
	rightSpectrum->clear();

	for(unsigned int i = 0; i <= (blockSize / 2); ++i) {
		double f = sampleRate * i / blockSize; //Frequency of fft bin

		try {
			//Put the energy from this bin into the appropriate location
			rightSpectrum->get(f).addEnergy(sqr(fftOut[i][0]) + sqr(fftOut[i][1]));
		}
		catch(const Exception& e) {
			if(e.getErrorCode() != Spectrum::ERROR_BIN_NOT_FOUND) {
				std::cout << "[Error] SpectrumAnalyzer::cbAudio Exception caught: "
					<< e.what() << std::endl;
			}
			else {
				//This is not an error
				//The frequency is not in the range of interest for the spectrum
			}
		}
	}

	//Update stats for both spectrums
	leftSpectrum->updateStats();
	rightSpectrum->updateStats();

	//Call all listeners
	sigSpectrumUpdate(this, leftSpectrum, rightSpectrum);
}

void SpectrumAnalyzer::generateWindow() {
	//Hanning window
	window.clear();
	window.reserve(blockSize);

	for(unsigned int i = 0; i < blockSize; i++) {
		window.push_back(0.5 * (1. - std::cos((2*3.141592654*i)/(blockSize - 1))));
	}
}

//Helper function
double SpectrumAnalyzer::sqr(const double x) {
	return x*x;
}
