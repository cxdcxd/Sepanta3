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
 * MainX.cpp
 *
 *  Created on: Nov 17, 2011
 *      Author: Georg Nebehay
 */


#include "Main.h"
#include "Config.h"
#include "ImAcq.h"
#include "Gui.h"
#include "TLDUtil.h"
#include "Trajectory.h"
#include "Timing.h"


using namespace tld;
using namespace cv;
Mat grey;

bool  Main::callback_cmd(sepanta_msgs::command::Request& request, sepanta_msgs::command::Response& response)
{

    if ( request.id == "learn_on" )
    {
          tld->learningEnabled = true;
    }
    else
    if ( request.id == "learn_off" )
    {
           tld->learningEnabled = false;
    }
    else
    if ( request.id == "setrec" )
    {
    	 int a = request.value1;
    	 int b = request.value2;
    	 int c = request.value3;
    	 int d = request.value4;

    	 CvRect box;
    	 box.x = a;
    	 box.y = b;
    	 box.width = c;
    	 box.height = d;
		
		 Rect r = Rect(box);
		 tld->selectObject(grey, &r);
    }
    
    return true;
}

void Main::init()
{
	service_cmd = my_node.advertiseService("opentld_cmd", &Main::callback_cmd , this);
}

void Main::doWork(const sensor_msgs::Image::ConstPtr& msg)
{
	//Get the image to work from sensor to openCV
	//CvImagePtr toCvCopy(const sensor_msgs::ImageConstPtr& source, const std::string& encoding = std::string());
	//CvImagePtr toCvCopy(const sensor_msgs::Image& source, const std::string& encoding = std::string());
	
	if(ros::Time::now()-msg->header.stamp<ros::Duration(0.1)){
		cv_bridge::CvImagePtr img2 =cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::RGB8);
						 
		Mat img(img2->image);
		
		cvtColor(img, grey, CV_BGR2GRAY);

		if(flag==true){
		
			std::cout << "INIT"<<std::endl;
			tld->detectorCascade->setImgSize(grey.cols, grey.rows, grey.step);

			#ifdef CUDA_ENABLED
			tld->learningEnabled = false;
			selectManually = false;

			if(tld->learningEnabled || selectManually)
				std::cerr << "Sorry. Learning and manual object selection is not supported with CUDA implementation yet!!!" << std::endl;
			#endif

			if(showTrajectory){
				trajectory.init(trajectoryLength);
			}

			if(selectManually){
				CvRect box;
				if(getBBFromUser(&img, box, gui) == PROGRAM_EXIT){
					return;
				}

				if(initialBB == NULL){
					initialBB = new int[4];
				}

				initialBB[0] = box.x;
				initialBB[1] = box.y;
				initialBB[2] = box.width;
				initialBB[3] = box.height;
			}

			if(printResults != NULL){
				resultsFile = fopen(printResults, "w");
			}

			if(loadModel && modelPath != NULL){
				tld->readFromFile(modelPath);
				reuseFrameOnce = true;
			}
			else if(initialBB != NULL){
				Rect bb = tldArrayToRect(initialBB);

				printf("Starting at %d %d %d %d\n", bb.x, bb.y, bb.width, bb.height);

				tld->selectObject(grey, &bb);
				skipProcessingOnce = true;
				reuseFrameOnce = true;
			}
			flag=false;
		}

	/****************main while*********************/

		//while(imAcqHasMoreFrames(imAcq)){    //Just a normal while condition
		//imAcqHasMoreFrames(imAcq);
		tick_t procInit, procFinal;
		double tic = cvGetTickCount();

		if(!skipProcessingOnce){
			//std::cout<<"Skip"<<std::endl;
			getCPUTick(&procInit);
			tld->processImage(img);
			getCPUTick(&procFinal);
			//PRINT_TIMING("FrameProcTime", procInit, procFinal, "\n");
		}
		else{
			skipProcessingOnce = false;
		}

		/*******************PRINT ON THE TERMINAL************************/
		if(printResults != NULL){
			if(tld->currBB != NULL){
				fprintf(resultsFile, "%d %.2d %.2d %.2d %.2d %f\n", imAcq->currentFrame - 1, tld->currBB->x, tld->currBB->y, tld->currBB->width, tld->currBB->height, tld->currConf);
			}
			else{//
				fprintf(resultsFile, "%d NaN NaN NaN NaN NaN\n", imAcq->currentFrame - 1);
			}
		}

		double toc = (cvGetTickCount() - tic) / cvGetTickFrequency();
		toc = toc / 1000000;
		float fps = 1 / toc;
		int confident = (tld->currConf >= threshold) ? 1 : 0;

		if(showOutput || saveDir != NULL){
			char string[128];
			char learningString[10] = "";
			if(tld->learning){
				strcpy(learningString, "Learning");
			}

			sprintf(string, "#%d,Posterior %.2f; fps: %.2f, #numwindows:%d, %s", imAcq->currentFrame - 1, tld->currConf, fps, tld->detectorCascade->numWindows, learningString);

			CvScalar yellow = CV_RGB(255, 255, 0);
			CvScalar blue = CV_RGB(0, 0, 255);
			CvScalar black = CV_RGB(0, 0, 0);
			CvScalar white = CV_RGB(255, 255, 255);

			if(tld->currBB != NULL){
				//Draw the rectangle here this is what need to be sent by the publisher ! ;)
				CvScalar rectangleColor = (confident) ? blue : yellow;
				rectangle(img, tld->currBB->tl(), tld->currBB->br(), rectangleColor, 8, 8, 0);
				if(showTrajectory){
					CvPoint center = cvPoint(tld->currBB->x+tld->currBB->width/2, tld->currBB->y+tld->currBB->height/2);
					line(img, cvPoint(center.x-2, center.y-2), cvPoint(center.x+2, center.y+2), rectangleColor, 2);
					line(img, cvPoint(center.x-2, center.y+2), cvPoint(center.x+2, center.y-2), rectangleColor, 2);
					line(img, cvPoint(center.x-2, center.y-2), cvPoint(center.x+2, center.y+2), rectangleColor, 2);
					line(img, cvPoint(center.x-2, center.y+2), cvPoint(center.x+2, center.y-2), rectangleColor, 2);
					trajectory.addPoint(center, rectangleColor);
				}
			}
			else if(showTrajectory){
				trajectory.addPoint(cvPoint(-1, -1), cvScalar(-1, -1, -1));
			}

			if(showTrajectory){
			trajectory.drawTrajectory(img);
			}

			int fontFace = FONT_HERSHEY_SCRIPT_SIMPLEX;
			rectangle(img, cvPoint(0, 0), cvPoint(grey.cols, 50), black, CV_FILLED, 8, 0);
			putText(img, string, cvPoint(25, 25), fontFace, 0.5, Scalar::all(255), 1, 8);

			if(showForeground){
				for(size_t i = 0; i < tld->detectorCascade->detectionResult->fgList->size(); i++){
					Rect r = tld->detectorCascade->detectionResult->fgList->at(i);
					rectangle(img, r.tl(), r.br(), white, 1);
				}
			}


			if(showOutput){
				/*GUI and keyboard controls************************/
				gui->showImage(img);

				/********************KEYBOARD**************/
				char key = gui->getKey();
				if(keyboardControl==true){
					//if(key == 'q') break;

					//if(key == 'b'){
					//    ForegroundDetector *fg = tld->detectorCascade->foregroundDetector;
					//    if(fg->bgImg.empty()){
					//    fg->bgImg = grey.clone();
					// }
					// else{
					//      fg->bgImg.release();
					//  }
					//}


					if(key == 'c'){
						//clear everything
						tld->release();
					}

					if(key == 'l'){
						tld->learningEnabled = !tld->learningEnabled;
						printf("LearningEnabled: %d\n", tld->learningEnabled);
					}

					if(key == 'a'){
						tld->alternating = !tld->alternating;
						printf("alternating: %d\n", tld->alternating);
					}

					if(key == 'e'){
						tld->writeToFile(modelExportFile);
					}

					if(key == 'i'){
						tld->readFromFile(modelPath);
					}

					if(key == 'r'){
						CvRect box;
						if(getBBFromUser(&img, box, gui) == PROGRAM_EXIT){
							//  break;
							exit(0);
						}
						Rect r = Rect(box);
						tld->selectObject(grey, &r);
					}
				}
		/***************************************************/
			}

			if(saveDir != NULL){
				char fileName[256];
				sprintf(fileName, "%s/%.5d.png", saveDir, imAcq->currentFrame - 1);

				imwrite(fileName, img);
			}
		}
		else{
			reuseFrameOnce = false;
		}

		(*this).publish(tld->currBB);

		if(exportModelAfterRun)
		{
			tld->writeToFile(modelExportFile);
		}
	}
	else{
		//std::cout<<"TROP LONG ;)"<<std::endl;
	}
}

/*********************PUBLISH FUNCtiON**********************/

void Main::publish(cv::Rect *currBB){
	geometry_msgs::PolygonStamped polyStamp;
	polyStamp.header.stamp=ros::Time::now();
	
	//Calcul du rectangle
	geometry_msgs::Point32 tl;
	geometry_msgs::Point32 br;
	if(currBB!=NULL){ 
		polyStamp.header.frame_id="/camera";
		tl.x=currBB->x; tl.y=currBB->y;
	
		br.x=currBB->x+currBB->width; br.y=currBB->y+currBB->height;
	
		tl.z=br.z=0;

	}
	else{
		polyStamp.header.frame_id="NONE";
		tl.x=-1; tl.y=-1;
	
		br.x=-1; br.y=-1;
	
		tl.z=br.z=0;

	}
	polyStamp.polygon.points.push_back(tl);
	polyStamp.polygon.points.push_back(br);
	poete->publish(polyStamp);
}



/*******************Param FUNTION**************************/


void Main::loadRosparam(){
	ros::param::param<bool>("/OpenTLD/Graphical_interface", showOutput, true);
	ros::param::param<bool>("/OpenTLD/ShowTrajectory", showTrajectory, false);
	ros::param::param<int>("/OpenTLD/Trajectory_length", trajectoryLength, 0);

}
