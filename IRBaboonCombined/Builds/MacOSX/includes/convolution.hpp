//
//  Copyright © 2020 Felix Postma. 
//


#pragma once


#include <fp_include_all.hpp>
#include <JuceHeader.h>


namespace fp {

	enum ChannelLayout{
		unknown,
		IRMonoAudioMono,
		IRMonoAudioStereo,
		IRStereoAudioMono,
		IRStereoAudioStereo
	};
	
namespace convolution {
	
	
	/* convolve() uses complex multiplication
	 * buffer1 is seen as "input audio" and buffer2 as the "IR" to convolve with */
	AudioBuffer<float> convolvePeriodic(AudioBuffer<float>& buffer1, AudioBuffer<float>& buffer2, int processBlockSize = 256);
	AudioBuffer<float> convolveNonPeriodic(AudioBuffer<float>& buffer1, AudioBuffer<float>& buffer2);
	
	/* Deconvolution is non-periodic, meaning it won't fold back, but output a buffer that is at least N_numerator + N_denominator + 1 samples.
	 * The deconvolution uses complex division.
	 * The denominator buffer needs to be mono! */
	AudioBuffer<float> deconvolve(AudioBuffer<float>* numeratorBuffer, AudioBuffer<float>* denominatorBuffer, double sampleRate, bool smoothing = true, bool includePhase = true, bool includeAmplitude = true);

	/* averagingFilter has a low-pass effect in the upper freqs if the fft method performForwardRealFreqOnly() has been used,
	 * because the upper bin range will eventually go into negative freq bin territory, where the contents of the bins are 0,
	 * but these bins still count towards the average. */
	void averagingFilter (AudioBuffer<float>* buffer, double octaveFraction, double sampleRate, bool logAvg, bool nullifyPhase = false, bool nullifyAmplitude = false);

} // convolution
	
} // fp
