#include "identifier.h"

int learnBackground(IplImage* image, CvBGCodeBookModel* model, IplImage* background){
    background = cvCloneImage(image);
    cvCvtColor( image, background, CV_BGR2YCrCb );
    cvBGCodeBookUpdate( model, background, cvRect(0,0,0,0), 0 );

    return 0;
}

IplImage* segObjectBookBGDiff(CvBGCodeBookModel* model, IplImage* rawImage, IplImage* background){
    int i, j;
    IplImage* temp = cvCreateImage( cvGetSize(rawImage), IPL_DEPTH_8U, 1 );
    cvCvtColor( rawImage, background, CV_BGR2YCrCb );//YUV For codebook method
    cvBGCodeBookDiff( model, background, temp, cvRect(0,0,0,0) );
    cvSegmentFGMask( temp, 0, 4.f, 0, cvPoint(0,0) ); 

    int BGCOLOR_DEL = 0;
    CvScalar BGR;
    for(i = 0; i < temp->height; i++){
        for(j = 0; j < temp->width; j++){
            BGR = cvGet2D(temp,i,j);
            if(BGR.val[0] != BGCOLOR_DEL)
                cvSet2D(temp,i,j,cvGet2D(rawImage,i,j));
        }
    }
    return temp;
}

