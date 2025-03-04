/*  Copyright 2011 AIT Austrian Institute of Technology
*
*   This file is part of OpenTLD.
*
*   OpenTLD is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   OpenTLD is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with OpenTLD.  If not, see <http://www.gnu.org/licenses/>.
*
*/

/*
 * main.h
 *
 *  Created on: Nov 18, 2011
 *      Author: Georg Nebehay
 */

#ifndef MAIN_H_
#define MAIN_H_

#include "Trajectory.h"
#include "ros/ros.h"
#include "std_msgs/String.h"
#include "std_msgs/Time.h"
#include "std_msgs/Duration.h"
#include <geometry_msgs/PolygonStamped.h>
#include <sensor_msgs/Image.h>
#include "TLD.h"
#include "ImAcq.h"
#include "Gui.h"
#include "cv_bridge/cv_bridge.h"
#include <sensor_msgs/image_encodings.h>
#include <sepanta_msgs/command.h>

using namespace tld;
using namespace cv;

enum Retval
{
    PROGRAM_EXIT = 0,
    SUCCESS = 1
};

class Main
{
public:
	tld::TLD *tld;
	ImAcq *imAcq;
	tld::Gui *gui;
	bool showOutput;
	bool showTrajectory;
	int trajectoryLength;
	const char *printResults;
	const char *saveDir;
	double threshold;
	bool showForeground;
	bool showNotConfident;
	bool selectManually;
	int *initialBB;
	bool reinit;
	bool exportModelAfterRun;
	bool loadModel;
	const char *modelPath;
	const char *modelExportFile;

	int seed;
	//New
	bool keyboardControl;
	bool flag;
	bool reuseFrameOnce;
	bool skipProcessingOnce;
	ros::ServiceServer service_cmd;
	ros::NodeHandle my_node;
	FILE *resultsFile;
	Trajectory trajectory;
	//ROS
	ros::Publisher *poete;

	Main()
	{
		tld = new tld::TLD();
		
		/*if(!ros::param::get("/OpenTLD/Graphical_interface", showOutput)){
			showOutput=true;
		}*/

		printResults = NULL;
		saveDir = ".";
		threshold = 0.5;
		showForeground = 0;

		showTrajectory = false;
		trajectoryLength = 0;

		selectManually = 0;

		initialBB = NULL;
		showNotConfident = true;

		reinit = 0;

		loadModel = false;
		keyboardControl = true;
		exportModelAfterRun = false;
		modelExportFile = "model";
		seed = 0;
		//News
		flag=true;
		reuseFrameOnce = false;
		skipProcessingOnce = false;
		resultsFile = NULL;
	}

    ~Main()
    {
        delete tld;
        imAcqFree(imAcq);
    }

    bool callback_cmd(sepanta_msgs::command::Request& request, sepanta_msgs::command::Response& response);
	void doWork(const sensor_msgs::Image::ConstPtr& msg);
	void publish(cv::Rect *currBB);
	void loadRosparam();
	void init();
    
};

#endif /* MAIN_H_ */
