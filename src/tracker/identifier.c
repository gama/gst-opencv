#include "identifier.h"

CvRect onlyBiggerObject(IplImage* frameBW){

    CvSeq *c, *cBig;
    CvMemStorage *tempStorage = cvCreateMemStorage(0);
    CvMat mstub;
    CvMat *mask = cvGetMat(frameBW, &mstub, 0, 0);
    CvPoint offset = cvPoint(0,0);
    CvRect rectRoi = cvRect(0, 0, 0, 0);

    CvContourScanner scanner = cvStartFindContours(mask, tempStorage,
        sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE,offset );

    // Identifies largest object
    CvSlice cvslice = { 0, 0x3fffffff };
    double area, areaBig = -1;
    while( (c = cvFindNextContour( scanner )) != 0 ){
        area = fabs(cvContourArea(c,cvslice));
        if(area > areaBig){
            areaBig = area;
            cBig = c;
        }
    }

    // If there is any object, it leaves only the image
    if(areaBig != -1){
        cvZero( mask );
        cvDrawContours( mask, cBig, cvScalarAll(255), cvScalarAll(0), -1,
                CV_FILLED, 8, cvPoint(-offset.x,-offset.y));
        rectRoi = cvBoundingRect(cBig, 0);
    }

    cvReleaseMemStorage(&tempStorage);
    return rectRoi;
}

CvRect segObjectBookBGDiff(CvBGCodeBookModel* model, IplImage* rawImage,
        IplImage* yuvImage){

    CvRect rectRoi;
    IplImage* temp = cvCreateImage(cvGetSize(rawImage), IPL_DEPTH_8U, 1);
    
    // Background subtraction
    cvCvtColor( rawImage, yuvImage, CV_BGR2YCrCb );
    cvBGCodeBookDiff( model, yuvImage, temp, cvRect(0,0,0,0) );
    cvSegmentFGMask(temp, 1, 4.f, 0, cvPoint(0,0));
    rectRoi = onlyBiggerObject(temp);

    cvReleaseImage(&temp);
    return rectRoi;
}

void canny(IplImage *image, CvRect rect, int edge_thresh, int smooth){

    if(rect.height == 0 || rect.width == 0) return;
    
    CvRect rectOrigin = (image->roi != 0x0)?cvROIToRect(*image->roi):cvRect(0, 0, 0, 0);
    cvSetImageROI(image, rect);

    // Convert to grayscale
    IplImage *gray = cvCreateImage(cvSize(rect.width,rect.height), IPL_DEPTH_8U, 1);
    cvCvtColor(image, gray, CV_RGB2GRAY);


    // Tratament
    cvSmooth( gray, gray, CV_BLUR, smooth, smooth, 0, 0 );
    cvNot( gray, gray );

    // Run the edge detector on grayscale
    cvCanny(gray, gray, (float)edge_thresh, (float)edge_thresh*3, 3);

    // Convert to RGBscale
    cvCvtColor(gray,image,CV_GRAY2RGB);
    cvReleaseImage(&gray);

    if(rectOrigin.height != 0) cvSetImageROI(image, rectOrigin);
    else cvResetImageROI( image );
}