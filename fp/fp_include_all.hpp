/* 
 * Copyright © 2020 Flixor. 
 */

/*
 * The fp namespace is to encompass all custom functions and classes,
 * as a sort of library-in-development.
 * This header is to include everything in the namespace easily,
 * and also houses several useful general definitions.
 */

#pragma once


#include <stdio.h>
#include <iostream>
#include <fstream>

#include "boost/filesystem.hpp"
#include "boost/algorithm/string.hpp"
#define BOOST_FILESYSTEM_NO_DEPRECATED // recommended by boost

#include "tools.hpp"
#include "CircularBufferArray.hpp"
#include "ParallelBufferPrinter.hpp"
#include "convolution.hpp"
#include "ExpSineSweep.hpp"
#include "ir.hpp"

using namespace fp;


#define NOT not

#define BREAK JUCE_BREAK_IN_DEBUGGER

