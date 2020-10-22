/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <fp_general.hpp>
#include <JuceHeader.h>

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

	void startCaptureTarg();
	void startCaptureBase();
	void startCaptureThdn();

	AudioSampleBuffer createTargetOrBaseIR(AudioSampleBuffer& numeratorBuf, AudioSampleBuffer& denominatorBuf);
	AudioSampleBuffer createMakeupIR();
	
	int getTotalSweepBreakSamples();
	int getMakeupIRLengthSamples();
	int getSamplerate();
	bool filtReady();
	AudioSampleBuffer getMakeupIR();
	
	void setPlayUnprocessed();
	void setPlayInvFilt();
	
	/* Print and thumbnail thread */
	void run() override;
	void saveIRTarg();
	void saveIRBase();
	void exportThdn();
	std::string getDateTimeString();
	void printDebug();
	std::string getPrintDirectoryDebug();
	std::string getSavedIRExtension();
	
	void setZoomTarg(float dB);
	void setZoomBase(float dB);
	void setZoomFilt(float dB);
	void setOutputVolume(float dB);
	
	void setNullifyPhaseFilt(bool nullifyPhase);
	void setNullifyAmplFilt(bool nullifyAmplitude);
	void setMakeupSize(int makeupSize);
	void swapTargetBase();
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
	int totalSweepBreakSamples = 4 * tools::nextPowerOfTwo(silenceBeginningLengthSamples + silenceEndLengthSamples + 1);
	int sweepLengthSamples = totalSweepBreakSamples - silenceBeginningLengthSamples - silenceEndLengthSamples;
	
	int samplesWaitBeforeInputCapture = 2048; // 12 * 256
	int buffersWaitForInputCapture = 0;
	int buffersWaitForResumeThroughput = 0;
	
	bool needToFadein = true;
	bool needToFadeout = false;
	
	AudioSampleBuffer sweepBuf;
	AudioSampleBuffer sweepBufForDeconv;
	CircularBufferArray sweepBufArray;
	CircularBufferArray inputCaptureArray;
	AudioSampleBuffer inputConsolidated;
	
	float thdnSigFreq = 1000.0;
	AudioSampleBuffer thdnSigBuf;
	CircularBufferArray thdnSigBufArray;
	dsp::WindowingFunction<float> window;
	int thdnExportLength = 8192;

	int makeupIRLengthSamples = 2048;
	bool captureTarget = false;
	bool captureBase = false;
	bool captureThdn = false;
	bool captureInput = false;
	bool inputIsTarget = false;
	bool inputIsBase = false;
	bool inputIsThdn = false;

	AudioSampleBuffer thdnRecBuf;
	ReferenceCountedBuffer::Ptr sweepTargRefPtr;
	ReferenceCountedBuffer::Ptr sweepBaseRefPtr;
	AudioSampleBuffer IRTarg;
	ReferenceCountedBuffer::Ptr IRTargRefPtr;
	AudioSampleBuffer IRBase;
	ReferenceCountedBuffer::Ptr IRBaseRefPtr;
	AudioSampleBuffer IRFilt;
	ReferenceCountedBuffer::Ptr IRFiltRefPtr;
	AudioSampleBuffer IRpulse;
	AudioSampleBuffer* IRtoConvolve;
	
	bool playUnprocessed = true;
	bool playFiltered = false;
	
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

	bool saveIRTargFlag = false;
	bool saveIRBaseFlag = false;
	bool exportThdnFlag = false;

	float zoomTargdB = 0.0;
	float zoomBasedB = 0.0;
	float zoomFiltdB = 0.0;
	
	bool nullifyPhaseFilt = false;
	bool nullifyAmplFilt = false;

	
	
	// ====== convolution ==========
	int irPartitions;
	
	const int processBlockSize = 256;
	int N, fftBlockSize;

	CircularBufferArray inputBufferArray;
	CircularBufferArray audioFftBufferArray;
	CircularBufferArray irFftBufferArray;
	
	dsp::FFT *fftIR, *fftForward, *fftInverse;
	AudioSampleBuffer inplaceBuffer;
	AudioSampleBuffer overlapBuffer;
	
	CircularBufferArray convResultBufferArray;
	CircularBufferArray outputBufferArray;
	CircularBufferArray bypassOutputBufferArray;
	
	int inputBufferSampleIndex = 0;
	int outputBufferSampleIndex = 0;
	int outputSampleIndex = 0;
	int blocksToProcess = 0;
	int blocksToOutputBuffer = 0;
	int savedIndex = 0;
	
	

	// ====== debug ==========
	ParallelBufferPrinter printer;
	


	
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoKalibraDemoAudioProcessor)
};
