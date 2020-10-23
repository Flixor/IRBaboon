 /*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"



//==============================================================================
// AutoKalibraDemoAudioProcessor
//==============================================================================

AutoKalibraDemoAudioProcessor::AutoKalibraDemoAudioProcessor()
     : AudioProcessor (BusesProperties()
					   .withInput  ("Input",  AudioChannelSet::stereo(), true)
					   .withOutput ("Output", AudioChannelSet::stereo(), true)
					   .withInput  ("Mic",  AudioChannelSet::mono(), true)
                       ),
		Thread ("Print and thumbnail")
{
	
	/* remove previously printed files */
	boost::filesystem::remove(printDirectoryDebug + "/targetForThumbnail.wav");
	boost::filesystem::remove(printDirectoryDebug + "/baseForThumbnail.wav");
	boost::filesystem::remove(printDirectoryDebug + "/filterForThumbnail.wav");
	

	
	// init sweep with pre and post silences
	ExpSineSweep sweeper;
	sweeper.generate( (((float) sweepLengthSamples) + 1)/ ((float) sampleRate), sampleRate, 20.0, sampleRate/2.0, sweepLeveldB );
	sweeper.linFadeout((11.0/12.0)*sampleRate/2.0); // fade out just under nyq
	AudioSampleBuffer sweep (sweeper.getSweepFloat());
	sweepBuf.setSize(1, totalSweepBreakSamples);
	sweepBuf.clear();
	sweepBuf.copyFrom(0, silenceBeginningLengthSamples, sweep, 0, 0, sweepLengthSamples);
	
	sweepBufForDeconv.setSize(1, totalSweepBreakSamples);
	sweepBufForDeconv.clear();
	sweepBufForDeconv.copyFrom(0, 0, sweep, 0, 0, sweepLengthSamples);
	sweepBufForDeconv.applyGain(tools::dBToLin(outputVolumedB + sweepLeveldB));
	
	printer.appendBuffer("sweep", sweepBuf);
	printer.printToWav(0, printer.getMaxBufferLength(), sampleRate, printDirectoryDebug);
	
	
	// init IRs
	IRpulse.makeCopyOf(tools::generatePulse(makeupIRLengthSamples, 100));
	
	IRTargPtr = new ReferenceCountedBuffer("IRTarg", 0, 0);
	IRBasePtr = new ReferenceCountedBuffer("IRBase", 0, 0);
	IRFiltPtr = new ReferenceCountedBuffer("IRFilt", 0, 0);
	
	sweepTargPtr = new ReferenceCountedBuffer("sweepref", 0, 0);
	sweepBasePtr = new ReferenceCountedBuffer("sweepcurr", 0, 0);

	
	
	// ======= convolution prep ========
	
	setLatencySamples(processBlockSize);
	
	N = 1;
	while (N < (processBlockSize * 2 - 1)){
		N *= 2;
	}
	fftBlockSize = N * 2;
	
	// initialise numChannels and numSamples for audio fft array buffers
	audioFftBufferArray.clearAndResize(0, generalInputAudioChannels, fftBlockSize);

	
	// initialise FFTs
	BigInteger fftBitMask = (BigInteger) N;
	
	fftIR = new dsp::FFT::FFT (fftBitMask.getHighestBit());
	fftForward = new dsp::FFT::FFT  (fftBitMask.getHighestBit());
	fftInverse = new dsp::FFT::FFT  (fftBitMask.getHighestBit());
	
	
	// initialise auxiliary buffers
	inplaceBuffer.setSize(generalInputAudioChannels, fftBlockSize);
	inplaceBuffer.clear();
	
	overlapBuffer.setSize(generalInputAudioChannels, processBlockSize);
	overlapBuffer.clear();
	
	
	 // Thread
	startThread();
	
}

AutoKalibraDemoAudioProcessor::~AutoKalibraDemoAudioProcessor()
{
	delete fftIR;
	delete fftForward;
	delete fftInverse;
	
	stopThread(-1); // A negative value in here will wait forever to time-out.
}

//==============================================================================
const String AutoKalibraDemoAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AutoKalibraDemoAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AutoKalibraDemoAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AutoKalibraDemoAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AutoKalibraDemoAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AutoKalibraDemoAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AutoKalibraDemoAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AutoKalibraDemoAudioProcessor::setCurrentProgram (int index)
{
}

const String AutoKalibraDemoAudioProcessor::getProgramName (int index)
{
    return {};
}

void AutoKalibraDemoAudioProcessor::changeProgramName (int index, const String& newName)
{
}




//==============================================================================
void AutoKalibraDemoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	sampleRate = (int) sampleRate;
	generalHostBlockSize = samplesPerBlock;
	
	if (generalHostBlockSize > processBlockSize) {
		setLatencySamples(generalHostBlockSize);
	}
	else {
		setLatencySamples(processBlockSize);
	}
	
	
	// delete previously existing printed bufs
	for (int name = 0; name < 5; name++){
		String fileName (printDirectoryDebug + "/" + printNames[name] + ".wav");
		File checkFile (fileName);
		if (checkFile.existsAsFile()) checkFile.deleteFile();
	}
	
	
	/* init sweep circ buf array */
	divideSweepBufIntoArray();

	/* init input capture circ buf array */
	inputCaptureArray.clearAndResize(sweepBufArray.getArraySize(), 1, generalHostBlockSize);
	
	
	/* init circ buf arrays for convolution */
	
	int inputArraySize = (int) std::ceil((float) generalHostBlockSize / (float) processBlockSize);
	
	int outputArraySize = (int) std::ceil((float) processBlockSize / (float) generalHostBlockSize);
	BigInteger outputArraySizebitmask = (BigInteger) outputArraySize;
	int power = outputArraySizebitmask.getHighestBit() + 1;
	outputArraySize = (int) std::pow(2.0f, power); // outputArraySize needs to be rounded up to the closest power of 2
	
	inputBufferArray.		clearAndResize(inputArraySize, generalInputAudioChannels, fftBlockSize);
	convResultBufferArray.	clearAndResize(inputArraySize, generalInputAudioChannels, fftBlockSize);
	outputBufferArray.		clearAndResize(outputArraySize, generalInputAudioChannels, generalHostBlockSize);
	bypassOutputBufferArray.clearAndResize(outputArraySize, generalInputAudioChannels, generalHostBlockSize);
	
	if (inputBufferArray.getArraySize() > audioFftBufferArray.getArraySize()){
		audioFftBufferArray.clearAndResize(inputBufferArray.getArraySize(), generalInputAudioChannels, fftBlockSize);
	}
	
	outputBufferArray.setReadIndex(1); // size is always at least 2, and we always want it to start 1 above the first one for proper latency behaviour
	
	savedIndex = audioFftBufferArray.getArraySize() - 1;
	
	inputBufferSampleIndex = 0;
	outputBufferSampleIndex = 0;
	
	
	
	irPartitions = (int) std::ceil((float) IRpulse.getNumSamples() / (float) processBlockSize);
	
	irFftBufferArray.clearAndResize(irPartitions, 1, fftBlockSize);
	
	int newAudioArraySize = std::max(irPartitions, inputBufferArray.getArraySize());
	
	audioFftBufferArray.changeArraySize(newAudioArraySize);
	audioFftBufferArray.setReadIndex(audioFftBufferArray.getWriteIndex());
	audioFftBufferArray.decrReadIndex();
	savedIndex = audioFftBufferArray.getReadIndex();
	
}





void AutoKalibraDemoAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
	
	inputBufferArray.changeArraySize(0);
	audioFftBufferArray.changeArraySize(0);
	irFftBufferArray.changeArraySize(0);
	convResultBufferArray.changeArraySize(0);
	outputBufferArray.changeArraySize(0);
}

bool AutoKalibraDemoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}





void AutoKalibraDemoAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // This is here to avoid people getting screaming feedback
    for (auto i = generalInputAudioChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

	
	int currentHostBlockSize = buffer.getNumSamples();
	
	if (currentHostBlockSize == 0){
		return;
	}
	
	
	
	// ===============================
	// Capture input
	// ===============================

	if (IRCapture.state == IRCAP_CAPTURE){
		
		auto micBuffer = getBusBuffer(buffer, true, 1); // second buffer block = mic input
		
		inputCaptureArray.getWriteBufferPtr()->copyFrom(0, 0, micBuffer, 0, 0, generalHostBlockSize);
		inputCaptureArray.incrWriteIndex();
		
		/* when capture is done */
		if (inputCaptureArray.getWriteIndex() == 0){

			/* there is a random buffer offset between sweep play and input capture. (what is the host doing...?)
			 * it's nice if IRinvfilt doesn't fold back though, when sweepref happens before sweepcurr.
			 * a 3 buffers-to-the-right offset was used before, but actually 0 seems to work fine now too. */
			inputConsolidated.makeCopyOf (inputCaptureArray.consolidate(0));
			
			if (IRCapture.type == IR_TARGET) {
				IRTargPtr->getBuffer()->makeCopyOf(createTargetOrBaseIR(inputConsolidated, sweepBufForDeconv));
				sweepTargPtr->getBuffer()->makeCopyOf(inputConsolidated);
				
				saveIRTargFlag = true;
			}
			else if (IRCapture.type == IR_BASE) {
				IRBasePtr->getBuffer()->makeCopyOf(createTargetOrBaseIR(inputConsolidated, sweepBufForDeconv));
				sweepBasePtr->getBuffer()->makeCopyOf(inputConsolidated);
				
				saveIRBaseFlag = true;
			}

			IRCapture.type = IR_NONE;

			/* create filter if both target and base have been captured */
			if (IRTargPtr->getBuffer()->getNumSamples() > 0 && IRBasePtr->getBuffer()->getNumSamples() > 0){
				createIRFilt();
			}
			
			IRCapture.state = IRCAP_END;

			notify();
		}
	}

	
	
	// ===============================
	// Play sweep 
	// ===============================
	
	if (IRCapture.state == IRCAP_PREP) {
	
		buffersWaitForInputCapture--;
		if (buffersWaitForInputCapture == 0){
			IRCapture.state = IRCAP_CAPTURE;
			IRCapture.playSweep = true;
		}
		
		if (IRCapture.doFadeout) {
			tools::linearFade (&buffer, false, 0, buffer.getNumSamples());
			IRCapture.doFadeout = false;
		}
	}

	
	if (IRCapture.playSweep) {
		
		/* output sweep */
		const float* readPtr = sweepBufArray.getReadBufferPtr()->getReadPointer(0, 0);
		for (int channel = 0; channel < totalNumOutputChannels; channel++){
			for (int sample = 0; sample < generalHostBlockSize; sample++){
				buffer.setSample(channel, sample, *(readPtr + sample));
			}
		}
		sweepBufArray.incrReadIndex();
		
		/* when sweep is done */
		if (sweepBufArray.getReadIndex() == 0){
			IRCapture.playSweep = false;
			buffersWaitForResumeThroughput = samplesWaitBeforeInputCapture / 256;
			IRCapture.doFadein = true;
		}
	}

	
	
	if (IRCapture.state == IRCAP_END) {
		buffersWaitForResumeThroughput--;
	}
	
	if (buffersWaitForResumeThroughput > 0){
		buffer.clear();
	}
	else if (IRCapture.state == IRCAP_END && buffersWaitForResumeThroughput == 0) {
		IRCapture.state = IRCAP_IDLE;
	}
	
	
	
	// ===============================
	// convolution
	// ===============================
	
	
	if (IRCapture.state == IRCAP_IDLE) {

		/* set IR to convolve with */
		if (playFiltered)
			IRtoConvolve = IRFiltPtr->getBuffer();
		else
			IRtoConvolve = &IRpulse;

		
		/* fade in buffer if necessary */
		if (IRCapture.doFadein) {
			tools::linearFade (&buffer, true, 0, buffer.getNumSamples());
			IRCapture.doFadein = false;
		}
		
			
		/*
		 * Copy input to input buffers, and input buffers to audio fft array
		 */
	
		for (int sample = 0; sample < currentHostBlockSize; sample++){
			for (int channel = 0; channel < generalInputAudioChannels; channel++){
				inputBufferArray.getWriteBufferPtr()->setSample(channel, inputBufferSampleIndex, buffer.getSample(channel, sample));
			}
			inputBufferSampleIndex++;
			
			/* when input buffer completed: copy to fft buffer array, perform fft, incr to next input buffers */
			if (inputBufferSampleIndex >= processBlockSize){
				
				inputBufferArray.setReadIndex(inputBufferArray.getWriteIndex());
				audioFftBufferArray.getWriteBufferPtr()->makeCopyOf(*inputBufferArray.getReadBufferPtr());
				
				
				for (int channel = 0; channel < generalInputAudioChannels; channel++){
					fftForward->performRealOnlyForwardTransform(audioFftBufferArray.getWriteBufferPtr()->getWritePointer(channel, 0), true);
				}
				audioFftBufferArray.incrWriteIndex();
				
				inputBufferArray.incrWriteIndex();
				inputBufferArray.getWriteBufferPtr()->clear();
				
				inputBufferSampleIndex = 0;
				blocksToProcess++;
			}
		}
	
	
		
		/*
		 * Process blocks
		 */
	
		while (blocksToProcess > 0){
			
			// always read an IR block
			irFftBufferArray.getWriteBufferPtr()->clear();

			irFftBufferArray.getWriteBufferPtr()->copyFrom(0, 0, *IRtoConvolve, 0, irFftBufferArray.getWriteIndex() * processBlockSize, processBlockSize);
			
			fftIR->performRealOnlyForwardTransform(irFftBufferArray.getWriteBufferPtr()->getWritePointer(0, 0), true);
			
			irFftBufferArray.incrWriteIndex();
			
			
			
			// DSP loop
			convResultBufferArray.getWriteBufferPtr()->clear();
			
			audioFftBufferArray.setReadIndex(savedIndex);
			audioFftBufferArray.incrReadIndex();
			savedIndex = audioFftBufferArray.getReadIndex();
			
			
			for (int channel = 0; channel < generalInputAudioChannels; channel++) {
				
				float* overlapBufferPtr = overlapBuffer.getWritePointer(channel, 0);
				float* convResultPtr = convResultBufferArray.getWriteBufferPtr()->getWritePointer(channel, 0);
				
				int irFftReadIndex = 0; // IR fft index should always start at 0 and not fold back, so its readIndex is not used
				audioFftBufferArray.setReadIndex(savedIndex); // for iteration >1 of the channel
				
				while (irFftReadIndex < irFftBufferArray.getArraySize()){
					
					inplaceBuffer.makeCopyOf(*audioFftBufferArray.getReadBufferPtr());
					float* inplaceBufferPtr = inplaceBuffer.getWritePointer(channel, 0);

					const float* irFftPtr = irFftBufferArray.getBufferPtrAtIndex(irFftReadIndex)->getReadPointer(0, 0);
					
					// complex multiply 1 audio fft block and 1 IR fft block
					for (int i = 0; i <= N; i += 2){
						tools::complexMul(inplaceBufferPtr + i,
											 inplaceBufferPtr + i + 1,
											 irFftPtr[i],
											 irFftPtr[i + 1]);
					}
					
					// sum multiplications in conv result buffer
					for (int i = 0; i < fftBlockSize; i++){
						convResultPtr[i] += inplaceBufferPtr[i];
					}
					irFftReadIndex++;
					audioFftBufferArray.decrReadIndex();
				}
				
				// FDL method --> IFFT after summing
				fftInverse->performRealOnlyInverseTransform(convResultPtr);
				
				// add existing overlap and save new overlap
				for (int i = 0; i < processBlockSize; i++){
					convResultPtr[i] += overlapBufferPtr[i];
					overlapBufferPtr[i] = convResultPtr[processBlockSize + i];
				}
				
			} // end DSP loop
			
			convResultBufferArray.incrWriteIndex();
			blocksToOutputBuffer++;
			blocksToProcess--;
			
		} // end while() Process blocks
	
	
	
		/*
		 * Write completed convolution result blocks to output buffers
		 */
	
		while (blocksToOutputBuffer > 0){
			
			for (int sample = 0; sample < processBlockSize; sample++){
				if (outputBufferSampleIndex == 0)
					outputBufferArray.getWriteBufferPtr()->clear();
				for (int channel = 0; channel < generalInputAudioChannels; channel++) {
				
					outputBufferArray.getWriteBufferPtr()->setSample(channel, outputBufferSampleIndex,
																	 convResultBufferArray.getReadBufferPtr()->getSample(channel, sample));
				}
				outputBufferSampleIndex++;
				if (outputBufferSampleIndex >= generalHostBlockSize){
					outputBufferArray.incrWriteIndex();
					outputBufferSampleIndex = 0;
				}
			}
			convResultBufferArray.incrReadIndex();
			blocksToOutputBuffer--;
		}
	
	
	
		/*
		 * Output buffers to real output
		 */
	
		for (int sample = 0; sample < currentHostBlockSize; sample++){
			
			for (int channel = 0; channel < generalInputAudioChannels; channel++)
				buffer.setSample(channel, sample, outputBufferArray.getReadBufferPtr()->getSample(channel, outputSampleIndex));
			
			outputSampleIndex++;
			if(outputSampleIndex >= generalHostBlockSize){
				outputBufferArray.incrReadIndex();
				outputSampleIndex = 0;
			}
		}
	
	
	} // end if capture state == IRCAP_IDLE (aka not playing sweep / capturing)
	
	
		
		
	// standard attenuation
	buffer.applyGain(tools::dBToLin(outputVolumedB));
	
	
	// ghetto limiter
	if (tools::linTodB(buffer.getMagnitude(0, 0, generalHostBlockSize)) > 0.0){
		tools::normalize(&buffer, 0.0, true);
	}
	
}





void AutoKalibraDemoAudioProcessor::processBlockBypassed(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
	bypassOutputBufferArray.getWriteBufferPtr()->makeCopyOf(buffer);
	bypassOutputBufferArray.incrWriteIndex();
	bypassOutputBufferArray.incrReadIndex();
	buffer.makeCopyOf(*bypassOutputBufferArray.getReadBufferPtr());
}



void AutoKalibraDemoAudioProcessor::divideSweepBufIntoArray(){
	if (sweepBuf.getNumSamples() == 0){
		DBG("divideSweepBufIntoArray() error: sweepBuf not initialised yet.\n");
		return;
	}
	
	if (generalHostBlockSize <= 0) return;
	
	int bufArraySize = sweepBuf.getNumSamples() / generalHostBlockSize;
	sweepBufArray.clearAndResize(bufArraySize, 1, generalHostBlockSize);
	
	for (int buf = 0; buf < bufArraySize; buf++){
		sweepBufArray.getWriteBufferPtr()->copyFrom(0, 0, sweepBuf, 0, buf * generalHostBlockSize, generalHostBlockSize);
		sweepBufArray.incrWriteIndex();
	}
}


void AutoKalibraDemoAudioProcessor::startCaptureTarg(){
	captureTarget = true;
	buffersWaitForInputCapture = samplesWaitBeforeInputCapture / processBlockSize;
	needToFadeout = true;
}


void AutoKalibraDemoAudioProcessor::startCaptureBase(){
	captureBase = true;
	buffersWaitForInputCapture = samplesWaitBeforeInputCapture / processBlockSize;
	needToFadeout = true;
}


void AutoKalibraDemoAudioProcessor::startCapture(IRCapType type){
	if (type == IR_NONE) return;
	
	IRCapture.type = type;
	IRCapture.state = IRCAP_PREP;
	IRCapture.doFadeout = true;

	buffersWaitForInputCapture = samplesWaitBeforeInputCapture / processBlockSize;
}



AudioSampleBuffer AutoKalibraDemoAudioProcessor::createTargetOrBaseIR(AudioSampleBuffer& numeratorBuf, AudioSampleBuffer& denominatorBuf){
	return convolver::deconvolveNonPeriodic2(&numeratorBuf, &denominatorBuf, sampleRate, true, false);
}



void AutoKalibraDemoAudioProcessor::createIRFilt(){
	AudioSampleBuffer makeupIR;
	AudioSampleBuffer* IRTarg = IRTargPtr->getBuffer();
	AudioSampleBuffer* IRBase = IRBasePtr->getBuffer();
	
	makeupIR.makeCopyOf(convolver::deconvolveNonPeriodic2(IRTarg, IRBase, sampleRate, true, nullifyPhaseFilt, nullifyAmplFilt));

	if (nullifyPhaseFilt) convolver::shifteroo(&makeupIR);
	
	ParallelBufferPrinter printer;
	printer.appendBuffer("IRmakeup unchopped", makeupIR);
	printer.printToWav(0, printer.getMaxBufferLength(), sampleRate, printDirectoryDebug);

//	AudioSampleBuffer IRinvfiltchop (convolver::IRchop(IRinvfilt, makeupIRLengthSamples, -24.0, 25));
//	return IRinvfiltchop;
	
	IRFiltPtr->getBuffer()->makeCopyOf(makeupIR);
}


int AutoKalibraDemoAudioProcessor::getTotalSweepBreakSamples(){
	return totalSweepBreakSamples;
}

int AutoKalibraDemoAudioProcessor::getMakeupIRLengthSamples(){
	return makeupIRLengthSamples;
}

int AutoKalibraDemoAudioProcessor::getSamplerate(){
	return sampleRate;
}


bool AutoKalibraDemoAudioProcessor::filtReady(){
	return (IRFiltPtr->getBuffer()->getNumSamples() > 0);
}


AudioSampleBuffer AutoKalibraDemoAudioProcessor::getMakeupIR(){
	return *IRFiltPtr->getBuffer();
}


void AutoKalibraDemoAudioProcessor::setPlayFiltered (bool filtered){
	playFiltered = filtered;
}



void AutoKalibraDemoAudioProcessor::run() {
	while (NOT threadShouldExit()) {
		if (saveIRTargFlag) {
			saveIRTarg();
		}
		if (saveIRBaseFlag) {
			saveIRBase();
		}
		
		if (saveIRTargFlag || saveIRBaseFlag){
			printDebug();
			saveIRTargFlag = false;
			saveIRBaseFlag = false;
		}
		
		/* A negative timeout value means that the method will wait indefinitely (until notify() is called) */
		wait (-1);
	}
}



void AutoKalibraDemoAudioProcessor::saveIRTarg(){

	std::string name = "IR target ";
	std::string date = getDateTimeString();
	name += date;
	
	AudioSampleBuffer savebuf (2, totalSweepBreakSamples);
	savebuf.copyFrom(0, 0, *(sweepTargPtr->getBuffer()), 0, 0, totalSweepBreakSamples);
	savebuf.copyFrom(1, 0, *(IRTargPtr->getBuffer()), 0, 0, totalSweepBreakSamples);
	
	ParallelBufferPrinter wavPrinter;
	wavPrinter.appendBuffer(name, savebuf);
	wavPrinter.printToWav(0, wavPrinter.getMaxBufferLength(), sampleRate, printDirectorySavedIRs);
	
	// replace .wav extension with custom extension
	std::string path = printDirectorySavedIRs + "/" + name + ".wav";
	std::string pathCustomExtension = printDirectorySavedIRs + "/" + name + savedIRExtension;
	File f (path);
	File f2 (pathCustomExtension);
	f.moveFileTo(f2); // function to rename with
}



void AutoKalibraDemoAudioProcessor::saveIRBase(){
	
	std::string name = "IR current ";
	std::string date = getDateTimeString();
	name += date;
	
	AudioSampleBuffer savebuf (2, totalSweepBreakSamples);
	savebuf.copyFrom(0, 0, *(sweepBasePtr->getBuffer()), 0, 0, totalSweepBreakSamples);
	savebuf.copyFrom(1, 0, *(IRBasePtr->getBuffer()), 0, 0, totalSweepBreakSamples);
	
	ParallelBufferPrinter wavPrinter;
	wavPrinter.appendBuffer(name, savebuf);
	wavPrinter.printToWav(0, wavPrinter.getMaxBufferLength(), sampleRate, printDirectorySavedIRs);

	/* replace .wav extension with custom extension */
	std::string path = printDirectorySavedIRs + "/" + name + ".wav";
	std::string pathCustomExtension = printDirectorySavedIRs + "/" + name + savedIRExtension;
	File f (path);
	File f2 (pathCustomExtension);
	f.moveFileTo(f2); // function to rename with
}



std::string AutoKalibraDemoAudioProcessor::getDateTimeString(){
	std::time_t now = std::time(NULL);
	std::tm* ptm = std::localtime(&now);
	char date[32];
	std::strftime(date, 32, "%Y-%m-%d %H_%M_%S", ptm);
	
	return date;
}



void AutoKalibraDemoAudioProcessor::printDebug() {
	ParallelBufferPrinter wavPrinter;
	ParallelBufferPrinter freqPrinter;

	/* dereference the buffers */
	AudioSampleBuffer sweeprefprint (*(sweepTargPtr->getBuffer()));
	AudioSampleBuffer IRrefprint (*(IRTargPtr->getBuffer()));
	AudioSampleBuffer sweepcurrprint (*(sweepBasePtr->getBuffer()));
	AudioSampleBuffer IRcurrprint (*(IRBasePtr->getBuffer()));
	AudioSampleBuffer IRinvfiltprint (*(IRFiltPtr->getBuffer()));

	/* print freq */
	if (IRrefprint.getNumSamples() > 0) {
		AudioSampleBuffer IRrefprint_fft (convolver::fftTransform(IRrefprint));
		freqPrinter.appendBuffer("IR target fft", IRrefprint_fft);
	}
	if (IRcurrprint.getNumSamples() > 0) {
		AudioSampleBuffer IRcurrprint_fft (convolver::fftTransform(IRcurrprint));
		freqPrinter.appendBuffer("IR base fft", IRcurrprint_fft);
	}
	if (IRinvfiltprint.getNumSamples() > 0) {
		AudioSampleBuffer IRinvfiltprint_fft (convolver::fftTransform(IRinvfiltprint));
		freqPrinter.appendBuffer("IR filter fft", IRinvfiltprint_fft);
	}
	if (freqPrinter.getMaxBufferLength() > 0){
		freqPrinter.printFreqToCsv(sampleRate, printDirectoryDebug);
	}
	
	// include separate buffers in printer
	wavPrinter.appendBuffer(printNames[0], sweeprefprint);
	wavPrinter.appendBuffer(printNames[1], sweepcurrprint);
	wavPrinter.appendBuffer(printNames[2], IRrefprint);
	wavPrinter.appendBuffer(printNames[3], IRcurrprint);
	wavPrinter.appendBuffer(printNames[4], IRinvfiltprint);
	
	// create target sweep & IR file for thumbnail
	AudioSampleBuffer referenceForThumbnail (2, std::max(sweeprefprint.getNumSamples(), IRrefprint.getNumSamples()));
	referenceForThumbnail.clear();
	if (sweeprefprint.getNumSamples() > 0)
		referenceForThumbnail.copyFrom(0, 0, sweeprefprint, 0, 0, sweeprefprint.getNumSamples());
	if (IRrefprint.getNumSamples() > 0)
		referenceForThumbnail.copyFrom(1, 0, IRrefprint, 0, 0, IRrefprint.getNumSamples());
	referenceForThumbnail.applyGain(tools::dBToLin(zoomTargdB));
	wavPrinter.appendBuffer("targetForThumbnail", referenceForThumbnail);
	
	// delete thumbnail file again if it has been swaped with empty current
	String nameReference (printDirectoryDebug + "/targetForThumbnail.wav");
	File fileReference (nameReference);
	if (referenceForThumbnail.getNumSamples() == 0 && fileReference.existsAsFile()){
		fileReference.deleteFile();
	}

	// create current sweep & IR file for thumbnail
	AudioSampleBuffer currentForThumbnail (2, std::max(sweepcurrprint.getNumSamples(), IRcurrprint.getNumSamples()));
	currentForThumbnail.clear();
	if (sweepcurrprint.getNumSamples() > 0)
		currentForThumbnail.copyFrom(0, 0, sweepcurrprint, 0, 0, sweepcurrprint.getNumSamples());
	if (IRcurrprint.getNumSamples() > 0)
		currentForThumbnail.copyFrom(1, 0, IRcurrprint, 0, 0, IRcurrprint.getNumSamples());
	currentForThumbnail.applyGain(tools::dBToLin(zoomBasedB));
	wavPrinter.appendBuffer("baseForThumbnail", currentForThumbnail);

	// delete thumbnail file again if it has been swaped with empty reference
	String nameCurrent (printDirectoryDebug + "/baseForThumbnail.wav");
	File fileCurrent (nameCurrent);
	if (currentForThumbnail.getNumSamples() == 0 && fileCurrent.existsAsFile()){
		fileCurrent.deleteFile();
	}

	// create makeup IR file for thumbnail
	AudioSampleBuffer InvfiltForThumbnail (1, IRinvfiltprint.getNumSamples());
	if (IRinvfiltprint.getNumSamples() > 0)
		InvfiltForThumbnail.copyFrom(0, 0, IRinvfiltprint, 0, 0, IRinvfiltprint.getNumSamples());
	InvfiltForThumbnail.applyGain(tools::dBToLin(zoomFiltdB));
	wavPrinter.appendBuffer("filterForThumbnail", InvfiltForThumbnail);
	
	// print everything
	wavPrinter.printToWav(0, wavPrinter.getMaxBufferLength(), sampleRate, printDirectoryDebug);
}


std::string AutoKalibraDemoAudioProcessor::getPrintDirectoryDebug(){
	return printDirectoryDebug;
}


std::string AutoKalibraDemoAudioProcessor::getSavedIRExtension(){
	return savedIRExtension;
}


void AutoKalibraDemoAudioProcessor::setZoomTarg(float dB){
	zoomTargdB = dB;
	notify();
}


void AutoKalibraDemoAudioProcessor::setZoomBase(float dB){
	zoomBasedB = dB;
	notify();
}


void AutoKalibraDemoAudioProcessor::setZoomFilt(float dB){
	zoomFiltdB = dB;
	notify();
}


void AutoKalibraDemoAudioProcessor::setOutputVolume(float dB){
	outputVolumedB = dB;
	tools::normalize(&sweepBufForDeconv, outputVolumedB + sweepLeveldB);
}


void AutoKalibraDemoAudioProcessor::setNullifyPhaseFilt(bool nullifyPhase){
	nullifyPhaseFilt = nullifyPhase;
	
	if (IRTargPtr->getBuffer()->getNumSamples() > 0 && IRBasePtr->getBuffer()->getNumSamples() > 0){
		createIRFilt();
	}
	
	notify();
}


void AutoKalibraDemoAudioProcessor::setNullifyAmplFilt(bool nullifyAmplitude){
	nullifyAmplFilt = nullifyAmplitude;
	
	if (IRTargPtr->getBuffer()->getNumSamples() > 0 && IRBasePtr->getBuffer()->getNumSamples() > 0){
		createIRFilt();
	}
	
	notify();
}


void AutoKalibraDemoAudioProcessor::setMakeupSize(int makeupSize){
	makeupIRLengthSamples = makeupSize;
	
	if (IRTargPtr->getBuffer()->getNumSamples() > 0 && IRBasePtr->getBuffer()->getNumSamples() > 0){
		createIRFilt();
	}

	notify();
}


void AutoKalibraDemoAudioProcessor::swapTargetBase(){
	
	// swap around
	AudioSampleBuffer savedSweep (*(sweepTargPtr->getBuffer()));
	AudioSampleBuffer savedIR (*(IRTargPtr->getBuffer()));
	sweepTargPtr->getBuffer()->makeCopyOf(*(sweepBasePtr->getBuffer()));
	IRTargPtr->getBuffer()->makeCopyOf(*(IRBasePtr->getBuffer()));
	sweepBasePtr->getBuffer()->makeCopyOf(savedSweep);
	IRBasePtr->getBuffer()->makeCopyOf(savedIR);
//	IRTarggg = *(IRTargRefPtr->getBuffer());
//	IRBaseee = *(IRBasePtr->getBuffer());

	// generate new makeup
	if (IRTargPtr->getBuffer()->getNumSamples() > 0 && IRBasePtr->getBuffer()->getNumSamples() > 0){
		createIRFilt();
	}
	
	// reprint
	notify();
}


void AutoKalibraDemoAudioProcessor::loadTarget(File file){
	
	// rename custom extension to .wav
	std::string pathCustomExtension = file.getFullPathName().toStdString();
	std::string pathWav = pathCustomExtension;
	boost::ireplace_all(pathWav, savedIRExtension, ".wav");
	boost::filesystem::rename(pathCustomExtension, pathWav);
	
	File fileToLoad (pathWav);
	AudioSampleBuffer sweepAndIR (tools::fileToBuffer(fileToLoad));
	
	if (IRTargPtr->getBuffer()->getNumSamples() == 0){
		sweepTargPtr->getBuffer()->setSize(1, totalSweepBreakSamples);
		IRTargPtr->getBuffer()->setSize(1, totalSweepBreakSamples);
	}
	
	sweepTargPtr->getBuffer()->clear();
	sweepTargPtr->getBuffer()->copyFrom(0, 0, sweepAndIR, 0, 0, totalSweepBreakSamples);
	IRTargPtr->getBuffer()->clear();
	IRTargPtr->getBuffer()->copyFrom(0, 0, sweepAndIR, 1, 0, totalSweepBreakSamples);
	
	// generate new makeup
	if (IRTargPtr->getBuffer()->getNumSamples() > 0 && IRBasePtr->getBuffer()->getNumSamples() > 0){
		createIRFilt();
	}
	
	// rerename wav
	boost::filesystem::rename(pathWav, pathCustomExtension);
	
	// reprint
	notify();
}



//==============================================================================
bool AutoKalibraDemoAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* AutoKalibraDemoAudioProcessor::createEditor()
{
    return new AutoKalibraDemoAudioProcessorEditor (*this);
}

//==============================================================================
void AutoKalibraDemoAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void AutoKalibraDemoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoKalibraDemoAudioProcessor();
}
