/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"



double DecibelSlider::getValueFromText (const String& text){
	double minusInfinitydB = -100.0;
	String decibelText = text.upToFirstOccurrenceOf ("dB", false, false).trim();
	return decibelText.equalsIgnoreCase ("-INF") ? minusInfinitydB
	: decibelText.getDoubleValue();
}

String DecibelSlider::getTextFromValue (double value){
	return Decibels::toString (value);
}



//==============================================================================
AutoKalibraDemoAudioProcessorEditor::AutoKalibraDemoAudioProcessorEditor (AutoKalibraDemoAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
		thumbnailReferenceCache (1),
		thumbnailCurrentCache (1),
		thumbnailInvfiltCache (1),
		thumbnailReference (processor.getTotalSweepBreakSamples(), formatManagerReference, thumbnailReferenceCache),
		thumbnailCurrent (processor.getTotalSweepBreakSamples(), formatManagerCurrent, thumbnailCurrentCache),
		thumbnailInvfilt (32768, formatManagerInvfilt, thumbnailInvfiltCache)
{
	

    setSize (750, 750);
	
	
	/* capture & play buttons */
	addAndMakeVisible (&captureReferenceButton);
	addAndMakeVisible (&captureCurrentButton);
	addAndMakeVisible (&captureThdnButton);
	addAndMakeVisible (&playUnprocessedAudioButton);
	addAndMakeVisible (&playInvFiltAudioButton);
	
	playInvFiltAudioButton.setColour(TextButton::buttonColourId, Colour (0xff805705));
	playInvFiltAudioButton.setColour(TextButton::buttonOnColourId, Colour (0xff805705));
	playInvFiltAudioButton.setVisible(false);

	captureReferenceButton.setColour(TextButton::buttonColourId, Colour (0xff365936));
	captureReferenceButton.onClick = [this] { setStartCaptureReference(); };
	captureCurrentButton.setColour(TextButton::buttonColourId, Colour (0xff602743));
	captureCurrentButton.onClick = [this] { setStartCaptureCurrent(); };
	captureThdnButton.setColour(TextButton::buttonColourId, Colour (0xff27605D));
	captureThdnButton.onClick = [this] { setStartCaptureThdn(); };

	playUnprocessedAudioButton.setClickingTogglesState(true);
	playInvFiltAudioButton.setClickingTogglesState(true);
	
	playUnprocessedAudioButton.onClick = [this] { playUnprocessedAudioClick(playUnprocessedAudioButton.getToggleState()); };
	playInvFiltAudioButton.onClick = [this] { playInvfiltAudioClick(playInvFiltAudioButton.getToggleState()); };
	
	playUnprocessedAudioButton.setRadioGroupId(playButtons);
	playInvFiltAudioButton.setRadioGroupId(playButtons);
	
	
	// ghetto polling solution for checking whether IRinvfilt is ready in Processor
	startTimerHz(8);
	
	
	
	// thumbnails
	formatManagerReference.registerBasicFormats();
	formatManagerCurrent.registerBasicFormats();
	formatManagerInvfilt.registerBasicFormats();
	thumbnailReference.addChangeListener (this);
	thumbnailCurrent.addChangeListener (this);
	thumbnailInvfilt.addChangeListener (this);
	
	Font labelFont (12);
	Justification justification (Justification::Flags::centred);
	
	addAndMakeVisible (&sweepRefLabel);
	sweepRefLabel.setText("Target\nsweep & IR", dontSendNotification);
	sweepRefLabel.setFont(labelFont);
	sweepRefLabel.setJustificationType(justification);

	addAndMakeVisible (&sweepCurrLabel);
	sweepCurrLabel.setText("Base\nsweep & IR", dontSendNotification);
	sweepCurrLabel.setFont(labelFont);
	sweepCurrLabel.setJustificationType(justification);

	addAndMakeVisible (&IRinvfiltLabel);
	IRinvfiltLabel.setText("Filter\nIR", dontSendNotification);
	IRinvfiltLabel.setFont(labelFont);
	IRinvfiltLabel.setJustificationType(justification);

	
	addAndMakeVisible (&titleLabel);
	titleLabel.setText("Felix Postma 2020", dontSendNotification);
	titleLabel.setJustificationType(justification);
	labelFont.setStyleFlags(Font::italic);
	labelFont.setHeight(14.0);
	labelFont.setExtraKerningFactor(0.1);
	titleLabel.setFont(labelFont);
	titleLabel.setColour(Label::textColourId, Colour (0xffb2b5b8));

	
	
	
	// sliders
	addAndMakeVisible(&referenceZoomSlider);
	referenceZoomSlider.setRange(-6, 60);
	referenceZoomSlider.setValue(0.0);
	referenceZoomSlider.onValueChange = [this] { processor.setZoomTarg(referenceZoomSlider.getValue()); };

	addAndMakeVisible(referenceZoomSliderLabel);
	referenceZoomSliderLabel.setText("Target zoom [dB]", dontSendNotification);
	referenceZoomSliderLabel.attachToComponent(&referenceZoomSlider, true);
	
	addAndMakeVisible(&currentZoomSlider);
	currentZoomSlider.setRange(-6, 60);
	currentZoomSlider.setValue(0.0);
	currentZoomSlider.onValueChange = [this] { processor.setZoomBase(currentZoomSlider.getValue()); };
	
	addAndMakeVisible(currentZoomSliderLabel);
	currentZoomSliderLabel.setText("Base zoom [dB]", dontSendNotification);
	currentZoomSliderLabel.attachToComponent(&currentZoomSlider, true);
	
	addAndMakeVisible(&InvfiltZoomSlider);
	InvfiltZoomSlider.setRange(-60, 36);
	InvfiltZoomSlider.setValue(0.0);
	InvfiltZoomSlider.onValueChange = [this] { processor.setZoomFilt(InvfiltZoomSlider.getValue()); };
	
	addAndMakeVisible(InvfiltZoomSliderLabel);
	InvfiltZoomSliderLabel.setText("Filter zoom [dB]", dontSendNotification);
	InvfiltZoomSliderLabel.attachToComponent(&InvfiltZoomSlider, true);

	addAndMakeVisible(&outputVolumeSlider);
	outputVolumeSlider.setRange(-60, -20);
	outputVolumeSlider.setValue(-30.0);
	outputVolumeSlider.onValueChange = [this] { processor.setOutputVolume(outputVolumeSlider.getValue()); };

	addAndMakeVisible(outputVolumeSliderLabel);
	outputVolumeSliderLabel.setText("Output volume [dB]", dontSendNotification);
	outputVolumeSliderLabel.attachToComponent(&outputVolumeSlider, true);

	
	// bottom buttons
	addAndMakeVisible(&toggleButtonLabel);
	toggleButtonLabel.setText("Filter:", dontSendNotification);
	toggleButtonLabel.attachToComponent(&nullifyAmplitudeButton, true);
	
	addAndMakeVisible (&nullifyPhaseButton);
	nullifyPhaseButton.setToggleState(true, dontSendNotification);
	nullifyPhaseButton.onClick = [this] { processor.setNullifyPhaseFilt(!nullifyPhaseButton.getToggleState()); }; // NEGATIVE because GUI implies activation, but functions are implemented as 'nullify'... code needs to be more clear
	
	addAndMakeVisible(&nullifyAmplitudeButton);
	nullifyAmplitudeButton.setToggleState(true, dontSendNotification);
	nullifyAmplitudeButton.onClick = [this] { processor.setNullifyAmplFilt(!nullifyAmplitudeButton.getToggleState()); }; // NEGATIVE because GUI implies activation, but functions are implemented as 'nullify'... code needs to be more clear
	
	addAndMakeVisible(&makeupSizeMenu);
	makeupSizeMenu.addItem("128", 1);
	makeupSizeMenu.addItem("256", 2);
	makeupSizeMenu.addItem("512", 3);
	makeupSizeMenu.addItem("1024", 4);
	makeupSizeMenu.addItem("2048", 5);
	makeupSizeMenu.addItem("4096", 6);
	makeupSizeMenu.addItem("8192", 7);
	makeupSizeMenu.addItem("16384", 8);
	makeupSizeMenu.addItem("32768", 9);
	makeupSizeMenu.setText("2048");
	makeupSizeMenu.onChange = [this] { makeupSizeMenuChanged(); };
	makeupSizeMenu.setVisible(false);

	
	addAndMakeVisible (&swapButton);
	swapButton.onClick = [this] { processor.swapTargetBase(); };
	
	addAndMakeVisible (&loadTargetButton);
	loadTargetButton.onClick = [this] { loadTargetClicked(); };
}

AutoKalibraDemoAudioProcessorEditor::~AutoKalibraDemoAudioProcessorEditor()
{
}



void AutoKalibraDemoAudioProcessorEditor::setStartCaptureReference(){
	processor.startCaptureTarg();
	captureCurrentButton.setVisible(false);
	captureThdnButton.setVisible(false);
	int ms = std::ceil( 1000 * ((float) processor.getTotalSweepBreakSamples()) / ((float) processor.getSamplerate()) );
	Timer::callAfterDelay(ms, [this] { captureCurrentButton.setVisible(true); } );
	Timer::callAfterDelay(ms, [this] { captureThdnButton.setVisible(true); } );
}

void AutoKalibraDemoAudioProcessorEditor::setStartCaptureCurrent(){
	processor.startCaptureBase();
	captureReferenceButton.setVisible(false);
	captureThdnButton.setVisible(false);
	int ms = std::ceil( 1000 * ((float) processor.getTotalSweepBreakSamples()) / ((float) processor.getSamplerate()) );
	Timer::callAfterDelay(ms, [this] { captureReferenceButton.setVisible(true); } );
	Timer::callAfterDelay(ms, [this] { captureThdnButton.setVisible(true); } );
}

void AutoKalibraDemoAudioProcessorEditor::setStartCaptureThdn(){
	processor.startCaptureThdn();
	captureReferenceButton.setVisible(false);
	captureCurrentButton.setVisible(false);
	int ms = std::ceil( 1000 * ((float) processor.getTotalSweepBreakSamples()) / ((float) processor.getSamplerate()) );
	Timer::callAfterDelay(ms, [this] { captureReferenceButton.setVisible(true); } );
	Timer::callAfterDelay(ms, [this] { captureCurrentButton.setVisible(true); } );
}


void AutoKalibraDemoAudioProcessorEditor::timerCallback(){
	// invfilt button
	if (processor.filtReady()){
		playInvFiltAudioButton.setVisible(true);
		makeupSizeMenu.setVisible(true);
	}

	
	// thumbnails
	String nameReference (processor.getPrintDirectoryDebug() + "/targetForThumbnail.wav");
	File fileReference (nameReference);
	if (fileReference.existsAsFile()){
		thumbnailReference.setSource(new FileInputSource(fileReference));
	}
	else {
		thumbnailReference.setSource(nullptr);
	}
	
	String nameCurrent (processor.getPrintDirectoryDebug() + "/baseForThumbnail.wav");
	File fileCurrent (nameCurrent);
	if (fileCurrent.existsAsFile()){
		thumbnailCurrent.setSource(new FileInputSource(fileCurrent));
	}
	else {
		thumbnailCurrent.setSource(nullptr);
	}
	
	String nameInvfilt (processor.getPrintDirectoryDebug() + "/filterForThumbnail.wav");
	File fileInvfilt (nameInvfilt);
	if (fileInvfilt.existsAsFile()){
		thumbnailInvfilt.setSource(new FileInputSource(fileInvfilt));
	}
	
}


void AutoKalibraDemoAudioProcessorEditor::playUnprocessedAudioClick (bool toggleState){
	processor.setPlayFiltered(false);
	
	if (toggleState) playUnprocessedAudioButton.setButtonText("PLAYING unprocessed audio");
	if (!toggleState) playUnprocessedAudioButton.setButtonText("Play unprocessed audio");
}



void AutoKalibraDemoAudioProcessorEditor::playInvfiltAudioClick (bool toggleState){
	processor.setPlayFiltered(true);

	if (toggleState) {
		playInvFiltAudioButton.setButtonText("PLAYING filtered audio");
		playUnprocessedAudioButton.setButtonText("Play unprocessed audio");
	}
	if (!toggleState) playInvFiltAudioButton.setButtonText("Play filtered audio");
}



void AutoKalibraDemoAudioProcessorEditor::loadTargetClicked(){
	FileChooser myChooser ("Please select the sweepandir you want to load...",
						   File("/Users/flixor/Documents/Super MakeupFilteraarder 2027/Saved IRs"),
						   String("*" + processor.getSavedIRExtension()));
	if (myChooser.browseForFileToOpen()) {
		processor.loadTarget(myChooser.getResult());
	}
}


void AutoKalibraDemoAudioProcessorEditor::makeupSizeMenuChanged(){
	
	switch (makeupSizeMenu.getSelectedId()){
		case 1: makeupSize = 128;	break;
		case 2: makeupSize = 256;	break;
		case 3: makeupSize = 512;	break;
		case 4: makeupSize = 1024;	break;
		case 5: makeupSize = 2048;	break;
		case 6: makeupSize = 4096;	break;
		case 7: makeupSize = 8192;	break;
		case 8: makeupSize = 16384;	break;
		case 9: makeupSize = 32768;	break;
	}
	
	processor.setMakeupSize(makeupSize);
	
	thumbnailInvfilt.reset(1, processor.getSamplerate(), makeupSize);
	
}


void AutoKalibraDemoAudioProcessorEditor::changeListenerCallback(ChangeBroadcaster* source){
	repaint();
}

//==============================================================================



//==============================================================================
void AutoKalibraDemoAudioProcessorEditor::paint (Graphics& g)
{
	
	// Target thumbnail
	
	Rectangle<int> thumbnailReferenceBounds (thumbnailSideMargin*3,
										  buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin,
										  getWidth() - thumbnailSideMargin*4,
										  thumbnailHeight*2);
	
	if (thumbnailReference.getNumChannels() == 0){
		g.setColour (Colours::black);
		g.fillRect (thumbnailReferenceBounds);
		g.setColour (Colours::white);
		g.drawFittedText ("No target captured yet", thumbnailReferenceBounds, Justification::centred, 1.0f);
	}
	else {
		g.setColour (Colours::black);
		g.fillRect (thumbnailReferenceBounds);
		g.setColour (Colour (0xff90ee90));
		auto audioLength = (float) thumbnailReference.getTotalLength();
		thumbnailReference.drawChannels (g, thumbnailReferenceBounds, 0.0, audioLength, 1.0f);
	}
	
	sweepRefLabel.setBounds(0,
							buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin + thumbnailHeight*0.5,
							thumbnailSideMargin*3 - 5,
							thumbnailHeight);


	
	// Current thumbnail
	
	Rectangle<int> thumbnailCurrentBounds (thumbnailSideMargin*3,
											 buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin + thumbnailHeight*2,
											 getWidth() - thumbnailSideMargin*4,
											 thumbnailHeight*2);
	
	if (thumbnailCurrent.getNumChannels() == 0){
		g.setColour (Colours::black);
		g.fillRect (thumbnailCurrentBounds);
		g.setColour (Colours::white);
		g.drawFittedText ("No base captured yet", thumbnailCurrentBounds, Justification::centred, 1.0f);
	}
	else {
		g.setColour (Colours::black);
		g.fillRect (thumbnailCurrentBounds);
		g.setColour (Colour (0xffff69b4));
		auto audioLength = (float) thumbnailCurrent.getTotalLength();
		thumbnailCurrent.drawChannels (g, thumbnailCurrentBounds, 0.0, audioLength, 1.0f);
	}

	sweepCurrLabel.setBounds(0,
							 buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin + thumbnailHeight*2.5,
							 thumbnailSideMargin*3 - 5,
							 thumbnailHeight);


	
	// Invfilt thumbnail
	
	Rectangle<int> thumbnailInvfiltBounds (thumbnailSideMargin*3,
									   buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin + thumbnailHeight*4,
									   getWidth() - thumbnailSideMargin*4,
									   thumbnailHeight);

	if (thumbnailInvfilt.getNumChannels() == 0){
		g.setColour (Colours::black);
		g.fillRect (thumbnailInvfiltBounds);
		g.setColour (Colours::white);
		g.drawFittedText ("No filter available yet", thumbnailInvfiltBounds, Justification::centred, 1.0f);
	}
	else {
		g.setColour (Colours::black);
		g.fillRect (thumbnailInvfiltBounds);
		g.setColour (Colour (0xffffae0b));
		thumbnailInvfilt.drawChannels (g, thumbnailInvfiltBounds, 0.0, (double) makeupSize / (double) processor.getSamplerate(), 1.0f);
	}

	IRinvfiltLabel.setBounds(0,
						  buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin + thumbnailHeight*4,
						  thumbnailSideMargin*3 - 5,
						  thumbnailHeight);

	
}




void AutoKalibraDemoAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
	
	
	// top buttons
	captureReferenceButton.setBounds 	(0,
										0,
										 getLocalBounds().getWidth()/3,
										 buttonHeight);
	captureCurrentButton.setBounds 		(getLocalBounds().getWidth()/3,
										 0,
										 getLocalBounds().getWidth()/3,
										 buttonHeight);
	captureThdnButton.setBounds 		(getLocalBounds().getWidth()*2/3,
										 0,
										 getLocalBounds().getWidth()/3,
										 buttonHeight);
	playUnprocessedAudioButton.setBounds (0,
										  buttonHeight,
										  getLocalBounds().getWidth()/2,
										  buttonHeight);
	playInvFiltAudioButton.setBounds (getLocalBounds().getWidth()/2,
									  buttonHeight,
									  getLocalBounds().getWidth()/2,
									  buttonHeight);
	
	// title label
	titleLabel.setBounds(0,
						 buttonHeight*2 + sliderHeight + sliderHeightMargin,
						 getLocalBounds().getWidth(),
						 thumbnailHeightMargin);
	
	// output volume slider
	outputVolumeSlider.setBounds(labelOffset,
								 2*buttonHeight + sliderHeightMargin,
								 getWidth() - labelOffset,
								 sliderHeight);
	
	
	// zoom sliders
	referenceZoomSlider.setBounds	(labelOffset,
								 2*buttonHeight + 5*thumbnailHeight + thumbnailHeightMargin + 2*sliderHeightMargin + sliderHeight,
								 getWidth() - labelOffset,
								 sliderHeight);
	currentZoomSlider.setBounds	(labelOffset,
								 2*buttonHeight + 5*thumbnailHeight + thumbnailHeightMargin + 3*sliderHeightMargin + 2*sliderHeight,
								 getWidth() - labelOffset,
								 sliderHeight);
	InvfiltZoomSlider.setBounds(labelOffset,
								 2*buttonHeight + 5*thumbnailHeight + thumbnailHeightMargin + 4*sliderHeightMargin + 3*sliderHeight,
								 getWidth() - labelOffset,
								 sliderHeight);


	// bottom buttons
	nullifyAmplitudeButton.setBounds(getLocalBounds().getWidth() * 2/12,
								 getLocalBounds().getHeight() - buttonHeight,
								 getLocalBounds().getWidth() * 1/12,
								 buttonHeight);
	nullifyPhaseButton.setBounds(getLocalBounds().getWidth() * 3/12 ,
								 getLocalBounds().getHeight() - buttonHeight,
								 getLocalBounds().getWidth() * 1/12 + 10,
								 buttonHeight);
	makeupSizeMenu.setBounds(getLocalBounds().getWidth() * 4/12 + 10,
							 getLocalBounds().getHeight() - buttonHeight,
							 getLocalBounds().getWidth() * 1/8,
							 buttonHeight);
	swapButton.setBounds(getLocalBounds().getWidth()*2/4,
						 getLocalBounds().getHeight() - buttonHeight,
						 getLocalBounds().getWidth()/4,
						 buttonHeight);
	loadTargetButton.setBounds(getLocalBounds().getWidth()*3/4,
						 getLocalBounds().getHeight() - buttonHeight,
						 getLocalBounds().getWidth()/4,
						 buttonHeight);
}









