/*
 *  Felix Postma 2021
 */

#pragma once

#include <fp_include_all.hpp>
#include <JuceHeader.h>


namespace fp {


/* The convolution namespace contains the Convolver class which can be used for uniform partitioned convolution,
 * and also the same in the form of standalone functions.
 */	

enum class ChannelLayout {
	unknown,
	IRMonoAudioMono,
	IRMonoAudioStereo,
	IRStereoAudioMono,
	IRStereoAudioStereo
};

namespace convolution {
	

class Convolver : public Thread {

#ifdef JUCE_UNIT_TESTS
    friend class FpConvolutionTest;
#endif

public:

	enum class ProcessBlockSize { 
		Size64 = 64, 
		Size128 = 128, 
		Size256 = 256, 
		Size512 = 512 
	};

	Convolver(ProcessBlockSize size);
	~Convolver();

	bool prepare(int samplesPerBlock);
	void setIR(AudioBuffer<float>& IR);

	bool inputSamples(AudioBuffer<float>& input);
	AudioBuffer<float> getOutput();

	void releaseResources();

	void run() override;



	int getProcessBlockSize();

private:
	int generalInputAudioChannels = 2;
	int hostBlockSize;

	const int processBlockSize;
	
	int N, fftBlockSize;

	int irPartitions;

	CircularBufferArray inputBufArray;
	CircularBufferArray inputFftBufArray;
	CircularBufferArray irFftBufArray;
	
	std::unique_ptr<juce::dsp::FFT> fftIR, fftForward, fftInverse;
	AudioBuffer<float> inplaceBuf;
	AudioBuffer<float> overlapBuf;
	
	CircularBufferArray convResultBufArray;
	CircularBufferArray outputBufArray;
	CircularBufferArray bypassOutputBufArray;

	AudioBuffer<float> outputBuf;
	
	int inputBufferSampleIndex = 0;
	int outputBufferSampleIndex = 0;
	int outputSampleIndex = 0;
	int blocksToProcess = 0;
	int blocksToOutputBuffer = 0;
	int savedIndex = 0;

	AudioBuffer<float> IRtoConvolve;
};



	
/* convolve() uses complex multiplication
 * buffer1 is seen as "input audio" and buffer2 as the "IR" to convolve with */
AudioBuffer<float> convolvePeriodic(AudioBuffer<float>& buffer1, AudioBuffer<float>& buffer2, int processBlockSize = 256);
AudioBuffer<float> convolveNonPeriodic(AudioBuffer<float>& buffer1, AudioBuffer<float>& buffer2);

/* Deconvolution is non-periodic, meaning it won't fold back, 
 * but outputs a buffer that is at least N_numerator + N_denominator + 1 samples.
 * The deconvolution uses complex division.
 * The denominator buffer needs to be mono! */
AudioBuffer<float> deconvolve(AudioBuffer<float>* numeratorBuffer, AudioBuffer<float>* denominatorBuffer, double sampleRate, bool smoothing = true, bool includePhase = true, bool includeAmplitude = true);

/* averagingFilter has a low-pass effect in the upper freqs if the fft method performForwardRealFreqOnly() has been used,
 * because the upper bin range will eventually go into negative freq bin territory, where the contents of the bins are 0,
 * but these bins still count towards the average. 
 * negative logAvg = linear average instead. */
void averagingFilter (AudioBuffer<float>* buffer, double octaveFraction, double sampleRate, bool logAvg, bool nullifyPhase = false, bool nullifyAmplitude = false);



/*
 * Unit tests
 */
#ifdef JUCE_UNIT_TESTS
class FpConvolutionTest : public UnitTest{

public:
    FpConvolutionTest() : UnitTest("FpConvolutionTest", "fp convolution") {};
    
    void runTest() override;
    
private:

};

static FpConvolutionTest fpConvolutionTester;

#endif

} // convolution
	
} // fp
