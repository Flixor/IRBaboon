//
//  FP_Tools.hpp
//
//  Created by Felix Postma on 27/03/2019.
//

#include <stdio.h>
#include <iostream>
#include <fstream>

#include <FP_general.h>

#include "FP_Tools.hpp"
#include "FP_Convolver.hpp"
#include "FP_CircularBufferArray.hpp"


void FP_Tools::sumToMono(AudioSampleBuffer* buffer){
	
	if (buffer->getNumChannels() != 2){
		DBG("FP_Tools::sumToMono() error: input is not stereo\n");
		return;
	}
	
	float* p0 = buffer->getWritePointer(0, 0);
	float* p1 = buffer->getWritePointer(1, 0);
	
	int n = buffer->getNumSamples();
	for (int i = 0; i < n; i++){
		p0[i] += p1[i];
		p0[i] /= 2.0f;		// -6 dB
		p1[i] = 0.0f;
	}
}


void FP_Tools::makeStereo(AudioSampleBuffer* buffer){
	if(buffer->getNumChannels() != 1) {
		DBG("FP_Tools::makeStereo() error: input is not mono\n");
		return;
	}
	
	buffer->setSize(2, buffer->getNumSamples(), true);
	buffer->copyFrom(1, 0, *buffer, 0, 0, buffer->getNumSamples());
}


void FP_Tools::complexMul(float* a, float* b, float c, float d){
	float re, im;
	
	re = (*a)*c - (*b)*d;
	im = (*b)*c + (*a)*d;
	
	*a = re;
	*b = im;
};


void FP_Tools::complexDivPolar(float* a, float* b, float c, float d){
	
	// how to avoid dividing by 0?
	
	float amplA = sqrt((*a) * (*a) + (*b) * (*b));
	float phaseA = atan2((*b), (*a));
	float amplB = sqrt(c * c + d * d);
	float phaseB = atan2(d, c);

	amplA /= amplB;
	phaseA -= phaseB;
		
	*a = amplA * cos(phaseA);
	*b = amplA * sin(phaseA);
}


void FP_Tools::complexDivCartesian(float* a, float* b, float c, float d){

	if (c == 0.0 && d == 0.0) {
		DBG("FP_Tools::complexDivCartesian: c and d are both 0.\n");
		return;
	}

	float re, im;
	
	re = ( (*a)*c + (*b)*d ) / (c*c + d*d);
	im = ( (*b)*c - (*a)*d ) / (c*c + d*d);
	
	*a = re;
	*b = im;
}


void FP_Tools::normalize(AudioSampleBuffer* buffer, float dBGoalLevel, bool printGain){
	float realGoal = FP_Tools::dBToLin(dBGoalLevel);
	float gain = realGoal / buffer->getMagnitude(0, buffer->getNumSamples());
	buffer->applyGain(gain);
	
	if(printGain) printf("FP_Tools::normalize gain: %g, dB: %9.6f\n", gain, linTodB(gain));
};


float FP_Tools::dBToLin (float dB){
	return pow(10.0f, dB / 20.0f);
};


double FP_Tools::dBToLin (double dB){
	return pow(10.0, dB / 20.0);
};


float FP_Tools::linTodB (float lin){
	if (lin == 0.0f)	return -333.0f;
	else				return 20.0f * log(lin) / log(10.0f);
};


double FP_Tools::linTodB (double lin){
	if (lin == 0.0)		return -333.0;
	else				return 20.0 * log(lin) / log(10.0);
};


AudioSampleBuffer FP_Tools::fileToBuffer (String fileName, int startSample, int numSamples){
	AudioSampleBuffer buffer (0,0);
	File file (fileName);
	
	if (!file.existsAsFile()){
		std::string errorStr = "FP_Tools::fileToBuffer() error: Specified file does not exist, is a directory or can't be accessed:\n";
		errorStr += fileName.toStdString().c_str();
		errorStr += "\n";
		DBG(errorStr);
		return buffer;
	}
	
	AudioFormatManager formatManager;
	formatManager.registerBasicFormats();
	std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor(file));
	
	if (reader == nullptr){
		DBG("FP_Tools::fileToBuffer() error: File specified is not a suitable audio file. \n");
		return buffer;
	}
	
	if (numSamples <= 0){
		numSamples = (int) reader->lengthInSamples;
	}
	
	buffer.setSize(reader->numChannels, numSamples);
	reader->read(&buffer, 0, numSamples, startSample, true, true);
	
	return buffer;
};


AudioSampleBuffer FP_Tools::fileToBuffer (File file, int startSample, int numSamples){
	AudioSampleBuffer buffer (0,0);
	
	if (!file.existsAsFile()){
		std::string errorStr = "FP_Tools::fileToBuffer() error: Specified file does not exist, is a directory or can't be accessed:\n";
		errorStr += file.getFileName().toStdString().c_str();
		errorStr += "\n";
		DBG(errorStr);
		return buffer;
	}
	
	AudioFormatManager formatManager;
	formatManager.registerBasicFormats();
	std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor(file));
	
	if (reader == nullptr){
		DBG("FP_Tools::fileToBuffer() error: File specified is not a suitable audio file. \n");
		return buffer;
	}
	
	if (numSamples <= 0){
		numSamples = (int) reader->lengthInSamples;
	}
	
	buffer.setSize(reader->numChannels, numSamples);
	reader->read(&buffer, 0, numSamples, startSample, true, true);
	
	return buffer;
};



bool FP_Tools::isPowerOfTwo(int x){
	return (x != 0) && ((x & (x - 1)) == 0);
}


int FP_Tools::nextPowerOfTwo(int x, int result){
	
	if (!isPowerOfTwo(result)) result = 1;
	
	if (isPowerOfTwo(x)) return x;
	else if (result > x) return result;
	else return nextPowerOfTwo(x, result * 2);
}


void FP_Tools::roundToZero(float* x, float threshold){
	
	if (threshold < 0.0f){
		DBG("FP_tools::roundToZero error: threshold should be positive\n");
		return;
	}
	
	// signbit() == true if value is negative
	if(!signbit(*x) && *x < threshold)		*x = 0.0f;
	if(signbit(*x) && *x > -(threshold))	*x = 0.0f;
}



void FP_Tools::roundTo1TenQuadrillionth(float* x){
	
	// signbit() == true if value is negative
	if(!signbit(*x) && *x < 1e-16)	*x = 1e-16;
	if(signbit(*x) && *x > -1e-16)	*x = -1e-16;
}



float FP_Tools::binAmpl(float* binPtr){
	return sqrt(pow(*(binPtr), 2.0) +
				pow(*(binPtr + 1), 2.0));
}


float FP_Tools::binPhase(float* binPtr){ // arctan(b/a)
	return atan2(*(binPtr + 1),
				 *(binPtr));
}



AudioSampleBuffer FP_Tools::generatePulse (int numSamples, int pulseOffset){
	AudioSampleBuffer pulse (1, numSamples);
	pulse.clear();
	if (pulseOffset >= numSamples || pulseOffset < 0) pulseOffset = 0;
	pulse.setSample(0, 0 + abs(pulseOffset), 1.0);
	return pulse;
}


void FP_Tools::linearFade (AudioSampleBuffer* buffer, bool fadeIn, int startSample, int numSamples){
	
	if (startSample + numSamples > buffer->getNumSamples()){
		DBG("FP_Tools::linearFade: startSample + numSamples exceeds buffer size.\n");
		return;
	}
	
	float fadePerSample = 1.0 / numSamples;
	float gain;
	
	for (int channel = 0; channel < buffer->getNumChannels(); channel++){
		for (int sample = startSample; sample < startSample + numSamples; sample++){
			
			if (fadeIn) gain = (sample - startSample) * fadePerSample;
			else gain = 1.0 - (sample - startSample) * fadePerSample;
			
			buffer->setSample(channel, sample,
							  buffer->getSample(channel, sample) * gain);
		}
	}
}



AudioSampleBuffer FP_Tools::IRtoRealFFTRaw (AudioSampleBuffer& buffer, int irPartSize){
	int bufcount = buffer.getNumSamples()/irPartSize + 1;
	int N = irPartSize * 2; // actually also needs -1 but no one cares

	FP_Convolver convolver;
	FP_CircularBufferArray array (bufcount, 1, N);
	FP_ParallelBufferPrinter printer;
	
	// divide buffer into buffers with size fftBufferSize, fft separately, replace im(0) with re(N/2), place into bufferarray
	int bufferSample = 0;
	
	while (bufferSample < buffer.getNumSamples()){
		array.getWriteBufferPtr()->setSample(0, bufferSample % irPartSize, buffer.getSample(0, bufferSample));
		bufferSample++;
		
		if ((bufferSample % irPartSize == 0) || (bufferSample == buffer.getNumSamples())){
			AudioSampleBuffer fft (convolver.fftTransform(*array.getWriteBufferPtr()));
			
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


void FP_Tools::sineFill(AudioSampleBuffer *buffer, float freq, float sampleRate, float ampl){
	
	if (ampl > 1.0 || ampl <= 0.0)
		ampl = 1.0;
	
	for (int j = 0; j < buffer->getNumChannels(); j++){
		for(int i = 0; i < buffer->getNumSamples(); i++){
			buffer->setSample(j, i, ampl * cos(M_PI*2 * freq * i / sampleRate));
		}
	}
}



std::string FP_Tools::DescribeIosFailure(const std::ios& stream){
	
	std::string result;
	
	if (stream.eof()) {
		result = "Unexpected end of file.";
	}
	
#ifdef WIN32
	// GetLastError() gives more details than errno.
	else if (GetLastError() != 0) {
		result = FormatSystemMessage(GetLastError());
	}
#endif
	
	else if (errno) {
#if defined(__unix__)
		// We use strerror_r because it's threadsafe.
		// GNU's strerror_r returns a string and may ignore buffer completely.
		char buffer[255];
		result = std::string(strerror_r(errno, buffer, sizeof(buffer)));
#else
		result = std::string(strerror(errno));
#endif
	}
	
	else {
		result = "Unknown file error.";
	}
	
	boost::trim_right(result);  // from Boost String Algorithms library
	return result;
}






