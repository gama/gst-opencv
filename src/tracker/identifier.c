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

void canny(IplImage *image){

    static int edge_thresh = 1;

    // Create the output image
    IplImage *cedge = cvCreateImage(cvSize(image->width,image->height), IPL_DEPTH_8U, 3);

    // Convert to grayscale
    IplImage *gray = cvCreateImage(cvSize(image->width,image->height), IPL_DEPTH_8U, 1);
    IplImage *edge = cvCreateImage(cvSize(image->width,image->height), IPL_DEPTH_8U, 1);
    cvCvtColor(image, gray, CV_BGR2GRAY);

    cvSmooth( gray, edge, CV_BLUR, 3, 3, 0, 0 );
    cvNot( gray, edge );

    // Run the edge detector on grayscale
    cvCanny(gray, edge, (float)edge_thresh, (float)edge_thresh*3, 3);

    cvZero( cedge );
    // copy edge points
    cvCopy( image, cedge, edge );

    cvReleaseImage(&cedge);
    cvReleaseImage(&gray);
    cvReleaseImage(&edge);
}