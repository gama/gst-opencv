#ifndef __WEAK_CLASSIFIER_H__
#define __WEAK_CLASSIFIER_H__

#include "ImageRepresentation.h"

class WeakClassifier  
{

public:

	WeakClassifier();
	virtual ~WeakClassifier();

	virtual bool update(ImageRepresentation* image, Rect ROI, int target);

	virtual int eval(ImageRepresentation* image, Rect ROI);

	virtual float getValue (ImageRepresentation* image, Rect  ROI);

	virtual int getType();
};

#endif // __WEAK_CLASSIFIER_H__
