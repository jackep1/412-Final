/*----------------------------------------------------------------------------------+
 |	A Simple global header to check the development platform and load the proper	|
 |	OpenGL, glu, and glut headers.													|
 |	Supports macOS, Windows, Linux,													|
 |																					|
 |	Author:		Jean-Yves Hervé,	University of Rhode Island						|
 |									Dept. of Com[puter Science and Statistics		|
 |									3D Group for Interactive Visualization			|
 |	Fall 2013, modified Fall 2024													|
 +---------------------------------------------------------------------------------*/
#ifndef GL_PLATFORM_H
#define GL_PLATFORM_H

//-----------------------------------------------------------------------
//  Determines which OpenGL & glut header files to load, based on
//  the development platform and target (OS & compiler)
//-----------------------------------------------------------------------

//  Windows platform
#if (defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__) || (defined( __MWERKS__) && __INTEL__))
    //  Visual
    #if defined(_MSC_VER)
		#include <Windows.h>
        #include <GL\gl.h>
		#include <GL\glut.h>
		#define _USE_MATH_DEFINES
		#include <cmath>
    //  gcc-based compiler
    #elif defined(__CYGWIN__) || defined(__MINGW32__)
        #include <GL/gl.h>
        #include <GL/glut.h>
    #elif (defined( __MWERKS__) && __INTEL__))
    	#error Compilers not Supported
    #else
    	#error Unknown platform
    #endif

//  Linux and Unix
#elif  defined(linux) || defined(__linux__) || (defined(__FreeBSD__) || defined(sgi) || defined(__NetBSD__) || defined(__OpenBSD) || defined(__QNX__))
    #include <GL/gl.h>
    #include <GL/glut.h>

//  macOS
#else
	//	Nowadays Xcode + clang++ or g++ are the only games in town.
	#if 1
		//	Here ask Xcode/clang++ to suppress "deprecated" warnings
		#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		#include <GLUT/GLUT.h>
// 	#else
// 		#include <GL/freeglut.h>
// 		#include <GL/gl.h>
	#endif
#endif

#endif	//	GL_PLATFORM_H
