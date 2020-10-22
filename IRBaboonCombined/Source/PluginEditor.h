/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <fp_general.h>
#include <JuceHeader.h>
#include "PluginProcessor.h"

#include "FP_Tools.hpp"
#include "FP_ParallelBufferPrinter.hpp"
#include "FP_Convolver.hpp"
#include "FP_ExpSineSweep.hpp"
#include "FP_CircularBufferArray.hpp"

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
	void setStartCaptureThdn();

	void timerCallback() override;
	
	void playUnprocessedAudioClick (bool toggleState);
	void playInvfiltAudioClick (bool toggleState);
	void loadTargetClicked();
	void makeupSizeMenuChanged();

	// thumbnail
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
	TextButton captureThdnButton  { "THD+N" };
	TextButton playUnprocessedAudioButton  { "PLAYING unprocessed audio" };
	TextButton playInvFiltAudioButton  { "Play filtered audio" };
	
	enum RadioButtonIds {
		playButtons = 1
	};
	
	AudioFormatManager formatManagerReference;
	AudioFormatManager formatManagerCurrent;
	AudioFormatManager formatManagerInvfilt;
	AudioThumbnailCache thumbnailReferenceCache;
	AudioThumbnailCache thumbnailCurrentCache;
	AudioThumbnailCache thumbnailInvfiltCache;
	AudioThumbnail thumbnailReference;
	AudioThumbnail thumbnailCurrent;
	AudioThumbnail thumbnailInvfilt;

	int thumbnailHeight = 95;
	int thumbnailHeightMargin = 30;
	int thumbnailSideMargin  = 25;
	int buttonHeight = 30;
	
	Label sweepRefLabel;
	Label IRRefLabel;
	Label sweepCurrLabel;
	Label IRCurrLabel;
	Label IRinvfiltLabel;

	Label titleLabel;
	
	DecibelSlider referenceZoomSlider;
	Label referenceZoomSliderLabel;
	DecibelSlider currentZoomSlider;
	Label currentZoomSliderLabel;
	DecibelSlider InvfiltZoomSlider;
	Label InvfiltZoomSliderLabel;
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
