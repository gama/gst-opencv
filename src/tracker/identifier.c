#include "identifier.h"

float onlyBiggerObject(IplImage* frameBW){

    CvSeq *c, *cBig;
    CvMemStorage* tempStorage = cvCreateMemStorage(0);
    CvMat mstub;
    CvMat *mask = cvGetMat(frameBW, &mstub, 0, 0);
    CvPoint offset = cvPoint(0,0);
    float perc = 0.;

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

        // Returns the percentage of area occupied by the object
        perc = (float) areaBig/(frameBW->height*frameBW->width);
    }

    cvReleaseMemStorage(&tempStorage);

    return perc;
}

CvRect segObjectBookBGDiff(CvBGCodeBookModel* model, IplImage* rawImage,
        IplImage* yuvImage){

    int i, j;
    CvRect rectRoi = cvRect(0, 0, 0, 0);
    IplImage* temp = cvCreateImage(cvGetSize(rawImage), IPL_DEPTH_8U, 1);
    
    // Bbackground subtraction
    cvCvtColor( rawImage, yuvImage, CV_BGR2YCrCb ); //YUV For codebook method
    cvBGCodeBookDiff( model, yuvImage, temp, cvRect(0,0,0,0) );
    cvSegmentFGMask(temp, 1, 4.f, 0, cvPoint(0,0)); //CH:1 (not convexHull)

    // The greater area object of segmented image
    float percentualOcupado = onlyBiggerObject(temp);

    // Roi of greater area object
    if(percentualOcupado){
        int max_x = 0;
        int max_y = 0;
        int min_x = temp->width;
        int min_y = temp->height;
        for(i = 0; i < temp->height; i++){
            for(j = 0; j < temp->width; j++){
                if(cvGet2D(temp,i,j).val[0] != 0){
                    if(min_x > j) min_x = j;
                    if(min_y > i) min_y = i;
                    if(max_x < j) max_x = j;
                    if(max_y < i) max_y = i;
                }
            }
        }
 
        rectRoi = cvRect(min_x, min_y, max_x-min_x, max_y-min_y);
    }

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