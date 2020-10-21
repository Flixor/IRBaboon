//
//  fpconvolver_graveyard.hpp
//  IRBaboon
//
//  Created by Felix Postma on 21/10/2020.
//  Copyright © 2020 Flixor. All rights reserved.
//

#ifndef fpconvolver_graveyard_hpp
#define fpconvolver_graveyard_hpp

#include <stdio.h>



/*
 ===================================================
 ================= GRAVEYARD =================
 ===================================================
 */


// manual inversion; doesn't work right now
void invertFilter (AudioBuffer<float>* buffer);

// new method does it work?? --> nope, socalled kirkeby inversion
AudioBuffer<float> invertFilter2 (AudioBuffer<float>& buffer);

// concept: mirror
// result has no LPF and HPF!!
// only use as denombuf in deconv!!
// input needs to be freq domain
void invertFilter3 (AudioBuffer<float>* buffer);


// apply bucket-shaped compensation filter, log-log scale
// JUCE fft buffer format: fftsize is 2 * N (the fft order)
// only real (positive)  freq bins + nyquist bin
// assuming nyquist of input buffer is 24kHz
// assuming input buffer has a power-of-two size, because it has been fft'd, so fftSize = numSamples and N = fftSize / 2
void applyBucket (AudioSampleBuffer* buffer, double lowCutoffFreq, double lowSlopeEndFreq, double highSlopeBeginFreq, double highCutoffFreq, double maxAmpl, double minAmpl, double sampleRate);

AudioBuffer<float> deconvolveNonPeriodic(AudioBuffer<float>& numeratorBuffer, AudioBuffer<float>& denominatorBuffer, double sampleRate, bool useBucket = false, bool useAveraging = false);



#endif /* fpconvolver_graveyard_hpp */
