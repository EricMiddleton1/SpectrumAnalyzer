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
	fftBlockIn = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * blockSize);
	fftBlockOut = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * blockSize);
	fftChunkIn = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * chunkSize);
	fftChunkOut = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * chunkSize);

	//Compute FFT plans
	fftBlockPlan = fftw_plan_dft_1d(blockSize, fftBlockIn, fftBlockOut,
		FFTW_FORWARD, FFTW_MEASURE);
	fftChunkPlan = fftw_plan_dft_1d(chunkSize, fftChunkIn, fftChunkOut,
		FFTW_FORWARD, FFTW_MEASURE);

	//Calculate frequency to start using chunk fft for
	double sampleRate = audioDevice->getSampleRate();
	double chunkFreq = 2*sampleRate /
		((std::pow(2., 1. / binsPerOctave) - 1.) * chunkSize);
	
	chunkFreq = leftSpectrum->get(chunkFreq).getFreqEnd();

	blockEndIndex = chunkFreq * blockSize / sampleRate;
	chunkStartIndex = chunkFreq * chunkSize / sampleRate;

	std::cout << "[Info] Using block FFT until block " << blockEndIndex
		<< " (" << (int)(sampleRate * blockEndIndex / blockSize) << "Hz)\n";
	
	std::cout << "[Info] Using chunk FFT starting at block " << chunkStartIndex
		<< " (" << (int)(sampleRate * chunkStartIndex / chunkSize) << "Hz)"
		<< std::endl;
	
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
	fftw_destroy_plan(fftBlockPlan);
	fftw_destroy_plan(fftChunkPlan);
	fftw_free(fftBlockIn);
	fftw_free(fftBlockOut);
	fftw_free(fftChunkIn);
	fftw_free(fftChunkOut);
}

void SpectrumAnalyzer::addListener(std::function<void(SpectrumAnalyzer*,
	std::shared_ptr<Spectrum>, std::shared_ptr<Spectrum>)> cb) {

	sigSpectrumUpdate.connect(cb);
}

void SpectrumAnalyzer::removeListener(std::function<void(SpectrumAnalyzer*,
	std::shared_ptr<Spectrum>, std::shared_ptr<Spectrum>)> /*cb*/) {

	//sigSpectrumUpdate.disconnect(cb);
}

std::shared_ptr<AudioDevice> SpectrumAnalyzer::getAudioDevice() {
	return audioDevice;
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

	//This value will be used often
	double sampleRate = audioDevice->getSampleRate();
	
	//Fill block FFT input buffer
	for(unsigned int i = 0; i < blockSize; i++) {
		fftBlockIn[i][0] = blockWindow[i] *
			((double)left[i] / INT16_MAX / blockSize);
		fftBlockIn[i][1] = 0.; //Imaginary
	}

	for(unsigned int i = 0, offset = blockSize - chunkSize;
		i < chunkSize; i++) {
		
		fftChunkIn[i][0] = chunkWindow[i] *
			((double)left[offset + i] / INT16_MAX / chunkSize);
		fftChunkIn[i][1] = 0.; //Imaginary
	}

	//Do FFT on left samples
	fftw_execute(fftBlockPlan); //Block FFT
	fftw_execute(fftChunkPlan); //Chunk FFT

	//Fill left spectrum with new FFT data
	leftSpectrum->clear();


	for(unsigned int i = 0; i < blockEndIndex; ++i) {
		double f = sampleRate * i / blockSize; //Frequency of fft bin

		try {
			//Put the energy from this bin into the appropriate location
			leftSpectrum->get(f).addEnergy(sqr(fftBlockOut[i][0]) +
				sqr(fftBlockOut[i][1]));
		}
		catch(const Exception& e) {
			if(e.getErrorCode() != Spectrum::ERROR_BIN_NOT_FOUND) {
				std::cout << "[Error] SpectrumAnalyzer::fftRoutine Exception caught: "
					<< e.what() << std::endl;
			}
			else {
				//This is not an error
				//The frequency is not in the range of interest for the spectrum
			}
		}
	}

	for(unsigned int i = chunkStartIndex; i <= (chunkSize / 2); ++i) {
		double f = sampleRate * i / chunkSize;

		try {
			leftSpectrum->get(f).addEnergy(sqr(fftChunkOut[i][0]) +
				sqr(fftChunkOut[i][1]));
		}
		catch(const Exception& e) {
			if(e.getErrorCode() != Spectrum::ERROR_BIN_NOT_FOUND) {
				std::cout << "[Error] SpectrumAnalyzer::fftRoutine Exception caught: "
					<< e.what() << std::endl;
			}
		}
	}

	//Now do right FFT
	//Copy real audio data into complex fft input array and scale to [-1., 1.]
	for(unsigned int i = 0; i < blockSize; i++) {
		fftBlockIn[i][0] = blockWindow[i] * ((double)right[i] / INT16_MAX); //Real
		fftBlockIn[i][1] = 0.; //Imaginary
	}

	for(unsigned int i = 0, offset = blockSize - chunkSize;
		i < chunkSize; i++) {
		
		fftChunkIn[i][0] = chunkWindow[i] *
			((double)right[offset + i] / INT16_MAX / chunkSize);
		fftChunkIn[i][1] = 0.; //Imaginary
	}

	//Do FFT on right samples
	fftw_execute(fftBlockPlan);
	fftw_execute(fftChunkPlan);

	//Fill right spectrum with new FFT data
	rightSpectrum->clear();

	for(unsigned int i = 0; i < blockEndIndex; ++i) {
		double f = sampleRate * i / blockSize; //Frequency of fft bin

		try {
			//Put the energy from this bin into the appropriate location
			rightSpectrum->get(f).addEnergy(sqr(fftBlockOut[i][0]) +
				sqr(fftBlockOut[i][1]));
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

	for(unsigned int i = chunkStartIndex; i <= (chunkSize / 2); ++i) {
		double f = sampleRate * i / chunkSize;

		try {
			rightSpectrum->get(f).addEnergy(sqr(fftChunkOut[i][0]) +
				sqr(fftChunkOut[i][1]));
		}
		catch(const Exception& e) {
			if(e.getErrorCode() != Spectrum::ERROR_BIN_NOT_FOUND) {
				std::cout << "[Error] SpectrumAnalyzer::fftRoutine Exception caught: "
					<< e.what() << std::endl;
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
	blockWindow.clear();
	blockWindow.reserve(blockSize);

	for(unsigned int i = 0; i < blockSize; i++) {
		blockWindow.push_back
			(0.5 * (1. - std::cos((2*3.141592654*i)/(blockSize - 1))));
	}

	chunkWindow.clear();
	chunkWindow.reserve(chunkSize);

	for(unsigned int i = 0; i < chunkSize; i++) {
		chunkWindow.push_back
			(0.5 * (1. - std::cos((2.*3.141592654*i)/(chunkSize - 1.))));
	}
}

//Helper function
double SpectrumAnalyzer::sqr(const double x) {
	return x*x;
}
