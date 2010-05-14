#include "Classifier.h"

Classifier::Classifier(void) {}

Classifier::Classifier(IplImage *image, Rect trackedPatch){
    init(image, trackedPatch);
}

Classifier::~Classifier(void) {
    delete curFrameRep;
    delete classifier;
}

Rect Classifier::getTrackingROI(float searchFactor, Rect trackedPatch) {
    Rect searchRegion;

    searchRegion = trackedPatch * (searchFactor);
    //check
    if (searchRegion.upper + searchRegion.height > validROI.height)
        searchRegion.height = validROI.height - searchRegion.upper;
    if (searchRegion.left + searchRegion.width > validROI.width)
        searchRegion.width = validROI.width - searchRegion.left;

    return searchRegion;
}

float Classifier::getSumAlphaClassifier() {
    return classifier->getSumAlpha();
}

StrongClassifier* Classifier::getClassifier() {
    return classifier;
}

void Classifier::update_dataCh(IplImage *m_grayImage) {

    IplImage * temp;
    temp = cvCreateImage(cvGetSize(m_grayImage), 8, 1);
    cvCvtColor(m_grayImage, temp, CV_RGB2GRAY);

    int rows = temp->height;
    int cols = temp->width;
    int iplCols = temp->widthStep;

    this->dataCh = new unsigned char[rows * cols];
    unsigned char *buffer = reinterpret_cast<unsigned char*> (temp->imageData);

    for (int i = 0; i < rows; i++) {
        memcpy(this->dataCh + i*cols, buffer + i*iplCols, sizeof (unsigned char) * cols);
    }

    cvReleaseImage(&temp);
    return;
}

void Classifier::init(IplImage *image, Rect trackedPatch) {

    update_dataCh(image);

    numBaseClassifier = 100;
    searchFactor = 2;
    overlap = 0.99;

    Size imageSize2;
    imageSize2.height = image->height;
    imageSize2.width = image->width;
    this->validROI = imageSize2;

    this->curFrameRep = new ImageRepresentation(this->dataCh, imageSize2);

    int numWeakClassifier = numBaseClassifier * 10;
    bool useFeatureExchange = true;
    int iterationInit = 50;
    Size patchSize;
    patchSize = trackedPatch;
    init_trackingRect = trackedPatch;
    trackingRectSize = init_trackingRect;

    classifier = new StrongClassifierDirectSelection(numBaseClassifier, numWeakClassifier, patchSize, useFeatureExchange, iterationInit);

    Rect trackingROI = getTrackingROI(searchFactor, trackedPatch);
    Size trackedPatchSize;
    trackedPatchSize = trackedPatch;
    Patches* trackingPatches = new PatchesRegularScan(trackingROI, this->validROI, trackedPatchSize, 0.99f);

    iterationInit = 50;
    for (int curInitStep = 0; curInitStep < iterationInit; curInitStep++) {
        classifier->update(this->curFrameRep, trackingPatches->getSpecialRect("UpperLeft"), -1);
        classifier->update(this->curFrameRep, trackedPatch, 1);
        classifier->update(this->curFrameRep, trackingPatches->getSpecialRect("UpperRight"), -1);
        classifier->update(this->curFrameRep, trackedPatch, 1);
        classifier->update(this->curFrameRep, trackingPatches->getSpecialRect("LowerLeft"), -1);
        classifier->update(this->curFrameRep, trackedPatch, 1);
        classifier->update(this->curFrameRep, trackingPatches->getSpecialRect("LowerRight"), -1);
        classifier->update(this->curFrameRep, trackedPatch, 1);
    }

    delete trackingPatches;
}

float Classifier::classify(ImageRepresentation* image, Rect trackedPatch) {
    return classifier->eval(image, trackedPatch);
}

bool Classifier::train(IplImage *image, Rect trackedPatch) {
    update_dataCh(image);

    Patches *trackingPatches;

    Rect searchRegion;
    searchRegion = this->getTrackingROI(searchFactor, trackedPatch);
    trackingPatches = new PatchesRegularScan(searchRegion, this->validROI, trackingRectSize, overlap);
    this->curFrameRep->setNewImageAndROI(this->dataCh, searchRegion);

    classifier->update(this->curFrameRep, trackingPatches->getSpecialRect("UpperLeft"), -1);
    classifier->update(this->curFrameRep, trackedPatch, 1);
    classifier->update(this->curFrameRep, trackingPatches->getSpecialRect("UpperRight"), -1);
    classifier->update(this->curFrameRep, trackedPatch, 1);
    classifier->update(this->curFrameRep, trackingPatches->getSpecialRect("LowerLeft"), -1);
    classifier->update(this->curFrameRep, trackedPatch, 1);
    classifier->update(this->curFrameRep, trackingPatches->getSpecialRect("LowerRight"), -1);
    classifier->update(this->curFrameRep, trackedPatch, 1);

    return true;
}