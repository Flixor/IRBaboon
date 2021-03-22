/*
 *  Copyright © 2021 Felix Postma. 
 */


#include <fp_include_all.hpp>


namespace fp {

namespace convolution {


/******************************
 * Convolver class
 *******************************/

Convolver::Convolver(ProcessBlockSize size) :
	Thread("Convolver loading"),
	processBlockSize (static_cast<int>(size)) {

	startThread();
}

Convolver::~Convolver(){

	stopThread(5000); 
}


bool Convolver::prepare(int samplesPerBlock){

	hostBlockSize = samplesPerBlock;

	/* 
	 * constructor
	 */

	/* Officially: N >= (processBlockSize * 2 - 1) */
	N = processBlockSize * 2;
	fftBlockSize = N * 2;
	
	// initialise numChannels and numSamples for input fft array buffers
	inputFftBufArray.clearAndResize(0, generalInputAudioChannels, fftBlockSize);
	
	// initialise FFTs
	BigInteger fftBitMask = (BigInteger) N;
	fftIR.reset 	 (new dsp::FFT(fftBitMask.getHighestBit()));
	fftForward.reset (new dsp::FFT(fftBitMask.getHighestBit()));
	fftInverse.reset (new dsp::FFT(fftBitMask.getHighestBit()));
	
	// initialise auxiliary buffers
	inplaceBuf.setSize(generalInputAudioChannels, fftBlockSize);
	inplaceBuf.clear();
	
	overlapBuf.setSize(generalInputAudioChannels, processBlockSize);
	overlapBuf.clear();



	/* 
	 * prepareToPlay
	 */

	/* init circ buf arrays for convolution */
	int inputArraySize = (int) std::ceil((float) hostBlockSize / (float) processBlockSize);
	/* outputArraySize needs to be rounded up to the closest power of 2 */
	int outputArraySize = tools::nextPowerOfTwo(std::ceil((float) processBlockSize / (float) hostBlockSize) + 1);

	/* apply array sizes */
	inputBufArray.		 clearAndResize(inputArraySize, generalInputAudioChannels, fftBlockSize);
	convResultBufArray.	 clearAndResize(inputArraySize, generalInputAudioChannels, fftBlockSize);
	outputBufArray.		 clearAndResize(outputArraySize, generalInputAudioChannels, hostBlockSize);
	bypassOutputBufArray.clearAndResize(outputArraySize, generalInputAudioChannels, hostBlockSize);
	
	if (inputBufArray.getArraySize() > inputFftBufArray.getArraySize()){
		inputFftBufArray.clearAndResize(inputBufArray.getArraySize(), generalInputAudioChannels, fftBlockSize);
	}
	
	/* size is always at least 2, and we always want it to start 1 above the first one for proper latency behaviour */
	outputBufArray.setReadIndex(1);
	
	inputBufferSampleIndex = 0;
	outputBufferSampleIndex = 0;
	savedIndex = inputFftBufArray.getArraySize() - 1;
	
	/* 100 = IRPulse length */
	irPartitions = (int) std::ceil((float) 100 / (float) processBlockSize);
	irFftBufArray.clearAndResize(irPartitions, 1, fftBlockSize);
	
	/* tricky tricks */
	int newAudioArraySize = std::max(irPartitions, inputBufArray.getArraySize());
	inputFftBufArray.changeArraySize(newAudioArraySize);
	inputFftBufArray.setReadIndex(inputFftBufArray.getWriteIndex());
	inputFftBufArray.decrReadIndex();
	savedIndex = inputFftBufArray.getReadIndex();


	outputBuf.setSize(generalInputAudioChannels, hostBlockSize);
	
	return true;
}



void Convolver::setIR(AudioBuffer<float>& IR){

	IRtoConvolve = IR;
}



bool Convolver::inputSamples(AudioBuffer<float>& input){

	if (input.getNumSamples() == 0)
		return false;

	int currentHostBlockSize = hostBlockSize;

	/*
	 * Copy input to input buffers, and input buffers to audio fft array
	 */

	for (int sample = 0; sample < currentHostBlockSize; sample++){
		for (int channel = 0; channel < generalInputAudioChannels; channel++){
			inputBufArray.getWriteBufferPtr()->setSample(channel, inputBufferSampleIndex, input.getSample(channel, sample));
		}
		inputBufferSampleIndex++;
		
		/* when input buffer completed: copy to fft buffer array, perform fft, incr to next input buffers */
		if (inputBufferSampleIndex >= processBlockSize){
			
			inputBufArray.setReadIndex(inputBufArray.getWriteIndex());
			inputFftBufArray.getWriteBufferPtr()->makeCopyOf(*inputBufArray.getReadBufferPtr());
			
			
			for (int channel = 0; channel < generalInputAudioChannels; channel++){
				fftForward->performRealOnlyForwardTransform(inputFftBufArray.getWriteBufferPtr()->getWritePointer(channel, 0), true);
			}
			inputFftBufArray.incrWriteIndex();
			
			inputBufArray.incrWriteIndex();
			inputBufArray.getWriteBufferPtr()->clear();
			
			inputBufferSampleIndex = 0;
			blocksToProcess++;
		}
	}

	
	/*
	 * Process blocks
	 */

	while (blocksToProcess > 0){
		
		/* always read an IR block */
		irFftBufArray.getWriteBufferPtr()->clear();

		irFftBufArray.getWriteBufferPtr()->copyFrom(0, 0, IRtoConvolve, 0, irFftBufArray.getWriteIndex() * processBlockSize, processBlockSize);
		
		fftIR->performRealOnlyForwardTransform(irFftBufArray.getWriteBufferPtr()->getWritePointer(0, 0), true);
		
		irFftBufArray.incrWriteIndex();
		
		
		
		/* DSP loop */
		convResultBufArray.getWriteBufferPtr()->clear();
		
		inputFftBufArray.setReadIndex(savedIndex);
		inputFftBufArray.incrReadIndex();
		savedIndex = inputFftBufArray.getReadIndex();
		
		for (int channel = 0; channel < generalInputAudioChannels; channel++) {
			
			float* overlapBufferPtr = overlapBuf.getWritePointer(channel, 0);
			float* convResultPtr = convResultBufArray.getWriteBufferPtr()->getWritePointer(channel, 0);
			
			int irFftReadIndex = 0; // IR fft index should always start at 0 and not fold back, so its readIndex is not used
			inputFftBufArray.setReadIndex(savedIndex); // for iteration >1 of the channel
			
			while (irFftReadIndex < irFftBufArray.getArraySize()){
				
				inplaceBuf.makeCopyOf(*inputFftBufArray.getReadBufferPtr());
				float* inplaceBufferPtr = inplaceBuf.getWritePointer(channel, 0);

				const float* irFftPtr = irFftBufArray.getBufferPtrAtIndex(irFftReadIndex)->getReadPointer(0, 0);
				
				/* complex multiplication of 1 audio fft block and 1 IR fft block */
				for (int i = 0; i <= N; i += 2){
					tools::complexMul(inplaceBufferPtr + i,
										 inplaceBufferPtr + i + 1,
										 irFftPtr[i],
										 irFftPtr[i + 1]);
				}
				
				/* sum multiplications in convolution result buffer */
				for (int i = 0; i < fftBlockSize; i++){
					convResultPtr[i] += inplaceBufferPtr[i];
				}
				irFftReadIndex++;
				inputFftBufArray.decrReadIndex();
			}
			
			/* FDL method -> IFFT after summing */
			fftInverse->performRealOnlyInverseTransform(convResultPtr);
			
			/* add existing overlap and save new overlap */
			for (int i = 0; i < processBlockSize; i++){
				convResultPtr[i] += overlapBufferPtr[i];
				overlapBufferPtr[i] = convResultPtr[processBlockSize + i];
			}
			
		} // end DSP loop
		
		convResultBufArray.incrWriteIndex();
		blocksToOutputBuffer++;
		blocksToProcess--;
		
	} // end while() Process blocks

	return true;
}



AudioBuffer<float> Convolver::getOutput(){

	/*
	 * Write completed convolution result blocks to output buffers
	 */

	while (blocksToOutputBuffer > 0){
		
		for (int sample = 0; sample < processBlockSize; sample++){
			if (outputBufferSampleIndex == 0)
				outputBufArray.getWriteBufferPtr()->clear();
			for (int channel = 0; channel < generalInputAudioChannels; channel++) {
			
				outputBufArray.getWriteBufferPtr()->setSample(channel, outputBufferSampleIndex,
																 convResultBufArray.getReadBufferPtr()->getSample(channel, sample));
			}
			outputBufferSampleIndex++;
			if (outputBufferSampleIndex >= hostBlockSize){
				outputBufArray.incrWriteIndex();
				outputBufferSampleIndex = 0;
			}
		}
		convResultBufArray.incrReadIndex();
		blocksToOutputBuffer--;
	}


	/*
	 * Output buffers to real output
	 */

	for (int sample = 0; sample < hostBlockSize; sample++){
		
		for (int channel = 0; channel < generalInputAudioChannels; channel++)
			outputBuf.setSample(channel, sample, outputBufArray.getReadBufferPtr()->getSample(channel, outputSampleIndex));
		
		outputSampleIndex++;
		if(outputSampleIndex >= hostBlockSize){
			outputBufArray.incrReadIndex();
			outputSampleIndex = 0;
		}
	}

	return outputBuf;
}


void Convolver::releaseResources(){

	inputBufArray.changeArraySize(0);
	inputFftBufArray.changeArraySize(0);
	irFftBufArray.changeArraySize(0);
	convResultBufArray.changeArraySize(0);
	outputBufArray.changeArraySize(0);
}


void Convolver::run(){

}

int Convolver::getProcessBlockSize(){
	return processBlockSize;
}




/******************************
 * Standalone functions
 *******************************/


AudioBuffer<float> convolvePeriodic(AudioSampleBuffer& buffer1, AudioSampleBuffer& buffer2, int processBlockSize){
	
	/*
	 * In here, buffer1 is seen as "input audio" and buffer2 as the "IR" to convolve with
	 */
	
	AudioSampleBuffer outputBuffer (buffer1.getNumChannels(), buffer1.getNumSamples() + buffer2.getNumSamples() - 1);
	outputBuffer.clear();
	
	
	
	int numChannelsAudio = buffer1.getNumChannels();
	int numChannelsIR = buffer2.getNumChannels();
	
	ChannelLayout chLayout = ChannelLayout::unknown;
	
	if (numChannelsIR == 1 && numChannelsAudio == 1)
		chLayout = ChannelLayout::IRMonoAudioMono;
	else if (numChannelsIR == 1 && numChannelsAudio == 2)
		chLayout = ChannelLayout::IRMonoAudioStereo;
	else if (numChannelsIR == 2 && numChannelsAudio == 1)
		chLayout = ChannelLayout::IRStereoAudioMono;
	else if (numChannelsIR == 2 && numChannelsAudio == 2)
		chLayout = ChannelLayout::IRStereoAudioStereo;
	
	if (chLayout == ChannelLayout::unknown){
		DBG("Either buffer1 or buffer2 is not mono nor stereo. Abort abort \n");
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
		case ChannelLayout::IRMonoAudioMono:		irFftFwdChMax = 1; break;
		case ChannelLayout::IRMonoAudioStereo:		irFftFwdChMax = 1; break;
		case ChannelLayout::IRStereoAudioStereo:	irFftFwdChMax = 2; break;
		case ChannelLayout::IRStereoAudioMono:		irFftFwdChMax = 1; break;
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
			if (chLayout == ChannelLayout::IRStereoAudioMono)
				tools::sumToMono(&irFftBufferArray[processIncrementor]);
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
				if (chLayout == ChannelLayout::IRMonoAudioMono || chLayout == ChannelLayout::IRStereoAudioStereo)
					irCh = channel;
				else // if IRStereoAudioMono: ir has been summed into channel 0
					irCh = 0;
				const float* irFftPtr = irFftBufferArray[irBlockIncrementor].getReadPointer(irCh, 0);
				
				// complex multiply 1 audio fft block and 1 IR fft block
				for (int i = 0; i <= N; i += 2){
					tools::complexMul(inplaceBufferPtr + i,
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



AudioBuffer<float> convolveNonPeriodic(AudioSampleBuffer& buffer1, AudioSampleBuffer& buffer2){
	/*
	 * buffer1 is seen as "input audio" and buffer2 as the "IR" to convolve with
	 */
	
	int convResultSamples = buffer1.getNumSamples() + buffer2.getNumSamples() - 1;
	
	AudioSampleBuffer inputBuffer (buffer1);
	AudioSampleBuffer irBuffer (buffer2);

	
	int numChannelsAudio = inputBuffer.getNumChannels();
	int numChannelsIR = irBuffer.getNumChannels();
	
	ChannelLayout chLayout = ChannelLayout::unknown;
	
	if (numChannelsIR == 1 && numChannelsAudio == 1)
		chLayout = ChannelLayout::IRMonoAudioMono;
	else if (numChannelsIR == 1 && numChannelsAudio == 2)
		chLayout = ChannelLayout::IRMonoAudioStereo;
	else if (numChannelsIR == 2 && numChannelsAudio == 1)
		chLayout = ChannelLayout::IRStereoAudioMono;
	else if (numChannelsIR == 2 && numChannelsAudio == 2)
		chLayout = ChannelLayout::IRStereoAudioStereo;
	
	if (chLayout == ChannelLayout::unknown){
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
		case ChannelLayout::IRMonoAudioMono: 		irFftFwdChMax = 1; break;
		case ChannelLayout::IRMonoAudioStereo:		irFftFwdChMax = 1; break;
		case ChannelLayout::IRStereoAudioStereo:	irFftFwdChMax = 2; break;
		case ChannelLayout::IRStereoAudioMono:		tools::sumToMono(&irBuffer);
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
		if (chLayout == ChannelLayout::IRMonoAudioMono || chLayout == ChannelLayout::IRStereoAudioStereo)
			irCh = channel;
		else 	// if IRStereoAudioMono: ir has been summed into channel 0
			irCh = 0;
		const float* irFftPtr = irBuffer.getReadPointer(irCh, 0);
		
		
		for (int i = 0; i <= N; i += 2){
			tools::complexMul(audioFftPtr + i,
							   audioFftPtr + i + 1,
							   irFftPtr[i],
							   irFftPtr[i + 1]);
			
			/* unnecessary to "normalize" to JUCE FFT standard again (max ampl in freq domain = N) by dividing by N!
			 * JUCE does this automatically */
		}
		
		fftInverse.performRealOnlyInverseTransform(audioFftPtr);
	}
	
	
	AudioSampleBuffer outputBuffer (inputBuffer.getNumChannels(), convResultSamples);
	
	for (int channel = 0; channel < outputBuffer.getNumChannels(); channel++){
		outputBuffer.copyFrom(channel, 0, inputBuffer.getReadPointer(channel), outputBuffer.getNumSamples());
	}
	
	return outputBuffer;
}



AudioBuffer<float> deconvolve(AudioBuffer<float>* numeratorBuffer, AudioBuffer<float>* denominatorBuffer, double sampleRate, bool smoothing, bool includePhase, bool includeAmplitude){
	
	AudioSampleBuffer numBuf (*numeratorBuffer);
	numBuf.setSize(1, numBuf.getNumSamples(), true);
	AudioSampleBuffer denomBuf (*denominatorBuffer);
	denomBuf.setSize(1, denomBuf.getNumSamples(), true);

	/* match buffer lengths */
	if (numBuf.getNumSamples() > denomBuf.getNumSamples())
		denomBuf.setSize(denomBuf.getNumChannels(), numBuf.getNumSamples(), true);
	else if (numBuf.getNumSamples() < denomBuf.getNumSamples())
		numBuf.setSize(numBuf.getNumChannels(), denomBuf.getNumSamples(), true);

	/* FFT */
	AudioSampleBuffer numBufFft (tools::fftTransform(numBuf));
	AudioSampleBuffer denomBufFft (tools::fftTransform(denomBuf));


	/* DSP loop */
	int N = numBufFft.getNumSamples() / 2;
	float* numBufFftPtr = numBufFft.getWritePointer(0, 0);
	const float* denomBufFftPtr = denomBufFft.getReadPointer(0, 0);
	for (int i = 0; i <= N; i += 2){
		tools::complexDivCartesian(numBufFftPtr + i,
									  numBufFftPtr + i + 1,
									  *(denomBufFftPtr + i),
									  *(denomBufFftPtr + i + 1));
		
		/* unnecessary to "normalize" to JUCE FFT standard again (max ampl in freq domain = N) by multiplying with N!
		 * JUCE does this automatically */
	}

	
	/* averagingFilter() applies a averaging based on moving, octave-based, rectangular window.
	 * Rectangular window functions suck for FFT of course, but when you convolve a rectangular window with itself,
	 * you get a triangular window, and if you convolve that with the rectangular window, you get a Gaussian window!
	 * Which is perfectly fine for FFT. And this way, a lot easier to implement than directly programming a Gaussian window!
	 */
	if (smoothing){
		float smoothPerAvg = 1.0/13.0;
		for (int i = 0; i < 3; i++) {
			averagingFilter(&numBufFft, smoothPerAvg, sampleRate, true, includePhase, includeAmplitude);
		}
	}

	/* IFFT */
	numBuf = tools::fftInvTransform(numBufFft);
	
	/* If phase is ignored, the peak will be in the middle of the buffer, so it needs to be shifted to the start */
	if (not includePhase) ir::shifteroo(&numBuf);
	
	return numBuf;
}


void averagingFilter (AudioSampleBuffer* buffer, double octaveFraction, double sampleRate, bool logAvg, bool includePhase, bool includeAmplitude){
	
	
	int fftSize = buffer->getNumSamples();
	int numChannels = buffer->getNumChannels();
	
	if (!tools::isPowerOfTwo(fftSize)) {
		DBG("applyBucket() error: input buffer size is not power of 2.\n");
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
			*(oldAmplBufPtr + bin/2) = tools::binAmpl(bufPtr + bin);
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
//			float linPerOctHPF = tools::dBToLin(dBPerOctHPF);
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
					tools::roundTo1TenQuadrillionth(&subtractAmpl);
					subtractAmpl = log(subtractAmpl);
					runningSum -= subtractAmpl;
					subtractBin += 2;
				}

				// add bins to running sum
				int addBin = prevUpperBin + 2;
				while(addBin <= upperBin){
					if (addBin < fftSize){
						float addAmpl = *(oldAmplBufPtr + addBin/2);
						tools::roundTo1TenQuadrillionth(&addAmpl);
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
			tools::roundTo1TenQuadrillionth(&newAmpl);
			*(newAmplBufPtr + bin) = newAmpl;
			
			prevLowerBin = lowerBin;
			prevUpperBin = upperBin;
			
		}
		
		
		// put new averaged ampl in output buffer
		float* bufPtr = buffer->getWritePointer(channel);
		
		for (int bin = 0; bin <= N; bin += 2){
			float ampl = *(newAmplBufPtr + bin);
			tools::roundToZero(bufPtr + bin, 1e-11);	// rounding only seems necessary for determining phase per bin
			tools::roundToZero(bufPtr + bin + 1, 1e-11);
			float phase = tools::binPhase(bufPtr + bin);
			if (not includeAmplitude) ampl = 1.0;
			if (not includePhase) phase = 0.0;
			float re = ampl * cos(phase);
			float im = ampl * sin(phase);
			*(bufPtr + bin) = re;
			*(bufPtr + bin + 1) = im;
		}

	}
}



#ifdef JUCE_UNIT_TESTS

void FpConvolutionTest::runTest() {

	beginTest("Empty test");
	size_t success = 1;
	expect(success);

}

#endif

	
} // convolution
} // fp


	
	
