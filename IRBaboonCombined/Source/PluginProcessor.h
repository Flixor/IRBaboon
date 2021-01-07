//
//  Copyright © 2020 Felix Postma. 
//


#pragma once

#include <fp_include_all.hpp>
#include <JuceHeader.h>

#include <ctime>



class IRBaboonAudioProcessor  : public AudioProcessor, public Thread
{
public:
	
	/* From Juce tutorial
	 https://docs.juce.com/master/tutorial_looping_audio_sample_buffer_advanced.html
	 */
	class ReferenceCountedBuffer : public ReferenceCountedObject{
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
		
			AudioSampleBuffer* getBuffer() {
				return &buffer;
			}
		
			bool bufferNotEmpty() {
				return buffer.getNumSamples() > 0;
			}
		
		private:
			String name;
			AudioSampleBuffer buffer;
			JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceCountedBuffer)
	};
	
	enum IRType {
		IR_NONE,
		IR_TARGET,
		IR_BASE
	};
	
	enum IRCapState {
		IRCAP_IDLE,
		IRCAP_PREP,
		IRCAP_CAPTURE,
		IRCAP_END,
	};
	
	struct IRCapStruct {
		IRType 	type;
		IRCapState	state;
		bool		playSweep;
		bool 		doFadeout;
	};
	
    IRBaboonAudioProcessor();
    ~IRBaboonAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;
	void processBlockBypassed(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

	void startCapture(IRType type);

	AudioSampleBuffer createIR(AudioSampleBuffer& numeratorBuf, AudioSampleBuffer& denominatorBuf);
	void createIRFilt();
	
	int getTotalSweepBreakSamples();
	int getMakeupIRLengthSamples();
	int getSamplerate();
	bool filtReady();
	AudioSampleBuffer getIRFilt();
	
	void setPlayFiltered(bool filtered);
	
	/* Print and thumbnail thread */
	void run() override;
	void saveIRTarg();
	void saveIRBase();
	void saveCustomExt(IRType type);
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
	void setPresweepSilence(int presweepSilence);
	void setMakeupSize(int makeupSize);
	void swapTargetBase();
	void loadTarget(File file);

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

	
	
private:
	int sampleRate = 48000;
	int generalHostBlockSize = 0;
	

	int silenceEndLengthSamples = 16384;
	int totalSweepBreakSamples = 4 * silenceEndLengthSamples;
	int sweepLengthSamples = totalSweepBreakSamples - silenceEndLengthSamples;
	
	int samplesWaitBeforeInputCapture = 16384;
	int buffersWaitForInputCapture = 0;
	int buffersWaitForResumeThroughput = 0;
	
	AudioSampleBuffer sweepBuf;
	AudioSampleBuffer sweepBufForDeconv;
	CircularBufferArray sweepBufArray;
	CircularBufferArray inputCaptureArray;

	int makeupIRLengthSamples = 2048;
	
	IRCapStruct IRCapture;

	ReferenceCountedBuffer::Ptr sweepTargPtr;
	ReferenceCountedBuffer::Ptr sweepBasePtr;
	ReferenceCountedBuffer::Ptr IRTargPtr;
	ReferenceCountedBuffer::Ptr IRBasePtr;
	ReferenceCountedBuffer::Ptr IRFiltPtr;
	AudioSampleBuffer IRpulse;
	AudioSampleBuffer* IRtoConvolve;
	
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
	std::string printDirectoryDebug = "/Users/flixor/Projects/IRBaboon/IRBaboonCombined/Debug/";
	std::string printDirectorySavedIRs = "/Users/flixor/Projects/IRBaboon/IRBaboonCombined/Debug/sweepir/";
	std::string savedIRExtension = ".sweepandir";

	IRType saveIRCustomType = IR_NONE;

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
	ParallelBufferPrinter debugPrinter;
	


	
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IRBaboonAudioProcessor)
};
