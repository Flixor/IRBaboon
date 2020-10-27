//
//  CircularBufferArray.cpp
//  ConvUniformPartPlugin - Shared Code
//
//  Created by Felix Postma on 14/03/2019.
//

#include <fp_include_all.hpp>


namespace fp {
	

CircularBufferArray::CircularBufferArray(){
	channelsPerBuffer = 0;
	samplesPerBuffer = 0;
	readIndex = 0;
	writeIndex = 0;
	arraySize = 0;
}


CircularBufferArray::CircularBufferArray(int amountOfBuffers, int bufferChannelSize, int bufferSampleSize){
	initBuffers(amountOfBuffers, bufferChannelSize, bufferSampleSize);
}


CircularBufferArray::~CircularBufferArray(){
	bufferArray.clear();
}


void CircularBufferArray::clearAndResize(int amountOfBuffers, int bufferChannelSize, int bufferSampleSize){
	bufferArray.clear();
	initBuffers(amountOfBuffers, bufferChannelSize, bufferSampleSize);
}


void CircularBufferArray::changeArraySize(int amountOfBuffers){
	
	if(amountOfBuffers == arraySize) return;
	
	if(amountOfBuffers == 0){
		bufferArray.clear();
		readIndex = 0;
		writeIndex = 0;
		arraySize = 0;
		return;
	}
	
	else if(amountOfBuffers > arraySize){
		for (int i = 0; i < (amountOfBuffers - arraySize); i++){
			bufferArray.push_back({channelsPerBuffer, samplesPerBuffer});
			bufferArray[i].clear();
		}
		arraySize = (int) bufferArray.size();
	}
	
	else if(amountOfBuffers < arraySize){
		
		if (lastWrittenIndex != -1){ // this means that there has actually been written
			std::vector<AudioSampleBuffer> tempArray;
		
			/* save distance from last written buffer to readindex and writeindex */
			int readIndexDiff = lastWrittenIndex - readIndex;
			int writeIndexDiff = lastWrittenIndex - writeIndex;
		
			readIndex = lastWrittenIndex;
			for(int i = 0; i < amountOfBuffers; i++){
				auto it = tempArray.begin();
				tempArray.insert(it, *getReadBufferPtr()); // using insert(), the most recently written buffer will appear at the back,
														   // conducive to incrWriteIndex() methodology
				decrReadIndex();
			}
		
			bufferArray = tempArray;
			arraySize = (int) bufferArray.size();
			int lastElement = arraySize - 1;
		
			if(readIndexDiff > lastElement) readIndex = 0;
			else if (readIndexDiff < 0) readIndex = lastElement - (arraySize + readIndexDiff);
			else readIndex = lastElement - readIndexDiff;
		
			if(writeIndexDiff > lastElement) writeIndex = 0;
			else if (writeIndexDiff < 0) writeIndex = lastElement - (arraySize + writeIndexDiff);
			else writeIndex = lastElement - writeIndexDiff;
		}
		/* otherwise: buffers shouldbe empty, we can use normal resize */
		else {
			bufferArray.resize(amountOfBuffers);
		}
	}
	
}


AudioSampleBuffer* CircularBufferArray::getReadBufferPtr(){
	return &bufferArray[readIndex];
};


AudioSampleBuffer* CircularBufferArray::getWriteBufferPtr(){
	lastWrittenIndex = writeIndex;
	return &bufferArray[writeIndex];
};


AudioSampleBuffer* CircularBufferArray::getBufferPtrAtIndex(int index){
	return &bufferArray[index];
};


void CircularBufferArray::incrReadIndex(){
	readIndex++;
	if (readIndex >= arraySize)
		readIndex = 0;
};


void CircularBufferArray::decrReadIndex(){
	readIndex--;
	if (readIndex < 0)
		readIndex = arraySize - 1;
};


void CircularBufferArray::incrWriteIndex(){
	writeIndex++;
	if (writeIndex >= arraySize)
		writeIndex = 0;
};



AudioSampleBuffer CircularBufferArray::consolidate(int bufOffset){
	AudioSampleBuffer consolidation (channelsPerBuffer, samplesPerBuffer * arraySize);
	
	int savedReadIndex = readIndex;
	
	if (bufOffset < 0)
		readIndex = arraySize - 1 - abs(bufOffset);
	else
		readIndex = bufOffset;
	
	for (int channel = 0; channel < channelsPerBuffer; channel++){
		for (int buf = 0; buf < arraySize; buf++){
			consolidation.copyFrom(channel, buf * samplesPerBuffer, *getReadBufferPtr(), channel, 0, samplesPerBuffer);
			incrReadIndex();
		}
	}
	readIndex = savedReadIndex;
	
	return consolidation;
}



int CircularBufferArray::getReadIndex(){
	return readIndex;
};


void CircularBufferArray::setReadIndex(int index){
	readIndex = index;
};


int CircularBufferArray::getWriteIndex(){
	return writeIndex;
};


void CircularBufferArray::setWriteIndex(int index){
	writeIndex = index;
};


int CircularBufferArray::getArraySize(){
	return arraySize;
};


int CircularBufferArray::getChannelsPerBuffer(){
	return channelsPerBuffer;
}

int CircularBufferArray::getSamplesPerBuffer(){
	return samplesPerBuffer;
}


// ==========================================
// Private
// ==========================================

void CircularBufferArray::initBuffers(int amountOfBuffers, int bufferChannelSize, int bufferSampleSize){
	channelsPerBuffer = bufferChannelSize;
	samplesPerBuffer = bufferSampleSize;
	
	for (int i = 0; i < amountOfBuffers; i++){
		bufferArray.push_back({channelsPerBuffer, bufferSampleSize});
		bufferArray[i].clear();
	}
	
	readIndex = 0;
	writeIndex = 0;
	arraySize = (int) bufferArray.size();
}
	
} // fp
