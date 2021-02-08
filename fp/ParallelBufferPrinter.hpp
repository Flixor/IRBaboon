/*
 *  Copyright © 2021 Felix Postma. 
 */

#pragma once

#include <fp_include_all.hpp>
#include <JuceHeader.h>


namespace fp {


/* The ParallelBufferPrinter provides a way to debug AudioSampleBuffers in XCode.
 * During runtime buffers can be added to the printer, and then one of the print functions
 * can be used to check the contents at the time of adding. 
 */	

class ParallelBufferPrinter{
    
public:

	class PrintBuffer {
	    
	public:
	    PrintBuffer();
		PrintBuffer(std::string h, AudioBuffer<float>& b);
	    ~PrintBuffer();
	    
	    std::string header;
	    AudioBuffer<float> buffer;
	    bool empty;
	};

    ParallelBufferPrinter();
    ~ParallelBufferPrinter();
    

	void appendBuffer(std::string name, AudioBuffer<float>& buffer);
	void appendCircularBufferArray(std::string name, fp::CircularBufferArray& circularBufferArray);

	// will replace the buffer in this location if one already exists
    void insertBuffer(std::string name, AudioBuffer<float>& buffer, int insertIndex);
	
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
	
} // fp
