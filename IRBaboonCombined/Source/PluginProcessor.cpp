//
//  Copyright © 2020 Felix Postma. 
//


#include "PluginProcessor.h"
#include "PluginEditor.h"



IRBaboonAudioProcessor::IRBaboonAudioProcessor()
     : AudioProcessor (BusesProperties()
					   .withInput  ("Input",  AudioChannelSet::stereo(), true)
					   .withOutput ("Output", AudioChannelSet::stereo(), true)
					   .withInput  ("Mic",  AudioChannelSet::mono(), true)
                       ),
		Thread ("Print and thumbnail")
{
	
	/* remove previously printed files */
	// TODO: regex all 'thumbnail' and remove
	boost::filesystem::remove(printDirectoryDebug + "thumbnailBase.wav");
	boost::filesystem::remove(printDirectoryDebug + "thumbnailTarg.wav");
	boost::filesystem::remove(printDirectoryDebug + "thumbnailFilt.wav");
	

	/* init sweep with pre and post silences */
	ExpSineSweep sweeper;
	sweeper.generate( (((float) sweepLengthSamples) + 1)/ ((float) sampleRate), sampleRate, 20.0, sampleRate/2.0, sweepLeveldB );
	sweeper.linFadeout((11.0/12.0)*sampleRate/2.0); // fade out just under nyq
	
	sweepBuf.setSize(1, totalSweepBreakSamples);
	sweepBuf.clear();
	sweepBuf.copyFrom(0, 0, sweeper.getSweepFloat(), 0, 0, sweepLengthSamples);
	
	sweepBufForDeconv.setSize(1, totalSweepBreakSamples);
	sweepBufForDeconv.clear();
	sweepBufForDeconv.copyFrom(0, 0, sweeper.getSweepFloat(), 0, 0, sweepLengthSamples);
	sweepBufForDeconv.applyGain(tools::dBToLin(outputVolumedB + sweepLeveldB));
	
	debugPrinter.appendBuffer("sweep", sweepBuf);
	debugPrinter.printToWav(0, debugPrinter.getMaxBufferLength(), sampleRate, printDirectoryDebug);
	
	
	/* init IRs */
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

IRBaboonAudioProcessor::~IRBaboonAudioProcessor()
{
	delete fftIR;
	delete fftForward;
	delete fftInverse;
	
	stopThread(-1); // A negative value in here will wait forever to time-out.
}

//==============================================================================
const String IRBaboonAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool IRBaboonAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool IRBaboonAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool IRBaboonAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double IRBaboonAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int IRBaboonAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int IRBaboonAudioProcessor::getCurrentProgram()
{
    return 0;
}

void IRBaboonAudioProcessor::setCurrentProgram (int index)
{
}

const String IRBaboonAudioProcessor::getProgramName (int index)
{
    return {};
}

void IRBaboonAudioProcessor::changeProgramName (int index, const String& newName)
{
}




//==============================================================================
void IRBaboonAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	sampleRate = (int) sampleRate;
	generalHostBlockSize = samplesPerBlock;
	
	if (generalHostBlockSize > processBlockSize) {
		setLatencySamples(generalHostBlockSize);
	}
	else {
		setLatencySamples(processBlockSize);
	}
	
	
	/* delete previously existing printed bufs */
	for (int name = 0; name < 5; name++){
		boost::filesystem::remove(printDirectoryDebug + printNames[name] + ".wav");
	}
	
	
	/* init sweep circ buf array */
	if (sweepBuf.getNumSamples() == 0){
		DBG("divideSweepBufIntoArray() error: sweepBuf not initialised yet.\n");
		return;
	}
		
	int sweepBufArraySize = sweepBuf.getNumSamples() / generalHostBlockSize;
	sweepBufArray.clearAndResize(sweepBufArraySize, 1, generalHostBlockSize);
	
	for (int buf = 0; buf < sweepBufArraySize; buf++){
		sweepBufArray.getWriteBufferPtr()->copyFrom(0, 0, sweepBuf, 0, buf * generalHostBlockSize, generalHostBlockSize);
		sweepBufArray.incrWriteIndex();
	}

	
	/* input capture array is the same size as the sweep array */
	inputCaptureArray.clearAndResize(sweepBufArray.getArraySize(), 1, generalHostBlockSize);
	
	
	/* init circ buf arrays for convolution */
	int inputArraySize = (int) std::ceil((float) generalHostBlockSize / (float) processBlockSize);
	/* outputArraySize needs to be rounded up to the closest power of 2 */
	int outputArraySize = tools::nextPowerOfTwo(std::ceil((float) processBlockSize / (float) generalHostBlockSize) + 1);

	/* apply array sizes */
	inputBufferArray.		clearAndResize(inputArraySize, generalInputAudioChannels, fftBlockSize);
	convResultBufferArray.	clearAndResize(inputArraySize, generalInputAudioChannels, fftBlockSize);
	outputBufferArray.		clearAndResize(outputArraySize, generalInputAudioChannels, generalHostBlockSize);
	bypassOutputBufferArray.clearAndResize(outputArraySize, generalInputAudioChannels, generalHostBlockSize);
	
	if (inputBufferArray.getArraySize() > audioFftBufferArray.getArraySize()){
		audioFftBufferArray.clearAndResize(inputBufferArray.getArraySize(), generalInputAudioChannels, fftBlockSize);
	}
	
	/* size is always at least 2, and we always want it to start 1 above the first one for proper latency behaviour */
	outputBufferArray.setReadIndex(1);
	
	inputBufferSampleIndex = 0;
	outputBufferSampleIndex = 0;
	savedIndex = audioFftBufferArray.getArraySize() - 1;
	
	
	irPartitions = (int) std::ceil((float) IRpulse.getNumSamples() / (float) processBlockSize);
	irFftBufferArray.clearAndResize(irPartitions, 1, fftBlockSize);
	
	/* tricky tricks */
	int newAudioArraySize = std::max(irPartitions, inputBufferArray.getArraySize());
	audioFftBufferArray.changeArraySize(newAudioArraySize);
	audioFftBufferArray.setReadIndex(audioFftBufferArray.getWriteIndex());
	audioFftBufferArray.decrReadIndex();
	savedIndex = audioFftBufferArray.getReadIndex();
}





void IRBaboonAudioProcessor::releaseResources()
{
    /* memory used by buffer arrays is freed up */
	inputBufferArray.changeArraySize(0);
	audioFftBufferArray.changeArraySize(0);
	irFftBufferArray.changeArraySize(0);
	convResultBufferArray.changeArraySize(0);
	outputBufferArray.changeArraySize(0);
}

/* Boilerplate JUCE, to check whether channel layouts are supported. */
bool IRBaboonAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}





void IRBaboonAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
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
	
	AudioSampleBuffer micBuffer = getBusBuffer(buffer, true, 1); // second buffer block = mic input
	
	/*
	 * Capture input
	 */

	if (IRCapture.state == IRCAP_CAPTURE){
		
		inputCaptureArray.getWriteBufferPtr()->copyFrom(0, 0, micBuffer, 0, 0, generalHostBlockSize);
		inputCaptureArray.incrWriteIndex();
		
		/* when capture is done */
		if (inputCaptureArray.getWriteIndex() == 0){

			/* there is a random buffer offset between sweep play and input capture. (what is the host doing...?)
			 * it's nice if IRFilt doesn't fold back though, when sweepTarg happens before sweepBase.
			 * a 3 buffers-to-the-right offset was used before, but actually 0 seems to work fine now too.
			 */
			
			if (IRCapture.type == IR_TARGET) {
				sweepTargPtr->getBuffer()->makeCopyOf(inputCaptureArray.consolidate(0));
				IRTargPtr->getBuffer()->makeCopyOf(createIR(*sweepTargPtr->getBuffer(), sweepBufForDeconv));

				saveIRCustomType = IR_TARGET;
			}
			else if (IRCapture.type == IR_BASE) {
				sweepBasePtr->getBuffer()->makeCopyOf(inputCaptureArray.consolidate(0));
				IRBasePtr->getBuffer()->makeCopyOf(createIR(*sweepBasePtr->getBuffer(), sweepBufForDeconv));

				saveIRCustomType = IR_BASE;
			}
			

			IRCapture.type = IR_NONE;

			/* create filter if both target and base have been captured */
			if (IRTargPtr->bufferNotEmpty() && IRBasePtr->bufferNotEmpty()){
				createIRFilt();
			}
			
			IRCapture.state = IRCAP_END;
			
			/* run print thumbnail thread */
			notify();
			
		} // when capture is done
		
	} // if (IRCapture.state == IRCAP_CAPTURE)

	
	/*
	 * Play empty buffers before starting sweep / capture
	 */
	
	if (IRCapture.state == IRCAP_PREP) {
	
		/* first, fade out last buffer of throughput */
		if (IRCapture.doFadeout) {
			tools::linearFade (&buffer, false, 0, buffer.getNumSamples());
			IRCapture.doFadeout = false;
		}
		else {
			buffersWaitForInputCapture--;
			buffer.clear();
		}
		
		/* when done: play sweep now, start input capture next block */
		if (buffersWaitForInputCapture <= 0){
			IRCapture.state = IRCAP_CAPTURE;
			IRCapture.playSweep = true;
		}
	}

	
	/*
	 * Play sweep
	 */
	
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
		}
	}

	
	/*
	 * Capture / sweep done: empty buffers before resuming throughput
	 */
	
	if (IRCapture.state == IRCAP_END) {
		buffersWaitForResumeThroughput--;

		if (buffersWaitForResumeThroughput > 0){
			buffer.clear();
		}
		else {
			IRCapture.state = IRCAP_IDLE;
			/* fade in on first buffer */
			tools::linearFade (&buffer, true, 0, buffer.getNumSamples());
		}
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
			
			/* always read an IR block */
			irFftBufferArray.getWriteBufferPtr()->clear();

			irFftBufferArray.getWriteBufferPtr()->copyFrom(0, 0, *IRtoConvolve, 0, irFftBufferArray.getWriteIndex() * processBlockSize, processBlockSize);
			
			fftIR->performRealOnlyForwardTransform(irFftBufferArray.getWriteBufferPtr()->getWritePointer(0, 0), true);
			
			irFftBufferArray.incrWriteIndex();
			
			
			
			/* DSP loop */
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
					audioFftBufferArray.decrReadIndex();
				}
				
				/* FDL method -> IFFT after summing */
				fftInverse->performRealOnlyInverseTransform(convResultPtr);
				
				/* add existing overlap and save new overlap */
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
	
	} // if (IRCapture.state == IRCAP_IDLE)  [aka not playing sweep / capturing]
	
		
		
	/* standard attenuation */
	buffer.applyGain(tools::dBToLin(outputVolumedB));
	
	
	/* makeshift limiter lol */
	if (tools::linTodB(buffer.getMagnitude(0, 0, generalHostBlockSize)) > 0.0){
		tools::normalize(&buffer, 0.0, false);
		DBG("processBlock limiter engaged\n");
	}
	
}




/* when plugin is bypassed, the latency of 1 processBlock should be maintained.
 * A CircularBufferArray is used for this.
 */
void IRBaboonAudioProcessor::processBlockBypassed(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
	bypassOutputBufferArray.getWriteBufferPtr()->makeCopyOf(buffer);
	bypassOutputBufferArray.incrWriteIndex();
	bypassOutputBufferArray.incrReadIndex();
	buffer.makeCopyOf(*bypassOutputBufferArray.getReadBufferPtr());
}




void IRBaboonAudioProcessor::startCapture(IRType type){
	if (type == IR_NONE) return;
	
	IRCapture.type = type;
	IRCapture.state = IRCAP_PREP;
	IRCapture.doFadeout = true;

	buffersWaitForInputCapture = samplesWaitBeforeInputCapture / processBlockSize;
}



AudioSampleBuffer IRBaboonAudioProcessor::createIR(AudioSampleBuffer& numeratorBuf, AudioSampleBuffer& denominatorBuf){
	return convolver::deconvolve(&numeratorBuf, &denominatorBuf, sampleRate);
}



void IRBaboonAudioProcessor::createIRFilt(){
	AudioSampleBuffer IR;
	
	IR.makeCopyOf(convolver::deconvolve(IRTargPtr->getBuffer(), IRBasePtr->getBuffer(), sampleRate, true, includePhaseFilt, includeAmplFilt));

	if (not includePhaseFilt) convolver::shifteroo(&IR);
	
	IRFiltPtr->getBuffer()->makeCopyOf(IR);
}


int IRBaboonAudioProcessor::getTotalSweepBreakSamples(){
	return totalSweepBreakSamples;
}

int IRBaboonAudioProcessor::getMakeupIRLengthSamples(){
	return makeupIRLengthSamples;
}

int IRBaboonAudioProcessor::getSamplerate(){
	return sampleRate;
}


bool IRBaboonAudioProcessor::filtReady(){
	return (IRFiltPtr->bufferNotEmpty());
}


AudioSampleBuffer IRBaboonAudioProcessor::getIRFilt(){
	return *IRFiltPtr->getBuffer();
}


void IRBaboonAudioProcessor::setPlayFiltered (bool filtered){
	playFiltered = filtered;
}



void IRBaboonAudioProcessor::run() {
	while (NOT threadShouldExit()) {
		
		saveCustomExt(saveIRCustomType);
		
		if (saveIRCustomType != IR_NONE){
			printDebug();
			saveIRCustomType = IR_NONE;
		}
		
		/* A negative timeout value means that the method will wait indefinitely (until notify() is called) */
		wait (-1);
	}
}



void IRBaboonAudioProcessor::saveCustomExt(IRType type){
	
	AudioSampleBuffer savebuf (2, totalSweepBreakSamples);
	std::string name = getDateTimeString();

	switch (type) {
		case IR_BASE:
			name += " IR Base";
			savebuf.copyFrom(0, 0, *(sweepBasePtr->getBuffer()), 0, 0, totalSweepBreakSamples);
			savebuf.copyFrom(1, 0, *(IRBasePtr->getBuffer()), 0, 0, totalSweepBreakSamples);
			break;

		case IR_TARGET:
			name += " IR Target";
			savebuf.copyFrom(0, 0, *(sweepTargPtr->getBuffer()), 0, 0, totalSweepBreakSamples);
			savebuf.copyFrom(1, 0, *(IRTargPtr->getBuffer()), 0, 0, totalSweepBreakSamples);
			break;

		default:
			return;
	}
	
	ParallelBufferPrinter wavPrinter;
	wavPrinter.appendBuffer(name, savebuf);
	wavPrinter.printToWav(0, wavPrinter.getMaxBufferLength(), sampleRate, printDirectorySavedIRs);
	
	/* replace .wav extension with custom extension */
	std::string path = printDirectorySavedIRs + name + ".wav";
	std::string pathCustomExtension = printDirectorySavedIRs + name + savedIRExtension;
	File f (path);
	File f2 (pathCustomExtension);
	f.moveFileTo(f2); // function to rename with
}



std::string IRBaboonAudioProcessor::getDateTimeString(){
	std::time_t now = std::time(NULL);
	std::tm* ptm = std::localtime(&now);
	char date[32];
	std::strftime(date, 32, "%Y-%m-%d %H%M%S", ptm);
	
	return date;
}



void IRBaboonAudioProcessor::printDebug() {
	ParallelBufferPrinter wavPrinter;
	ParallelBufferPrinter freqPrinter;

	/* dereference the buffers */
	AudioSampleBuffer sweepPrintTarg (*(sweepTargPtr->getBuffer()));
	AudioSampleBuffer IRPrintTarg (*(IRTargPtr->getBuffer()));
	AudioSampleBuffer sweepPrintBase (*(sweepBasePtr->getBuffer()));
	AudioSampleBuffer IRPrintBase (*(IRBasePtr->getBuffer()));
	AudioSampleBuffer IRPrintFilt (*(IRFiltPtr->getBuffer()));

	/* print freq */
	if (IRPrintTarg.getNumSamples() > 0) {
		AudioSampleBuffer IRrefprint_fft (convolver::fftTransform(IRPrintTarg));
		freqPrinter.appendBuffer("fft IR target", IRrefprint_fft);
	}
	if (IRPrintBase.getNumSamples() > 0) {
		AudioSampleBuffer IRcurrprint_fft (convolver::fftTransform(IRPrintBase));
		freqPrinter.appendBuffer("fft IR base", IRcurrprint_fft);
	}
	if (IRPrintFilt.getNumSamples() > 0) {
		AudioSampleBuffer IRinvfiltprint_fft (convolver::fftTransform(IRPrintFilt));
		freqPrinter.appendBuffer("fft IR filter", IRinvfiltprint_fft);
	}
	if (freqPrinter.getMaxBufferLength() > 0){
		freqPrinter.printFreqToCsv(sampleRate, printDirectoryDebug);
	}
	
	// include separate buffers in printer
	wavPrinter.appendBuffer(printNames[0], sweepPrintTarg);
	wavPrinter.appendBuffer(printNames[1], sweepPrintBase);
	wavPrinter.appendBuffer(printNames[2], IRPrintTarg);
	wavPrinter.appendBuffer(printNames[3], IRPrintBase);
	wavPrinter.appendBuffer(printNames[4], IRPrintFilt);
	
	// create target sweep & IR file for thumbnail
	AudioSampleBuffer thumbnailTarg (2, std::max(sweepPrintTarg.getNumSamples(), IRPrintTarg.getNumSamples()));
	thumbnailTarg.clear();
	if (sweepPrintTarg.getNumSamples() > 0)
		thumbnailTarg.copyFrom(0, 0, sweepPrintTarg, 0, 0, sweepPrintTarg.getNumSamples());
	if (IRPrintTarg.getNumSamples() > 0)
		thumbnailTarg.copyFrom(1, 0, IRPrintTarg, 0, 0, IRPrintTarg.getNumSamples());
	thumbnailTarg.applyGain(tools::dBToLin(zoomTargdB));
	wavPrinter.appendBuffer("thumbnailTarg", thumbnailTarg);
	
	// delete thumbnail file again if it has been swaped with empty current
	String nameTarg (printDirectoryDebug + "thumbnailTarg.wav");
	File fileTarg (nameTarg);
	if (thumbnailTarg.getNumSamples() == 0 && fileTarg.existsAsFile()){
		fileTarg.deleteFile();
	}

	// create current sweep & IR file for thumbnail
	AudioSampleBuffer currentForThumbnail (2, std::max(sweepPrintBase.getNumSamples(), IRPrintBase.getNumSamples()));
	currentForThumbnail.clear();
	if (sweepPrintBase.getNumSamples() > 0)
		currentForThumbnail.copyFrom(0, 0, sweepPrintBase, 0, 0, sweepPrintBase.getNumSamples());
	if (IRPrintBase.getNumSamples() > 0)
		currentForThumbnail.copyFrom(1, 0, IRPrintBase, 0, 0, IRPrintBase.getNumSamples());
	currentForThumbnail.applyGain(tools::dBToLin(zoomBasedB));
	wavPrinter.appendBuffer("thumbnailBase", currentForThumbnail);

	// delete thumbnail file again if it has been swaped with empty reference
	String nameCurrent (printDirectoryDebug + "thumbnailBase.wav");
	File fileCurrent (nameCurrent);
	if (currentForThumbnail.getNumSamples() == 0 && fileCurrent.existsAsFile()){
		fileCurrent.deleteFile();
	}

	// create makeup IR file for thumbnail
	AudioSampleBuffer InvfiltForThumbnail (1, IRPrintFilt.getNumSamples());
	if (IRPrintFilt.getNumSamples() > 0)
		InvfiltForThumbnail.copyFrom(0, 0, IRPrintFilt, 0, 0, IRPrintFilt.getNumSamples());
	InvfiltForThumbnail.applyGain(tools::dBToLin(zoomFiltdB));
	wavPrinter.appendBuffer("thumbnailFilt", InvfiltForThumbnail);
	
	// print everything
	wavPrinter.printToWav(0, wavPrinter.getMaxBufferLength(), sampleRate, printDirectoryDebug);
}


std::string IRBaboonAudioProcessor::getPrintDirectoryDebug(){
	return printDirectoryDebug;
}


std::string IRBaboonAudioProcessor::getSavedIRExtension(){
	return savedIRExtension;
}


void IRBaboonAudioProcessor::setZoomTarg(float dB){
	zoomTargdB = dB;
	notify();
}


void IRBaboonAudioProcessor::setZoomBase(float dB){
	zoomBasedB = dB;
	notify();
}


void IRBaboonAudioProcessor::setZoomFilt(float dB){
	zoomFiltdB = dB;
	notify();
}


void IRBaboonAudioProcessor::setOutputVolume(float dB){
	outputVolumedB = dB;
	tools::normalize(&sweepBufForDeconv, outputVolumedB + sweepLeveldB);
}


void IRBaboonAudioProcessor::setPhaseFilt(bool includePhase){
	includePhaseFilt = includePhase;
	
	if (IRTargPtr->bufferNotEmpty() && IRBasePtr->bufferNotEmpty()){
		createIRFilt();
	}
	
	notify();
}


void IRBaboonAudioProcessor::setAmplFilt(bool includeAmplitude){
	includeAmplFilt = includeAmplitude;
	
	if (IRTargPtr->bufferNotEmpty() && IRBasePtr->bufferNotEmpty()){
		createIRFilt();
	}
	
	notify();
}


void IRBaboonAudioProcessor::setPresweepSilence(int presweepSilence){
	samplesWaitBeforeInputCapture = presweepSilence;
}


void IRBaboonAudioProcessor::setMakeupSize(int makeupSize){
	makeupIRLengthSamples = makeupSize;
	
	if (IRTargPtr->bufferNotEmpty() && IRBasePtr->bufferNotEmpty()){
		createIRFilt();
	}

	notify();
}


void IRBaboonAudioProcessor::swapTargetBase(){
	
	AudioSampleBuffer savedSweep (*(sweepTargPtr->getBuffer()));
	AudioSampleBuffer savedIR (*(IRTargPtr->getBuffer()));
	sweepTargPtr->getBuffer()->makeCopyOf(*(sweepBasePtr->getBuffer()));
	IRTargPtr->getBuffer()->makeCopyOf(*(IRBasePtr->getBuffer()));
	sweepBasePtr->getBuffer()->makeCopyOf(savedSweep);
	IRBasePtr->getBuffer()->makeCopyOf(savedIR);

	/* generate new makeup */
	if (IRTargPtr->bufferNotEmpty() && IRBasePtr->bufferNotEmpty()){
		createIRFilt();
	}
	
	/* reprint */
	notify();
}


void IRBaboonAudioProcessor::loadTarget(File file){
	
	/* rename custom extension to .wav */
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
	if (IRTargPtr->bufferNotEmpty() && IRBasePtr->bufferNotEmpty()){
		createIRFilt();
	}
	
	// rerename wav
	boost::filesystem::rename(pathWav, pathCustomExtension);
	
	// reprint
	notify();
}



//==============================================================================
bool IRBaboonAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* IRBaboonAudioProcessor::createEditor()
{
    return new IRBaboonAudioProcessorEditor (*this);
}

//==============================================================================
void IRBaboonAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void IRBaboonAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IRBaboonAudioProcessor();
}
