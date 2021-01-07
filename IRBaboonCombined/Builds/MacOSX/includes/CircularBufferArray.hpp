//
//  Copyright © 2020 Felix Postma. 
//


#pragma once


#include <fp_include_all.hpp>
#include <JuceHeader.h>


namespace fp {
	

class CircularBufferArray {
	
public:
	CircularBufferArray();
	CircularBufferArray(int amountOfBuffers, int bufferChannelSize, int bufferSampleSize);
	~CircularBufferArray();

	void clearAndResize(int amountOfBuffers, int bufferChannelSize, int bufferSampleSize);
	
	/*
	Will retain all current data if new size is larger, or will retain the most recently written to buffers (the buffers most recently called by getWriteBufferPtr) if new is smaller. In the latter case, the most recently written to buffers will be organised in ascending order. Related to this, you are encouraged to always write to buffers in incremental (rather than decremental) order.
	 This method can also be used to remove all buffers (0), but retain channels and samples info.
	 */
	void changeArraySize(int amountOfBuffers);

	AudioBuffer<float>* getReadBufferPtr();
	AudioBuffer<float>* getWriteBufferPtr();
	AudioBuffer<float>* getBufferPtrAtIndex(int index);
	void incrReadIndex();	// built-in foldback
	void decrReadIndex();	// built-in foldback
	void incrWriteIndex();	// built-in foldback
	
	/* returns 1 buffer which is array elements 0-n appended in order */
	AudioBuffer<float> consolidate(int bufOffset = 0);

	int getReadIndex();
	void setReadIndex(int index);
	int getWriteIndex();
	void setWriteIndex(int index);
	int getArraySize();
	int getChannelsPerBuffer();
	int getSamplesPerBuffer();

private:
	void initBuffers(int amountOfBuffers, int bufferChannelSize, int bufferSampleSize);
	
	std::vector<AudioBuffer<float>> bufferArray;
	int channelsPerBuffer;
	int samplesPerBuffer;
	int readIndex;
	int writeIndex;
	int arraySize;
	int lastWrittenIndex = -1;
};

} // fp

