//
//  Copyright © 2020 Felix Postma. 
//

#include <stdio.h>

#include <fp_include_all.hpp>
#include <JuceHeader.h>


namespace fp {
namespace tools {
	
	
	void sumToMono(AudioSampleBuffer* buffer){
		
		if (buffer->getNumChannels() != 2){
			DBG("sumToMono() error: input is not stereo\n");
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
	
	
	void makeStereo(AudioSampleBuffer* buffer){
		if(buffer->getNumChannels() != 1) {
			DBG("makeStereo() error: input is not mono\n");
			return;
		}
		
		buffer->setSize(2, buffer->getNumSamples(), true);
		buffer->copyFrom(1, 0, *buffer, 0, 0, buffer->getNumSamples());
	}
	
	
	void complexMul(float* a, float* b, float c, float d){
		float re, im;
		
		re = (*a)*c - (*b)*d;
		im = (*b)*c + (*a)*d;
		
		*a = re;
		*b = im;
	};
	
	
	void complexDivPolar(float* a, float* b, float c, float d){
		
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
	
	
	void complexDivCartesian(float* a, float* b, float c, float d){
		
		if (c == 0.0 && d == 0.0) {
			// no action necessary
			return;
		}
		
		float re, im;
		
		re = ( (*a)*c + (*b)*d ) / (c*c + d*d);
		im = ( (*b)*c - (*a)*d ) / (c*c + d*d);
		
		*a = re;
		*b = im;
	}
	
	
	float dBToLin (float dB){
		return pow(10.0f, dB / 20.0f);
	};
	
	
	double dBToLin (double dB){
		return pow(10.0, dB / 20.0);
	};
	
	
	float linTodB (float lin){
		if (lin == 0.0f)	return -333.0f;
		else				return 20.0f * log(lin) / log(10.0f);
	};
	
	
	double linTodB (double lin){
		if (lin == 0.0)		return -333.0;
		else				return 20.0 * log(lin) / log(10.0);
	};
	
	
	void normalize(AudioSampleBuffer* buffer, float dBGoalLevel, bool printGain){
		float realGoal = dBToLin(dBGoalLevel);
		float gain = realGoal / buffer->getMagnitude(0, buffer->getNumSamples());
		buffer->applyGain(gain);
		
		if(printGain) printf("normalize gain: %g, dB: %9.6f\n", gain, linTodB(gain));
	};
	
	
	AudioSampleBuffer fileToBuffer (String fileName, int startSample, int numSamples){
		AudioSampleBuffer buffer (0,0);
		File file (fileName);
		
		if (!file.existsAsFile()){
			std::string errorStr = "fileToBuffer() error: Specified file does not exist, is a directory or can't be accessed:\n";
			errorStr += fileName.toStdString().c_str();
			errorStr += "\n";
			DBG(errorStr);
			return buffer;
		}
		
		AudioFormatManager formatManager;
		formatManager.registerBasicFormats();
		std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor(file));
		
		if (reader == nullptr){
			DBG("fileToBuffer() error: File specified is not a suitable audio file. \n");
			return buffer;
		}
		
		if (numSamples <= 0){
			numSamples = (int) reader->lengthInSamples;
		}
		
		buffer.setSize(reader->numChannels, numSamples);
		reader->read(&buffer, 0, numSamples, startSample, true, true);
		
		return buffer;
	};
	
	
	AudioSampleBuffer fileToBuffer (File file, int startSample, int numSamples){
		AudioSampleBuffer buffer (0,0);
		
		if (!file.existsAsFile()){
			std::string errorStr = "fileToBuffer() error: Specified file does not exist, is a directory or can't be accessed:\n";
			errorStr += file.getFileName().toStdString().c_str();
			errorStr += "\n";
			DBG(errorStr);
			return buffer;
		}
		
		AudioFormatManager formatManager;
		formatManager.registerBasicFormats();
		std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor(file));
		
		if (reader == nullptr){
			DBG("fileToBuffer() error: File specified is not a suitable audio file. \n");
			return buffer;
		}
		
		if (numSamples <= 0){
			numSamples = (int) reader->lengthInSamples;
		}
		
		buffer.setSize(reader->numChannels, numSamples);
		reader->read(&buffer, 0, numSamples, startSample, true, true);
		
		return buffer;
	};
	
	
	
	bool isPowerOfTwo(int x){
		return (x != 0) && ((x & (x - 1)) == 0);
	}
	
	
	int nextPowerOfTwo(int x, int result){
		
		if (!isPowerOfTwo(result)) result = 1;
		
		if (isPowerOfTwo(x)) return x;
		else if (result > x) return result;
		else return nextPowerOfTwo(x, result * 2);
	}
	
	
	void roundToZero(float* x, float threshold){
		
		if (threshold < 0.0f){
			DBG("tools::roundToZero error: threshold should be positive\n");
			return;
		}
		
		// signbit() == true if value is negative
		if(!signbit(*x) && *x < threshold)		*x = 0.0f;
		if(signbit(*x) && *x > -(threshold))	*x = 0.0f;
	}
	
	
	
	void roundTo1TenQuadrillionth(float* x){
		
		// signbit() == true if value is negative
		if(!signbit(*x) && *x < 1e-16)	*x = 1e-16;
		if(signbit(*x) && *x > -1e-16)	*x = -1e-16;
	}
	
	
	
	float binAmpl(float* binPtr){
		return sqrt(pow(*(binPtr), 2.0) +
					pow(*(binPtr + 1), 2.0));
	}
	
	
	float binPhase(float* binPtr){ // arctan(b/a)
		return atan2(*(binPtr + 1),
					 *(binPtr));
	}
	
	
	
	AudioSampleBuffer generatePulse (int numSamples, int pulseOffset){
		AudioSampleBuffer pulse (1, numSamples);
		pulse.clear();
		if (pulseOffset >= numSamples || pulseOffset < 0) pulseOffset = 0;
		pulse.setSample(0, 0 + abs(pulseOffset), 1.0);
		return pulse;
	}
	
	
	void linearFade (AudioSampleBuffer* buffer, bool fadeIn, int startSample, int numSamples){
		
		if (startSample + numSamples > buffer->getNumSamples()){
			DBG("linearFade: startSample + numSamples exceeds buffer size.\n");
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
	
	
	
	
	
	void sineFill(AudioSampleBuffer *buffer, float freq, float sampleRate, float ampl){
		
		if (ampl > 1.0 || ampl <= 0.0)
			ampl = 1.0;
		
		for (int j = 0; j < buffer->getNumChannels(); j++){
			for(int i = 0; i < buffer->getNumSamples(); i++){
				buffer->setSample(j, i, ampl * cos(M_PI*2 * freq * i / sampleRate));
			}
		}
	}
	
	
	
	std::string DescribeIosFailure(const std::ios& stream){
		
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
		
		boost::algorithm::trim_right(result);  // from Boost String Algorithms library
		//		boost::trim_right(result);  // from Boost String Algorithms library
		return result;
	}
	
} // tools
} // fp


