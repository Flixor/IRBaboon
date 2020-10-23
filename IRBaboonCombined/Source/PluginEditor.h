/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <fp_include_all.hpp>
#include <JuceHeader.h>
#include "PluginProcessor.h"


/*
 * Coped straight from Juce decibel slider tutorial
 */
class DecibelSlider : public Slider
{
public:
	DecibelSlider() {}
	double getValueFromText (const String& text) override;
	String getTextFromValue (double value) override;
	
private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DecibelSlider)
};



//==============================================================================
/**
*/
class AutoKalibraDemoAudioProcessorEditor  : public AudioProcessorEditor,
											public Timer,
											public ChangeListener
{
public:
    AutoKalibraDemoAudioProcessorEditor (AutoKalibraDemoAudioProcessor&);
    ~AutoKalibraDemoAudioProcessorEditor();
	//==============================================================================

	void setStartCaptureReference();
	void setStartCaptureCurrent();

	void timerCallback() override;
	
	void playUnprocessedAudioClick (bool toggleState);
	void playFiltAudioClick (bool toggleState);
	void loadTargetClicked();
	void makeupSizeMenuChanged();

	/* thumbnails */
	void changeListenerCallback(ChangeBroadcaster* source) override;
	
    //==============================================================================
    void paint (Graphics&) override;
	
    void resized() override;
	
	
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AutoKalibraDemoAudioProcessor& processor;

	TextButton captureReferenceButton  { "Capture target IR" };
	TextButton captureCurrentButton  { "Capture base IR" };
	TextButton playUnprocessedAudioButton  { "PLAYING unprocessed audio" };
	TextButton playFiltAudioButton  { "Play filtered audio" };
	
	enum RadioButtonIds {
		playButtons = 1
	};
	
	AudioFormatManager formatManagerTarg;
	AudioFormatManager formatManagerBase;
	AudioFormatManager formatManagerFilt;
	AudioThumbnailCache thumbnailCacheTarg;
	AudioThumbnailCache thumbnailCacheBase;
	AudioThumbnailCache thumbnailCacheFilt;
	AudioThumbnail thumbnailTarg;
	AudioThumbnail thumbnailBase;
	AudioThumbnail thumbnailFilt;

	int thumbnailHeight = 95;
	int thumbnailHeightMargin = 30;
	int thumbnailSideMargin  = 25;
	int buttonHeight = 30;
	
	Label sweepTargLabel;
	Label IRTargLabel;
	Label sweepBaseLabel;
	Label IRBaseLabel;
	Label IRFiltLabel;

	Label titleLabel;
	
	DecibelSlider sliderZoomTarg;
	Label sliderZoomTargLabel;
	DecibelSlider sliderZoomBase;
	Label sliderZoomBaseLabel;
	DecibelSlider sliderZoomFilt;
	Label sliderZoomFiltLabel;
	DecibelSlider outputVolumeSlider;
	Label outputVolumeSliderLabel;

	int labelOffset = 150;
	int sliderHeight = 25;
	int sliderHeightMargin = 10;
	
	Label toggleButtonLabel;
	ToggleButton nullifyPhaseButton { "Phase" };
	ToggleButton nullifyAmplitudeButton { "EQ" };
	ComboBox makeupSizeMenu;
	int makeupSize = 2048;
	TextButton swapButton { "Swap target <-> base" };
	TextButton loadTargetButton { "Load target..." };
	
	
	float refZoomLin = 1.0f;
	

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoKalibraDemoAudioProcessorEditor)
};
