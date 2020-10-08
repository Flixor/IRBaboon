//
//  FP_Convolver.cpp
//  SineSweepGeneration - ConsoleApp
//
//  Created by Felix Postma on 22/03/2019.
//

#include "../includes/FP_Convolver.hpp"



FP_Convolver::FP_Convolver(){
	// ?
}

FP_Convolver::~FP_Convolver(){
	// ?
}

// ================================================================
AudioBuffer<float> FP_Convolver::convolvePeriodic(AudioSampleBuffer& buffer1, AudioSampleBuffer& buffer2){
	
	/*
	 * In here, buffer1 is seen as "input audio" and buffer2 as the "IR" to convolve with
	 */
	
	AudioSampleBuffer outputBuffer (buffer1.getNumChannels(), buffer1.getNumSamples() + buffer2.getNumSamples() - 1);
	outputBuffer.clear();
	
	
	
	int numChannelsAudio = buffer1.getNumChannels();
	int numChannelsIR = buffer2.getNumChannels();
	
	ChannelLayout chLayout = unknown;
	
	if (numChannelsIR == 1 && numChannelsAudio == 1)
		chLayout = IRMonoAudioMono;
	else if (numChannelsIR == 1 && numChannelsAudio == 2)
		chLayout = IRMonoAudioStereo;
	else if (numChannelsIR == 2 && numChannelsAudio == 1)
		chLayout = IRStereoAudioMono;
	else if (numChannelsIR == 2 && numChannelsAudio == 2)
		chLayout = IRStereoAudioStereo;
	
	if (chLayout == unknown){
		printf("Either buffer1 or buffer2 is not mono nor stereo. Abort abort \n");
		return outputBuffer;
	}
	
	
	int N = 1;
	while (N < (2 * processBlockSize - 1)) {
		N *= 2;
	}
	int fftBlockSize = N * 2;
	
	
	int irPartitions = (int) std::ceil( (float) buffer2.getNumSamples() / (float) processBlockSize);
	
	// allocate memory for buffers
	std::vector<AudioSampleBuffer> irFftBufferArray;
	int irFftBufferArraySize = 0;
	for (int i = 0; i < irPartitions; i++){
		irFftBufferArray.push_back({numChannelsIR, fftBlockSize});
		irFftBufferArray[i].clear();
	}
	
	// audio input array will have as many partitions as the IR --> circular buffer
	std::vector<AudioSampleBuffer> audioFftBufferArray;
	int audioFftBufferArrayIndex = 0;
	for (int i = 0; i < irPartitions; i++){
		audioFftBufferArray.push_back({numChannelsAudio, fftBlockSize});
		audioFftBufferArray[i].clear();
	}
	
	//  bitmask is handy to determine the exact power of 2 N is (juce fft object wants this for constructor)
	BigInteger fftBitMask = (BigInteger) N;
	
	// initialise juce FFT object.
	// Juce recommends cacheing an FFT object per size and direction
	dsp::FFT::FFT fftIR         (fftBitMask.getHighestBit());
	dsp::FFT::FFT fftForward    (fftBitMask.getHighestBit());
	dsp::FFT::FFT fftInverse    (fftBitMask.getHighestBit());
	
	// auxiliary buffers init
	AudioSampleBuffer convResultBuffer (numChannelsAudio, fftBlockSize);
	convResultBuffer.clear();
	
	AudioSampleBuffer inplaceBuffer (numChannelsAudio, fftBlockSize);
	inplaceBuffer.clear();
	
	AudioSampleBuffer overlapBuffer (numChannelsAudio, processBlockSize); // actually audioBlockSize - 1
	overlapBuffer.clear();
	
	
	
	int processIncrementor = 0;
	int endAudioIncrementor = 0;
	
	int irFftFwdChMax = 0;
	switch (chLayout) {
		case IRMonoAudioMono: 		irFftFwdChMax = 1; break;
		case IRMonoAudioStereo:		irFftFwdChMax = 1; break;
		case IRStereoAudioStereo:	irFftFwdChMax = 2; break;
		case IRStereoAudioMono:		irFftFwdChMax = 1; break;
		default: break;
	}
	
	
	do {
		// read and fft IR block if not all of IR has been read
		if (processIncrementor < irPartitions){

			for (int channel = 0; channel < numChannelsIR; channel++){
				irFftBufferArray[processIncrementor].copyFrom(channel,		// destChannel,
															  0, 		// destStartSample,
															  buffer2,	// AudioBuffer< float > & 	source,
															  channel, 		// sourceChannel,
															  processIncrementor * processBlockSize,		// sourceStartSample,
															  (processIncrementor + 1) * processBlockSize <= buffer2.getNumSamples() ? // numSamples
															  		processBlockSize :
															  		buffer2.getNumSamples() - processIncrementor * processBlockSize
															  // copyFrom() doesn't 'see' the end of the buffer; with this conditional copyFrom() doesn't go out of bounds of the buffer
															  )	;
			}
			if (chLayout == IRStereoAudioMono)
				FP_Tools::sumToMono(&irFftBufferArray[processIncrementor]);
			for (int channel = 0; channel < irFftFwdChMax; channel++)
				fftIR.performRealOnlyForwardTransform(irFftBufferArray[processIncrementor].getWritePointer(channel, 0), true);
			irFftBufferArraySize++;
		}
		
		// read and fft audio block if not all audio has been read
		if (processIncrementor * processBlockSize <= buffer1.getNumSamples()){
			audioFftBufferArrayIndex = processIncrementor % irPartitions;       // fold back
			audioFftBufferArray[audioFftBufferArrayIndex].clear();
	
			for (int channel = 0; channel < numChannelsAudio; channel++){
				audioFftBufferArray[audioFftBufferArrayIndex].copyFrom(channel,		// destChannel,
																	   0, 		// destStartSample,
																	   buffer1,	// AudioBuffer< float > & 	source,
																	   channel, 		// sourceChannel,
																	   processIncrementor * processBlockSize,		// sourceStartSample,
																	   (processIncrementor + 1) * processBlockSize <= buffer1.getNumSamples() ? // numSamples
																	   		processBlockSize :
																	   		buffer1.getNumSamples() - processIncrementor * processBlockSize
																	   // copyFrom() doesn't 'see' the end of the buffer; with this conditional copyFrom() doesn't go out of bounds of the buffer
																	   )	;
				
				fftForward.performRealOnlyForwardTransform(audioFftBufferArray[audioFftBufferArrayIndex].getWritePointer(channel, 0), true);
			}
			
			if ((processIncrementor + 1) * processBlockSize > buffer1.getNumSamples() )
				endAudioIncrementor++;
		}
		// for the tail when audio input has ended
		else {
			endAudioIncrementor++;
		}
		
		
		
		// DSP loop per audio channel
		convResultBuffer.clear();
		
		for (int channel = 0; channel < numChannelsAudio; channel++) {
			
			// prep
			float* convResultPtr = convResultBuffer.getWritePointer(channel, 0);
			float* overlapBufferPtr = overlapBuffer.getWritePointer(channel, 0);
			
			int irBlockIncrementor = std::max(0, endAudioIncrementor - 1);
			int audioBlockDecrementor = audioFftBufferArrayIndex;
			
			
			// DSP loop: compl mult, summing and ifft
			while (irBlockIncrementor < irFftBufferArraySize) {
				
				inplaceBuffer.makeCopyOf(audioFftBufferArray[audioBlockDecrementor]);
				float* inplaceBufferPtr = inplaceBuffer.getWritePointer(channel, 0);
				
				int irCh = 0;
				if (chLayout == IRMonoAudioMono || chLayout == IRStereoAudioStereo)
					irCh = channel;
				else // if IRStereoAudioMono: ir has been summed into channel 0
					irCh = 0;
				const float* irFftPtr = irFftBufferArray[irBlockIncrementor].getReadPointer(irCh, 0);
				
				// complex multiply 1 audio fft block and 1 IR fft block
				for (int i = 0; i <= N; i += 2){
					FP_Tools::complexMul(inplaceBufferPtr + i,
							   inplaceBufferPtr + i + 1,
							   irFftPtr[i],
							   irFftPtr[i + 1]);
				}
				
				
				// add to total conv result buffer
				for (int i = 0; i < fftBlockSize; i++){
					convResultPtr[i] += inplaceBufferPtr[i];
				}
				
				
				irBlockIncrementor++;
				audioBlockDecrementor--;
				if (audioBlockDecrementor < 0)
					audioBlockDecrementor = irPartitions - 1;   // fold back
			}
			
			
			// FDL method --> summing and then IFFT
			fftInverse.performRealOnlyInverseTransform(convResultPtr);
			
			
			// add existing overlap and save new overlap
			for (int i = 0; i < overlapBuffer.getNumSamples(); i++){
				convResultPtr[i] += overlapBufferPtr[i];
				overlapBufferPtr[i] = convResultPtr[processBlockSize + i];
			}
			
		}
		
		// write result to output
		
		for (int channel = 0; channel < numChannelsAudio; channel++){
			outputBuffer.copyFrom(channel,		// destChannel,
								  processIncrementor * processBlockSize, 		// destStartSample,
								  convResultBuffer,	// AudioBuffer< float > & 	source,
								  channel, 		// sourceChannel,
								  0,		// sourceStartSample,
								  (processIncrementor + 1) * processBlockSize <= outputBuffer.getNumSamples() ? // numSamples
								  		processBlockSize :
								  		outputBuffer.getNumSamples() - processIncrementor * processBlockSize
								  // copyFrom() doesn't 'see' the end of the buffer; with this conditional copyFrom() doesn't go out of bounds of the buffer
								  )	;
		}
		processIncrementor++;
	}
	while(endAudioIncrementor < irPartitions); // end DSP loop
	
	
	/*
	 * It turns it it's not necessary to output the remainder of the overlapBuffer
	 */
	
	
	return outputBuffer;
}





AudioBuffer<float> FP_Convolver::convolveNonPeriodic(AudioSampleBuffer& buffer1, AudioSampleBuffer& buffer2){
	/*
	 * buffer1 is seen as "input audio" and buffer2 as the "IR" to convolve with
	 */
	
	int convResultSamples = buffer1.getNumSamples() + buffer2.getNumSamples() - 1;
	
	AudioSampleBuffer inputBuffer (buffer1);
	AudioSampleBuffer irBuffer (buffer2);

	
	int numChannelsAudio = inputBuffer.getNumChannels();
	int numChannelsIR = irBuffer.getNumChannels();
	
	ChannelLayout chLayout = unknown;
	
	if (numChannelsIR == 1 && numChannelsAudio == 1)
		chLayout = IRMonoAudioMono;
	else if (numChannelsIR == 1 && numChannelsAudio == 2)
		chLayout = IRMonoAudioStereo;
	else if (numChannelsIR == 2 && numChannelsAudio == 1)
		chLayout = IRStereoAudioMono;
	else if (numChannelsIR == 2 && numChannelsAudio == 2)
		chLayout = IRStereoAudioStereo;
	
	if (chLayout == unknown){
		printf("Either buffer1 or buffer2 is not mono nor stereo. Abort abort \n");
		inputBuffer.clear();
		return inputBuffer;
	}
	
	
	int N = 1;
	while (N < convResultSamples) {
		N *= 2;
	}
	int fftBlockSize = N * 2;
	
	BigInteger fftBitMask = (BigInteger) N;

	dsp::FFT::FFT fftIR         (fftBitMask.getHighestBit());
	dsp::FFT::FFT fftForward    (fftBitMask.getHighestBit());
	dsp::FFT::FFT fftInverse    (fftBitMask.getHighestBit());
	
	
	inputBuffer.setSize(numChannelsAudio, fftBlockSize, true);
	
	irBuffer.setSize(numChannelsIR, fftBlockSize, true);

	
	int irFftFwdChMax = 0;
	switch (chLayout) {
		case IRMonoAudioMono: 		irFftFwdChMax = 1; break;
		case IRMonoAudioStereo:		irFftFwdChMax = 1; break;
		case IRStereoAudioStereo:	irFftFwdChMax = 2; break;
		case IRStereoAudioMono:		FP_Tools::sumToMono(&irBuffer);
			irFftFwdChMax = 1; break;
		default: break;
	}
	
	// FFT
	for (int channel = 0; channel < irFftFwdChMax; channel++)
		fftIR.performRealOnlyForwardTransform(irBuffer.getWritePointer(channel, 0), true);
	
	// DSP loop
	for (int channel = 0; channel < numChannelsAudio; channel++){
		
		float* audioFftPtr = inputBuffer.getWritePointer(channel, 0);
		
		fftForward.performRealOnlyForwardTransform(audioFftPtr, true);
		
		
		int irCh = 0;
		if (chLayout == IRMonoAudioMono || chLayout == IRStereoAudioStereo)
			irCh = channel;
		else 	// if IRStereoAudioMono: ir has been summed into channel 0
			irCh = 0;
		const float* irFftPtr = irBuffer.getReadPointer(irCh, 0);
		
		
		for (int i = 0; i <= N; i += 2){
			FP_Tools::complexMul(audioFftPtr + i,
							   audioFftPtr + i + 1,
							   irFftPtr[i],
							   irFftPtr[i + 1]);
			
//			*(audioFftPtr + i) /= N;
//			*(audioFftPtr + i + 1) /= N;
		}
		
		fftInverse.performRealOnlyInverseTransform(audioFftPtr);
	}
	
	
	AudioSampleBuffer outputBuffer (inputBuffer.getNumChannels(), convResultSamples);
	
	for (int channel = 0; channel < outputBuffer.getNumChannels(); channel++){
		outputBuffer.copyFrom(channel, 0, inputBuffer.getReadPointer(channel), outputBuffer.getNumSamples());
	}
	
	return outputBuffer;
}






AudioBuffer<float> FP_Convolver::deconvolveNonPeriodic2(AudioBuffer<float>& numeratorBuffer, AudioBuffer<float>& denominatorBuffer, double sampleRate, bool smoothing, bool nullifyPhase, bool nullifyAmplitude){
	
	AudioSampleBuffer numBuf (numeratorBuffer);
	numBuf.setSize(1, numBuf.getNumSamples(), true);
	AudioSampleBuffer denomBuf (denominatorBuffer);
	denomBuf.setSize(1, denomBuf.getNumSamples(), true);

	// match buffer lengths
	if (numBuf.getNumSamples() > denomBuf.getNumSamples())
		denomBuf.setSize(denomBuf.getNumChannels(), numBuf.getNumSamples(), true);
	else if (numBuf.getNumSamples() < denomBuf.getNumSamples())
		numBuf.setSize(numBuf.getNumChannels(), denomBuf.getNumSamples(), true);

	// FFT
	AudioSampleBuffer numBufFft (fftTransform(numBuf));
	AudioSampleBuffer denomBufFft (fftTransform(denomBuf));


	// DSP loop
	int N = numBufFft.getNumSamples() / 2;
	float* numBufFftPtr = numBufFft.getWritePointer(0, 0);
	const float* denomBufFftPtr = denomBufFft.getReadPointer(0, 0);
	for (int i = 0; i <= N; i += 2){
		FP_Tools::complexDivCartesian(numBufFftPtr + i,
									  numBufFftPtr + i + 1,
									  *(denomBufFftPtr + i),
									  *(denomBufFftPtr + i + 1));
		
		// unnecessary to "normalize" to JUCE FFT standard again (max ampl in freq domain = N) by multiplying with N
		// JUCE does this automatically
	}


	
	// in total 3*smoothPerAvg gaussian smoothing
	if (smoothing){
		averagingFilter(&numBufFft, smoothPerAvg, sampleRate, true, nullifyPhase, nullifyAmplitude);
		averagingFilter(&numBufFft, smoothPerAvg, sampleRate, true, nullifyPhase, nullifyAmplitude);
		averagingFilter(&numBufFft, smoothPerAvg, sampleRate, true, nullifyPhase, nullifyAmplitude);
	}

	
	// IFFT
	numBuf = fftInvTransform(numBufFft);
	
	
	return numBuf;
}


													   
													   

void FP_Convolver::averagingFilter (AudioSampleBuffer* buffer, double octaveFraction, double sampleRate, bool logAvg, bool nullifyPhase, bool nullifyAmplitude){
	
	
	int fftSize = buffer->getNumSamples();
	int numChannels = buffer->getNumChannels();
	
	if (!FP_Tools::isPowerOfTwo(fftSize)) {
		DBG("FP_Convolver::applyBucket() error: input buffer size is not power of 2.\n");
		return;
	}
	
	AudioBuffer<float> oldAmplBuf (numChannels, fftSize/2); // will only contain ampls for real freqs
	AudioBuffer<float> newAmplBuf (numChannels, fftSize);
	oldAmplBuf.clear();
	newAmplBuf.clear();
	
	int N = fftSize / 2;
	double fractPerSide = octaveFraction / 2.0;
	double nyquist = sampleRate / 2;
	double freqPerBin = nyquist / (double) (N/2); // N/2 because juce fft style is {re, im, re im, ...}
	
	
	// put 'old' amplitudes of input buffer into a buffer
	for (int channel = 0; channel < numChannels; channel++){
		float* bufPtr = buffer->getWritePointer(channel);
		float* oldAmplBufPtr = oldAmplBuf.getWritePointer(channel);
		
		for (int bin = 0; bin <= N; bin += 2){
			*(oldAmplBufPtr + bin/2) = FP_Tools::binAmpl(bufPtr + bin);
		}
	}
	
	
	for (int channel = 0; channel < numChannels; channel++){
		
		float runningSum = 0.0;
		int prevLowerBin = 0;
		int prevUpperBin = -2;
		
		float* oldAmplBufPtr = oldAmplBuf.getWritePointer(channel);
		float* newAmplBufPtr = newAmplBuf.getWritePointer(channel);
		
		// actual averaging
		for (int bin = 0; bin <= N; bin += 2){
			
			double binFreq = (bin/2) * freqPerBin;
			
			double lowerFreq = binFreq / pow(2.0, fractPerSide);
			int lowerBin = 2 * (int) round(lowerFreq / freqPerBin);
			double upperFreq = binFreq * pow(2.0, fractPerSide);
			int upperBin = 2 * (int) round(upperFreq / freqPerBin);
			
			double binRangeLength = (double) (upperBin - lowerBin) / 2.0 + 1.0;

			
			// HPF for <20Hz freq
			// we don't want this for meetsysteem
//			float dBPerOctHPF = 300.0;	// in actuality it turns out to be less than this... so algorithm doesn't technically work that well lol, but w/e
//			float linPerOctHPF = FP_Tools::dBToLin(dBPerOctHPF);
//			float lnLinPerOctHPF = log(linPerOctHPF); // necessary for runningsum log/exp method
//			float cutoffFreqHPF = 20.0;
//			float octavesDiff = 0.0;
//			float HPFadjustment = 0.0;
//			if (binFreq < cutoffFreqHPF){
//				if (binFreq < cutoffFreqHPF / 8) // 3 octaves down
//					octavesDiff = 3;
//				else {
//					octavesDiff = log(cutoffFreqHPF / binFreq) / log(2.0);
//				}
//				HPFadjustment = -(lnLinPerOctHPF * octavesDiff);
//			}

			
			// true running sum; only works for logAvg, not for linear avg, because lin values
			// lead to rounding errors when doing mass addition / subtraction
			// for logAvg running sum is still used though for efficiency.
			if (logAvg) {
				
				// remove bins from running sum
				int subtractBin = prevLowerBin;
				while(subtractBin < lowerBin){
					float subtractAmpl = *(oldAmplBufPtr + subtractBin/2);
					FP_Tools::roundTo1TenQuadrillionth(&subtractAmpl);
					subtractAmpl = log(subtractAmpl);
					runningSum -= subtractAmpl;
					subtractBin += 2;
				}

				// add bins to running sum
				int addBin = prevUpperBin + 2;
				while(addBin <= upperBin){
					if (addBin < fftSize){
						float addAmpl = *(oldAmplBufPtr + addBin/2);
						FP_Tools::roundTo1TenQuadrillionth(&addAmpl);
						addAmpl = log(addAmpl);
						runningSum += addAmpl;
					}
					addBin += 2;
				}
			}
			
			// if !logAvg, not true runningSum, because reasons mentioned above.
			else {
				runningSum = 0.0;
				for (int i = lowerBin; i <= upperBin; i+=2){
					float addAmpl = *(oldAmplBufPtr + i/2);
					runningSum += addAmpl;
				}
			}
			
			// avg and into new ampl buf
//			float newAmpl = (runningSum + HPFadjustment) / binRangeLength;
			float newAmpl = runningSum / binRangeLength;
			if (logAvg) newAmpl = exp(newAmpl);
			FP_Tools::roundTo1TenQuadrillionth(&newAmpl);
			*(newAmplBufPtr + bin) = newAmpl;
			
			prevLowerBin = lowerBin;
			prevUpperBin = upperBin;
			
		}
		
		
		// put new averaged ampl in output buffer
		float* bufPtr = buffer->getWritePointer(channel);
		
		for (int bin = 0; bin <= N; bin += 2){
			float ampl = *(newAmplBufPtr + bin);
			FP_Tools::roundToZero(bufPtr + bin, 1e-11);	// rounding only seems necessary for determining phase per bin
			FP_Tools::roundToZero(bufPtr + bin + 1, 1e-11);
			float phase = FP_Tools::binPhase(bufPtr + bin);
			if (nullifyAmplitude) ampl = 1.0;
			if (nullifyPhase) phase = 0.0;
			float re = ampl * cos(phase);
			float im = ampl * sin(phase);
			*(bufPtr + bin) = re;
			*(bufPtr + bin + 1) = im;
		}

	}
}





AudioBuffer<float> FP_Convolver::fftTransform(AudioBuffer<float> &buffer, bool formatAmplPhase) {
	
	int N = FP_Tools::nextPowerOfTwo(buffer.getNumSamples());
	int fftBlockSize = N * 2;
	
	AudioSampleBuffer fftBuffer (buffer.getNumChannels(), fftBlockSize);
	fftBuffer.clear();
	fftBuffer.copyFrom(0, 0, buffer.getReadPointer(0), buffer.getNumSamples());
	
	BigInteger fftBitMask = (BigInteger) N;
	dsp::FFT::FFT fftForward    (fftBitMask.getHighestBit());
	
	for (int channel = 0; channel < fftBuffer.getNumChannels(); channel++){
		float* fftBufferPtr = fftBuffer.getWritePointer(channel);
		fftForward.performRealOnlyForwardTransform(fftBufferPtr, true);
		
		if(formatAmplPhase){
			for (int bin = 0; bin <= N; bin += 2){
				*(fftBufferPtr + bin) = FP_Tools::binAmpl(fftBufferPtr + bin);
				*(fftBufferPtr + bin + 1) = FP_Tools::binPhase(fftBufferPtr + bin);
			}
		}
	}
	
	return fftBuffer;
}




AudioBuffer<float> FP_Convolver::fftInvTransform(AudioBuffer<float> &buffer) {

	int fftSize = buffer.getNumSamples();
	int N  = fftSize / 2;
	
	AudioSampleBuffer fftBuffer (buffer);
	
	BigInteger fftBitMask = (BigInteger) N;
	dsp::FFT::FFT fftInverse   (fftBitMask.getHighestBit());
	
	for (int channel = 0; channel < fftBuffer.getNumChannels(); channel++){
		float* fftBufferPtr = fftBuffer.getWritePointer(channel);
		fftInverse.performRealOnlyInverseTransform(fftBufferPtr);
	}
	
	fftBuffer.setSize(fftBuffer.getNumChannels(), N, true); // performRealOnlyInverseTransform() returns the result in the first half of the buffer
	
	return fftBuffer;
}




AudioBuffer<float> FP_Convolver::invertFilter4 (AudioBuffer<float>& buffer, int sampleRate){
	
	AudioSampleBuffer pulse = FP_Tools::generatePulse(buffer.getNumSamples());
	AudioSampleBuffer result = deconvolveNonPeriodic2(pulse, buffer, sampleRate);
	return result;
}




AudioSampleBuffer FP_Convolver::IRchop (AudioSampleBuffer& buffer, int IRlength, float thresholdLeveldB, int consecutiveSamplesBelowThreshold){
	
	// put sampleCursor on the sample that contains maxAmpl
	// if finding that sample doesn't work for whatever reason: set sampleCursor to 0
	float maxAmpl = buffer.getMagnitude(0, buffer.getNumSamples());
	const float* bufPtr = buffer.getReadPointer(0);
	int sampleCursor = 0;
	while (abs(*(bufPtr + sampleCursor)) != maxAmpl && sampleCursor < buffer.getNumSamples()) sampleCursor++;
	if (sampleCursor == buffer.getNumSamples()) sampleCursor = 0;
	
	
	// determine chop start point
	float maxAmpldB = FP_Tools::linTodB(abs(maxAmpl));
	int chopStartSample = 0;
	int samplesBelowThreshold = 0;
	
	
	// find starting sample with foldback
	int samplesPerused = 0;
	while (samplesBelowThreshold < consecutiveSamplesBelowThreshold){
		if ( FP_Tools::linTodB(abs(buffer.getSample(0, sampleCursor))) - maxAmpldB < thresholdLeveldB )
			samplesBelowThreshold++;
		else
			samplesBelowThreshold = 0;
		
		sampleCursor--;
		if (sampleCursor < 0)
			sampleCursor = buffer.getNumSamples() - 1;
		
		samplesPerused++;
		if (samplesPerused == IRlength)
			break;
	}
	chopStartSample = sampleCursor;
	

	// copy IRlength from buffer
	int copyLimit = IRlength;
	if (IRlength + chopStartSample > buffer.getNumSamples() - 1){
		copyLimit = buffer.getNumSamples() - chopStartSample;
	}
	
	AudioSampleBuffer IR (1, IRlength);
	IR.clear();
	IR.copyFrom(0, 0, buffer, 0, chopStartSample, copyLimit);
	
	// if chopStartSample happens to be near end: continue copying from sample 0 of buffer, to grab the fold-around
	if (copyLimit < IRlength){
		IR.copyFrom(0, copyLimit, buffer, 0, 0, IRlength - copyLimit);
	}
	
	
	// apply lin fadeout
	int fadeoutLength = IRlength / 4;
	FP_Tools::linearFade(&IR, false, IRlength - 1 - fadeoutLength, fadeoutLength);
	
	// apply lin fadein
	int fadeinLength = samplesPerused / 4;
	FP_Tools::linearFade(&IR, true, 0, fadeinLength);

	return IR;
}




void FP_Convolver::shifteroo(AudioSampleBuffer* buffer){
	
	if (buffer->getNumSamples() < 2)
		return;
	
	int size2ndHalf = buffer->getNumSamples() / 2;
	int size1stHalf = size2ndHalf;
	if (buffer->getNumSamples() % 2 != 0)
		size1stHalf += 1;
	
	AudioSampleBuffer newBuf (buffer->getNumChannels(), buffer->getNumSamples());
	
	for (int channel = 0; channel < buffer->getNumChannels(); channel++){
		newBuf.copyFrom(channel, 0, *buffer, channel, size1stHalf, size2ndHalf);
		newBuf.copyFrom(channel, size2ndHalf, *buffer, channel, 0, size1stHalf);
	}
	
	*buffer = newBuf;
}


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

	
	