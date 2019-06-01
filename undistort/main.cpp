#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <opencv2/opencv.hpp>
#include "undistort.h"

void getCameraRT(cv::Mat &rvec, cv::Mat &tvec, cv::Mat &RT);
void undistort_point(int x, int y, int width, int height, cv::Mat &cameraMatrix, cv::Mat &distCoeffs, int *rx, int *ry);
void get_chess_points3f(std::vector<cv::Point3f> &, int,int,int);
void detect_board(cv::Mat &src, cv::Size p_size, std::vector<cv::Point2f> &corners, int draw);

int main(int argc, char **argv){
    int board_col;
    int board_row;
    int board_size;
    printf("Input chess board cols:");
    scanf("%d",&board_col);
    printf("Input chess board rows:");
    scanf("%d",&board_row);
    printf("Input size of pattern:");
    scanf("%d",&board_size);
    Undistort::board_T chessboard = {board_col,board_row,board_size};
    int capture_device = 0;
    cv::VideoCapture cam;
    if(argc == 2) capture_device = atoi(argv[1]);
    if(capture_device >= 0){
        //cam.set(CV_CAP_PROP_FPS, 30);
        cam.open(capture_device);
        cam.set(CV_CAP_PROP_FRAME_WIDTH, 1280);
        cam.set(CV_CAP_PROP_FRAME_HEIGHT, 1024);
    }else{
        cam.open(0);
    }
    if(!cam.isOpened()){
        printf("device cannot open.");
        return -1;
    }
    Undistort ud;
    std::vector<cv::Mat> caps;
    cv::Mat frame;
    int num=0;
    int undistort_flag = 0;
    while(cam.read(frame)){
        cv::Mat img = frame.clone();
        std::vector<cv::Point2f> corners;
        if(undistort_flag == 1){
            /* undistort */
            cv::Mat img_und;
            ud.undistortImage(frame,img_und);
            /* solvepnp */
            cv::Mat cameraRT;
            int ret = ud.solvePnP(frame,chessboard,cameraRT);
            if(ret == 0){
                std::cout << " ----- " << std::endl;
                std::cout << cameraRT << std::endl;
                /* drawing */
                int l = 4 * board_size;
                std::vector<cv::Point3f> axis = {cv::Point3f(0,0,0),cv::Point3f(l,0,0),cv::Point3f(0,l,0),cv::Point3f(0,0,-l)};
                std::vector<cv::Point2f> imgPts;
                ud.projectPoints(axis, cameraRT, img.cols, img.rows, imgPts);
                if(imgPts.size() == 4){
                    cv::line(img,imgPts[0],imgPts[1],cv::Scalar(255,0,0), 5); // x: red
                    cv::line(img,imgPts[0],imgPts[2],cv::Scalar(0,255,0), 5); // y: green
                    cv::line(img,imgPts[0],imgPts[3],cv::Scalar(0,0,255), 5); // -z: blue
                }
            }
            /* undistort_points */
            for(int cx=50; cx<img.cols-50; cx+=10){
                for(int cy=50; cy<img.rows-50; cy+=10){
                    float color_col = float(cx) / img.cols;
                    float color_row = float(cy) / img.rows;
                    cv::circle(img,cv::Point2d(cx,cy),2,cv::Scalar(0,255*color_row,255*color_col),-1);
                    float rx, ry;
                    ud.undistortPoints(cx,cy,img.cols,img.rows,&rx,&ry );
                    rx = rx * img.cols;
                    ry = ry * img.cols;
                    cv::circle(img_und,cv::Point2d(rx,ry),2,cv::Scalar(0,255*color_row,255*color_col),-1);
                }
            }
            cv::imshow("undistort",img_und);
            /* Remap */
            /*
            cv::Mat newCameraMat = cv::getOptimalNewCameraMatrix(cameraMatrix,distCoeffs,img.size(),1);
            cv::Mat map1, map2, R;
            cv::initUndistortRectifyMap(cameraMatrix,distCoeffs,R,newCameraMat,img.size(),CV_32FC1,map1,map2);
            cv::Mat img_remap;
            cv::remap(frame,img_remap,map1,map2,cv::INTER_NEAREST);
            cv::imshow("remap",img_remap);
            */
        }
        else{
            detect_board(img,cv::Size(board_col-1, board_row-1),corners,1 );
        }
        cv::imshow("cam",img);
        const int key=cv::waitKey(1) & 0xff;
        if(key == 'q'){
            break;
        }
        else if(key == 'a'){
            if(corners.size() == ((chessboard.cols-1)*(chessboard.rows-1))){
                cv::Mat m = frame.clone();
                caps.push_back(m);
                num++;
                std::cout << "num:" << num <<std::endl;
            }
        }
        else if(key == 'c'){
            if(num > 0){
                ud.calibrateCamera(caps, chessboard);
                ud.writeCameraParams("camera_parameter.txt");
                undistort_flag = 1;
            }
        }
    }
    for(int i=0;i<caps.size();i++){
        char fn[256];
        sprintf(fn,"board%03d.png",i);
        cv::imwrite(fn,caps[i]);
    }
    return 0;
}

void getCameraRT(cv::Mat &rvec, cv::Mat &tvec, cv::Mat &RT){
    cv::Mat camera_rt = cv::Mat::zeros(4,4,CV_64FC1);
    cv::Mat Rotate;
    cv::Rodrigues(rvec,Rotate);
    Rotate.copyTo(camera_rt(cv::Rect(0,0,3,3)));
    tvec.copyTo(camera_rt(cv::Rect(3,0,1,3)));
    RT = camera_rt.clone();
}

void undistort_point(int xi, int yi, int width, int height,cv::Mat &cameraMatrix, cv::Mat &distCoeffs, int *rx, int *ry){
    float x = float(xi);
    float y = float(yi);
    cv::Mat src(1, 1, CV_64FC2);
    std::vector<cv::Point2d> dst(1);

    src.at<cv::Vec2d>(0, 0)[0] = x;
    src.at<cv::Vec2d>(0, 0)[1] = y;
    
    cv::Mat R,P;
    cv::undistortPoints(src, dst, cameraMatrix, distCoeffs,R,P=cameraMatrix);
    //std::cout << " ------ " << std::endl;
    //std::cout << "x:" << x << " -> ux:" << dst[0].x << std::endl;
    //std::cout << "y:" << y << " -> uy:" << dst[0].y << std::endl;
    *rx = int(dst[0].x);
    *ry = int(dst[0].y);
}

void get_chess_points3f(std::vector<cv::Point3f> &objPoint, int cols, int rows, int patternSize){
    for(int y=0; y<rows; y++){
        for(int x=0; x<cols; x++){
            cv::Point3f p(x*patternSize, y*patternSize, 0.0f);
            objPoint.push_back(p);
        }
    }
}

void detect_board(cv::Mat &src, cv::Size p_size, std::vector<cv::Point2f> &corners, int draw=0){
    cv::Mat gray;
    cv::cvtColor(src,gray,CV_BGR2GRAY);
    bool ret = cv::findChessboardCorners(gray,p_size, corners,CV_CALIB_CB_ADAPTIVE_THRESH + CV_CALIB_CB_NORMALIZE_IMAGE + CV_CALIB_CB_FAST_CHECK);
    if(ret){
        cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),cv::TermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 30, 0.1));
    }
    if(draw){
        cv::drawChessboardCorners(src, p_size, cv::Mat(corners), ret);
    }
}
