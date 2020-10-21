/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <FP_general.h>
#include <JuceHeader.h>

#include "FP_ParallelBufferPrinter.hpp"
#include "FP_Convolver.hpp"
#include "FP_Tools.hpp"
#include "FP_ExpSineSweep.hpp"
#include "FP_CircularBufferArray.hpp"

#include <ctime>



//==============================================================================




class AutoKalibraDemoAudioProcessor  : public AudioProcessor, public Thread
{
public:
	
	class ReferenceCountedBuffer : public ReferenceCountedObject
	{
	public:
		typedef ReferenceCountedObjectPtr<ReferenceCountedBuffer> Ptr;
		ReferenceCountedBuffer (const String& nameToUse, AudioSampleBuffer buf)
		: name (nameToUse), buffer (buf) {
		}
		
		ReferenceCountedBuffer (const String& nameToUse, int numChannels, int numSamples)
		: name (nameToUse), buffer (numChannels, numSamples) {
		}
		
		~ReferenceCountedBuffer() {
		}
		
		AudioSampleBuffer* getAudioSampleBuffer() {
			return &buffer;
		}
		
	private:
		String name;
		AudioSampleBuffer buffer;
		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceCountedBuffer)
	};
	
    //==============================================================================
    AutoKalibraDemoAudioProcessor();
    ~AutoKalibraDemoAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;
	void processBlockBypassed(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

	void divideSweepBufIntoArray();
	void divideThdnSigBufIntoArray();

	void startCaptureReference();
	void startCaptureCurrent();
	void startCaptureThdn();

	AudioSampleBuffer createTargetOrCurrentIR(AudioSampleBuffer& numeratorBuf, AudioSampleBuffer& denominatorBuf);
	AudioSampleBuffer createMakeupIR();
	
	int getTotalSweepBreakSamples();
	int getMakeupIRLengthSamples();
	int getSamplerate();
	bool invFiltReady();
	AudioSampleBuffer getMakeupIR();
	
	void setPlayUnprocessed();
	void setPlayInvFilt();
	
	// Print and thumbnail thread
	void run() override;
	void saveIRref();
	void saveIRcurr();
	void exportThdn();
	std::string getDateTimeString();
	void printDebug();
	std::string getPrintDirectoryDebug();
	std::string getSavedIRExtension();
	
	void setReferenceZoomdB(float dB);
	void setCurrentZoomdB(float dB);
	void setInvfiltZoomdB(float dB);
	void setOutputVolume(float dB);
	
	void setNullifyPhaseInvfilt(bool nullifyPhase);
	void setNullifyAmplitudeInvfilt(bool nullifyAmplitude);
	void setMakeupSize(int makeupSize);
	void swapTargetAndCurrent();
	void loadTarget(File file);

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==============================================================================
	int sampleRate = 48000;
	int generalHostBlockSize = 0;
	
	int silenceBeginningLengthSamples = 4096;
	int silenceEndLengthSamples = 16384;
	/* total samples = 4 * 32786 = 131072 */
	int totalSweepBreakSamples = 4 * FP_Tools::nextPowerOfTwo(silenceBeginningLengthSamples + silenceEndLengthSamples + 1);
	int sweepLengthSamples = totalSweepBreakSamples - silenceBeginningLengthSamples - silenceEndLengthSamples;
	
	int samplesWaitBeforeInputCapture = 2048; // 12 * 256
	int buffersWaitForInputCapture = 0;
	int buffersWaitForResumeThroughput = 0;
	
	bool needToFadein = true;
	bool needToFadeout = false;
	
	AudioSampleBuffer sweepBuf;
	AudioSampleBuffer sweepBufForDeconv;
	FP_CircularBufferArray sweepBufArray;
	FP_CircularBufferArray inputCaptureArray;
	AudioSampleBuffer inputConsolidated;
	
	float thdnSigFreq = 1000.0;
	AudioSampleBuffer thdnSigBuf;
	FP_CircularBufferArray thdnSigBufArray;
	dsp::WindowingFunction<float> window;
	int thdnExportLength = 8192;

	int makeupIRLengthSamples = 2048;
	bool captureReference = false;
	bool captureCurrent = false;
	bool captureThdn = false;
	bool captureInput = false;
	bool inputIsReference = false;
	bool inputIsCurrent = false;
	bool inputIsThdn = false;

	AudioSampleBuffer thdnRecBuf;
	ReferenceCountedBuffer::Ptr sweeprefObjectPtr;
	ReferenceCountedBuffer::Ptr sweepcurrObjectPtr;
	AudioSampleBuffer IRref;
	ReferenceCountedBuffer::Ptr IRrefObjectPtr;
	AudioSampleBuffer IRcurr;
	ReferenceCountedBuffer::Ptr IRcurrObjectPtr;
	AudioSampleBuffer IRinvfilt;
	ReferenceCountedBuffer::Ptr IRinvfiltObjectPtr;
	AudioSampleBuffer IRpulse;
	AudioSampleBuffer* IRtoConvolve;
	FP_Convolver convolver;
	
	bool playUnprocessed = true;
	bool playInvFilt = false;
	
	int generalInputAudioChannels = 2;
	int inputCaptureChannel = 2;
	
	
	float sweepLeveldB = 0.0;
	float outputVolumedB = -30.0;
	
	
	std::string printNames[5] = {
		"sweep target",
		"sweep base",
		"IR target",
		"IR base",
		"IR filter"
	};
	std::string printDirectoryDebug = "/Users/flixor/Documents/Super MakeupFilteraarder 2027/Debug exports";
	std::string printDirectorySavedIRs = "~/Documents/Super MakeupFilteraarder 2027/Saved IRs";
	std::string savedIRExtension = ".sweepandir";

	bool saveIRrefFlag = false;
	bool saveIRcurrFlag = false;
	bool exportThdnFlag = false;

	float referenceZoomdB = 0.0;
	float currentZoomdB = 0.0;
	float invfiltZoomdB = 0.0;
	
	bool nullifyPhaseInvfilt = false;
	bool nullifyAmplitudeInvfilt = false;

	
	
	// ====== convolution ==========
	int irPartitions;
	
	const int processBlockSize = 256;
	int N, fftBlockSize;

	FP_CircularBufferArray inputBufferArray;
	FP_CircularBufferArray audioFftBufferArray;
	FP_CircularBufferArray irFftBufferArray;
	
	dsp::FFT *fftIR, *fftForward, *fftInverse;
	AudioSampleBuffer inplaceBuffer;
	AudioSampleBuffer overlapBuffer;
	FP_CircularBufferArray convResultBufferArray;
	FP_CircularBufferArray outputBufferArray;
	FP_CircularBufferArray bypassOutputBufferArray;
	
	int inputBufferSampleIndex = 0;
	int outputBufferSampleIndex = 0;
	int outputSampleIndex = 0;
	int blocksToProcess = 0;
	int blocksToOutputBuffer = 0;
	int savedIndex = 0;
	
	

	// ====== debug ==========
	FP_ParallelBufferPrinter printer;
	


	
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoKalibraDemoAudioProcessor)
};
