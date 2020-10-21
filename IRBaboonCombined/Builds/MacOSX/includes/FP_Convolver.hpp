//
//  FP_Convolver.hpp
//  SineSweepGeneration - ConsoleApp
//
//  Created by Felix Postma on 22/03/2019.
//

#ifndef FP_Convolver_hpp
#define FP_Convolver_hpp

#include <stdio.h>

#include <FP_general.h>
#include <JuceHeader.h>
#include "FP_Tools_nstest.hpp"
#include "FP_CircularBufferArray.hpp"
#include "FP_ParallelBufferPrinter.hpp"



class FP_Convolver {
	
public:
	FP_Convolver();
	~FP_Convolver();
	
	/*
	 * In the convolve methods buffer1 is seen as "input audio" and buffer2 as the "IR" to convolve with
	 */
	
	/* convolve() uses complex multiplication */
	AudioBuffer<float> convolvePeriodic(AudioBuffer<float>& buffer1, AudioBuffer<float>& buffer2);
	AudioBuffer<float> convolveNonPeriodic(AudioBuffer<float>& buffer1, AudioBuffer<float>& buffer2);
	
	/* deconcolve uses complex division
	 * Assumes that denominator is mono! */
	AudioBuffer<float> deconvolveNonPeriodic2(AudioBuffer<float>& numeratorBuffer, AudioBuffer<float>& denominatorBuffer, double sampleRate, bool smoothing = true, bool nullifyPhase = false, bool nullifyAmplitude = false);

	/* the filter has a low-pass effect in the upper freqs, if the fft method
	 * performForwardRealFreqOnly has been used, because the upper bin range
	 * will eventually go into negative freq bin territory, where the contents
	 * of the bins are 0, but still count towards the average. */
	void averagingFilter (AudioSampleBuffer* buffer, double octaveFraction, double sampleRate, bool logAvg, bool nullifyPhase = false, bool nullifyAmplitude = false);


	/* these transforms use the RealOnly transforms in the Juce library.
	 * This means that all the negative freq bins have contents 0.
	 * fftSize = length of result, N = fftSize / 2
	 * Beware: RealOnly means N/2+1 bins have data in them! (last one is nyquist) */
	AudioBuffer<float> fftTransform (AudioBuffer<float>& buffer, bool formatAmplPhase = false);
	AudioBuffer<float> fftInvTransform (AudioBuffer<float>& buffer);
	
	
	/* inverts by deconv pulse with buffer */
	AudioBuffer<float> invertFilter4 (AudioBuffer<float>& buffer, int samplerate);

	
	
	/* algorithm:
	 	- find sample with max ampl
	 	- chop start is sample before max ampl sample that is consecutiveSamplesBelowThreshold
	 	- IRlength is length, and linear fadeout in final 1/8 or IR
	 * if nullifiedphase: needs shifted buffer as input */
	AudioSampleBuffer IRchop (AudioSampleBuffer& buffer, int IRlength, float thresholdLeveldB, int consecutiveSamplesBelowThreshold);
	
	
	/* puts the second half of the buffer in front of the first half
	 * if uneven: 2nd half has 1 sample less than 1st half */
	void shifteroo (AudioSampleBuffer* buffer);

	
	// For ARM convolution
	// IR -> FFT -> format {0, N/2, re(1), im(1), ..., im((N/2)-1)} -> export (close to) raw bytes
	AudioSampleBuffer IRtoRealFFTRaw (AudioSampleBuffer& buffer, int fftBufferSize);


private:
	
	// for convolvePeriodic()
	int processBlockSize = 256;
	
	enum ChannelLayout{
		unknown,
		IRMonoAudioMono,
		IRMonoAudioStereo,
		IRStereoAudioMono,
		IRStereoAudioStereo
	};
	
	// for averagingFilter() total (gaussian) smoothing width will be 3*this
	float smoothPerAvg = 1.0/13.0;

};

#endif /* FP_Convolver_hpp */
