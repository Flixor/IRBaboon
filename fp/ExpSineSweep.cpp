//
//  Copyright © 2020 Felix Postma. 
//


#include <fp_include_all.hpp>


namespace fp {


ExpSineSweep::ExpSineSweep(){
	sweep.setSize(0, 0);
	sweepInv.setSize(0, 0);
}



ExpSineSweep::~ExpSineSweep(){
	// ?
}

// ===========================================================================


void ExpSineSweep::generate(double durationSecs, double sampleRate, double lowFreq, double highFreq, double dBGain){
	
	assignParameters(durationSecs, sampleRate, lowFreq, highFreq);

	int sweepLength = (int) T;
	sweep.setSize(1, sweepLength);
	
	double gainReal = tools::dBToLin(dBGain);
	
	double sinArg;
	for(int i = 0; i < sweepLength; i++){
		sinArg = K * (exp((double)i / L) - 1.0);
		sweep.setSample(0, i, gainReal * sin(sinArg));
	}
	
}



AudioBuffer<double> ExpSineSweep::getSweep(){
	return sweep;
}



AudioSampleBuffer ExpSineSweep::getSweepFloat(){
	AudioSampleBuffer floatsweep;
	floatsweep.makeCopyOf(getSweep());
	return floatsweep;
}



void ExpSineSweep::generateInv(){
	
	if (sweep.getNumSamples() == 0){
		DBG("ExpSineSweep::generateInv(): seems like generate() has not been used first to create sweep attribute. \n");
		return;
	}
	
	sweepInv.makeCopyOf(sweep);
	sweepInv.reverse(0, sweepInv.getNumSamples());
	
	// -6 dB/oct
	k = pow(10.0, (-6.0 * log2(w2/w1)) / 20.0 / T); // attentuation per sample
	kend = pow(k, T); // final gain value, unused for now
	
	double* ptr = sweepInv.getWritePointer(0, 0);
	double kIterator = k;
	for (int i = 0; i < sweepInv.getNumSamples(); i++){
		ptr[i] *= kIterator;
		kIterator *= k;
	}
}



void ExpSineSweep::generateInv(double durationSecs, double sampleRate, double lowFreq, double highFreq, double dBGain){
	
	generate(durationSecs, sampleRate, lowFreq, highFreq, dBGain);
	generateInv();
}



AudioBuffer<double> ExpSineSweep::getSweepInv(){
	return sweepInv;
}



AudioSampleBuffer ExpSineSweep::getSweepInvFloat(){
	AudioSampleBuffer floatsweep;
	floatsweep.makeCopyOf(getSweepInv());
	return floatsweep;
}



int ExpSineSweep::getSampleIndexAtFreq (double freq){
	
	if(sweep.getNumSamples() == 0){
		DBG("sweep has not been generated yet. Try overloaded function? \n");
		return -1;
	}
	
	return getSampleHelper(freq);

}


int ExpSineSweep::getSampleIndexAtFreq (double freq, double durationSecs, double sampleRate, double lowFreq, double highFreq){
	
	assignParameters(durationSecs, sampleRate, lowFreq, highFreq);
	
	return getSampleHelper(freq);
}



double ExpSineSweep::getFreqAtSampleIndex(int index){
	
	if(sweep.getNumSamples() == 0){
		DBG("sweep has not been generated yet. Try overloaded function? \n");
		return -1;
	}
	
	return getFreqAtSampleIndexHelper(index);
}



double ExpSineSweep::getFreqAtSampleIndex(int index, double durationSecs, double sampleRate, double lowFreq, double highFreq){
	
	assignParameters(durationSecs, sampleRate, lowFreq, highFreq);

	return getFreqAtSampleIndexHelper(index);
}



void ExpSineSweep::linFadeout (double freq){
	
	if(sweep.getNumSamples() == 0){
		DBG("Sweep has not been generated yet. \n");
		return;
	}
	
	int index = getSampleIndexAtFreq(freq);
	
	int fadeoutSampleLength = sweep.getNumSamples() - index;
	
	for (int i = index; i < sweep.getNumSamples(); i++){
		sweep.setSample(0, i,
						   sweep.getSample(0, i) * (fadeoutSampleLength - (i - index)) / fadeoutSampleLength
						   );
	}
	
}



void ExpSineSweep::dBFadeout (double freq){
	
	if(sweep.getNumSamples() == 0){
		DBG("Sweep has not been generated yet. \n");
		return;
	}
	
	int index = getSampleIndexAtFreq(freq);
	int fadeoutSampleLength = sweep.getNumSamples() - index;
	
	double goaldB = -80.0;
	double dBGainPerSample = goaldB / fadeoutSampleLength;
	double linGainPerSample = tools::dBToLin(dBGainPerSample);
	
	double g = linGainPerSample;
	for (int i = index; i < sweep.getNumSamples(); i++){
		sweep.setSample(0, i,
						   sweep.getSample(0, i) * g
						   );
		g *= linGainPerSample;
	};
	
}



void ExpSineSweep::brickwallFadeout (double freq){
	
	if(sweep.getNumSamples() == 0){
		DBG("Sweep has not been generated yet. \n");
		return;
	}

	int index = getSampleIndexAtFreq(freq);

	sweep.clear(index, sweep.getNumSamples() - index);
}



// ====================================================================
// Private
// ====================================================================

void ExpSineSweep::assignParameters(double durationSecs, double sampleRate, double lowFreq, double highFreq){
	SR = sampleRate;
	T = SR * durationSecs;
	w1 = lowFreq / SR * 2 * M_PI;
	w2 = highFreq / SR * 2 * M_PI;
	
	K = T * w1 / log(w2 / w1);
	L = T / log(w2 / w1);
}


double ExpSineSweep::getFreqAtSampleIndexHelper(int index){
	
	if(index < 0 || index >= (int) T){
		DBG("index out of bounds. \n");
		return -1;
	}
	
	double lowFreq = w1 * SR / (2 * M_PI);
	double totalOct = log(w2 / w1) / log(2);
	
	double oct = (double)index / T * totalOct;
	
	return lowFreq * pow(2.0, oct);
}


int ExpSineSweep::getSampleHelper(double freq){
	
	double lowFreq = (w1 * SR / (2 * M_PI));
	double oct = log(freq / lowFreq) / log(2);
	double totalOct = log(w2 / w1) / log(2);
	double t = oct / totalOct * T;
	
	int t_rounded = (int) round(t);
	
	if (t_rounded < 0 || t_rounded >= T){
		DBG("Specified frequency is out of bounds for this sweep. \n");
		return -1;
	}
	
	return t_rounded;
}
	
} // fp
