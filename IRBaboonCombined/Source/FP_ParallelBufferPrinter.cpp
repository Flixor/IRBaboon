//
//  ParallelPrintBuffer.cpp
//  ConvOfflineFreqDomain - ConsoleApp
//
//  Created by Felix Postma on 20/02/2019.
//

#include <stdio.h>
#include <iostream>
#include <fstream>

#include <fp_general.h>

#define BOOST_FILESYSTEM_NO_DEPRECATED // recommended by boost

#include "FP_ParallelBufferPrinter.hpp"


namespace fs = boost::filesystem;


//=============================================================
/*
 * PrintBuffer
 */

PrintBuffer::PrintBuffer(){
    header = "";
    buffer = AudioSampleBuffer(0,0);
    empty = true;
};


PrintBuffer::PrintBuffer(std::string h, AudioSampleBuffer& b){
	header = h;
	buffer = b;
	empty = true;
};



PrintBuffer::~PrintBuffer(){
    // niks?
};




//=============================================================
/*
 * ParallelBufferPrinter
 */

FP_ParallelBufferPrinter::FP_ParallelBufferPrinter(){
    maxBufferLength = 0;
};


FP_ParallelBufferPrinter::~FP_ParallelBufferPrinter(){
    bufferArray.clear ();
 };


void FP_ParallelBufferPrinter::appendBuffer(std::string name, AudioSampleBuffer& buffer) {

    bufferArray.push_back({name, buffer});

    if (bufferArray.back().buffer.getNumSamples() > maxBufferLength)
        maxBufferLength = bufferArray.back().buffer.getNumSamples();
	
	int samps = bufferArray.back().buffer.getNumSamples();
	if (samps > 0)
    	bufferArray.back().empty = false;
};


void FP_ParallelBufferPrinter::appendCircularBufferArray(std::string name, FP_CircularBufferArray& circularBufferArray) {
	
	int savedReadIndex = circularBufferArray.getReadIndex();
	circularBufferArray.setReadIndex(0);
	
	for(int buf = 0; buf < circularBufferArray.getArraySize(); buf++){
		bufferArray.push_back({name + "_" + std::to_string(buf), *circularBufferArray.getReadBufferPtr()});
		circularBufferArray.incrReadIndex();
		if (bufferArray.back().buffer.getNumSamples() > maxBufferLength)
			maxBufferLength = bufferArray.back().buffer.getNumSamples();
	}
	
	circularBufferArray.setReadIndex(savedReadIndex);
}


void FP_ParallelBufferPrinter::insertBuffer(std::string name, AudioSampleBuffer& buffer, int insertIndex){
    auto it = bufferArray.begin() + insertIndex;
    PrintBuffer p (name, buffer);
    
    if(insertIndex < bufferArray.size()) // replace existing array
        bufferArray.erase(it);

    bufferArray.insert(it, p);

    if (bufferArray[insertIndex].buffer.getNumSamples() > maxBufferLength)
        maxBufferLength = bufferArray.back().buffer.getNumSamples();
    
    bufferArray[insertIndex].empty = false;
};




void FP_ParallelBufferPrinter::printToConsole(int startSample, int numSamples, bool onlyCh0, bool displaydB) {
    
    if(startSample > maxBufferLength){
        printf("ParallelBufferPrinter::printAll(): startSample exceeds maximum buffer length.\n");
        return;
    }
    
    if(startSample + numSamples > maxBufferLength){
        printf("ParallelBufferPrinter::printAll(): startSample+numSamples exceeds maximum buffer length.\n");
        return;
    }
	
	
    if(bufferArray.size() <= 0){
        // printf("ParallelBufferPrinter::printToConsole(): no buffers to print.\n");
        return;
    }
	
	int chPrintLimit;
	
	int columnWidth = 5;    // tabs per column
	std::string emptyElementStr = "|";
	for (int i = 0; i < columnWidth; i++) emptyElementStr += "\t";
	int spacesPerTab = 4; 	// 4 spaces per tab, rounded down
	int tabsPerValue = 3;	// samples take up 3 tabs
	
	
	// count buffers to print
	int printableBufs = 0;
	for (int buf = 0; buf < bufferArray.size(); buf++){
		if(!bufferArray[buf].empty){
			printableBufs++;
		}
	}
	
	std::string horizFrameLineStr = "";
	std::string headersLineStr = "";
	std::string channelsLineStr = "";
	std::string emptyLineStr = "";
	for (int buf = 0; buf < printableBufs; buf++){
		
		// horiz frame line
		for (int i = 0; i < columnWidth * spacesPerTab; i++)
			horizFrameLineStr += "=";
		
		// header line
		if(onlyCh0) chPrintLimit = 1;
		else 		chPrintLimit = bufferArray[buf].buffer.getNumChannels();
		for (int channel = 0; channel < chPrintLimit; channel++){
			std::string printName = bufferArray[buf].header;
			printName.resize(columnWidth * spacesPerTab - 3, ' ');
			headersLineStr += "|" + printName;
	
		// channel nr line
			channelsLineStr += "|ch" + std::to_string(channel) + "\t\t\t\t";
		}

		// empty line
		emptyLineStr += emptyElementStr;
	}
	
	printf("%s\n%s\n%s\n%s\n", horizFrameLineStr.c_str(), headersLineStr.c_str(), channelsLineStr.c_str(), emptyLineStr.c_str());

	
	// samples
	for (int sample = startSample; sample < (startSample + numSamples); sample++){
		for (int buf = 0; buf < bufferArray.size(); buf++){
			if(!bufferArray[buf].empty){
				if (onlyCh0) chPrintLimit = 1;
				else		 chPrintLimit = bufferArray[buf].buffer.getNumChannels();
				for (int channel = 0; channel < chPrintLimit; channel++){
					if (sample < bufferArray[buf].buffer.getNumSamples()){
						float value = bufferArray[buf].buffer.getSample(channel, sample);
						if(displaydB) value = fp::tools::linTodB(abs(value));
						printf("|%u: %*g ", sample, tabsPerValue * spacesPerTab, value);
						for(int t = 0; t < (columnWidth - tabsPerValue - 1); t++)
							printf("\t");
					}
					else {
						printf("%s", emptyElementStr.c_str());
					}
				}
			}
		}
		printf("\n");
	}
	
	printf("%s\n%s\n%s\n%s\n", emptyLineStr.c_str(), headersLineStr.c_str(), channelsLineStr.c_str(), horizFrameLineStr.c_str());
	

};




void FP_ParallelBufferPrinter::printToWav(int startSample, int numSamples, int sampleRate, String directoryNameFullPath) {

	if(startSample > maxBufferLength){
		printf("ParallelBufferPrinter::printAll(): startSample exceeds maximum buffer length.\n");
		return;
	}
	
	if(startSample + numSamples > maxBufferLength){
		printf("ParallelBufferPrinter::printAll(): startSample+numSamples exceeds maximum buffer length.\n");
		return;
	}
	
	
	if(bufferArray.size() <= 0){
		// printf("ParallelBufferPrinter::printToWav(): no buffers to print.\n");
		return;
	}
	
	if (directoryNameFullPath == "-"){
		directoryNameFullPath = "/Users/flixor/Documents/Audio Ease/FP_ParallelBufferPrinter";
	}

	File wavDirectory (directoryNameFullPath);
	if(!wavDirectory.exists() || !wavDirectory.isDirectory())
		wavDirectory.createDirectory();
	
	for(int buf = 0; buf < bufferArray.size(); buf++){
		if(!bufferArray[buf].empty){
			
			String wavName = directoryNameFullPath+"/"+bufferArray[buf].header.c_str()+".wav";
			File wavFile (wavName);
			if (wavFile.exists()){
				wavFile.deleteFile();
			}
			wavFile.create();

			auto waf = std::make_unique<WavAudioFormat>();

			auto wavStream = wavFile.createOutputStream();

			AudioFormatWriter* writer = waf->createWriterFor(
															 wavStream.get(),
															 sampleRate,
															 bufferArray[buf].buffer.getNumChannels(),
															 24,
															 NULL,
															 0);
			
			/* releases ownership from unique_ptr */
			wavStream.release();

			if (writer != nullptr){
				writer->writeFromAudioSampleBuffer(bufferArray[buf].buffer,
												   startSample,
												   (startSample + numSamples) < bufferArray[buf].buffer.getNumSamples() ?
												   		numSamples :
												   		(bufferArray[buf].buffer.getNumSamples() - startSample) /* only print until end of buffer */
												   );
			}
			else {
				std::string str = "FP_ParallelBufferPrinter::printToWav(): writer is nullptr for buf " + std::to_string(buf) + "\n";
				DBG(str);
				JUCE_BREAK_IN_DEBUGGER;
			}
			
			delete writer;
		}
	}
};






void FP_ParallelBufferPrinter::printFreqToCsv (int sampleRate, std::string directoryNameFullPath ){
	
	double nyquist = sampleRate / 2;

	if (directoryNameFullPath == "-"){
		directoryNameFullPath = "/Users/flixor/Documents/Audio Ease/CSVs";
	}
	
	if (!fs::exists(directoryNameFullPath.c_str()))
		fs::create_directory(directoryNameFullPath.c_str());
	
	File pathCheck (directoryNameFullPath);
	if(!pathCheck.exists() || !pathCheck.isDirectory())
		pathCheck.createDirectory();
	
	for(int buf = 0; buf < bufferArray.size(); buf++){

		int fftSize = bufferArray[buf].buffer.getNumSamples();
		
		if (!fp::tools::isPowerOfTwo(fftSize)) {
			String errstr ("FP_ParallelBufferPrinter::printFreqToCsv error: buffer nr");
			DBG(errstr + std::to_string(buf) + " size is not power of 2.");
			continue;
		}
		
		int N = fftSize / 2;
		double freqPerBin = nyquist / (double) (N/2); // N/2 because juce fft style is {re, im, re im, ...}
		
		float* bufPtr = bufferArray[buf].buffer.getWritePointer(0);
		
		std::string fileName = bufferArray[buf].header;
		std::string headerName = fileName;
		boost::ireplace_all(fileName, " ", "_");
		std::string csvName = directoryNameFullPath+"/"+fileName+".tsv";
		boost::ireplace_all(headerName, "_", " ");
		
		if (fs::exists(csvName)){
			fs::remove(csvName);
		}
		
		std::ofstream fout;
		fout.open(csvName, std::ofstream::out);
		
		if (fout.is_open()){
			fout << "freq\t" << headerName.c_str() << "[lin]\t" << headerName.c_str() << "[dB]\t" << "bin\t" <<  headerName.c_str() << " phase[rad]\n";
			
			
			for (int bin = 0; bin <= N; bin += 2){
				fout << freqPerBin * bin/2 << "\t";
				fout << std::setprecision(8) << fp::tools::binAmpl(bufPtr + bin) << "\t";
				fout << std::setprecision(8) << fp::tools::linTodB(abs(fp::tools::binAmpl(bufPtr + bin))) << "\t";
				fout << bin/2 << "\t";
				fout << std::setprecision(8) << fp::tools::binPhase(bufPtr + bin) << "\n";
			}
			
			fout.close();
		}
		else {
			DBG("printFreqToCsv() error:"+fp::tools::DescribeIosFailure(fout));
			fout.close();
		}

	}
}



std::string FP_ParallelBufferPrinter::getVersion (std::string csvName){
	if (fs::exists(csvName)){
		csvName += " +"; 	// ghetto solution lol
		return getVersion(csvName);
	}
	else return csvName;
}


void FP_ParallelBufferPrinter::clearAll() {
    while (bufferArray.size() > 0){
		bufferArray.erase(bufferArray.begin());
    }

    maxBufferLength = 0;
};


int FP_ParallelBufferPrinter::getArraySize() {
    return (int) bufferArray.size();
};


int FP_ParallelBufferPrinter::getMaxBufferLength() {
    return maxBufferLength;
};


bool FP_ParallelBufferPrinter::getEmpty(int checkIndex) {
    return bufferArray[checkIndex].empty;
};






