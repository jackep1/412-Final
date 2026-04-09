 /*-------------------------------------------------------------------------+
 |	Final Project CSC412 - Fall 2024										|
 |	A graphic front end for a box-pushing simulation.						|
 |																			|
 |	This application simply creates a glut window with a pane to display	|
 |	a colored grid and the other to display some state information.			|
 |																			|
 |	Current GUI:															|
 |		- 'ESC' --> exit the application									|
 |		- ',' --> slows down the simulation									|
 |		- '.' --> apeeds up  he simulation									|
 |																			|
 |	Created by Jean-Yves Hervé on 2018-12-05 (C version)					|
 |	Revised 2024-12-04														|
 |																			|
 |	This is public domain code.  By all means appropriate it and change 	|
 |	is to your heart's content.												|
 +-------------------------------------------------------------------------*/

#include <iostream>
#include <sstream>
#include <string>
#include <random>
#include <vector>
#include <map>
#include <cstdio>
#include <cstdlib>
//
#include "glPlatform.h"
#include "typesAndConstants.h"
#include "gl_frontEnd.h"

using namespace std;

#if 0
//=================================================================
#pragma mark -
#pragma mark Function prototypes
//=================================================================
#endif

void displayGridPane(void);
void displayStatePane(void);
void printHorizontalBorder(ostringstream& outStream);
string printGrid(void);
void initializeApplication(void);
void cleanupAndQuit();
void generatePartitions();

#if 0
//=================================================================
#pragma mark -
#pragma mark Application-level global variables
//=================================================================
#endif

//	Don't touch
extern int	gMainWindow, gSubwindow[2];

//-------------------------------------
//	Don't rename any of these variables
//-------------------------------------
//	The state grid's dimensions (arguments to the program)
int numRows = -1;	//	height of the grid
int numCols = -1;	//	width
int numBoxes = -1;	//	also the number of robots
int numDoors = -1;	//	The number of doors.

int numLiveThreads = 0;		//	the number of live robot threads

//	robot sleep time between moves (in microseconds)
int robotSleepTime = 100000;

//	An array of C-string where you can store things you want displayed
//	in the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
const int MAX_NUM_MESSAGES = 8;
const int MAX_LENGTH_MESSAGE = 32;
char** message;

//	Only absolutely needed if you tackle the partition EC
SquareType** grid;


//-----------------------------
//	CHANGE THIS
//-----------------------------
//	Here I hard-code myself some data for robots and doors.  Obviously this code
//	must go away.  I just want to show you how information gets displayed.  
//	Obviously, you will need to allocate yourself some dynamic data structure to store 
//	that information once you know the dimensions of the grid, number of boxes/robots and
//	doors.  
//	Note that, even if you use the GUI version, it doesn't impose you a storage format at
//	all, since the drawing function draw a single robot/box/door at a time, and take that
//	object's parameters as individual argumenta.
//	So, feel free to go vectors all the way if you like it better than int**
//	Just don't feel free to declare oversized arrays, in the style of
//	int robotLoc[1000][2];
//	I can guarantee you that this kind of stuff will get penalized harshly (if might have
//	been convenient, borderline cute, in CSC211, but by now it's absolutely embarrassing)
//
//	Also:  Please note that because this is a grid-based problem, I never think of x and y but
//			row and column (well, the "I" dealing with the planning problem.  The "I" doing
//			the dirty work underneath has to translate all of that row and column data into
//			x and y pixel coordinates for OpenGL for rendering
//		   So,... In all of these arrays of 2 int values, the first value (index 0)
//			is a row coordinate and the second value (index 1) is a column coordinate.
int doorAssign[] = {1, 0, 0, 2, 1, 3};	//	door id assigned to each robot-box pair
int robotLoc[][2] = {{12, 8}, {6, 9}, {3, 14}, {11, 15}, {14, 1}, {8, 13}};
int boxLoc[][2] = {{6, 7}, {4, 12}, {13, 13}, {8, 12}, {7, 14}, {11, 9}};
int doorLoc[][2] = {{3, 3}, {8, 11}, {7, 10}, {12, 6}};

//	The above hard-coded intialization should be replaced by random generation in
//	initializeApplication().
//	Of course, this means that you need to modify the type of the above variables
//int** robotLoc;
//int** boxLoc;
//int** doorAssign;
//int** doorLoc;
//	Or with a bit of retooling
//vector<int> doorAssign;
//vector<GridPosition> robotLoc;
//vector<GridPosition> boxLoc;
//vector<GridPosition> doorLoc;

//	For extra credit section
random_device randDev;
default_random_engine engine(randDev());
vector<SlidingPartition> partitionList;
//	Change argument to 0.5 for equal probability of vertical and horizontal partitions
//	0 for only horizontal, and 1 for only vertical
bernoulli_distribution headsOrTails(1.0);

#if 0
//=================================================================
#pragma mark -
#pragma mark Function implementations
//=================================================================
#endif



//------------------------------------------------------------------------
//	You shouldn't have to change much in the main function besides
//	the initialization of numRows, numCos, numDoors, numBoxes.
//------------------------------------------------------------------------
int main(int argc, char** argv)
{
	//	We know that the arguments  of the program  are going
	//	to be the width (number of columns) and height (number of rows) of the
	//	grid, the number of boxes (and robots), and the number of doors.
	//	You are going to have to extract these.  For the time being,
	//	I hard code-some values
	numRows = 16;
	numCols = 20;
	numDoors = sizeof(doorLoc)/sizeof(doorLoc[0]);
	numBoxes = sizeof(boxLoc)/sizeof(boxLoc[0]);

	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv, displayGridPane, displayStatePane);

	//	Now we can do application-level initialization
	initializeApplication();

	string outStr = printGrid();
	cout << outStr << endl;
	
	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that 
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();
	
	cleanupAndQuit();
		
	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}

void cleanupAndQuit()
{
//	//	Free allocated resource before leaving (not absolutely needed, but
//	//	just nicer.  Also, if you crash there, you know something is wrong
//	//	in your code.
	for (int i=0; i< numRows; i++)
		delete []grid[i];
	delete []grid;
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		delete []message[k];
	delete []message;

	exit(0);
}

void initializeApplication(void)
{
	//	Allocate the grid
	grid = new SquareType*[numRows];
	for (int i=0; i<numRows; i++)
		grid[i] = new SquareType [numCols];
	
	message = new char*[MAX_NUM_MESSAGES];
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = new char[MAX_LENGTH_MESSAGE+1];
		
	//---------------------------------------------------------------
	//	This is the place where you should initialize the location
	//	of the doors, boxes, and robots, and create threads (not
	//	necessarily in that order).
	//---------------------------------------------------------------
	//	For extra credit
	//generatePartitions();
	
}

//multithreaded robots
void robotFunc(/* list of arguments */)
{
	bool isAlive = true;
	
	//	do planning (generate list of robot commands (move/push)
	
	
	while (isAlive)
	{
		//	execute one move

//		usleep(ROBOT_SLEEP_TIME);
//
//		isAlive = there are still commands in list of commands
	}
	
	return;
}

#if 0
//=================================================================
#pragma mark -
#pragma mark You probably don't need to look/edit below
//=================================================================
#endif


//	Rather that writing a function that prints out only to the terminal
//	and then
//		a. restricts me to a terminal-bound app;
//		b. forces me to duplicate the code if I also want to output
//			my grid to a file,
//	I have two options for a "does it all" function:
//		1. Use the stream class inheritance structure (the terminal is
//			an iostream, an output file is an ofstream, etc.)
//		2. Produce an output file and let the caller decide what they
//			want to do with it.
//	I said that I didn't want this course to do too much OOP (and, to be honest,
//	I have never looked seriously at the "stream" class hierarchy), so we will
//	go for the second solution.
string printGrid(void)
{
	//	some ugly hard-coded stuff
	static string doorStr[] = {"D0", "D1", "D2", "D3", "DD4", "D5", "D6", "D7", "D8", "D9"};
	static string robotStr[] = {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9"};
	static string boxStr[] = {"b0", "b1", "b2", "b3", "b4", "b5", "b6", "b7", "b8", "b9"};
	
	if (numDoors > 10 || numBoxes > 10)
	{
		return "Grid printout only works for small numbers of doors and robots.\n";
	}

	//	I use sparse storage for my grid
	map<int, map<int, string> > strGrid;
	
	//	addd doors
	for (int k=0; k<numDoors; k++)
	{
		strGrid[doorLoc[k][0]][doorLoc[k][1]] = doorStr[k];
	}
	//	add boxes
	for (int k=0; k<numBoxes; k++)
	{
		strGrid[boxLoc[k][0]][boxLoc[k][1]] = boxStr[k];
		strGrid[robotLoc[k][0]][robotLoc[k][1]] = robotStr[k];
	}
	
	ostringstream outStream;

	//	print top border
	printHorizontalBorder(outStream);
	
	for (int i=0; i<numRows; i++)
	{
		outStream << "|";
		for (int j=0; j<numCols; j++)
		{
			if (strGrid[i][j].length() > 0)
				outStream << " " << strGrid[i][j];
			else {
				outStream << " . ";
			}
		}
		outStream << "|" << endl;
	}
	//	print bottom border
	printHorizontalBorder(outStream);

	strGrid.clear();
	return outStream.str();
}

void printHorizontalBorder(ostringstream& outStream)
{
	outStream << "+--";
	for (int j=1; j<numCols; j++)
	{
		outStream << "---";
	}
	outStream << "-+" << endl;
}


void generatePartitions(void)
{
	const unsigned int NUM_PARTS = (numCols+numRows)/4;

	//	I decide that a partition length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_PARTITION_LENGTH = 3;
	const unsigned int MAX_HORIZ_PART_LENGTH = numCols / 4;
	const unsigned int MAX_VERT_PART_LENGTH = numRows / 4;
	const unsigned int MAX_NUM_TRIES = 20;
	uniform_int_distribution<unsigned int> horizPartLengthDist(MIN_PARTITION_LENGTH, MAX_HORIZ_PART_LENGTH);
	uniform_int_distribution<unsigned int> vertPartLengthDist(MIN_PARTITION_LENGTH, MAX_VERT_PART_LENGTH);
	uniform_int_distribution<unsigned int> rowDist(1, numRows-2);
	uniform_int_distribution<unsigned int> colDist(1, numCols-2);

	for (unsigned int w=0; w< NUM_PARTS; w++)
	{
		//	Case of a vertical partition
		if (headsOrTails(engine))
		{
			bool goodPart = false;

			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;

				//	select a column index
				unsigned int col = colDist(engine);
				unsigned int length = vertPartLengthDist(engine);

				//	now a random start row
				unsigned int startRow = 1 + rowDist(engine) % (numRows-length-1);
				for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
				}

				//	if the partition is possible,
				if (goodPart)
				{
					//	add it to the grid and to the partition list
					SlidingPartition part;
					part.isVertical = true;
					for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
					{
						grid[row][col] = SquareType::VERTICAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}

					partitionList.push_back(part);
				}
			}
		}
		// case of a horizontal partition
		else
		{
			bool goodPart = false;

			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;

				//	select a row index
				unsigned int row = rowDist(engine);
				unsigned int length = vertPartLengthDist(engine);

				//	now a random start row
				unsigned int startCol = 1 + colDist(engine) % (numCols-length-1);
				for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
				}

				//	if the wall first, add it to the grid and build SlidingPartition object
				if (goodPart)
				{
					SlidingPartition part;
					part.isVertical = false;
					for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
					{
						grid[row][col] = SquareType::HORIZONTAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}

					partitionList.push_back(part);
				}
			}
		}
	}
}

