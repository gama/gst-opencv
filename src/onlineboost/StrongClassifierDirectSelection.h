#ifndef __STRONG_CLASSIFIER_DIRECT_SELECTION_H__
#define __STRONG_CLASSIFIER_DIRECT_SELECTION_H__

#include "StrongClassifier.h"

class StrongClassifierDirectSelection : public StrongClassifier
{
public:

	StrongClassifierDirectSelection(int numBaseClassifier, int numWeakClassifier, Size patchSize, bool useFeatureExchange = false, int iterationInit = 0); 

	virtual ~StrongClassifierDirectSelection();

	bool update(ImageRepresentation *image, Rect ROI, int target, float importance = 1.0); 
	
private:

	bool * m_errorMask;
	float* m_errors;
	float* m_sumErrors;
};

#endif // __STRONG_CLASSIFIER_DIRECT_SELECTION_H__
