#include <opencv/cxtypes.h>

#include "Classifier.h"

Classifier cls;

extern "C" void classifier_intermediate_init(IplImage *image, CvRect rect) {
    Rect trackedPatch;
    trackedPatch.upper = rect.y;
    trackedPatch.left = rect.x;
    trackedPatch.height = rect.height;
    trackedPatch.width = rect.width;
    cls.init(image, trackedPatch);
}

extern "C" void classifier_intermediate_train(IplImage *image, CvRect rect) {
    Rect trackedPatch;
    trackedPatch.upper = rect.y;
    trackedPatch.left = rect.x;
    trackedPatch.height = rect.height;
    trackedPatch.width = rect.width;
    cls.train(image, trackedPatch);
}

extern "C" float classifier_intermediate_classify(IplImage *image, CvRect rect) {
    Rect trackedPatch;
    trackedPatch.upper = rect.y;
    trackedPatch.left = rect.x;
    trackedPatch.height = rect.height;
    trackedPatch.width = rect.width;
    return cls.classify(image, trackedPatch);
}
