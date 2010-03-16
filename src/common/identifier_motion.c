/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 * Copyright (C) 2009 Erickson Nascimento <erickson@vettalabs.com>
 */

#include "identifier_motion.h"
#include <glib.h>
#include <time.h>

// various tracking parameters (in seconds)
const double MHI_DURATION = 1;
const double MAX_TIME_DELTA = 0.5;
const double MIN_TIME_DELTA = 0.05;
const double MIN_PERC_PREENCHIMENTO_RECT = 0;//0.05;
const double MIN_AREA_CONSIDERADA = 400;//0.05;

#define MAX_MOTION_AREA 100
#define DIFF_THRESHOLD 30

// number of cyclic frame buffer used for motion detection
// (should, probably, depend on FPS)
const int N = 4;

// ring image buffer
IplImage **buf = 0;
int last = 0;

// temporary images
IplImage *mhi         = 0; // MHI
IplImage *orient      = 0; // orientation
IplImage *mask        = 0; // valid orientation mask
IplImage *segmask     = 0; // motion segmentation map
CvMemStorage* storage = 0; // temporary storage

CvRect motion_detect(IplImage *img, IplImage *motionHist)
{
    IplImage *silh;
    CvSeq    *seq;
    CvRect    comp_rect, comp_rect_bigger;
    CvSize    size;
    double    timestamp, count, count_comp_rect_bigger;
    gint      i, idx1, idx2;

    timestamp = 1.0f * clock() / CLOCKS_PER_SEC; // get current time in seconds
    size      = cvSize(img->width, img->height); // get current frame size
    idx1      = last;

    // allocate images at the beginning or
    // reallocate them if the frame size is changed
    if (!mhi || mhi->width != size.width || mhi->height != size.height) {
        if( buf == 0 ) {
            buf = (IplImage**)malloc(N*sizeof(buf[0]));
            memset( buf, 0, N*sizeof(buf[0]));
        }

        for (i = 0; i < N; i++) {
            cvReleaseImage(&buf[i]);
            buf[i] = cvCreateImage(size, IPL_DEPTH_8U, 1);
            cvZero(buf[i]);
        }
        cvReleaseImage(&mhi);
        cvReleaseImage(&orient);
        cvReleaseImage(&segmask);
        cvReleaseImage(&mask);

        mhi = cvCreateImage( size, IPL_DEPTH_32F, 1);
        cvZero(mhi); // clear MHI at the beginning
        orient = cvCreateImage(size, IPL_DEPTH_32F, 1);
        segmask = cvCreateImage(size, IPL_DEPTH_32F, 1);
        mask = cvCreateImage(size, IPL_DEPTH_8U, 1);
    }

    cvCvtColor(img, buf[last], CV_BGR2GRAY); // convert frame to grayscale

    idx2 = (last + 1) % N; // index of (last - (N-1))th frame
    last = idx2;

    silh = buf[idx2];
    cvAbsDiff( buf[idx1], buf[idx2], silh ); // get difference between frames

    cvThreshold(silh, silh, DIFF_THRESHOLD, 1, CV_THRESH_BINARY); // and threshold it
    cvUpdateMotionHistory(silh, mhi, timestamp, MHI_DURATION);    // update MHI

    // convert MHI to blue 8u image
    cvCvtScale(mhi, mask, 255./MHI_DURATION, (MHI_DURATION - timestamp)*255./MHI_DURATION);
    cvZero(motionHist);
    cvMerge(mask, 0, 0, 0, motionHist);

    // calculate motion gradient orientation and valid orientation mask
    cvCalcMotionGradient(mhi, mask, orient, MAX_TIME_DELTA, MIN_TIME_DELTA, 3);

    if (!storage)
        storage = cvCreateMemStorage(0);
    else
        cvClearMemStorage(storage);

    // segment motion: get sequence of motion components
    // segmask is marked motion components map. It is not used further
    seq = cvSegmentMotion(mhi, segmask, storage, timestamp, MAX_TIME_DELTA);


    // To biggest rect
    comp_rect_bigger = cvRect(0, 0, 0, 0);
    count_comp_rect_bigger = 0;

    // iterate through the motion components,
    for (i = 0; i < seq->total; i++) {
        comp_rect = ((CvConnectedComp*)cvGetSeqElem(seq, i))->rect;

        // reject very small components
        if (comp_rect.width * comp_rect.height < MIN_AREA_CONSIDERADA)
            continue;

        // calculate number of points within silhouette ROI
        cvSetImageROI(silh, comp_rect);
        count = cvNorm(silh, 0, CV_L1, 0);
        cvResetImageROI(silh);

        // check for the case of little motion
        if (MIN_PERC_PREENCHIMENTO_RECT &&
            count < comp_rect.width*comp_rect.height * MIN_PERC_PREENCHIMENTO_RECT)
            continue;

        // Verifica se é o maior preenchimento
        if (count > count_comp_rect_bigger) {
            count_comp_rect_bigger = count;
            comp_rect_bigger = comp_rect;
        }
    }

    /*
    // Select only most blue region
    if(i>0){
        int max_x = 0, max_y = 0;
        int min_x = motionHist->width, min_y = motionHist->height;
        for(i = 0; i < motionHist->height; i++)
            for(j = 0; j < motionHist->width; j++)
                if(cvGet2D(motionHist,i,j).val[0] == 255.){
                    if(min_x > j) min_x = j;
                    if(min_y > i) min_y = i;
                    if(max_x < j) max_x = j;
                    if(max_y < i) max_y = i;
                }
        if((max_x<min_x) && (max_y<min_y)) return cvRect(0,0,0,0);
        else return cvRect(min_x, min_y, max_x-min_x, max_y-min_y);
    }
    */
    return comp_rect_bigger;
}

CvSeq*
motion_detect_mult(IplImage* img, IplImage* motionHist)
{
    IplImage *silh;
    CvSeq    *seq;
    CvRect    comp_rect, rect_i, rect_j;
    CvSize    size;
    double    timestamp, count;
    gint      i, j, idx1, idx2, indexDel[MAX_MOTION_AREA];

    timestamp = 1.0f * clock() / CLOCKS_PER_SEC; // get current time in seconds
    size      = cvSize(img->width, img->height); // get current frame size
    idx1      = last;

    // allocate images at the beginning or
    // reallocate them if the frame size is changed
    if (!mhi || mhi->width != size.width || mhi->height != size.height) {
        if (buf == 0) {
            buf = (IplImage**) malloc(N*sizeof(buf[0]));
            memset(buf, 0, N*sizeof(buf[0]));
        }

        for (i = 0; i < N; i++) {
            cvReleaseImage(&buf[i]);
            buf[i] = cvCreateImage(size, IPL_DEPTH_8U, 1);
            cvZero(buf[i]);
        }
        cvReleaseImage(&mhi);
        cvReleaseImage(&orient);
        cvReleaseImage(&segmask);
        cvReleaseImage(&mask);

        mhi     = cvCreateImage(size, IPL_DEPTH_32F, 1);
        cvZero(mhi); // clear MHI at the beginning
        orient  = cvCreateImage(size, IPL_DEPTH_32F, 1);
        segmask = cvCreateImage(size, IPL_DEPTH_32F, 1);
        mask    = cvCreateImage(size, IPL_DEPTH_8U, 1);
    }

    cvCvtColor(img, buf[last], CV_BGR2GRAY); // convert frame to grayscale

    idx2 = (last + 1) % N; // index of (last - (N-1))th frame
    last = idx2;

    silh = buf[idx2];
    cvAbsDiff(buf[idx1], buf[idx2], silh); // get difference between frames

    cvThreshold(silh, silh, DIFF_THRESHOLD, 1, CV_THRESH_BINARY); // and threshold it
    cvUpdateMotionHistory(silh, mhi, timestamp, MHI_DURATION);    // update MHI

    // convert MHI to blue 8u image
    cvCvtScale(mhi, mask, 255./MHI_DURATION,
               (MHI_DURATION - timestamp)*255./MHI_DURATION);
    cvZero(motionHist);
    cvMerge(mask, 0, 0, 0, motionHist);

    // calculate motion gradient orientation and valid orientation mask
    cvCalcMotionGradient(mhi, mask, orient, MAX_TIME_DELTA, MIN_TIME_DELTA, 3);

    if (!storage)
        storage = cvCreateMemStorage(0);
    else
        cvClearMemStorage(storage);

    seq = cvSegmentMotion(mhi, segmask, storage, timestamp, MAX_TIME_DELTA);

    // Clean little rects
    for (i = 0; i < seq->total && i < MAX_MOTION_AREA; i++) {
        indexDel[i] = 1;
        comp_rect = ((CvConnectedComp*)cvGetSeqElem( seq, i ))->rect;

        // reject very small components or size full image
        if ((comp_rect.width * comp_rect.height < MIN_AREA_CONSIDERADA) ||
            ((img->width == comp_rect.width) && (img->height == comp_rect.height))) {
            indexDel[i] = 0;
            continue;
        }

        // calculate number of points within silhouette ROI
        cvSetImageROI(silh, comp_rect);
        count = cvNorm(silh, 0, CV_L1, 0);
        cvResetImageROI(silh);

        // check for the case of little motion
        if (MIN_PERC_PREENCHIMENTO_RECT &&
            (count < comp_rect.width*comp_rect.height*MIN_PERC_PREENCHIMENTO_RECT)) {
            //cvSeqRemove(seq, i);
            indexDel[i] = 0;
            continue;
        }
    }
    // Delete bad motions
    while (i--) if (!indexDel[i]) cvSeqRemove(seq, i);

    // Clean intercession rects
    for (i = 0; i < seq->total && i < MAX_MOTION_AREA; i++)
        indexDel[i] = 1;

    for (i = 0; i < seq->total && i < MAX_MOTION_AREA; i++) {
        rect_i = ((CvConnectedComp*) cvGetSeqElem(seq, i))->rect;
        for (j = i + 1; j < seq->total && j < MAX_MOTION_AREA; j++) {
            CvRect rect_sum;

            rect_j   = ((CvConnectedComp*) cvGetSeqElem(seq, j))->rect;
            rect_sum = cvMaxRect(&rect_i, &rect_j);

            // intercession
            if (!((rect_sum.width > (rect_i.width+rect_j.width)) && (rect_sum.height > (rect_i.height+rect_j.height)))) {
                ((CvConnectedComp*)cvGetSeqElem(seq, i))->rect.x      = rect_sum.x;
                ((CvConnectedComp*)cvGetSeqElem(seq, i))->rect.y      = rect_sum.y;
                ((CvConnectedComp*)cvGetSeqElem(seq, i))->rect.width  = rect_sum.width;
                ((CvConnectedComp*)cvGetSeqElem(seq, i))->rect.height = rect_sum.height;
                indexDel[j] = 0;
            }
        }
    }
    // Delete bad motions
    while (i--) if (!indexDel[i]) cvSeqRemove(seq, i);

    return seq;
}
