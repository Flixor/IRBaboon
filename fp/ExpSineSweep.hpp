/*
 *  Copyright © 2021 Felix Postma. 
 */

#pragma once

#include <fp_include_all.hpp>
#include <JuceHeader.h>


namespace fp {


/* The ExpSineSweep class creates and holds an exponential sine sweep,
 * used for capturing impulse responses. This is an implementation of:
 * Angelo Farina, "Simultaneous measurement of impulse response and distortion with a swept-sine technique", 2000.
 * There are also several helper methods that are useful during experimentation.
 */	

class ExpSineSweep {

public:
	ExpSineSweep();
	~ExpSineSweep();

	/* Farina 2000 exp sweep method */
	void generate(double durationSecs, double sampleRate, double lowFreq, double highFreq, double dBGain);
	AudioBuffer<double> getSweep();
	AudioBuffer<float> getSweepFloat();

	/* generates the same sweep but reversed */
	void generateInv();
	void generateInv(double durationSecs, double sampleRate, double lowFreq, double highFreq, double dBGain);
	AudioBuffer<double> getSweepInv();
	AudioBuffer<float> getSweepInvFloat();

	// returns the sample index where the instantaneous freq is closest to the specified freq
	int getSampleIndexAtFreq (double freq);
	int getSampleIndexAtFreq (double freq, double durationSecs, double sampleRate, double lowFreq, double highFreq);
	
	// return the instantaneous freq at specified sample index
	double getFreqAtSampleIndex(int index);
	double getFreqAtSampleIndex(int index, double durationSecs, double sampleRate, double lowFreq, double highFreq);
	
	// applies linear attenuation (until 0.0 linear level) on every sample after sample at specified freq
	void linFadeout (double freq);
	
	// applies steady dB attenuation (until -80 dB at end of file) on every sample after sample at specified freq
	void dBFadeout (double freq);
	
	// clears all samples after sample at specified instantaneous freq
	void brickwallFadeout (double freq);

	
private:
	void assignParameters(double durationSecs, double sampleRate, double lowFreq, double highFreq);
	double getFreqAtSampleIndexHelper(int index);
	int getSampleHelper(double freq);

	
	double SR, w1, w2, T, K, L, k, kend;
	AudioBuffer<double> sweep;
	AudioBuffer<double> sweepInv;
};

} // fp
