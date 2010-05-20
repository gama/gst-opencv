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
    float classify(IplImage *image, Rect trackedPatch);

    Rect getTrackingROI(float searchFactor, Rect trackedPatch);
    float getConfidence();
    void update_dataCh(IplImage *m_grayImage, unsigned char **dataCh);
    float getSumAlphaClassifier();
    StrongClassifier* getClassifier();
    Rect convert_cvrect_to_rect(CvRect rect);

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

#else

typedef struct Classifier {
} Classifier;

#endif



#ifdef __cplusplus

extern "C" {

#endif

    void classifier_intermediate_init(struct Classifier* cls, IplImage *image, CvRect rect);
    void classifier_intermediate_train(struct Classifier* cls, IplImage *image, CvRect rect);
    float classifier_intermediate_classify(struct Classifier* cls, IplImage *image, CvRect rect);

#ifdef __cplusplus

}

#endif

#endif // __CLASSIFIER_H__
