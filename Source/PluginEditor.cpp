//
//  Copyright © 2020 Felix Postma. 
//


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
IRBaboonAudioProcessorEditor::IRBaboonAudioProcessorEditor (IRBaboonAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
		thumbnailCacheTarg (1),
		thumbnailCacheBase (1),
		thumbnailCacheFilt (1),
		thumbnailTarg (processor.getTotalSweepBreakSamples(), formatManagerTarg, thumbnailCacheTarg),
		thumbnailBase (processor.getTotalSweepBreakSamples(), formatManagerBase, thumbnailCacheBase),
		thumbnailFilt (32768, formatManagerFilt, thumbnailCacheFilt)
{
	

    setSize (750, 750);
	
	
	/* capture & play buttons */
	addAndMakeVisible (&captureTargButton);
	addAndMakeVisible (&captureBaseButton);
	addAndMakeVisible (&playUnprocessedAudioButton);
	addAndMakeVisible (&playFiltAudioButton);
	
	playFiltAudioButton.setColour(TextButton::buttonColourId, Colour (0xff805705));
	playFiltAudioButton.setColour(TextButton::buttonOnColourId, Colour (0xff805705));
	playFiltAudioButton.setVisible(false);

	captureTargButton.setColour(TextButton::buttonColourId, Colour (0xff365936));
	captureTargButton.onClick = [this] { setStartCaptureTarget(); };
	captureBaseButton.setColour(TextButton::buttonColourId, Colour (0xff602743));
	captureBaseButton.onClick = [this] { setStartCaptureBase(); };

	playUnprocessedAudioButton.setClickingTogglesState(true);
	playFiltAudioButton.setClickingTogglesState(true);
	
	playUnprocessedAudioButton.onClick = [this] { playUnprocessedAudioClick(playUnprocessedAudioButton.getToggleState()); };
	playFiltAudioButton.onClick = [this] { playFiltAudioClick(playFiltAudioButton.getToggleState()); };
	
	playUnprocessedAudioButton.setRadioGroupId(playButtons);
	playFiltAudioButton.setRadioGroupId(playButtons);
	
	
	/* simple polling solution for checking whether IRFilt is ready in Processor */
	startTimerHz(8);
	
	
	
	/* thumbnails */
	formatManagerTarg.registerBasicFormats();
	formatManagerBase.registerBasicFormats();
	formatManagerFilt.registerBasicFormats();
	thumbnailTarg.addChangeListener (this);
	thumbnailBase.addChangeListener (this);
	thumbnailFilt.addChangeListener (this);
	
	Font labelFont (12);
	Justification justification (Justification::Flags::centred);
	
	addAndMakeVisible (&sweepTargLabel);
	sweepTargLabel.setText("Target\nsweep & IR", dontSendNotification);
	sweepTargLabel.setFont(labelFont);
	sweepTargLabel.setJustificationType(justification);

	addAndMakeVisible (&sweepBaseLabel);
	sweepBaseLabel.setText("Base\nsweep & IR", dontSendNotification);
	sweepBaseLabel.setFont(labelFont);
	sweepBaseLabel.setJustificationType(justification);

	addAndMakeVisible (&IRFiltLabel);
	IRFiltLabel.setText("Filter\nIR", dontSendNotification);
	IRFiltLabel.setFont(labelFont);
	IRFiltLabel.setJustificationType(justification);

	
	addAndMakeVisible (&titleLabel);
	titleLabel.setText("IRBaboon build "+processor.getDateTimeString(), dontSendNotification);
	titleLabel.setJustificationType(justification);
	labelFont.setStyleFlags(Font::italic);
	labelFont.setHeight(14.0);
	labelFont.setExtraKerningFactor(0.1);
	titleLabel.setFont(labelFont);
	titleLabel.setColour(Label::textColourId, Colour (0xffb2b5b8));

	
	
	
	/* sliders */
	addAndMakeVisible(&sliderZoomTarg);
	sliderZoomTarg.setRange(-6, 60);
	sliderZoomTarg.setValue(0.0);
	sliderZoomTarg.onValueChange = [this] {
		processor.setZoomTarg(sliderZoomTarg.getValue());
	};

	addAndMakeVisible(sliderZoomTargLabel);
	sliderZoomTargLabel.setText("Target zoom [dB]", dontSendNotification);
	sliderZoomTargLabel.attachToComponent(&sliderZoomTarg, true);
	
	addAndMakeVisible(&sliderZoomBase);
	sliderZoomBase.setRange(-6, 60);
	sliderZoomBase.setValue(0.0);
	sliderZoomBase.onValueChange = [this] {
		processor.setZoomBase(sliderZoomBase.getValue());
	};
	
	addAndMakeVisible(sliderZoomBaseLabel);
	sliderZoomBaseLabel.setText("Base zoom [dB]", dontSendNotification);
	sliderZoomBaseLabel.attachToComponent(&sliderZoomBase, true);
	
	addAndMakeVisible(&sliderZoomFilt);
	sliderZoomFilt.setRange(-60, 36);
	sliderZoomFilt.setValue(0.0);
	sliderZoomFilt.onValueChange = [this] {
		processor.setZoomFilt(sliderZoomFilt.getValue());
	};
	
	addAndMakeVisible(sliderZoomFiltLabel);
	sliderZoomFiltLabel.setText("Filter zoom [dB]", dontSendNotification);
	sliderZoomFiltLabel.attachToComponent(&sliderZoomFilt, true);

	addAndMakeVisible(&outputVolumeSlider);
	outputVolumeSlider.setRange(-60, 0);
	outputVolumeSlider.setValue(0.0);
	outputVolumeSlider.onValueChange = [this] {
		processor.setOutputVolume(outputVolumeSlider.getValue());
	};
	processor.setOutputVolume(outputVolumeSlider.getValue()); // set starting value immediately

	addAndMakeVisible(outputVolumeSliderLabel);
	outputVolumeSliderLabel.setText("Output volume [dB]", dontSendNotification);
	outputVolumeSliderLabel.attachToComponent(&outputVolumeSlider, true);

	
	/* bottom buttons */
	addAndMakeVisible(&toggleButtonLabel);
	toggleButtonLabel.setText("Filter:", dontSendNotification);
	toggleButtonLabel.attachToComponent(&amplButton, true);
	
	addAndMakeVisible (&phaseButton);
	phaseButton.setToggleState(true, dontSendNotification);
	phaseButton.onClick = [this] {
		processor.setPhaseFilt(phaseButton.getToggleState());
	};
	
	addAndMakeVisible(&amplButton);
	amplButton.setToggleState(true, dontSendNotification);
	amplButton.onClick = [this] {
		processor.setAmplFilt(amplButton.getToggleState());
	};

	
	addAndMakeVisible(&presweepSilenceMenu);
	/* for init these times are based on 48k samplerate */
	presweepSilenceMenu.addItem("0.085s", 1); // 4096
	presweepSilenceMenu.addItem("0.17s", 2); // 8192
	presweepSilenceMenu.addItem("0.34s", 3); // 16384
	presweepSilenceMenu.addItem("0.68s", 4); // 32768
	presweepSilenceMenu.addItem("1.37s", 5); // 65536
	presweepSilenceMenu.addItem("2.73s", 6); // 131072
	presweepSilenceMenu.setText("0.34s");
	presweepSilenceMenu.onChange = [this] { presweepSilenceMenuChanged(); };

	
	addAndMakeVisible(&IRFiltSizeMenu);
	IRFiltSizeMenu.addItem("128", 1);
	IRFiltSizeMenu.addItem("256", 2);
	IRFiltSizeMenu.addItem("512", 3);
	IRFiltSizeMenu.addItem("1024", 4);
	IRFiltSizeMenu.addItem("2048", 5);
	IRFiltSizeMenu.addItem("4096", 6);
	IRFiltSizeMenu.addItem("8192", 7);
	IRFiltSizeMenu.addItem("16384", 8);
	IRFiltSizeMenu.addItem("32768", 9);
	IRFiltSizeMenu.setText("2048");
	IRFiltSizeMenu.onChange = [this] { IRFiltSizeMenuChanged(); };
	IRFiltSizeMenu.setVisible(false);

	
	addAndMakeVisible (&swapButton);
	swapButton.onClick = [this] { processor.swapTargetBase(); };
	
	addAndMakeVisible (&loadTargetButton);
	loadTargetButton.onClick = [this] { loadTargetClicked(); };
}

IRBaboonAudioProcessorEditor::~IRBaboonAudioProcessorEditor()
{
}



void IRBaboonAudioProcessorEditor::setStartCaptureTarget(){
	processor.startCapture(IRBaboonAudioProcessor::IRType::Target);
	captureBaseButton.setVisible(false);
	presweepSilenceMenu.setVisible(false);
	int ms = std::ceil( 1000 * ((float) processor.getTotalSweepBreakSamples() + (float) presweepSilence) / ((float) processor.getSamplerate()) );
	Timer::callAfterDelay(ms, [this] {
		captureBaseButton.setVisible(true);
		presweepSilenceMenu.setVisible(true);
	} );
}

void IRBaboonAudioProcessorEditor::setStartCaptureBase(){
	processor.startCapture(IRBaboonAudioProcessor::IRType::Base);
	captureTargButton.setVisible(false);
	presweepSilenceMenu.setVisible(false);
	int ms = std::ceil( 1000 * ((float) processor.getTotalSweepBreakSamples() + (float) presweepSilence) / ((float) processor.getSamplerate()) );
	Timer::callAfterDelay(ms, [this] {
		captureTargButton.setVisible(true);
		presweepSilenceMenu.setVisible(true);
	} );
}



void IRBaboonAudioProcessorEditor::timerCallback(){
	// filt button
	if (processor.filtReady()){
		playFiltAudioButton.setVisible(true);
		IRFiltSizeMenu.setVisible(true);
	}

	// thumbnails
	String nameTarg (processor.getPrintDirectoryDebug() + "thumbnailTarg.wav");
	File fileTarg (nameTarg);
	if (fileTarg.existsAsFile()){
		thumbnailTarg.setSource(new FileInputSource(fileTarg));
	}
	else {
		thumbnailTarg.setSource(nullptr);
	}
	
	String nameBase (processor.getPrintDirectoryDebug() + "thumbnailBase.wav");
	File fileBase (nameBase);
	if (fileBase.existsAsFile()){
		thumbnailBase.setSource(new FileInputSource(fileBase));
	}
	else {
		thumbnailBase.setSource(nullptr);
	}
	
	String nameFilt (processor.getPrintDirectoryDebug() + "thumbnailFilt.wav");
	File fileFilt (nameFilt);
	if (fileFilt.existsAsFile()){
		thumbnailFilt.setSource(new FileInputSource(fileFilt));
	}
	
}


void IRBaboonAudioProcessorEditor::playUnprocessedAudioClick (bool toggleState){
	processor.setPlayFiltered(false);
	
	if (toggleState) playUnprocessedAudioButton.setButtonText("PLAYING unprocessed audio");
	if (!toggleState) playUnprocessedAudioButton.setButtonText("Play unprocessed audio");
}



void IRBaboonAudioProcessorEditor::playFiltAudioClick (bool toggleState){
	processor.setPlayFiltered(true);

	if (toggleState) {
		playFiltAudioButton.setButtonText("PLAYING filtered audio");
		playUnprocessedAudioButton.setButtonText("Play unprocessed audio");
	}
	if (!toggleState) playFiltAudioButton.setButtonText("Play filtered audio");
}



void IRBaboonAudioProcessorEditor::loadTargetClicked(){
	FileChooser myChooser ("Please select the sweepandir you want to load...",
						   File("../../../../../Saved IRs"),
						   String("*" + processor.getSavedIRExtension()));
	if (myChooser.browseForFileToOpen()) {
		processor.loadTarget(myChooser.getResult());
	}
}


void IRBaboonAudioProcessorEditor::presweepSilenceMenuChanged(){
	
	switch (presweepSilenceMenu.getSelectedId()){
		case 1: presweepSilence = 4096;		break;
		case 2: presweepSilence = 8192;		break;
		case 3: presweepSilence = 16384;	break;
		case 4: presweepSilence = 32768;	break;
		case 5: presweepSilence = 65536;	break;
		case 6: presweepSilence = 131072;	break;
	}
	
	processor.setPresweepSilence(presweepSilence);
	
	thumbnailFilt.reset(1, processor.getSamplerate(), IRFiltSize);
}


void IRBaboonAudioProcessorEditor::IRFiltSizeMenuChanged(){
	
	switch (IRFiltSizeMenu.getSelectedId()){
		case 1: IRFiltSize = 128;	break;
		case 2: IRFiltSize = 256;	break;
		case 3: IRFiltSize = 512;	break;
		case 4: IRFiltSize = 1024;	break;
		case 5: IRFiltSize = 2048;	break;
		case 6: IRFiltSize = 4096;	break;
		case 7: IRFiltSize = 8192;	break;
		case 8: IRFiltSize = 16384;	break;
		case 9: IRFiltSize = 32768;	break;
	}
	
	processor.setIRFiltSize(IRFiltSize);
	
	thumbnailFilt.reset(1, processor.getSamplerate(), IRFiltSize);
}


void IRBaboonAudioProcessorEditor::changeListenerCallback(ChangeBroadcaster* source){
	repaint();
}

//==============================================================================



//==============================================================================
void IRBaboonAudioProcessorEditor::paint (Graphics& g)
{
	
	/* Target thumbnail */
	
	Rectangle<int> thumbnailReferenceBounds (thumbnailSideMargin*3,
										  buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin,
										  getWidth() - thumbnailSideMargin*4,
										  thumbnailHeight*2);
	
	if (thumbnailTarg.getNumChannels() == 0){
		g.setColour (Colours::black);
		g.fillRect (thumbnailReferenceBounds);
		g.setColour (Colours::white);
		g.drawFittedText ("No target captured yet", thumbnailReferenceBounds, Justification::centred, 1.0f);
	}
	else {
		g.setColour (Colours::black);
		g.fillRect (thumbnailReferenceBounds);
		g.setColour (Colour (0xff90ee90));
		auto audioLength = (float) thumbnailTarg.getTotalLength();
		thumbnailTarg.drawChannels (g, thumbnailReferenceBounds, 0.0, audioLength, 1.0f);
	}
	
	sweepTargLabel.setBounds(0,
							buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin + thumbnailHeight*0.5,
							thumbnailSideMargin*3 - 5,
							thumbnailHeight);


	
	/* Base thumbnail */
	
	Rectangle<int> thumbnailCurrentBounds (thumbnailSideMargin*3,
											 buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin + thumbnailHeight*2,
											 getWidth() - thumbnailSideMargin*4,
											 thumbnailHeight*2);
	
	if (thumbnailBase.getNumChannels() == 0){
		g.setColour (Colours::black);
		g.fillRect (thumbnailCurrentBounds);
		g.setColour (Colours::white);
		g.drawFittedText ("No base captured yet", thumbnailCurrentBounds, Justification::centred, 1.0f);
	}
	else {
		g.setColour (Colours::black);
		g.fillRect (thumbnailCurrentBounds);
		g.setColour (Colour (0xffff69b4));
		auto audioLength = (float) thumbnailBase.getTotalLength();
		thumbnailBase.drawChannels (g, thumbnailCurrentBounds, 0.0, audioLength, 1.0f);
	}

	sweepBaseLabel.setBounds(0,
							 buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin + thumbnailHeight*2.5,
							 thumbnailSideMargin*3 - 5,
							 thumbnailHeight);


	
	/* Filt thumbnail */
	
	Rectangle<int> thumbnailInvfiltBounds (thumbnailSideMargin*3,
									   buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin + thumbnailHeight*4,
									   getWidth() - thumbnailSideMargin*4,
									   thumbnailHeight);

	if (thumbnailFilt.getNumChannels() == 0){
		g.setColour (Colours::black);
		g.fillRect (thumbnailInvfiltBounds);
		g.setColour (Colours::white);
		g.drawFittedText ("No filter available yet", thumbnailInvfiltBounds, Justification::centred, 1.0f);
	}
	else {
		g.setColour (Colours::black);
		g.fillRect (thumbnailInvfiltBounds);
		g.setColour (Colour (0xffffae0b));
		thumbnailFilt.drawChannels (g, thumbnailInvfiltBounds, 0.0, (double) IRFiltSize / (double) processor.getSamplerate(), 1.0f);
	}

	IRFiltLabel.setBounds(0,
						  buttonHeight*2 + thumbnailHeightMargin + sliderHeight + sliderHeightMargin + thumbnailHeight*4,
						  thumbnailSideMargin*3 - 5,
						  thumbnailHeight);

	
}




void IRBaboonAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
	
	
	/* top buttons */
	captureTargButton.setBounds 	(0,
										0,
										 getLocalBounds().getWidth()/2,
										 buttonHeight);
	captureBaseButton.setBounds 		(getLocalBounds().getWidth()/2,
										 0,
										 getLocalBounds().getWidth()/2,
										 buttonHeight);
	playUnprocessedAudioButton.setBounds (0,
										  buttonHeight,
										  getLocalBounds().getWidth()/2,
										  buttonHeight);
	playFiltAudioButton.setBounds (getLocalBounds().getWidth()/2,
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
	sliderZoomTarg.setBounds	(labelOffset,
								 2*buttonHeight + 5*thumbnailHeight + thumbnailHeightMargin + 2*sliderHeightMargin + sliderHeight,
								 getWidth() - labelOffset,
								 sliderHeight);
	sliderZoomBase.setBounds	(labelOffset,
								 2*buttonHeight + 5*thumbnailHeight + thumbnailHeightMargin + 3*sliderHeightMargin + 2*sliderHeight,
								 getWidth() - labelOffset,
								 sliderHeight);
	sliderZoomFilt.setBounds(labelOffset,
								 2*buttonHeight + 5*thumbnailHeight + thumbnailHeightMargin + 4*sliderHeightMargin + 3*sliderHeight,
								 getWidth() - labelOffset,
								 sliderHeight);


	// bottom buttons
	amplButton.setBounds(getLocalBounds().getWidth() * 1/12,
								 getLocalBounds().getHeight() - buttonHeight,
								 getLocalBounds().getWidth() * 1/12,
								 buttonHeight);
	phaseButton.setBounds(getLocalBounds().getWidth() * 2/12 ,
								 getLocalBounds().getHeight() - buttonHeight,
								 getLocalBounds().getWidth() * 1/12,
								 buttonHeight);
	presweepSilenceMenu.setBounds(getLocalBounds().getWidth() * 3/12 ,
							 getLocalBounds().getHeight() - buttonHeight,
							 getLocalBounds().getWidth() * 1/8,
							 buttonHeight);
	IRFiltSizeMenu.setBounds(getLocalBounds().getWidth() * 5/12,
							 getLocalBounds().getHeight() - buttonHeight,
							 getLocalBounds().getWidth() * 1/8,
							 buttonHeight);
	swapButton.setBounds(getLocalBounds().getWidth()*7/12,
						 getLocalBounds().getHeight() - buttonHeight,
						 getLocalBounds().getWidth()/4,
						 buttonHeight);
	loadTargetButton.setBounds(getLocalBounds().getWidth()*10/12,
						 getLocalBounds().getHeight() - buttonHeight,
						 getLocalBounds().getWidth()/6,
						 buttonHeight);
}









