//
//  gl_frontEnd.h
//  GL threads
//
//  Created by Jean-Yves Hervé on 2024-12-01
//

#ifndef GL_FRONT_END_H
#define GL_FRONT_END_H

//-----------------------------------------------------------------------------
//	Function prototypes
//-----------------------------------------------------------------------------

//	I don't want to impose how you store the information about your robots,
//	boxes and doors, so the two functions below will have to be called once for
//	each pair robot/box and once for each door.

//	This function draws together a robot and the box it is supposed to move
//	Since a robot corresponds to a box, they should have the same index in their
//	respective array, so only one id needs to be passed.
//	We also pass the number of the door assigned to the robot/box pair, so that
//	can display them all with a matching color
void drawRobotAndBox(int id,
					 int robotRow, int robotCol,
					 int boxRow, int boxCol,
					 int doorNumber);

void drawDoor(int doorNumber, int doorRow, int doorCol);
void drawGrid(void);
void drawPartitions(void);
void drawState(int numMessages, char** message);
void initializeFrontEnd(int argc, char** argv,
						void (*gridCB)(void), void (*stateCB)(void));

void speedupRobots(void);
void slowdownRobots(void);

#endif // GL_FRONT_END_H
