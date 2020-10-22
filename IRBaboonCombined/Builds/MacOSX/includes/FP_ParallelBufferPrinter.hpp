//
//  ParallelPrintBuffer.h
//  ConvOfflineFreqDomain
//
//  Created by Felix Postma on 20/02/2019.
//

#ifndef ParallelPrintBuffer_h
#define ParallelPrintBuffer_h

#include <fp_general.h>
#include <JuceHeader.h>

#include "boost/filesystem.hpp"
#include "boost/algorithm/string.hpp"



class PrintBuffer {
    
public:
    PrintBuffer();
	PrintBuffer(std::string h, AudioSampleBuffer& b);
    ~PrintBuffer();
    
    std::string header;
    AudioSampleBuffer buffer;
    bool empty;
};



class FP_ParallelBufferPrinter{
    
public:
    FP_ParallelBufferPrinter();
    ~FP_ParallelBufferPrinter();
    
	void appendBuffer(std::string name, AudioSampleBuffer& buffer);
	void appendCircularBufferArray(std::string name, fp::CircularBufferArray& circularBufferArray);

	// will replace the buffer in this location if one already exists
    void insertBuffer(std::string name, AudioSampleBuffer& buffer, int insertIndex);
	
	void printToConsole(int startSample, int numSamples, bool onlyCh0 = false, bool displaydB = false);
	
	void printToWav(int startSample, int numSamples, int sampleRate, String directoryNameFullPath = "-");


	// assumes that all added buffers have the same samplerate
	// don't forget to include libboost_filesystem.dylib in your project!
	void printFreqToCsv (int sampleRate, std::string directoryNameFullPath = "-");
	
	void clearAll();
    
    int getArraySize();
    int getMaxBufferLength();
    bool getEmpty(int checkIndex);
    
private:
	std::string getVersion (std::string csvName);

    std::vector<PrintBuffer> bufferArray;
    int maxBufferLength;
};


#endif /* ParallelPrintBuffer_h */


