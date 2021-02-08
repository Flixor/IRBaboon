//
//  fpconvolver_graveyard.cpp
//  IRBaboon
//
//  Created by Felix Postma on 21/10/2020.
//  Copyright © 2020 Flixor. All rights reserved.
//

#include "fpconvolver_graveyard.hpp"




/*
 ===================================================================================================================================================================
 ===================================================== GRAVEYARD =====================================================
 ===================================================================================================================================================================
 */


void FP_Convolver::applyBucket(AudioSampleBuffer* buffer, double lowCutoffFreq, double lowSlopeEndFreq, double highSlopeBeginFreq, double highCutoffFreq, double maxAmpl, double minAmpl, double sampleRate){
	
	if( !(lowCutoffFreq <= lowSlopeEndFreq &&
		  lowSlopeEndFreq <= highSlopeBeginFreq &&
		  highSlopeBeginFreq <= highCutoffFreq) ){
		DBG("FP_Convolver::applyBucket() error: specified slope frequencies should all be <= the next one.");
		return;
	}
	
	if( !(maxAmpl > minAmpl) ){
		DBG("FP_Convolver::applyBucket() error: maxAmpl should be bigger than minAmpl.");
		return;
	}
	
	int fftSize = buffer->getNumSamples();
	
	if (!FP_Tools::isPowerOfTwo(fftSize)) {
		DBG("FP_Convolver::applyBucket() error: input buffer size is not power of 2.");
		return;
	}
	
	int N = fftSize / 2;
	AudioBuffer<double> bucket (1, fftSize); // only real freq bins, incl nyquist freq bin
	bucket.clear();
	
	double amplDiff = maxAmpl - minAmpl;
	
	double nyquist = sampleRate / 2;
	double freqPerBin = nyquist / (double) (N/2); // N/2 because juce fft style is {re, im, re im, ...}
	
	double lowSlopeWidthOct = log(lowSlopeEndFreq / lowCutoffFreq) / log(2.0);
	double highSlopeWidthOct = log(highCutoffFreq / highSlopeBeginFreq) / log(2.0);
	
	int lowCutoffBin = 2 * (int) round(lowCutoffFreq / freqPerBin); // 2 * because juce fft style is {re, im, re im, ...}
	int lowSlopeEndBin = 2 * (int) round(lowSlopeEndFreq / freqPerBin);
	int highSlopeBeginBin = 2 * (int) round(highSlopeBeginFreq / freqPerBin);
	int highCutoffBin = 2 * (int) round(highCutoffFreq / freqPerBin);
	
	
	// lower max ampl (flat) part
	for(int bin = 0; bin < lowCutoffBin; bin += 2){
		bucket.setSample(0, bin, FP_Tools::dBToLin(maxAmpl));
	}
	
	// low slope
	for(int bin = lowCutoffBin; bin <= lowSlopeEndBin; bin += 2){
		double binFreq = (bin/2) * freqPerBin;
		double octFract = log(binFreq / lowCutoffFreq) / log(2.0);
		double bindB = maxAmpl - (octFract / lowSlopeWidthOct * amplDiff);
		bucket.setSample(0, bin, FP_Tools::dBToLin(bindB));
	}
	
	// min ampl (flat) part
	for(int bin = lowSlopeEndBin+2; bin < highCutoffBin; bin += 2){
		bucket.setSample(0, bin, FP_Tools::dBToLin(minAmpl));
	}
	
	// high slope
	for(int bin = highSlopeBeginBin; bin <= highCutoffBin; bin += 2){
		double binFreq = (bin/2) * freqPerBin;
		double octFract = log(binFreq / highSlopeBeginFreq) / log(2.0);
		double bindB = minAmpl + (octFract / highSlopeWidthOct * amplDiff);
		bucket.setSample(0, bin, FP_Tools::dBToLin(bindB));
	}
	
	// upper max ampl (flat) part
	for(int bin = highCutoffBin+2; bin <= N; bin += 2){
		bucket.setSample(0, bin, FP_Tools::dBToLin(maxAmpl));
	}
	
	
	// add bucket to input buffer
	for (int channel = 0; channel < buffer->getNumChannels(); channel++){
		float* bufPtr = buffer->getWritePointer(channel);
		
		for (int bin = 0; bin <= N; bin += 2){
			float phase = atan2(*(bufPtr + bin),
								*(bufPtr + bin + 1));
			*(bufPtr + bin) 	+= N * bucket.getSample(0, bin) * cos(phase);
			*(bufPtr + bin + 1) += N * bucket.getSample(0, bin) * sin(phase);
		}
	}
	
}




void FP_Convolver::invertFilter (AudioBuffer<float>* buffer){
	int N = 1;
	while (N < buffer->getNumSamples()) {
		N *= 2;
	}
	int fftBlockSize = N * 2;
	
	AudioSampleBuffer fftBuffer (*buffer);
	fftBuffer.setSize(buffer->getNumChannels(), fftBlockSize, true);
	
	BigInteger fftBitMask = (BigInteger) N;
	
	dsp::FFT::FFT fftForward    (fftBitMask.getHighestBit());
	dsp::FFT::FFT fftInverse    (fftBitMask.getHighestBit());
	
	for (int channel = 0; channel < fftBuffer.getNumChannels(); channel++){
		float* fftBufferPtr = fftBuffer.getWritePointer(channel);
		
		fftForward.performRealOnlyForwardTransform(fftBufferPtr, true);
		
		float x, a, b, c, x2, a2, b2, c2;
		for (int i = 0; i <= N; i += 2){
			x = *(fftBufferPtr + i);
			if(x > 0.0) {
				a = x / N;
				b = 1.0 / a;
				c = b * N;
				*(fftBufferPtr + i) = c;
			}
			
			x2 = *(fftBufferPtr + i + 1);
			if (x2 > 0.0){
				a2 = x2 / N;
				b2 = 1.0 / a2;
				c2 = b2 * N;
				*(fftBufferPtr + i + 1) = c2;
			}
		}
		
		fftInverse.performRealOnlyInverseTransform(fftBufferPtr);
		
		buffer->copyFrom(channel, 0, fftBufferPtr, buffer->getNumSamples());
	}
	
}



AudioBuffer<float> FP_Convolver::invertFilter2 (AudioBuffer<float>& buffer){
	
	AudioSampleBuffer pulse (buffer.getNumChannels(), 1);
	for(int ch = 0; ch < pulse.getNumChannels(); ch++){
		pulse.setSample(ch, 0, 1.0);
	}
	
	AudioSampleBuffer result (deconvolveNonPeriodic2(pulse, buffer, 48000.0));
	
	return result;
}




void FP_Convolver::invertFilter3 (AudioBuffer<float>* buffer){
	
	float mirrorLevel = 1.0;
	
	int N = buffer->getNumSamples() / 2;
	
	float* bufPtr = buffer->getWritePointer(0, 0);
	
	for (int bin = 0; bin <= N; bin += 2){
		float ampl = FP_Tools::binAmpl(bufPtr + bin);
		float difference = mirrorLevel / ampl;
		float mirrorGain = difference * difference;
		*(bufPtr + bin) *= mirrorGain;
		*(bufPtr + bin + 1) *= mirrorGain;
	}
}



AudioBuffer<float> FP_Convolver::deconvolveNonPeriodic(AudioBuffer<float>& numeratorBuffer, AudioBuffer<float>& denominatorBuffer, double sampleRate, bool useBucket, bool useAveraging){
	
	
	//	FP_ParallelBufferPrinter printer;
	
	
	int convResultSamples = numeratorBuffer.getNumSamples() + denominatorBuffer.getNumSamples() - 1;
	
	AudioSampleBuffer numBuf (numeratorBuffer);
	AudioSampleBuffer denomBuf (denominatorBuffer);
	
	int numChannelsAudio = numBuf.getNumChannels();
	int numChannelsIR = denomBuf.getNumChannels();
	
	int N = 1;
	while (N < convResultSamples) {
		N *= 2;
	}
	int fftBlockSize = N * 2;
	
	BigInteger fftBitMask = (BigInteger) N;
	
	dsp::FFT::FFT fftIR         (fftBitMask.getHighestBit());
	dsp::FFT::FFT fftForward    (fftBitMask.getHighestBit());
	dsp::FFT::FFT fftInverse    (fftBitMask.getHighestBit());
	
	numBuf.setSize(numChannelsAudio, fftBlockSize, true);
	denomBuf.setSize(numChannelsIR, fftBlockSize, true);
	
	fftIR.performRealOnlyForwardTransform(denomBuf.getWritePointer(0, 0), true);
	
	
	if (useBucket) applyBucket(&denomBuf, 20.0, 40.0, 2500.0, 6300.0, -60.0, -180.0, sampleRate);
	
	// DSP loop
	for (int channel = 0; channel < numChannelsAudio; channel++){
		
		float* numBufFftPtr = numBuf.getWritePointer(channel, 0);
		
		fftForward.performRealOnlyForwardTransform(numBufFftPtr, true);
		
		const float* denomBufFftPtr = denomBuf.getReadPointer(0, 0);
		
		for (int i = 0; i <= N; i += 2){
			FP_Tools::complexDivCartesian(numBufFftPtr + i,
										  numBufFftPtr + i + 1,
										  *(denomBufFftPtr + i),
										  *(denomBufFftPtr + i + 1));
			
			// "normalize" to JUCE FFT standard again (max ampl in freq domain = N)
			*(numBufFftPtr + i) *= N;
			*(numBufFftPtr + i + 1) *= N;
		}
	}
	
	//	printer.appendBuffer("logavg 0x 0,11oct", numBuf);
	
	if (useAveraging) averagingFilter(&numBuf, 1.0/9.0, sampleRate, true);
	if (useAveraging) averagingFilter(&numBuf, 1.0/9.0, sampleRate, true);
	if (useAveraging) averagingFilter(&numBuf, 1.0/9.0, sampleRate, true);
	//	printer.appendBuffer("logavg 3x 0,11oct", numBuf);
	
	//	printer.printFreqToCsv(sampleRate);
	
	for (int channel = 0; channel < numChannelsAudio; channel++){
		float* numBufFftPtr = numBuf.getWritePointer(channel, 0);
		fftInverse.performRealOnlyInverseTransform(numBufFftPtr);
	}
	
	AudioSampleBuffer outputBuffer (numBuf.getNumChannels(), convResultSamples/2);
	
	for (int channel = 0; channel < outputBuffer.getNumChannels(); channel++){
		outputBuffer.copyFrom(channel, 0, numBuf.getReadPointer(channel), outputBuffer.getNumSamples());
	}
	
	return outputBuffer;
}

