//
//  Felix Postma 2021
//


#include "PluginProcessor.h"
#include "PluginEditor.h"



IRBaboonAudioProcessor::IRBaboonAudioProcessor()
     : AudioProcessor (BusesProperties()
					   .withInput  ("Input",  AudioChannelSet::stereo(), true)
					   .withOutput ("Output", AudioChannelSet::stereo(), true)
					   .withInput  ("Mic",  AudioChannelSet::mono(), true)
                       ),
		Thread ("Print and thumbnail"),
		convolver(Convolver::ProcessBlockSize::Size256)
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
	IRpulse.makeCopyOf(tools::generatePulse(IRFiltSize, 100));
	
	IRTargPtr = new ReferenceCountedBuffer("IRTarg", 0, 0);
	IRBasePtr = new ReferenceCountedBuffer("IRBase", 0, 0);
	IRFiltPtr = new ReferenceCountedBuffer("IRFilt", 0, 0);
	
	sweepTargPtr = new ReferenceCountedBuffer("sweepref", 0, 0);
	sweepBasePtr = new ReferenceCountedBuffer("sweepcurr", 0, 0);

	
	
	// ======= convolution prep ========
	
	// TODO: necessary here, calls in prepareToPlay() are not enough?
	setLatencySamples(convolver.getProcessBlockSize());
	
	// Thread
	startThread();
	
}

IRBaboonAudioProcessor::~IRBaboonAudioProcessor()
{	
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
	hostBlockSize = samplesPerBlock;
	
	if (hostBlockSize > convolver.getProcessBlockSize()) {
		setLatencySamples(hostBlockSize);
	}
	else {
		setLatencySamples(convolver.getProcessBlockSize());
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
		
	int sweepBufArraySize = sweepBuf.getNumSamples() / hostBlockSize;
	sweepBufArray.clearAndResize(sweepBufArraySize, 1, hostBlockSize);
	
	for (int buf = 0; buf < sweepBufArraySize; buf++){
		sweepBufArray.getWriteBufferPtr()->copyFrom(0, 0, sweepBuf, 0, buf * hostBlockSize, hostBlockSize);
		sweepBufArray.incrWriteIndex();
	}

	
	/* input capture array is the same size as the sweep array */
	inputCaptureArray.clearAndResize(sweepBufArray.getArraySize(), 1, hostBlockSize);
	

	/* prepare convolution */ 
	convolver.prepare(hostBlockSize);


	int outputArraySize = tools::nextPowerOfTwo(std::ceil((float) convolver.getProcessBlockSize() / (float) hostBlockSize) + 1);
	bypassOutputBufArray.clearAndResize(outputArraySize, generalInputAudioChannels, hostBlockSize);

}





void IRBaboonAudioProcessor::releaseResources()
{
	convolver.releaseResources();
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

	
	// int currentHostBlockSize = buffer.getNumSamples();
	int currentHostBlockSize = hostBlockSize;
	
	if (currentHostBlockSize == 0){
		return;
	}
	
	AudioSampleBuffer micBuffer = getBusBuffer(buffer, true, 1); // second buffer block = mic input
	
	/*
	 * Capture input
	 */

	if (IRCapture.state == IRCapState::Capture){
		
		inputCaptureArray.getWriteBufferPtr()->copyFrom(0, 0, micBuffer, 0, 0, hostBlockSize);
		inputCaptureArray.incrWriteIndex();
		
		/* when capture is done */
		if (inputCaptureArray.getWriteIndex() == 0){

			/* there is a random buffer offset between sweep play and input capture. (what is the host doing...?)
			 * it's nice if IRFilt doesn't fold back though, when sweepTarg happens before sweepBase.
			 * a 3 buffers-to-the-right offset was used before, but actually 0 seems to work fine now too.
			 */
			
			if (IRCapture.type == IRType::Target) {
				sweepTargPtr->getBuffer()->makeCopyOf(inputCaptureArray.consolidate(0));
				IRTargPtr->getBuffer()->makeCopyOf(convolution::deconvolve(sweepTargPtr->getBuffer(), &sweepBufForDeconv, sampleRate));

				saveIRCustomType = IRType::Target;
			}
			else if (IRCapture.type == IRType::Base) {
				sweepBasePtr->getBuffer()->makeCopyOf(inputCaptureArray.consolidate(0));
				IRBasePtr->getBuffer()->makeCopyOf(convolution::deconvolve(sweepBasePtr->getBuffer(), &sweepBufForDeconv, sampleRate));

				saveIRCustomType = IRType::Base;
			}
			

			IRCapture.type = IRType::None;

			/* create filter if both target and base have been captured */
			if (IRTargPtr->bufferNotEmpty() && IRBasePtr->bufferNotEmpty()){
				createIRFilt();
			}
			
			IRCapture.state = IRCapState::End;
			
			/* run print thumbnail thread */
			notify();
			
		} // when capture is done
		
	} // if (IRCapture.state == IRCapState::Capture)

	
	/*
	 * Play empty buffers before starting sweep / capture
	 */
	
	if (IRCapture.state == IRCapState::Prep) {
	
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
			IRCapture.state = IRCapState::Capture;
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
			for (int sample = 0; sample < hostBlockSize; sample++){
				buffer.setSample(channel, sample, *(readPtr + sample));
			}
		}
		sweepBufArray.incrReadIndex();
		
		/* when sweep is done */
		if (sweepBufArray.getReadIndex() == 0){
			IRCapture.playSweep = false;
			buffersWaitForResumeThroughput = samplesWaitBeforeInputCapture / currentHostBlockSize;
		}
	}

	
	/*
	 * Capture / sweep done: empty buffers before resuming throughput
	 */
	
	if (IRCapture.state == IRCapState::End) {
		buffersWaitForResumeThroughput--;

		if (buffersWaitForResumeThroughput > 0){
			buffer.clear();
		}
		else {
			IRCapture.state = IRCapState::Idle;
			/* fade in on first buffer */
			tools::linearFade (&buffer, true, 0, buffer.getNumSamples());
		}
	}
	

	
	
	
	// ===============================
	// convolution
	// ===============================
	
	
	/* not capturing/playing sweep */
	if (IRCapture.state == IRCapState::Idle) {

		/* set IR to convolve with */
		// if (playFiltered)
		// 	IRtoConvolve = IRFiltPtr->getBuffer();
		// else
		// 	IRtoConvolve = &IRpulse;
		if (playFiltered){
			AudioBuffer<float> filtje (*IRFiltPtr->getBuffer());
			convolver.setIR(filtje);
		}
		else
			convolver.setIR(IRpulse);
		

		convolver.inputSamples(buffer);

		buffer = convolver.getOutput();
	
	} 
	
		
		
	/* standard attenuation */
	buffer.applyGain(tools::dBToLin(outputVolumedB));
	
	
	/* makeshift limiter lol */
	if (tools::linTodB(buffer.getMagnitude(0, 0, hostBlockSize)) > 0.0){
		tools::normalize(&buffer, 0.0, false);
		DBG("processBlock limiter engaged\n");
	}
	
}




/* when plugin is bypassed, the latency of 1 processBlock should be maintained.
 * A CircularBufferArray is used for this.
 */
void IRBaboonAudioProcessor::processBlockBypassed(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
	bypassOutputBufArray.getWriteBufferPtr()->makeCopyOf(buffer);
	bypassOutputBufArray.incrWriteIndex();
	bypassOutputBufArray.incrReadIndex();
	buffer.makeCopyOf(*bypassOutputBufArray.getReadBufferPtr());
}



void IRBaboonAudioProcessor::startCapture(IRType type){
	if (type == IRType::None) return;
	
	IRCapture.type = type;
	IRCapture.state = IRCapState::Prep;
	IRCapture.doFadeout = true;

	buffersWaitForInputCapture = samplesWaitBeforeInputCapture / convolver.getProcessBlockSize();
}



void IRBaboonAudioProcessor::createIRFilt(){
	
	IRFiltPtr->getBuffer()->makeCopyOf(convolution::deconvolve(IRTargPtr->getBuffer(),
															   IRBasePtr->getBuffer(),
															   sampleRate,
															   true,
															   includePhaseFilt,
															   includeAmplFilt)
									   );
}


int IRBaboonAudioProcessor::getSamplerate(){
	return sampleRate;
}

int IRBaboonAudioProcessor::getNumInputChannels(){
	return generalInputAudioChannels;
}

int IRBaboonAudioProcessor::getTotalSweepBreakSamples(){
	return totalSweepBreakSamples;
}

int IRBaboonAudioProcessor::getIRFiltSize(){
	return IRFiltSize;
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
		
		/* always print debug tsv and wav and thumbnails */
		printDebug();
		printThumbnails();
		
		/* A negative timeout value means that the method will wait indefinitely (until notify() is called) */
		wait (-1);
	}
}


/* saves with .sweepir extension */
void IRBaboonAudioProcessor::saveCustomExt(IRType type){
	
	AudioSampleBuffer savebuf (2, totalSweepBreakSamples);
	std::string name = getDateTimeString();

	switch (type) {
		case IRType::Base:
			name += " IR Base";
			savebuf.copyFrom(0, 0, *(sweepBasePtr->getBuffer()), 0, 0, totalSweepBreakSamples);
			savebuf.copyFrom(1, 0, *(IRBasePtr->getBuffer()), 0, 0, totalSweepBreakSamples);
			break;

		case IRType::Target:
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
		AudioSampleBuffer IRrefprint_fft (tools::fftTransform(IRPrintTarg));
		freqPrinter.appendBuffer("fft IR target", IRrefprint_fft);
	}
	if (IRPrintBase.getNumSamples() > 0) {
		AudioSampleBuffer IRcurrprint_fft (tools::fftTransform(IRPrintBase));
		freqPrinter.appendBuffer("fft IR base", IRcurrprint_fft);
	}
	if (IRPrintFilt.getNumSamples() > 0) {
		AudioSampleBuffer IRinvfiltprint_fft (tools::fftTransform(IRPrintFilt));
		freqPrinter.appendBuffer("fft IR filter", IRinvfiltprint_fft);
	}
	if (freqPrinter.getMaxBufferLength() > 0){
		freqPrinter.printFreqToCsv(sampleRate, printDirectoryDebug);
	}
	
	/* include separate buffers in printer */
	wavPrinter.appendBuffer(printNames[0], sweepPrintTarg);
	wavPrinter.appendBuffer(printNames[1], sweepPrintBase);
	wavPrinter.appendBuffer(printNames[2], IRPrintTarg);
	wavPrinter.appendBuffer(printNames[3], IRPrintBase);
	wavPrinter.appendBuffer(printNames[4], IRPrintFilt);
	
	/* print everything */
	wavPrinter.printToWav(0, wavPrinter.getMaxBufferLength(), sampleRate, printDirectoryDebug);
}


void IRBaboonAudioProcessor::printThumbnails() {
	ParallelBufferPrinter wavPrinter;

	/* dereference the buffers */
	AudioSampleBuffer sweepPrintTarg (*(sweepTargPtr->getBuffer()));
	AudioSampleBuffer IRPrintTarg (*(IRTargPtr->getBuffer()));
	AudioSampleBuffer sweepPrintBase (*(sweepBasePtr->getBuffer()));
	AudioSampleBuffer IRPrintBase (*(IRBasePtr->getBuffer()));
	AudioSampleBuffer IRPrintFilt (*(IRFiltPtr->getBuffer()));
	
	// create target sweep & IR file for thumbnail
	AudioSampleBuffer thumbnailTarg (2, std::max(sweepPrintTarg.getNumSamples(), IRPrintTarg.getNumSamples()));
	thumbnailTarg.clear();
	if (sweepPrintTarg.getNumSamples() > 0)
		thumbnailTarg.copyFrom(0, 0, sweepPrintTarg, 0, 0, sweepPrintTarg.getNumSamples());
	if (IRPrintTarg.getNumSamples() > 0)
		thumbnailTarg.copyFrom(1, 0, IRPrintTarg, 0, 0, IRPrintTarg.getNumSamples());
	thumbnailTarg.applyGain(tools::dBToLin(zoomTargdB));
	wavPrinter.appendBuffer("thumbnailTarg", thumbnailTarg);
	
	// delete thumbnail file again if it has been swapped with empty current
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
	
	// delete thumbnail file again if it has been swapped with empty reference
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


void IRBaboonAudioProcessor::setIRFiltSize(int IRFiltSize){
	IRFiltSize = IRFiltSize;
	
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
	
	/* generate new makeup */
	if (IRTargPtr->bufferNotEmpty() && IRBasePtr->bufferNotEmpty()){
		createIRFilt();
	}
	
	/* rerename wav */
	boost::filesystem::rename(pathWav, pathCustomExtension);
	
	/* reprint */
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
