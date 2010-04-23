#ifndef __STRONG_CLASSIFIER_STANDARD_SEMI_H__
#define __STRONG_CLASSIFIER_STANDARD_SEMI_H__

#include "StrongClassifier.h"

class StrongClassifierStandardSemi : public StrongClassifier
{

public:

	StrongClassifierStandardSemi(int numBaseClassifier, int numWeakClassifier, 
                                 Size patchSize,  bool useFeatureExchange = false,
                                 int iterationInit = 0);

	virtual ~StrongClassifierStandardSemi();
	
	bool updateSemi(ImageRepresentation *image, Rect ROI, float priorConfidence);
	void getPseudoValues(ImageRepresentation *image, Rect ROI, float priorConfidence, float* pseudoLambdaInOut, int* pseudoTargetInOut);
	
private:

	bool *m_errorMask;
	float *m_errors;
	float* m_pseudoLambda;
	int* m_pseudoTarget;
};

#endif // __STRONG_CLASSIFIER_STANDARD_SEMI_H__
