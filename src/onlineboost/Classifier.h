#ifndef __CLASSIFIER_H__
#define __CLASSIFIER_H__

#ifdef __cplusplus

#include "ImageRepresentation.h"
#include "Patches.h"
#include "StrongClassifier.h"
#include "StrongClassifierDirectSelection.h"

class Classifier {
public:

    Classifier();
    Classifier(IplImage *image, Rect trackedPatch);
    virtual ~Classifier();

    void init(IplImage *image, Rect trackedPatch);
    bool train(IplImage *image, Rect trackedPatch);
    float classify(ImageRepresentation* image, Rect trackedPatch);

    Rect getTrackingROI(float searchFactor, Rect trackedPatch);
    float getConfidence();
    void update_dataCh(IplImage *m_grayImage);
    float getSumAlphaClassifier();
    StrongClassifier* getClassifier();

private:

    StrongClassifier* classifier;
    ImageRepresentation* curFrameRep;
    Rect init_trackingRect;
    unsigned char *dataCh;
    Rect validROI;
    int numBaseClassifier;
    float searchFactor;
    float overlap;
    Size trackingRectSize;
};

#endif // __cplusplus

extern "C" {
    void classifier_intermediate_init(IplImage *image, CvRect rect);
    void classifier_intermediate_train(IplImage *image, CvRect rect);
    float classifier_intermediate_classify(IplImage *image, CvRect rect);
}

#endif // __CLASSIFIER_H__