 /*-------------------------------------------------------------------------+
 |	Final Project CSC412 - Spring 2023										|
 |	A graphic front end for a box-pushing simulation.						|
 |																			|
 |	This application simply creates a glut window with a pane to display	|
 |	a colored grid and the other to display some state information.			|
 |	Sets up callback functions to handle menu, mouse and keyboard events.	|
 |	Normally, you shouldn't have to touch anything in this code, unless		|
 |	you want to change some of the things displayed, add menus, etc.		|
 |	Only mess with this after everything else works and making a backup		|
 |	copy of your project.  OpenGL & glut are tricky, and it's really easy	|
 |	to break everything with a single line of code.							|
 |																			|
 |	Created by Jean-Yves Hervé on 2018-12-05 (C version)					|
 |	Revised 2024-12-02														|
 +-------------------------------------------------------------------------*/

#include <vector>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <mutex>
#include "glPlatform.h"
#include "typesAndConstants.h"
#include "gl_frontEnd.h"

using namespace std;

#if 0
//-----------------------------------------------------------------
#pragma mark -
#pragma mark Application-wide global variables (do not touch)
//-----------------------------------------------------------------
#endif

//
extern int numRows;
extern int numCols;
extern int numBoxes;
extern int numDoors;
extern char** message;
extern int numLiveThreads;
extern int robotSleepTime;
extern vector<int> doorAssignments;
extern vector<GridPosition> robotLocations;
extern vector<GridPosition> boxLocations;
extern vector<GridPosition> doorLocations;
extern vector<bool> robotStatuses;
extern chrono::time_point<chrono::steady_clock> startTime;
extern vector<SlidingPartition> partitionList;
mutex drawStateMutex;

#if 0
//=================================================================
#pragma mark -
#pragma mark Prototypes of private functions
//=================================================================
#endif

void myResize(int w, int h);
void displayTextualInfo(const char* infoStr, int x, int y, FontSize fontSize);
void myMouse(int b, int s, int x, int y);
void myGridPaneMouse(int b, int s, int x, int y);
void myStatePaneMouse(int b, int s, int x, int y);
void myKeyboard(unsigned char c, int x, int y);
void myTimerFunc(int val);
void createDoorColors();
void freeDoorColors();
void drawMutexGrid();

#if 0
//=================================================================
#pragma mark -
#pragma mark Interface constants
//=================================================================
#endif

#define SMALL_DISPLAY_FONT    GLUT_BITMAP_HELVETICA_10
#define MEDIUM_DISPLAY_FONT   GLUT_BITMAP_HELVETICA_12
#define LARGE_DISPLAY_FONT    GLUT_BITMAP_HELVETICA_18
 constexpr int TEXT_PADDING = 0;
 constexpr float TEXT_COLOR[4] = {1.f, 1.f, 1.f, 1.f};
 constexpr float PARTITION_COLOR[4] = {0.6f, 0.6f, 0.6f, 1.f};
 constexpr int   INIT_WIN_X = 100,
				 INIT_WIN_Y = 40;

#if 0
//=================================================================
#pragma mark -
#pragma mark File-level global variables
//=================================================================
#endif

void (*gridDisplayFunc)();
void (*stateDisplayFunc)();

//	We use a window split into two panes/sub-windows.  The sub-windows
//	will be accessed by an index.
int	GRID_PANE = 0,
	STATE_PANE = 1;
int	gMainWindow,
	gSubWindow[2];
float** doorColor;


#if 0
//=================================================================
#pragma mark -
#pragma mark Drawing functions
//=================================================================
#endif

//---------------------------------------------------------------------------
//	These are the functions that tie the simulation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//---------------------------------------------------------------------------

void displayGridPane()
{
	// single-threaded version
//	for (all robots)
//	{
//		execute one move
//	}
	
	//	This is OpenGL/glut magic.  Do not touch
	//---------------------------------------------
	glutSetWindow(gSubWindow[GRID_PANE]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	// move to top of the pane
	glTranslatef(0.f, GRID_PANE_WIDTH, 0.f);
	// flip the vertical axis pointing down, in regular "grid" orientation
	glScalef(1.f, -1.f, 1.f);

	for (int i=0; i<numBoxes; i++)
	{
		if (robotStatuses[i])
		{
			drawRobotAndBox(i, robotLocations[i].row, robotLocations[i].col, boxLocations[i].row, boxLocations[i].col, doorAssignments[i]);
		}
	}

	for (int i=0; i<numDoors; i++)
	{
		//	here I would test if the robot thread is still alive
		drawDoor(i, doorLocations[i].row, doorLocations[i].col);
	}

	//	This call does nothing important. It only draws lines
	//	There is nothing to synchronize here
	drawGrid();

	//	Enable when you have a grid of mutex locks
	//drawMutexGrid();
	
	//	Enable this if you do the EC
	//drawPartitions();
	
	//	This is OpenGL/glut magic.  Do not touch
	glutSwapBuffers();	
	glutSetWindow(gMainWindow);
}

void displayStatePane()
{
	//	This is OpenGL/glut magic.  Do not touch
	glutSetWindow(gSubWindow[STATE_PANE]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//	Here I hard-code a few messages that I want to see displayed
	//	in my state pane.  The number of live robot threads will
	//	always get displayed.  No need to pass a message about it.
	int numMessages = 3;
	snprintf(message[0], 30, "We have %d doors", numDoors);
	snprintf(message[1], 30, "We have %d boxes", numBoxes);
	chrono::time_point<chrono::steady_clock> now = chrono::steady_clock::now();
	snprintf(message[2], 30, "Elapsed time: %ld", chrono::duration_cast<chrono::seconds>(now-startTime).count());
	// snprintf(message[2], 30, "System time is %ld", time(nullptr));
	
	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//
	//	You *must* synchronize this call.
	//
	//---------------------------------------------------------
	drawStateMutex.lock();
	drawState(numMessages, message);
	drawStateMutex.unlock();

	
	//	This is OpenGL/glut magic.  Do not touch
	glutSwapBuffers();
	
	glutSetWindow(gMainWindow);
}

//	This is the function that does the actual grid drawing
void drawGrid()
{
	// (the -1 business is a hack to make sure the top and bottom
	//	edges get drawn)
	const float	DH = (1.f*GRID_PANE_WIDTH-1) / numCols,
				DV = (1.f*GRID_PANE_HEIGHT-1) / numRows;

	//	Draw a grid of lines on top of the squares
	glColor4f(0.5f, 0.5f, 0.5f, 1.f);
	glBegin(GL_LINES);
		//	Horizontal
		for (int i=0; i<= numRows; i++)
		{
			glVertex2f(0, i*DV);
			glVertex2f(numCols*DH, i*DV);
		}
		//	Vertical
		for (int j=0; j<= numCols; j++)
		{
			glVertex2f(j*DH, 0);
			glVertex2f(j*DH, numRows*DV);
		}
	glEnd();
}


void drawRobotAndBox(int id,
					 int robotRow, int robotCol,
					 int boxRow, int boxCol,
					 int doorNumber)
{
	static const float	DH = 1.f*GRID_PANE_WIDTH / numCols,
						DV = 1.f*GRID_PANE_HEIGHT / numRows;

	//	my boxes are brown with a contour the same color as destination door
	glColor4f(0.25f, 0.25f, 0.f, 1.f);
	glPushMatrix();
	glTranslatef((boxCol+ 0.125f)*DH, (boxRow+0.125f)*DV, 0.f);
	glScalef(0.75f, 0.75f, 1.f);
	glBegin(GL_POLYGON);
		glVertex2f(0, 0);
		glVertex2f(DH, 0);
		glVertex2f(DH, DV);
		glVertex2f(0, DV);
	glEnd();
	glColor4fv(doorColor[doorNumber]);
	glBegin(GL_LINE_LOOP);
		glVertex2f(0, 0);
		glVertex2f(DH, 0);
		glVertex2f(DH, DV);
		glVertex2f(0, DV);
	glEnd();
	glPopMatrix();
	char boxStr[4];
	snprintf(boxStr, sizeof(boxStr), "B%d", id);
	displayTextualInfo(boxStr, (boxCol+0.3f)*DH, (boxRow+0.5f)*DV, SMALL_FONT_SIZE);

	//	my robots are dark gray with a contour the same color as destination door
	glColor4f(0.25f, 0.25f, 0.f, 1.f);
	glPushMatrix();
	glTranslatef((robotCol+0.5f)*DH, (robotRow+0.125f)*DV, 0.f);
	glScalef(0.375f, 0.375f, 31.f);
	glBegin(GL_POLYGON);
		glVertex2f(0, 0);
		glVertex2f(DH, DV);
		glVertex2f(0, 2*DV);
		glVertex2f(-DH, DV);
	glEnd();
	glColor4fv(doorColor[doorNumber]);
	glBegin(GL_LINE_LOOP);
		glVertex2f(0, 0);
		glVertex2f(DH, DV);
		glVertex2f(0, 2*DV);
		glVertex2f(-DH, DV);
	glEnd();
	glPopMatrix();
	char robotStr[4];
	snprintf(robotStr, sizeof(robotStr), "R%d", id);
	displayTextualInfo(robotStr, (robotCol+0.3f)*DH, (robotRow+0.5f)*DV, SMALL_FONT_SIZE);
}

//	This function assigns a color to the door based on its number
void drawDoor(int doorNumber, int doorRow, int doorCol)
{
	//	Yes, I know that it's inefficient to recompute this each and every time,
	//	but gcc on Ubuntu doesn't let me define these as static [!??]
	const float	DH = 1.f*GRID_PANE_WIDTH / numCols,
				DV = 1.f*GRID_PANE_HEIGHT / numRows;

	glColor4fv(doorColor[doorNumber]);
	glBegin(GL_POLYGON);
		glVertex2f(doorCol*DH, doorRow*DV);
		glVertex2f((doorCol+1)*DH, doorRow*DV);
		glVertex2f((doorCol+1)*DH, (doorRow+1)*DV);
		glVertex2f(doorCol*DH, (doorRow+1)*DV);
	glEnd();
	
	//	A little black square underneath the text
	glColor4f(0.f, 0.f, 0.f, 1.f);
	glBegin(GL_POLYGON);
		glVertex2f((doorCol+0.15f)*DH, (doorRow+0.30f)*DV);
		glVertex2f((doorCol+0.15f)*DH, (doorRow+0.55f)*DV);
		glVertex2f((doorCol+0.65f)*DH, (doorRow+0.55f)*DV);
		glVertex2f((doorCol+0.65f)*DH, (doorRow+0.30f)*DV);
	glEnd();
	
	char doorStr[4];
	snprintf(doorStr, sizeof(doorStr), "D%d", doorNumber);
	displayTextualInfo(doorStr, (doorCol+0.25f)*DH, (doorRow+0.5f)*DV, SMALL_FONT_SIZE);
}

void drawPartitions()
{
	static const GLfloat	DH = (GRID_PANE_WIDTH - 2.f)/ numCols,
							DV = (GRID_PANE_HEIGHT - 2.f) / numRows;
	static const GLfloat	PS = 0.3f, PE = 1.f - PS;

	for (SlidingPartition part : partitionList)
	{
		unsigned int startRow = part.blockList[0].row,
					 startCol = part.blockList[0].col;

		glColor4fv(PARTITION_COLOR);
		if (part.isVertical)
		{
			glBegin(GL_POLYGON);
				glVertex2f((startCol+PS)*DH, startRow*DV);
				glVertex2f((startCol+PS)*DH, (startRow+part.blockList.size())*DV);
				glVertex2f((startCol+PE)*DH, (startRow+part.blockList.size())*DV);
				glVertex2f((startCol+PE)*DH, startRow*DV);
			glEnd();
		}
		else
		{
			glBegin(GL_POLYGON);
				glVertex2f(startCol*DH, (startRow+PS)*DV);
				glVertex2f(startCol*DH, (startRow+PE)*DV);
				glVertex2f((startCol+part.blockList.size())*DH, (startRow+PE)*DV);
				glVertex2f((startCol+part.blockList.size())*DH, (startRow+PS)*DV);
			glEnd();
		}
	}
}

void drawMutexGrid()
{
	static const float	DH = 1.f*GRID_PANE_WIDTH / numCols,
						DV = 1.f*GRID_PANE_HEIGHT / numRows;

	//	draw the walls and partitions
	for (int i = 0; i < numRows; i++)
	{
		for (int j = 0; j < numCols; j++)
		{
//	Enable when your 2D grid of mutexes exists
// 			if (lockGrid[i][j].try_lock())
// 			{
// 				lockGrid[i][j].unlock();
// 			}
// 			else
			{
				//	     red  green blue
				glColor4f(1.f, 0.f, 1.f, 1.f);

				constexpr float LOCK_SIZE = 0.2f;	//	fraction of square size
				glPushMatrix();
				glTranslatef(j*DH, (i+1.f-LOCK_SIZE)*DV, 0.f);
				glScalef(LOCK_SIZE*DH, LOCK_SIZE*DV, 1.f);
				glBegin(GL_POLYGON);
					glVertex2f(0.f, 0.f);
					glVertex2f(0.f, 1.f);
					glVertex2f(1.f, 1.f);
					glVertex2f(1.f, 0.0f);
				glEnd();
				glPopMatrix();
			}
		}
	}
}


void displayTextualInfo(const char* infoStr, int xPos, int yPos, FontSize fontSize)
{
    //-----------------------------------------------
    //  0.  get current material properties
    //-----------------------------------------------
    float oldAmb[4], oldDif[4], oldSpec[4], oldShiny;
    glGetMaterialfv(GL_FRONT, GL_AMBIENT, oldAmb);
    glGetMaterialfv(GL_FRONT, GL_DIFFUSE, oldDif);
    glGetMaterialfv(GL_FRONT, GL_SPECULAR, oldSpec);
    glGetMaterialfv(GL_FRONT, GL_SHININESS, &oldShiny);

    glPushMatrix();

    //-----------------------------------------------
    //  1.  Build the string to display <-- parameter
    //-----------------------------------------------
    int infoLn = static_cast<int>(strlen(infoStr));

    //-----------------------------------------------
    //  2.  Determine the string's length (in pixels)
    //-----------------------------------------------
    int textWidth = 0; (void) textWidth;
	switch(fontSize)
	{
		case SMALL_FONT_SIZE:
			for (int k=0; k<infoLn; k++)
			{
				textWidth += glutBitmapWidth(SMALL_DISPLAY_FONT, infoStr[k]);
			}
			break;
		
		case MEDIUM_FONT_SIZE:
			for (int k=0; k<infoLn; k++)
			{
				textWidth += glutBitmapWidth(MEDIUM_DISPLAY_FONT, infoStr[k]);
			}
			break;
		
		case LARGE_FONT_SIZE:
			for (int k=0; k<infoLn; k++)
			{
				textWidth += glutBitmapWidth(LARGE_DISPLAY_FONT, infoStr[k]);
			}
			break;
			
		default:
			break;
	}
		
	//  add a few pixels of padding
    textWidth += 2 * TEXT_PADDING;  // NOLINT
	
    //-----------------------------------------------
    //  4.  Draw the string
    //-----------------------------------------------    
    glColor4fv(TEXT_COLOR);
    int x = xPos;
	switch(fontSize)
	{
		case SMALL_FONT_SIZE:
			for (int k=0; k<infoLn; k++)
			{
				glRasterPos2i(x, yPos);
				glutBitmapCharacter(SMALL_DISPLAY_FONT, infoStr[k]);
				x += glutBitmapWidth(SMALL_DISPLAY_FONT, infoStr[k]);
			}
			break;
		
		case MEDIUM_FONT_SIZE:
			for (int k=0; k<infoLn; k++)
			{
				glRasterPos2i(x, yPos);
				glutBitmapCharacter(MEDIUM_DISPLAY_FONT, infoStr[k]);
				x += glutBitmapWidth(MEDIUM_DISPLAY_FONT, infoStr[k]);
			}
			break;
		
		case LARGE_FONT_SIZE:
			for (int k=0; k<infoLn; k++)
			{
				glRasterPos2i(x, yPos);
				glutBitmapCharacter(LARGE_DISPLAY_FONT, infoStr[k]);
				x += glutBitmapWidth(LARGE_DISPLAY_FONT, infoStr[k]);
			}
			break;
			
		default:
			break;
	}

    //-----------------------------------------------
    //  5.  Restore old material properties
    //-----------------------------------------------
	glMaterialfv(GL_FRONT, GL_AMBIENT, oldAmb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, oldDif);
	glMaterialfv(GL_FRONT, GL_SPECULAR, oldSpec);
	glMaterialf(GL_FRONT, GL_SHININESS, oldShiny);  
    
    //-----------------------------------------------
    //  6.  Restore reference frame
    //-----------------------------------------------
    glPopMatrix();
}



void drawState(int numMessages, char** message)
{
	//	I compute once the dimensions for all the rendering of my state info
	//	One other place to rant about that desperately lame gcc compiler.  It's
	//	positively disgusting that the code below is rejected.
	int LEFT_MARGIN = STATE_PANE_WIDTH / 12;
	int V_PAD = STATE_PANE_HEIGHT / 12;

	for (int k=0; k<numMessages; k++)
	{
		displayTextualInfo(	message[k],
							LEFT_MARGIN,
							3*STATE_PANE_HEIGHT/4 - k*V_PAD,
							LARGE_FONT_SIZE);
	}

	//	display info about number of live threads
	char infoStr[256];
	snprintf(infoStr, 30, "Live Threads: %d", numLiveThreads);
	displayTextualInfo(infoStr, LEFT_MARGIN, 7*STATE_PANE_HEIGHT/8, LARGE_FONT_SIZE);
}

#if 0
//=================================================================
#pragma mark -
#pragma mark Callback functions
//=================================================================
#endif

//	This callback function is called when the window is resized
//	(generally by the user of the application).
//	It is also called when the window is created, why I placed there the
//	code to set up the virtual camera for this application.
//
void myResize(int w, int h)
{
	if ((w != WINDOW_WIDTH) || (h != WINDOW_HEIGHT))
	{
		glutReshapeWindow(WINDOW_WIDTH, WINDOW_HEIGHT);
	}
	else
	{
		glutPostRedisplay();
	}
}


void myDisplay()
{
    glutSetWindow(gMainWindow);

    glMatrixMode(GL_MODELVIEW);
    glClear(GL_COLOR_BUFFER_BIT);
    glutSwapBuffers();

	gridDisplayFunc();
	stateDisplayFunc();
	
    glutSetWindow(gMainWindow);	
}

//	This function is called when a mouse event occurs just in the tiny
//	space between the two sub-windows.
//
void myMouse(int button, int state, int x, int y)
{
	glutSetWindow(gMainWindow);
	glutPostRedisplay();
}

//	This function is called when a mouse event occurs in the grid pane
//
void myGridPaneMouse(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON)
	{
		if (state == GLUT_DOWN)
		{
			(void) button;  // quiets warnings
			//	do something
		}
		else if (state == GLUT_UP)
		{
			//	exit(0);
		}
	}
	// switch (button)
	// {
	// 	case GLUT_LEFT_BUTTON:
	// 		if (state == GLUT_DOWN)
	// 		{
	// 			//	do something
	// 		}
	// 		else if (state == GLUT_UP)
	// 		{
	// 			//	exit(0);
	// 		}
	// 		break;
	//
	// 	default:
	// 		break;
	// }

	glutSetWindow(gMainWindow);
	glutPostRedisplay();
}

//	This function is called when a mouse event occurs in the state pane
void myStatePaneMouse(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON)
	{
		if (state == GLUT_DOWN)
		{
			(void) button;  // quiets warnings
			//	do something
		}
		else if (state == GLUT_UP)
		{
			//	exit(0);
		}
	}
	// switch (button)
	// {
	// 	case GLUT_LEFT_BUTTON:
	// 		if (state == GLUT_DOWN)
	// 		{
	// 			//	do something
	// 		}
	// 		else if (state == GLUT_UP)
	// 		{
	// 			//	exit(0);
	// 		}
	// 		break;
	//
	// 	default:
	// 		break;
	// }

	glutSetWindow(gMainWindow);
	glutPostRedisplay();
}


//	This callback function is called when a keyboard event occurs
//
void myKeyboard(unsigned char c, int x, int y)
{
	int ok = 0;
	
	switch (c)
	{
		//	'esc' to quit
		case 27:
			exit(0);
			break;  // NOLINT

		//	slowdown
		case ',':
			slowdownRobots();
			break;

		//	speedup
		case '.':
			speedupRobots();
			break;

		default:
			ok = 1;
			break;
	}
	if (!ok)
	{
		//	do something?
	}
	
	glutSetWindow(gMainWindow);
	glutPostRedisplay();
}

//------------------------------------------------------------------------
//	You shouldn't have to touch this one.  Definitely if you don't
//	add the "producer" threads, and probably not even if you do.
//------------------------------------------------------------------------
void speedupRobots()
{
	//	decrease sleep time by 20%, but don't get too small
	int newSleepTime = (8 * robotSleepTime) / 10;
	
	if (newSleepTime > MIN_SLEEP_TIME)
	{
		robotSleepTime = newSleepTime;
	}
}

void slowdownRobots()
{
	//	increase sleep time by 20%
	robotSleepTime = (12 * robotSleepTime) / 10;
}


void myTimerFunc(int val)
{
	//	Re-prime the timer
    glutTimerFunc(10, myTimerFunc, val);
    
	//	And finally I perform the rendering
	glutPostRedisplay();
}

#if 0
//=================================================================
#pragma mark -
#pragma mark Initialization & cleanup
//=================================================================
#endif

void initializeFrontEnd(int argc, char** argv, void (*gridDisplayCB)(),
						void (*stateDisplayCB)())
{
	//	Initialize glut and create a new window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);


	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutInitWindowPosition(INIT_WIN_X, INIT_WIN_Y);
	gMainWindow = glutCreateWindow("Box Pushing -- CSC 412 - Spring 2023");
	glClearColor(0.2f, 0.2f, 0.2f, 1.f);
	
	//	set up the callbacks for the main window
	glutDisplayFunc(myDisplay);
	glutReshapeFunc(myResize);
	glutMouseFunc(myMouse);
    glutTimerFunc(10, myTimerFunc, 0);
	
	gridDisplayFunc = gridDisplayCB;
	stateDisplayFunc = stateDisplayCB;
	
	//	create the two panes as glut sub-windows
	gSubWindow[GRID_PANE] = glutCreateSubWindow(gMainWindow,
												0, 0,							//	origin
												GRID_PANE_WIDTH, GRID_PANE_HEIGHT);
    glViewport(0, 0, GRID_PANE_WIDTH, GRID_PANE_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glOrtho(0.0f, GRID_PANE_WIDTH, 0.0f, GRID_PANE_HEIGHT, -1, 1);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glutKeyboardFunc(myKeyboard);
	glutMouseFunc(myGridPaneMouse);
	glutDisplayFunc(gridDisplayCB);
	
	
	glutSetWindow(gMainWindow);
	gSubWindow[STATE_PANE] = glutCreateSubWindow(gMainWindow,
												GRID_PANE_WIDTH + H_PADDING, 0,	//	origin
												STATE_PANE_WIDTH, STATE_PANE_HEIGHT);
    glViewport(0, 0, STATE_PANE_WIDTH, STATE_PANE_WIDTH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glOrtho(0.0f, STATE_PANE_WIDTH, 0.0f, STATE_PANE_HEIGHT, -1, 1);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glutKeyboardFunc(myKeyboard);
	glutMouseFunc(myGridPaneMouse);
	glutDisplayFunc(stateDisplayCB);
	
	createDoorColors();
}

void createDoorColors()
{
	doorColor = static_cast<float **>(malloc(numDoors * sizeof(float *)));

	float hueStep = 360.f / numDoors;
	
	for (int k=0; k<numDoors; k++)
	{
		doorColor[k] = static_cast<float *>(malloc(4 * sizeof(float)));
		
		//	compute a hue for the door
		float hue = k*hueStep;
		//	convert the hue to an RGB color
		switch (static_cast<int>(hue / 60))
		{
				//  hue in [0, 60] -- red-green, dominant red
			case 0:
				doorColor[k][0] = 1.f;					//  red is max
				doorColor[k][1] = hue / 60.f;			//  green calculated
				doorColor[k][2] = 0.f;					//  blue is zero
				break;

				//  hue in [60, 120] -- red-green, dominant green
			case 1:
				doorColor[k][0] = (120.f - hue) / 60.f;	//  red is calculated
				doorColor[k][1] = 1.f;					//  green max
				doorColor[k][2] = 0.f;					//  blue is zero
				break;

				//  hue in [120, 180] -- green-blue, dominant green
			case 2:
				doorColor[k][0] = 0.f;					//  red is zero
				doorColor[k][1] = 1.f;					//  green max
				doorColor[k][2] = (hue - 120.f) / 60.f;	//  blue is calculated
				break;

				//  hue in [180, 240] -- green-blue, dominant blue
			case 3:
				doorColor[k][0] = 0.f;					//  red is zero
				doorColor[k][1] = (240.f - hue) / 60;	//  green calculated
				doorColor[k][2] = 1.f;					//  blue is max
				break;

				//  hue in [240, 300] -- blue-red, dominant blue
			case 4:
				doorColor[k][0] = (hue - 240.f) / 60;	//  red is calculated
				doorColor[k][1] = 0;						//  green is zero
				doorColor[k][2] = 1.f;					//  blue is max
				break;

				//  hue in [300, 360] -- blue-red, dominant red
			case 5:
				doorColor[k][0] = 1.f;					//  red is max
				doorColor[k][1] = 0;						//  green is zero
				doorColor[k][2] = (360.f - hue) / 60;	//  blue is calculated
				break;

			default:
				break;

		}
		doorColor[k][3] = 1.f;					//  alpha --> full opacity
	}
}

void freeDoorColors()
{
	for (int k=0; k<numDoors; k++)
		free(doorColor[k]);
	
	free(doorColor);
}
