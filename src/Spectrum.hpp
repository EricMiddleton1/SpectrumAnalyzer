#pragma once

#include <vector>
#include <algorithm>
#include <limits>

#include <Exception.hpp>

//Forward declaration of class Spectrum
class Spectrum;

class FrequencyBin
{
public:
	FrequencyBin(double fStart, double fEnd, double energy = 0.);

	//Operators
	FrequencyBin& operator=(const double energy);
	FrequencyBin& operator+=(const double energy);

	double getFreqStart() const;
	double getFreqEnd() const;

	double getFreqCenter() const;
	double getQ() const;

	double getEnergy() const;
	double getEnergyDB() const;

	void setEnergy(const double energy);
	void setEnergyDB(const double energyDB);

	void addEnergy(const double energy);
	void addEnergyDB(const double energyDB);

private:
	friend class Spectrum;

	double fStart, fEnd, energy;
};


class Spectrum
{
public:
	static const int ERROR_BIN_NOT_FOUND = 0x1000;

	Spectrum(double fStart, double fEnd, double binsPerOctave);

	size_t getBinCount();

	FrequencyBin& get(double frequency);

	FrequencyBin& getByIndex(size_t index);


	void clear();

	void updateStats();

	double getAverageEnergy() const;
	double getAverageEnergyDB() const;
	double getTotalEnergy() const;
	double getTotalEnergyDB() const;

	double getMinFrequency() const;
	double getMaxFrequency() const;

	//Iterators
	std::vector<FrequencyBin>::iterator begin();
	std::vector<FrequencyBin>::iterator end();

private:
	std::vector<FrequencyBin> bins;

	double sum;
	double min, max;
	double minFreq, maxFreq;

};
