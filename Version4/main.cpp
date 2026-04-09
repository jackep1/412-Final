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
 |		- '.' --> speeds up the simulation									|
 |																			|
 |	Created by Jean-Yves Hervé on 2018-12-05 (C version)					|
 |	Revised 2024-12-04														|
 |																			|
 |	This is public domain code.  By all means appropriate it and change 	|
 |	is to your heart's content.												|
 +-------------------------------------------------------------------------*/

#include <iostream>
#include <fstream>
#include <mutex>
#include <thread>
#include <sstream>
#include <string>
#include <random>
#include <vector>
#include <map>
#include <cstdlib>
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

void globalTimer();
void displayGridPane();
void displayStatePane();
void printHorizontalBorder(ostringstream& outStream);
string printGrid();
void initializeApplication();
void cleanupAndQuit();
void generatePartitions();
GridPosition getNewFreePosition();
void robotFunc(int robotId, GridPosition* robotPos, GridPosition* boxPos, unsigned int doorId);
vector<GridPosition> generateRandomDoorPositions(unsigned int numDoors);
vector<GridPosition> generateRandomRobotPositions(unsigned int numRobots);
vector<GridPosition> generateRandomBoxPositions(unsigned int numBoxes);
vector<Instruction> generateInstructions(int robotId, GridPosition robotPos, GridPosition boxPos, unsigned int doorId);

#if 0
//=================================================================
#pragma mark -
#pragma mark Application-level global variables
//=================================================================
#endif

//	Don't touch
extern int	gMainWindow, gSubWindow[2];

//-------------------------------------
//	Don't rename any of these variables
//-------------------------------------
//	The state grid's dimensions (arguments to the program)
int numRows = -1;	//	height of the grid
int numCols = -1;	//	width
int numBoxes = -1;	//	also the number of robots
int numDoors = -1;	//	The number of doors.
int numLiveThreads = 0; mutex liveThreadsLock;   //	the number of live robot threads
int robotSleepTime = 1000000;	//	robot sleep time between moves (in microseconds)

//	An array of C-string where you can store things you want displayed
//	in the state pane to display (for debugging purposes?)
//	Don't change the dimensions as this may break the front end
constexpr int MAX_NUM_MESSAGES = 8;
constexpr int MAX_LENGTH_MESSAGE = 32;
char** message;
SquareType** grid;		//	Only absolutely needed if you tackle the partition EC
vector<vector<unique_ptr<mutex>>> gridLocks;

mutex outputMutex; string outFilePath = "robotSimulOut.txt";

vector<unsigned int> doorAssignments;  // Index by which to access the door a robot/box pair has been assigned
vector<GridPosition> robotLocations;
vector<GridPosition> boxLocations;
vector<GridPosition> doorLocations;
vector<bool> robotStatuses;
chrono::time_point<chrono::steady_clock> startTime;
int timeLimit = 60; // Default maximum of 1 minute run time

//* Randomization tools *//
random_device randDev;
default_random_engine engine(randDev());
uniform_int_distribution<unsigned int> rowGenerator;
uniform_int_distribution<unsigned int> colGenerator;
uniform_int_distribution<unsigned int> doorGenerator;
uniform_int_distribution<unsigned int> nonEdgeRowGenerator;
uniform_int_distribution<unsigned int> nonEdgeColGenerator;


#if 0
//=================================================================
#pragma mark -
#pragma mark Function implementations
//=================================================================
#endif

int main(const int argc, char** argv)
{
	numRows = stoi(argv[1]);
	numCols = stoi(argv[2]);
	numBoxes = stoi(argv[3]);
	numDoors = stoi(argv[4]);
	// If more than five arguments are provided, we're running it as a batch program.
	// In this case, the sixth argument will be the name of the output file.
	// The seventh argument will be the time limit for the program.
	if (argc > 5) outFilePath = argv[5];
	if (argc > 6) timeLimit = stoi(argv[6]);

	ofstream outFile(outFilePath);
	outFile << numRows << " " << numCols << " " << numBoxes << " " << numDoors << endl << endl;
	outFile.close();

	initializeFrontEnd(argc, argv, displayGridPane, displayStatePane);

	startTime = chrono::steady_clock::now();
	thread timerThread(globalTimer);
	timerThread.detach();

	//	Now we can do application-level initialization
	initializeApplication();
	
	//	Now we enter the main loop of the program and to a large extend "lose control" over its execution.
	glutMainLoop();

	cleanupAndQuit();

	return 0;
}

void cleanupAndQuit()
{
	//	Free allocated resource before leaving (not absolutely needed, but
	//	just nicer).  Also, if you crash there, you know something is wrong
	//	in your code.
	for (int i=0; i< numRows; i++)
		delete []grid[i];
	delete []grid;
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		delete []message[k];
	delete []message;

	exit(0);
}

void globalTimer()
{
	// Sleep for 1 minute, then terminate the program
	this_thread::sleep_for(chrono::seconds(timeLimit));
	cout << "Time limit for program reached." << endl;
	exit(1);
}

void initializeApplication()
{
	//	Allocate the grid
	grid = new SquareType*[numRows];
	for (int i=0; i<numRows; i++)
		grid[i] = new SquareType [numCols];

	gridLocks.resize(numRows);
	for (int i = 0; i < numRows; i++)
	{
		gridLocks[i].resize(numCols);
		for (int j = 0; j < numCols; j++)
		{
			gridLocks[i][j] = make_unique<mutex>();
		}
	}

	message = new char*[MAX_NUM_MESSAGES];
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = new char[MAX_LENGTH_MESSAGE+1];
		
	//---------------------------------------------------------------
	//	This is the place where you should initialize the location
	//	of the doors, boxes, and robots, and create threads (not
	//	necessarily in that order).
	//---------------------------------------------------------------

	// Initialize random generators
	rowGenerator = uniform_int_distribution<unsigned int>(0, numRows-1);
	colGenerator = uniform_int_distribution<unsigned int>(0, numCols-1);
	doorGenerator = uniform_int_distribution<unsigned int>(0, numDoors-1);
	nonEdgeRowGenerator = uniform_int_distribution<unsigned int>(1, numRows-2);
	nonEdgeColGenerator = uniform_int_distribution<unsigned int>(1, numCols-2);

	doorLocations = generateRandomDoorPositions(numDoors);
	boxLocations = generateRandomBoxPositions(numBoxes);
	robotLocations = generateRandomRobotPositions(numBoxes);
	robotStatuses.resize(numBoxes, true);

	for (int i = 0; i < numBoxes; i++)
	{
		doorAssignments.push_back(doorGenerator(engine));
		cout << "Robot " << i << " assigned to door " << doorAssignments[i] << " at " << doorLocations[doorAssignments[i]].row << ", " << doorLocations[doorAssignments[i]].col << endl;
	}

	// Generate threads for each robot
	for (int i = 0; i < numBoxes; i++)
	{
		thread robotThread(robotFunc, i, &robotLocations[i], &boxLocations[i], doorAssignments[i]);
		numLiveThreads++;
		robotThread.detach();
	}

	//	For extra credit
	//generatePartitions();
}

GridPosition getNewFreePosition()
{
	GridPosition pos{};
	bool noGoodPos = true;
	while (noGoodPos)
	{
		unsigned int row = rowGenerator(engine);
		unsigned int col = colGenerator(engine);
		if (grid[row][col] == SquareType::FREE_SQUARE)
		{
			pos.row = row;
			pos.col = col;
			noGoodPos = false;
		}
	}
	return pos;
}

GridPosition getNewNonEdgeFreePosition()
{
	GridPosition pos{};
	bool noGoodPos = true;
	while (noGoodPos)
	{
		unsigned int row = nonEdgeRowGenerator(engine);
		unsigned int col = nonEdgeColGenerator(engine);
		if (grid[row][col] == SquareType::FREE_SQUARE)
		{
			pos.row = row;
			pos.col = col;
			noGoodPos = false;
		}
	}
	return pos;
}

vector<GridPosition> generateRandomDoorPositions(const unsigned int numDoors)
{
	vector<GridPosition> doorPositions;
	ofstream outFile(outFilePath, ios::app);
	for (unsigned int i = 0; i < numDoors; i++)
	{
		GridPosition doorPos = getNewFreePosition();
		grid[doorPos.row][doorPos.col] = SquareType::DOOR;
		doorPositions.push_back(doorPos);
		outFile << "Door " << i << " at " << doorPos.row << ", " << doorPos.col << endl;
	}
	outFile << endl; outFile.close();
	return doorPositions;
}

vector<GridPosition> generateRandomRobotPositions(const unsigned int numRobots)
{
	vector<GridPosition> robotPositions;
	ofstream outFile(outFilePath, ios::app);
	for (unsigned int i = 0; i < numRobots; i++)
	{
		GridPosition robotPos = getNewFreePosition();
		gridLocks[robotPos.row][robotPos.col]->lock(); // Lock the square the robot is placed on
		grid[robotPos.row][robotPos.col] = SquareType::ROBOT;
		robotPositions.push_back(robotPos);
		outFile << "Robot " << i << " at " << robotPos.row << ", " << robotPos.col << endl;
	}
	outFile << endl; outFile.close();
	return robotPositions;
}

vector<GridPosition> generateRandomBoxPositions(const unsigned int numBoxes)
{
	vector<GridPosition> boxPositions;
	ofstream outFile(outFilePath, ios::app);
	for (unsigned int i = 0; i < numBoxes; i++)
	{
		GridPosition boxPos = getNewNonEdgeFreePosition();
		gridLocks[boxPos.row][boxPos.col]->lock(); // Lock the square the box is placed on
		grid[boxPos.row][boxPos.col] = SquareType::BOX;
		boxPositions.push_back(boxPos);
		outFile << "Box " << i << " at " << boxPos.row << ", " << boxPos.col << endl;
	}
	outFile << endl; outFile.close();
	return boxPositions;
}

GridPosition calculatePushStartPosition(const GridPosition boxPos, const GridPosition doorPos)
{
	// Determine the square from which the robot should begin pushing the box
	// Horizontal moves are made first, so any horizontal difference means the box starts
	// on the same row, and column adjusted. A box already in line with its door instead
	// means that the robot starts above or below the box.
	GridPosition pushStart{};
	if (boxPos.col < doorPos.col)
	{
		pushStart.col = boxPos.col - 1;
		pushStart.row = boxPos.row;
	} else if (boxPos.col > doorPos.col)
	{
		pushStart.col = boxPos.col + 1;
		pushStart.row = boxPos.row;
	} else
	{
		pushStart.col = boxPos.col;
		if (boxPos.row < doorPos.row)
		{
			pushStart.row = boxPos.row - 1;
		} else if (boxPos.row > doorPos.row)
		{
			pushStart.row = boxPos.row + 1;
		} else
		{
			cout << "Box is already at the exit" << endl << flush;
			exit(100);
		}
	}
	return pushStart;
}

vector<Instruction> genInstrToPushStart(const GridPosition robotPos, const GridPosition pushStart)
{
	vector<Instruction> instructions;
	const int rowDiff = static_cast<int>(robotPos.row) - static_cast<int>(pushStart.row);
	const int colDiff = static_cast<int>(robotPos.col) - static_cast<int>(pushStart.col);

	Direction colDir = colDiff < 0 ? EAST : WEST;
	for (int i = 0; i < abs(colDiff); i++) { instructions.push_back({MOVE, colDir}); }

	Direction rowDir = rowDiff < 0 ? SOUTH : NORTH;
	for (int i = 0; i < abs(rowDiff); i++) { instructions.push_back({MOVE, rowDir}); }

	return instructions;
}

vector<Instruction> genInstrForHorizontalPush(const GridPosition boxPos, const GridPosition doorPos)
{
	vector<Instruction> instructions;
	int colDiff = static_cast<int>(boxPos.col) - static_cast<int>(doorPos.col);
	Direction colDir = colDiff < 0 ? EAST : WEST;
	for (int i = 0; i < abs(colDiff); i++) { instructions.push_back({PUSH, colDir}); }

	return instructions;
}

vector<Instruction> genInstrForRobotReposition(GridPosition robotPos, const GridPosition boxPos, const GridPosition doorPos)
{
	vector<Instruction> instructions;
	int rowDiff = static_cast<int>(boxPos.row) - static_cast<int>(doorPos.row);
	int colDiff = static_cast<int>(robotPos.col) - static_cast<int>(boxPos.col);

	if (rowDiff == 0)  // If the robot is already in line with the box
	{
		return instructions;
	}

	// If the robot needs to push the box north, move it south, and vice versa
	if (colDiff != 0)
	{
		Direction rowDir = rowDiff < 0 ? NORTH : SOUTH;
		instructions.push_back({MOVE, rowDir});
	}

	if (colDiff != 0)  // If the robot is not already in line with the box
	{
		// If the robot is left of the box, move it east, and vice versa
		Direction colDir = colDiff < 0 ? EAST : WEST;
		instructions.push_back({MOVE, colDir});
	}

	return instructions;
}

vector<Instruction> genInstrForVerticalPush(const GridPosition boxPos, const GridPosition doorPos)
{
	vector<Instruction> instructions;
	int rowDiff = static_cast<int>(boxPos.row) - static_cast<int>(doorPos.row);
	Direction rowDir = rowDiff < 0 ? SOUTH : NORTH;
	for (int i = 0; i < abs(rowDiff); i++) { instructions.push_back({PUSH, rowDir}); }

	return instructions;
}

void moveRobot(GridPosition* robotPos, const Direction direction)
{
	unsigned int oldRow = robotPos->row, oldCol = robotPos->col;  // Store the robot's current position
	unsigned int newRow, newCol;
	int rowDiff, colDiff;
	switch (direction)
	{
		case NORTH:
			newRow = robotPos->row - 1; rowDiff = -1;  // Calculate the new row and the difference
			newCol = robotPos->col; colDiff = 0; // Calculate the new column and the difference
			break;
		case SOUTH:
			newRow = robotPos->row + 1; rowDiff = 1;
			newCol = robotPos->col; colDiff = 0;
			break;
		case EAST:
			newRow = robotPos->row; rowDiff = 0;
			newCol = robotPos->col + 1; colDiff = 1;
			break;
		case WEST:
			newRow = robotPos->row; rowDiff = 0;
			newCol = robotPos->col - 1; colDiff = -1;
			break;
		default:
			cout << "Invalid direction" << endl << flush;
			exit(99);
	}
	gridLocks[newRow][newCol]->lock(); // Lock the square the robot is moving to
	robotPos->row += rowDiff; robotPos->col += colDiff; // Update the location that glut sees
	grid[newRow][newCol] = SquareType::ROBOT; // Update the grid with the new robot position
	grid[oldRow][oldCol] = SquareType::FREE_SQUARE; // Update the grid with the old robot position
	gridLocks[oldRow][oldCol]->unlock(); // Unlock the square the robot is moving from
}

void moveBox(GridPosition* boxPos, const Direction direction)
{
	unsigned int oldRow = boxPos->row, oldCol = boxPos->col;
	unsigned int newRow, newCol;
	int rowDiff, colDiff;
	switch (direction)
	{
		case NORTH:
			newRow = boxPos->row - 1; rowDiff = -1;
			newCol = boxPos->col; colDiff = 0;
			break;
		case SOUTH:
			newRow = boxPos->row + 1; rowDiff = 1;
			newCol = boxPos->col; colDiff = 0;
			break;
		case EAST:
			newRow = boxPos->row; rowDiff = 0;
			newCol = boxPos->col + 1; colDiff = 1;
			break;
		case WEST:
			newRow = boxPos->row; rowDiff = 0;
			newCol = boxPos->col - 1; colDiff = -1;
			break;
		default:
			cout << "Invalid direction" << endl << flush;
			exit(99);
	}
	gridLocks[newRow][newCol]->lock(); // Lock the square the box is moving to
	boxPos->row += rowDiff; boxPos->col += colDiff; // Update the location that glut sees
	grid[newRow][newCol] = SquareType::BOX; // Update the grid with the new box position
	grid[oldRow][oldCol] = SquareType::FREE_SQUARE; // Update the grid with the old box position
	gridLocks[oldRow][oldCol]->unlock(); // Unlock the square the box is moving from
}

void deleteRobot(const GridPosition* robotPos, const GridPosition* boxPos, int robotId)
{
	gridLocks[robotPos->row][robotPos->col]->unlock(); // Unlock the square the robot is on
	grid[robotPos->row][robotPos->col] = SquareType::FREE_SQUARE; // Update the grid with the robot's old position
	gridLocks[boxPos->row][boxPos->col]->unlock(); // Unlock the square the box is on
	grid[boxPos->row][boxPos->col] = SquareType::FREE_SQUARE; // Update the grid with the box's old position

	robotStatuses[robotId] = false;
}

void handleInstruction(GridPosition* robotPos, GridPosition* boxPos, const Instruction instr)
{
	switch (instr.op)
	{
		case MOVE:
			moveRobot(robotPos, instr.direction);
			break;
		case PUSH:
			moveBox(boxPos, instr.direction);
			moveRobot(robotPos, instr.direction);
			break;
		case END:
			break;
		default:
			cout << "Invalid operation type" << endl << flush;
			exit(99);
	}
}

void printInstructions(const vector<Instruction>& instructions, const int robotId)
{
	outputMutex.lock();
	ofstream outFile(outFilePath, ios::app);
	for (auto instr : instructions)
	{
		string dir;
		if (instr.op == MOVE || PUSH)
		{
			switch (instr.direction)
			{
				case (NORTH):
					dir = "north"; break;
				case (SOUTH):
					dir = "south"; break;
				case(EAST):
					dir = "east"; break;
				case(WEST):
					dir = "west"; break;
				default:
					break;
			}
		}
		if (instr.op == MOVE)
		{
			outFile << "robot " << robotId << " move " << dir << endl;
		}

		if (instr.op == PUSH)
		{
			outFile << "robot " << robotId << " push " << dir << endl;
		}

		if (instr.op == END)
		{
			outFile << "robot " << robotId << " end" << endl;
		}
	}
	outFile.close();
	outputMutex.unlock();
}

// Multithreaded robots
void robotFunc(const int robotId, GridPosition* robotPos, GridPosition* boxPos, const unsigned int doorId)
{
	vector<Instruction> instructions;

	auto [row, col] = doorLocations[doorId];
	// Determine the square from which the robot should begin pushing the box
	GridPosition robotPushStart = calculatePushStartPosition(*boxPos, {row, col});

	// Generate instructions for moving the robot to the push start position
	vector<Instruction> pushStartInstructions = genInstrToPushStart(*robotPos, robotPushStart);
	for (auto instr : pushStartInstructions) { handleInstruction(robotPos, boxPos, instr); usleep(robotSleepTime); }

	// Use robot to push the box horizontally to the door's column
	vector<Instruction> horizontalPushInstructions = genInstrForHorizontalPush(*boxPos, {row, col});
	for (auto instr : horizontalPushInstructions) { handleInstruction(robotPos, boxPos, instr); usleep(robotSleepTime); }

	// Reposition the robot to the box's column to push vertically
	vector<Instruction> repositionInstructions = genInstrForRobotReposition(*robotPos, *boxPos, {row, col});
	for (auto instr : repositionInstructions) { handleInstruction(robotPos, boxPos, instr); usleep(robotSleepTime); }

	// Use robot to push the box vertically to the door's position
	vector<Instruction> verticalPushInstructions = genInstrForVerticalPush(*boxPos, {row, col});
	for (auto instr : verticalPushInstructions) { handleInstruction(robotPos, boxPos, instr); usleep(robotSleepTime); }

	handleInstruction(robotPos, boxPos, {END, NUM_TRAVEL_DIRECTIONS});

	deleteRobot(robotPos, boxPos, robotId);

	instructions.insert(instructions.end(), pushStartInstructions.begin(), pushStartInstructions.end());
	instructions.insert(instructions.end(), horizontalPushInstructions.begin(), horizontalPushInstructions.end());
	instructions.insert(instructions.end(), repositionInstructions.begin(), repositionInstructions.end());
	instructions.insert(instructions.end(), verticalPushInstructions.begin(), verticalPushInstructions.end());
	instructions.push_back({END, NUM_TRAVEL_DIRECTIONS});
	printInstructions(instructions, robotId);

	cout << "Robot thread " << robotId << " has completed its task" << endl << flush;
	liveThreadsLock.lock();
	numLiveThreads--;
	liveThreadsLock.unlock();
}

#if 0
//=================================================================
#pragma mark -
#pragma mark You probably do not need to look/edit below
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
string printGrid()
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
		strGrid[static_cast<int>(doorLocations[k].row)][static_cast<int>(doorLocations[k].col)] = doorStr[k];
	}
	//	add boxes
	for (int k=0; k<numBoxes; k++)
	{
		strGrid[static_cast<int>(boxLocations[k].row)][static_cast<int>(boxLocations[k].col)] = boxStr[k];
		strGrid[static_cast<int>(robotLocations[k].row)][static_cast<int>(robotLocations[k].col)] = robotStr[k];
	}
	
	ostringstream outStream;

	//	print top border
	printHorizontalBorder(outStream);
	
	for (int i=0; i<numRows; i++)
	{
		outStream << "|";
		for (int j=0; j<numCols; j++)
		{
			if (!strGrid[i][j].empty())
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
//
// void generatePartitions()
// {
// 	const unsigned int NUM_PARTS = (numCols+numRows)/4;
//
// 	//	I decide that a partition length  cannot be less than 3  and not more than
// 	//	1/4 the grid dimension in its Direction
// 	constexpr unsigned int MIN_PARTITION_LENGTH = 3;
// 	const unsigned int MAX_HORIZ_PART_LENGTH = numCols / 4;
// 	const unsigned int MAX_VERT_PART_LENGTH = numRows / 4;
// 	uniform_int_distribution<unsigned int> horizPartLengthDist(MIN_PARTITION_LENGTH, MAX_HORIZ_PART_LENGTH);  // NOLINT
// 	uniform_int_distribution<unsigned int> vertPartLengthDist(MIN_PARTITION_LENGTH, MAX_VERT_PART_LENGTH);
// 	uniform_int_distribution<unsigned int> rowDist(1, numRows-2);
// 	uniform_int_distribution<unsigned int> colDist(1, numCols-2);
//
// 	for (unsigned int w=0; w< NUM_PARTS; w++)
// 	{
// 		constexpr unsigned int MAX_NUM_TRIES = 20;
// 		//	Case of a vertical partition
// 		if (headsOrTails(engine))
// 		{
// 			bool goodPart = false;
//
// 			//	I try a few times before giving up
// 			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
// 			{
// 				//	let's be hopeful
// 				goodPart = true;
//
// 				//	select a column index
// 				unsigned int col = colDist(engine);
// 				unsigned int length = vertPartLengthDist(engine);
//
// 				//	now a random start row
// 				unsigned int startRow = 1 + rowDist(engine) % (numRows-length-1);
// 				for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
// 				{
// 					if (grid[row][col] != SquareType::FREE_SQUARE)
// 						goodPart = false;
// 				}
//
// 				//	if the partition is possible,
// 				if (goodPart)
// 				{
// 					//	add it to the grid and to the partition list
// 					SlidingPartition part;
// 					part.isVertical = true;
// 					for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)  // NOLINT
// 					{
// 						grid[row][col] = SquareType::VERTICAL_PARTITION;
// 						GridPosition pos = {row, col};
// 						part.blockList.push_back(pos);
// 					}
//
// 					partitionList.push_back(part);
// 				}
// 			}
// 		}
// 		// case of a horizontal partition
// 		else
// 		{
// 			bool goodPart = false;
//
// 			//	I try a few times before giving up
// 			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
// 			{
// 				//	let's be hopeful
// 				goodPart = true;
//
// 				//	select a row index
// 				unsigned int row = rowDist(engine);
// 				unsigned int length = vertPartLengthDist(engine);
//
// 				//	now a random start row
// 				unsigned int startCol = 1 + colDist(engine) % (numCols-length-1);
// 				for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
// 				{
// 					if (grid[row][col] != SquareType::FREE_SQUARE)
// 						goodPart = false;
// 				}
//
// 				//	if the wall first, add it to the grid and build SlidingPartition object
// 				if (goodPart)
// 				{
// 					SlidingPartition part;
// 					part.isVertical = false;
// 					for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)  // NOLINT
// 					{
// 						grid[row][col] = SquareType::HORIZONTAL_PARTITION;
// 						GridPosition pos = {row, col};
// 						part.blockList.push_back(pos);
// 					}
//
// 					partitionList.push_back(part);
// 				}
// 			}
// 		}
// 	}
// }