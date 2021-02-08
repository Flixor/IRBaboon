/*
 *  Copyright © 2021 Felix Postma. 
 */


#include <fp_include_all.hpp>


namespace fp {

namespace ir {

	AudioBuffer<float> invertFilter (AudioBuffer<float>& buffer, int sampleRate){
		
		AudioSampleBuffer pulse = tools::generatePulse(buffer.getNumSamples());
		AudioSampleBuffer result = convolution::deconvolve(&pulse, &buffer, sampleRate);
		return result;
	}


	AudioSampleBuffer IRchop (AudioSampleBuffer& buffer, int IRlength, float thresholdLeveldB, int consecutiveSamplesBelowThreshold){
		
		// put sampleCursor on the sample that contains maxAmpl
		// if finding that sample doesn't work for whatever reason: set sampleCursor to 0
		float maxAmpl = buffer.getMagnitude(0, buffer.getNumSamples());
		const float* bufPtr = buffer.getReadPointer(0);
		int sampleCursor = 0;
		while (abs(*(bufPtr + sampleCursor)) != maxAmpl && sampleCursor < buffer.getNumSamples()) sampleCursor++;
		if (sampleCursor == buffer.getNumSamples()) sampleCursor = 0;
		
		
		// determine chop start point
		float maxAmpldB = tools::linTodB(abs(maxAmpl));
		int chopStartSample = 0;
		int samplesBelowThreshold = 0;
		
		
		// find starting sample with foldback
		int samplesPerused = 0;
		while (samplesBelowThreshold < consecutiveSamplesBelowThreshold){
			if ( tools::linTodB(abs(buffer.getSample(0, sampleCursor))) - maxAmpldB < thresholdLeveldB )
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
		tools::linearFade(&IR, false, IRlength - 1 - fadeoutLength, fadeoutLength);
		
		// apply lin fadein
		int fadeinLength = samplesPerused / 4;
		tools::linearFade(&IR, true, 0, fadeinLength);

		return IR;
	}


	void shifteroo(AudioSampleBuffer* buffer){
		
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


	AudioSampleBuffer IRtoRealFFTRaw (AudioSampleBuffer& buffer, int irPartSize){
		int bufcount = buffer.getNumSamples()/irPartSize + 1;
		int N = irPartSize * 2; // actually also needs -1 but no one cares
		
		CircularBufferArray array (bufcount, 1, N);
		ParallelBufferPrinter printer;
		
		// divide buffer into buffers with size fftBufferSize, fft separately, replace im(0) with re(N/2), place into bufferarray
		int bufferSample = 0;
		
		while (bufferSample < buffer.getNumSamples()){
			array.getWriteBufferPtr()->setSample(0, bufferSample % irPartSize, buffer.getSample(0, bufferSample));
			bufferSample++;
			
			if ((bufferSample % irPartSize == 0) || (bufferSample == buffer.getNumSamples())){
				AudioSampleBuffer fft (tools::fftTransform(*array.getWriteBufferPtr()));
				
				// replace im(0) with re(N/2)
				fft.setSample(0, 1, fft.getSample(0, fft.getNumSamples()/2));
				
				// replace array buffer with fft version
				for (int j = 0; j < N; j++){
					array.getWriteBufferPtr()->setSample(0, j, fft.getSample(0, j));
				}
				array.incrWriteIndex();
			}
		}
		
		AudioSampleBuffer cons (array.consolidate());
		
		// output floats
		std::ofstream fout ("/Users/flixor/Desktop/IR.txt");
		for(int i = 0; i < cons.getNumSamples(); i++){
			if (i % 8 == 0) fout << '\n';
			if (i % N == 0) fout << '\n';
			fout << cons.getSample(0, i) << ", ";
		}
		fout.close();
		
		
		return cons;
	}

} // ir

} // fp
