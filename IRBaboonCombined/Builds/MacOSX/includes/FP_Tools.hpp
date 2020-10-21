//
//  FP_Tools.hpp
//
//  Created by Felix Postma on 27/03/2019.
//

#ifndef FP_Tools_hpp
#define FP_Tools_hpp

#include <stdio.h>

#include <FP_general.h>
#include <JuceHeader.h>


class FP_Tools {
public:
	
	// in-place stereo sum to mono, into ch0 of buffer
	static void sumToMono(AudioSampleBuffer* buffer);

	// makes a mono buffer stereo, and copies contents to the new 2nd channel
	static void makeStereo(AudioSampleBuffer* buffer);
	
	// in-place complex multiplication, i.e. (a,b) := (a,b)*(c,d)
	static void complexMul(float* a, float* b, float c, float d);

	// in-place complex division, i.e. (a,b) := (a,b) / (c,d)
	static void complexDivCartesian(float* a, float* b, float c, float d);
	// for polar division, cartesian is first converted to polar:
	// Polar() is less efficient than Cartesian()
	static void complexDivPolar(float* amplA, float* phaseA, float amplB, float phaseB);

	// applies gain to all buffer samples, such that the maximum sample level is equal to dBGoalLevel
	static void normalize(AudioSampleBuffer* buffer, float dBGoalLevel = 0.0f, bool printGain = false);
	
	// converts dB to a linear value (0.0 dB = 1.0 linear)
	static float dBToLin (float dB);
	static double dBToLin (double dB);

	// converts a linear value to dB (0.0 dB = 1.0 linear)
	static float linTodB (float lin);
	static double linTodB (double lin);
	
	// puts the contents of a file into a buffer
	static AudioSampleBuffer fileToBuffer (String fileName, int startSample = 0, int numSamples = 0);
	static AudioSampleBuffer fileToBuffer (File file, int startSample = 0, int numSamples = 0);

	// does what is says
	static bool isPowerOfTwo(int x);
	
	// returns the power of 2 that is >= x. User needs to only specify x.
	// even if the result argument is malicious, safeguards are in place
	static int nextPowerOfTwo(int x, int result = 1);
	
	// for rounding off very small values (exponents closer to 0 than than 1e-11) to positive 0.0
	static void roundToZero(float* x, float threshold);
	
	// round to 1e-16, positive or negative
	static void roundTo1TenQuadrillionth(float* x);

	// calculate ampl and phase for pair of samples := {re, im}
	static float binAmpl(float* binPtr);
	static float binPhase(float* binPtr);
	
	// generate pulse buffer: {1.0, 0.0, 0.0, ...}
	// with optional offset = delay on the pulse
	static AudioSampleBuffer generatePulse (int numSamples, int pulseOffset = 0);

	// performs linear fade, fade per sample being 1.0 / numSamples.
	// !fadeIn means fadeout
	static void linearFade (AudioSampleBuffer* buffer, bool fadeIn, int startSample, int numSamples);
	
	// For ARM convolution
	// IR -> FFT -> format {0, N/2, re(1), im(1), ..., im((N/2)-1)} -> export (close to) raw bytes
	static AudioSampleBuffer IRtoRealFFTRaw (AudioSampleBuffer& buffer, int fftBufferSize);
	
	/* fills every channel of the buffer with a sine wave with the specified freq and ampl */
	static void sineFill (AudioSampleBuffer *buffer, float freq, float sampleRate, float ampl = 1.0f);
	
	
	/*
	 Get clear iostream errors
	 https://stackoverflow.com/questions/1725714/why-ofstream-would-fail-to-open-the-file-in-c-reasons
	 */
	static std::string DescribeIosFailure(const std::ios& stream);
};

#endif /* FP_Tools_hpp */
