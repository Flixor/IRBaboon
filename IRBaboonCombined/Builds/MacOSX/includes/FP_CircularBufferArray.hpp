//
//  AudioSampleBufferArray.hpp
//  ConvUniformPartPlugin - Shared Code
//
//  Created by Felix Postma on 14/03/2019.
//

#ifndef AudioSampleBufferArray_hpp
#define AudioSampleBufferArray_hpp

#include <stdio.h>
#include <JuceHeader.h>



class FP_CircularBufferArray {
	
public:
	FP_CircularBufferArray();
	FP_CircularBufferArray(int amountOfBuffers, int bufferChannelSize, int bufferSampleSize);
	~FP_CircularBufferArray();

	void clearAndResize(int amountOfBuffers, int bufferChannelSize, int bufferSampleSize);
	
	/*
	Will retain all current data if new size is larger, or will retain the most recently written to buffers (the buffers most recently called by getWriteBufferPtr) if new is smaller. In the latter case, the most recently written to buffers will be organised in ascending order. Related to this, you are encouraged to always write to buffers in incremental (rather than decremental) order.
	 This method can also be used to remove all buffers (0), but retain channels and samples info.
	 */
	void changeArraySize(int amountOfBuffers);

	AudioSampleBuffer* getReadBufferPtr();
	AudioSampleBuffer* getWriteBufferPtr();
	AudioSampleBuffer* getBufferPtrAtIndex(int index);
	void incrReadIndex();	// built-in foldback
	void decrReadIndex();	// built-in foldback
	void incrWriteIndex();	// built-in foldback
	
	// returns 1 buffer which is array elements 0-n appended in order
	AudioSampleBuffer consolidate(int bufOffset = 0);

	int getReadIndex();
	void setReadIndex(int index);
	int getWriteIndex();
	void setWriteIndex(int index);
	int getArraySize();
	int getChannelsPerBuffer();
	int getSamplesPerBuffer();

private:
	void initBuffers(int amountOfBuffers, int bufferChannelSize, int bufferSampleSize);
	
	std::vector<AudioSampleBuffer> bufferArray;
	int channelsPerBuffer;
	int samplesPerBuffer;
	int readIndex;
	int writeIndex;
	int arraySize;
	int lastWrittenIndex = -1;
};



#endif /* AudioSampleBufferArray_hpp */
