//
//  Copyright © 2021 Felix Postma. 
//


#pragma once


#include <fp_include_all.hpp>
#include <JuceHeader.h>


namespace fp {

namespace ir {

	/* inverts by deconv pulse with buffer */
	AudioBuffer<float> invertFilter (AudioBuffer<float>& buffer, int samplerate);
	
	/* chopping algorithm:
	 	- find sample with max ampl
	 	- chop start is sample before max ampl sample that is consecutiveSamplesBelowThreshold
	 	- IRlength is length, and linear fadeout in final 1/8 or IR
	 * if nullifiedphase: needs shifted buffer as input */
	AudioBuffer<float> IRchop (AudioBuffer<float>& buffer, int IRlength, float thresholdLeveldB, int consecutiveSamplesBelowThreshold);
	
	/* puts the second half of the buffer in front of the first half
	 * if uneven: 2nd half has 1 sample less than 1st half */
	void shifteroo (AudioBuffer<float>* buffer);
	
	// For ARM convolution
	// IR -> FFT -> format {0, N/2, re(1), im(1), ..., im((N/2)-1)} -> export (close to) raw bytes
	AudioBuffer<float> IRtoRealFFTRaw (AudioBuffer<float>& buffer, int fftBufferSize);

} // ir

} // fp
