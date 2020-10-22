//
//  FP_Tools_nstest.hpp
//  IRBaboon
//
//  Created by Felix Postma on 21/10/2020.
//  Copyright © 2020 Flixor. All rights reserved.
//

#ifndef FP_Tools_nstest_h
#define FP_Tools_nstest_h

#include <stdio.h>

#include <fp_general.h>
#include <JuceHeader.h>

#include "FP_Convolver.hpp"
#include "FP_CircularBufferArray.hpp"
#include "FP_ParallelBufferPrinter.hpp"

#include "boost/algorithm/string.hpp"


namespace fp {
namespace tools {

	// in-place stereo sum to mono, into ch0 of buffer
	void sumToMono(AudioSampleBuffer* buffer);
	
	// makes a mono buffer stereo, and copies contents to the new 2nd channel
	void makeStereo(AudioSampleBuffer* buffer);
	
	// in-place complex multiplication, i.e. (a,b) := (a,b)*(c,d)
	void complexMul(float* a, float* b, float c, float d);
	
	// in-place complex division, i.e. (a,b) := (a,b) / (c,d)
	void complexDivCartesian(float* a, float* b, float c, float d);
	// for polar division, cartesian is first converted to polar:
	// Polar() is less efficient than Cartesian()
	void complexDivPolar(float* amplA, float* phaseA, float amplB, float phaseB);
	
	// applies gain to all buffer samples, such that the maximum sample level is equal to dBGoalLevel
	void normalize(AudioSampleBuffer* buffer, float dBGoalLevel = 0.0f, bool printGain = false);
	
	// converts dB to a linear value (0.0 dB = 1.0 linear)
	float dBToLin (float dB);
	double dBToLin (double dB);
	
	// converts a linear value to dB (0.0 dB = 1.0 linear)
	float linTodB (float lin);
	double linTodB (double lin);
	
	// puts the contents of a file into a buffer
	AudioSampleBuffer fileToBuffer (String fileName, int startSample = 0, int numSamples = 0);
	AudioSampleBuffer fileToBuffer (File file, int startSample = 0, int numSamples = 0);
	
	// does what is says
	bool isPowerOfTwo(int x);
	
	// returns the power of 2 that is >= x. User needs to only specify x.
	// even if the result argument is malicious, safeguards are in place
	int nextPowerOfTwo(int x, int result = 1);
	
	// for rounding off very small values (exponents closer to 0 than than 1e-11) to positive 0.0
	void roundToZero(float* x, float threshold);
	
	// round to 1e-16, positive or negative
	void roundTo1TenQuadrillionth(float* x);
	
	// calculate ampl and phase for pair of samples := {re, im}
	float binAmpl(float* binPtr);
	float binPhase(float* binPtr);
	
	// generate pulse buffer: {1.0, 0.0, 0.0, ...}
	// with optional offset = delay on the pulse
	AudioSampleBuffer generatePulse (int numSamples, int pulseOffset = 0);
	
	// performs linear fade, fade per sample being 1.0 / numSamples.
	// !fadeIn means fadeout
	void linearFade (AudioSampleBuffer* buffer, bool fadeIn, int startSample, int numSamples);
	
	/* fills every channel of the buffer with a sine wave with the specified freq and ampl */
	void sineFill (AudioSampleBuffer *buffer, float freq, float sampleRate, float ampl = 1.0f);
	
	
	/*
	 Get clear iostream errors
	 https://stackoverflow.com/questions/1725714/why-ofstream-would-fail-to-open-the-file-in-c-reasons
	 */
	std::string DescribeIosFailure(const std::ios& stream);
	
} // tools
} // fp

#endif /* FP_Tools_nstest_h */
