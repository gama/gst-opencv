#ifndef __CLASSIFIER_THRESHOLD_H__
#define __CLASSIFIER_THRESHOLD_H__

#include <math.h>

#include "EstimatedGaussDistribution.h"

class ClassifierThreshold 
{
public:

	ClassifierThreshold();
	virtual ~ClassifierThreshold();

	void update(float value, int target);
	int eval(float value);

	void* getDistribution(int target);

private:

	EstimatedGaussDistribution* m_posSamples;
	EstimatedGaussDistribution* m_negSamples;

	float m_threshold;
	int m_parity;
};

#endif // __CLASSIFIER_THRESHOLD_H__
