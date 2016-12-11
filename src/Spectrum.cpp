#include "Spectrum.hpp"

#include <cmath>
#include <iostream>
#include <algorithm>

FrequencyBin::FrequencyBin(double _fStart, double _fEnd, double _energy)
	:	fStart{_fStart}
	,	fEnd{_fEnd}
	,	energy{_energy} {

}

FrequencyBin& FrequencyBin::operator=(const double _energy) {
	energy = _energy;

	return *this;
}

FrequencyBin& FrequencyBin::operator+=(double _energy) {
	energy += _energy;

	return *this;
}

double FrequencyBin::getFreqStart() const {
	return fStart;
}

double FrequencyBin::getFreqEnd() const {
	return fEnd;
}

double FrequencyBin::getFreqCenter() const {
	//Return geometric mean of fStart, fEnd
	return std::sqrt(fStart * fEnd);
}

double FrequencyBin::getQ() const {
	//Return quality factor of frequency bin
	return std::sqrt(fStart * fEnd) / (fEnd - fStart);
}

double FrequencyBin::getEnergy() const {
	return energy;
}

double FrequencyBin::getEnergyDB() const {
	return 20 * std::log10(energy);
}

void FrequencyBin::setEnergy(const double _energy) {
	energy = _energy;
}

void FrequencyBin::setEnergyDB(const double _energyDB) {
	energy = std::pow(10., _energyDB / 20.);
}

void FrequencyBin::addEnergy(const double _energy) {
	energy += _energy;
}

void FrequencyBin::addEnergyDB(const double _energyDB) {
	energy += std::pow(10., _energyDB / 20.);
}



Spectrum::Spectrum(double fStart, double fEnd, double binsPerOctave) {
	double multiplier = std::pow(2., 1./binsPerOctave);
	double curFreq = fStart;

	while(curFreq < fEnd) {
		//Calculate new end frequency based on standard octave frequency doubling
		double curEnd = curFreq * multiplier;

		//If center frequency for this bin is above fEnd, combine this bin
		//with the previous bin
		double fCenter = std::sqrt(curFreq*curEnd);
		if(fCenter > fEnd) {
			bins[bins.size() - 1].fEnd = fEnd; //Extend the previous bin

			std::cout << "Extending previous bin fEnd to " << fEnd << "Hz\n";

			break;
		}
		else {
			//Construct a new frequency bin at the end of the vector
			bins.emplace_back(curFreq, curEnd);

			std::cout << "New bin: [" << curFreq << "hz, " << curEnd << "hz), Q = "
				<< (bins.end() - 1)->getQ() << "\n";

			curFreq = curEnd;
		}
	}

	std::cout << std::endl;
}

FrequencyBin& Spectrum::get(double frequency) {
	auto foundBin = std::find_if(std::begin(bins),
		std::end(bins),
		[&](const FrequencyBin& bin) {
			return frequency >= bin.fStart && frequency < bin.fEnd;
		});

	if(foundBin == std::end(bins)) {
		throw Exception(ERROR_BIN_NOT_FOUND,
			"Spectrum::get: Frequency bin not found");
	}

	return *foundBin;
}

FrequencyBin& Spectrum::getByIndex(size_t index) {
	return bins[index];
}

size_t Spectrum::getBinCount() {
	return bins.size();
}

void Spectrum::clear() {
	for(auto& bin : bins) {
		bin.energy = 0.;
	}
}

void Spectrum::updateStats() {
	sum = 0;

	FrequencyBin *minBin = &bins[0];
	FrequencyBin *maxBin = &bins[0];

	for(auto& bin : bins) {
		sum += bin.energy;

		if(bin.energy < minBin->energy) {
			minBin = &bin;
		}
		if(bin.energy > maxBin->energy) {
			maxBin = &bin;
		}
	}

	minFreq = minBin->getFreqCenter();
	maxFreq = maxBin->getFreqCenter();
}

double Spectrum::getAverageEnergy() const {
	return sum / bins.size();
}

double Spectrum::getAverageEnergyDB() const {
	return 20. * std::log10(getAverageEnergy());
}

double Spectrum::getTotalEnergy() const {
	return sum;
}

double Spectrum::getTotalEnergyDB() const {
	return 20. * std::log10(getTotalEnergy());
}

double Spectrum::getMinFrequency() const {
	return minFreq;
}

double Spectrum::getMaxFrequency() const {
	return maxFreq;
}

std::vector<FrequencyBin>::iterator Spectrum::begin() {
	return std::begin(bins);
}

std::vector<FrequencyBin>::iterator Spectrum::end() {
	return std::end(bins);
}


