#include "identifier.h"

CvRect onlyBiggerObject( IplImage* frameBW ){

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

CvRect segObjectBookBGDiff( CvBGCodeBookModel* model, IplImage* rawImage,
        IplImage* yuvImage ){

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

void showBorder( IplImage *srcGray, IplImage *dst, CvScalar borderColor,
        int edge_thresh, int smooth, int borderIncreaseSize ){

    CvSize size;
    IplImage *edge, *workGray;
    if( srcGray->roi ){
        size = cvSize( srcGray->roi->width,srcGray->roi->height );
        cvSetImageROI(dst, cvRect( srcGray->roi->xOffset, srcGray->roi->yOffset,
                srcGray->roi->width, srcGray->roi->height ));
    }else{
        size = cvSize(srcGray->width,srcGray->height);
    }
    edge        = cvCreateImage(size, IPL_DEPTH_8U, 1);
    workGray    = cvCreateImage(size, IPL_DEPTH_8U, 1);

    cvSmooth( srcGray, workGray, CV_BLUR, smooth, smooth, 0, 0 );
    cvNot( workGray, workGray );

    cvCanny( workGray, edge, (float)edge_thresh, (float)edge_thresh*3, 3 );

    if( borderIncreaseSize ) cvDilate( edge, edge, 0, borderIncreaseSize );
    cvAddS( dst, borderColor, dst, edge );
    
    cvReleaseImage( &edge );
    cvReleaseImage( &workGray );
    cvResetImageROI( dst );
}
