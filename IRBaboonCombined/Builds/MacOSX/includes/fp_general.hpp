//
//  FP_general.h
//  IRBaboon
//
//  Created by Felix Postma on 21/10/2020.
//  Copyright © 2020 Flixor. All rights reserved.
//

#ifndef FP_GENERAL_HPP
#define FP_GENERAL_HPP

#include <stdio.h>
#include <iostream>
#include <fstream>

#include "boost/filesystem.hpp"
#include "boost/algorithm/string.hpp"
#define BOOST_FILESYSTEM_NO_DEPRECATED // recommended by boost

#include "tools.hpp"
#include "CircularBufferArray.hpp"
#include "ParallelBufferPrinter.hpp"
#include "convolver.hpp"
#include "ExpSineSweep.hpp"

using namespace fp;


#define NOT not

#define BREAK JUCE_BREAK_IN_DEBUGGER


#endif /* FP_GENERAL_HPP */
