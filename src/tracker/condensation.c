#include "condensation.h"

void centroid(CvPoint2D32f *measurement, int size, double *x, double *y){
    int i;
    *x = *y = 0;
    for(i = 0; i < size; i++){
        *x += measurement[i].x;
        *y += measurement[i].y;
    }
    *x /= size;
    *y /= size;
}

double stdev(double *sample, int sample_size){
    int i;
    double mean = 0, sum = 0;

    for(i = 0; i < sample_size; i++){
        mean += sample[i];
    }
    for(i = 0; i < sample_size; i++){
        sum += ( pow(sample[i] - mean,2) );
    }

    return sqrt(sum/sample_size);
}

double euclid_dist(CvPoint p1, CvPoint p2){
    return sqrt( pow(p1.x-p2.x, 2)+pow(p1.y-p2.y,2));    
}


CvConDensation *initCondensation(int state_vec_dim, int measurement_vec_dim, int sample_size, int max_width, int max_height){

    int i;
	CvMat* lowerBound;
	CvMat* upperBound;

	lowerBound = cvCreateMat(state_vec_dim, 1, CV_32F);
	upperBound = cvCreateMat(state_vec_dim, 1, CV_32F);

	CvConDensation *ConDens = cvCreateConDensation( state_vec_dim, measurement_vec_dim, sample_size );
    // coord x	
	cvmSet( lowerBound, 0, 0, 0.0 ); 
	cvmSet( upperBound, 0, 0, max_width);
	
    // coord y
	cvmSet( lowerBound, 1, 0, 0.0 ); 
	cvmSet( upperBound, 1, 0, max_height);

    // x speed (dx/dt)
	cvmSet( lowerBound, 2, 0, 0.0 ); 
	cvmSet( upperBound, 2, 0, 1.0 );
	
    // y speed (dy/dt)
	cvmSet( lowerBound, 3, 0, 0.0 ); 
	cvmSet( upperBound, 3, 0, 1.0 );
	

    // how to set dynamic matrix? 
    // M = [1,0,1,0;0,1,0,1;0,0,1,0;0,0,0,1]
	//for (int i=0;i<state_vec_dim*state_vec_dim;i++){
	//	ConDens->DynamMatr[i]= M->data.fl[i];
	//}
	ConDens->DynamMatr[0]= 1;
	ConDens->DynamMatr[1]= 0;
	ConDens->DynamMatr[2]= 1;
	ConDens->DynamMatr[3]= 0;

	ConDens->DynamMatr[4]= 0;
	ConDens->DynamMatr[5]= 1;
	ConDens->DynamMatr[6]= 0;
	ConDens->DynamMatr[7]= 1;

	ConDens->DynamMatr[8]= 0;
	ConDens->DynamMatr[9]= 0;
	ConDens->DynamMatr[10]= 1;
	ConDens->DynamMatr[11]= 0;

	ConDens->DynamMatr[12]= 0;
	ConDens->DynamMatr[13]= 0;
	ConDens->DynamMatr[14]= 0;
	ConDens->DynamMatr[15]= 1;


	cvConDensInitSampleSet(ConDens, lowerBound, upperBound);
	
	CvRNG rng_state = cvRNG(0xffffffff);
	
	for(i=0; i < sample_size; i++){
		ConDens->flSamples[i][0] = cvRandInt( &rng_state ) % max_width; //0 -> x coord
		ConDens->flSamples[i][1] = cvRandInt( &rng_state ) % max_height; //1 -> y coord
	}
	
	//ConDens->DynamMatr=(float*)indexMat[0];
	//ConDens->State[0]=maxWidth/2;ConDens->State[1]=maxHeight/2;ConDens->State[2]=0;ConDens->State[3]=0;
    return ConDens;
}


void resample(IplImage* image, CvConDensation* ConDens, double measurement_x, double measurement_y, int show_particles, double *stdev_x, double *stdev_y){	
    int i;
    double var_x, var_y, dist;
	double *sample_x; 
	double *sample_y;

	sample_x = (double*) malloc(ConDens->SamplesNum*sizeof(double));
    sample_y = (double*) malloc(ConDens->SamplesNum*sizeof(double));
	
	//float stdev = sqrt(var/ConDens->SamplesNum);
	
	for(i = 0; i < ConDens->SamplesNum; i++){
		sample_x[i] = ConDens->flSamples[i][0];
		sample_y[i] = ConDens->flSamples[i][1];
	}	
	
	*stdev_x = stdev(sample_x, ConDens->SamplesNum);
	*stdev_y = stdev(sample_y, ConDens->SamplesNum);

    var_x = pow(*stdev_x,2);
    var_y = pow(*stdev_y,2);

	for(i = 0; i < ConDens->SamplesNum; i++){


#if 0		
		float p_x = 1.0,  p_y = 1.0;
		p_x *= (float) exp( -1 * (measurement_x - ConDens->flSamples[i][0]) * (measurement_x - ConDens->flSamples[i][0]) / (2 * var_x ) );
		p_y *= (float) exp( -1 * (measurement_y - ConDens->flSamples[i][1]) * (measurement_y - ConDens->flSamples[i][1]) / (2 * var_y ) );
		ConDens->flConfidence[i] = p_x * p_y;
#else
        dist = sqrt(pow(ConDens->flSamples[i][0]-measurement_x, 2)+pow(ConDens->flSamples[i][1]-measurement_y, 2));
        if (dist == 0)
            ConDens->flConfidence[i] =  1;
        else ConDens->flConfidence[i] =  1/pow(dist,2);
#endif

      if (show_particles == 1)
        cvCircle( image, cvPoint(ConDens->flSamples[i][0], ConDens->flSamples[i][1]), 10*ConDens->flConfidence[i], CV_RGB(0,0,255), 3, 8,0);

	}
    free(sample_x);
    free(sample_y);
}
   
void updateCondensation(IplImage* image, CvConDensation* ConDens, double measurement_x, double measurement_y, int show_particles, double *predicted_x, double *predicted_y){
    double stdev_x, stdev_y;

	resample(image, ConDens, measurement_x, measurement_y, show_particles, &stdev_x, &stdev_y);
	cvConDensUpdateByTime(ConDens);
	*predicted_x = ConDens->State[0];
    *predicted_y = ConDens->State[1];
}

void  getParticlesBoundary(CvConDensation *ConDens,CvRect *particlesBoundary, int max_width, int max_height){
    int i;

    particlesBoundary->x = max_width;
    particlesBoundary->y = max_height;
    particlesBoundary->width = 0;
    particlesBoundary->height = 0;

 	for(i = 0; i < ConDens->SamplesNum; i++){
        if (ConDens->flSamples[i][0] < particlesBoundary->x && ConDens->flSamples[i][0] >= 0 && ConDens->flSamples[i][0] < max_width )
            particlesBoundary->x = ConDens->flSamples[i][0];

        if (ConDens->flSamples[i][1] < particlesBoundary->y && ConDens->flSamples[i][0] >= 0 && ConDens->flSamples[i][0] < max_height  )
            particlesBoundary->y = ConDens->flSamples[i][1];

        if (ConDens->flSamples[i][0] > particlesBoundary->width && ConDens->flSamples[i][0] >= 0 && ConDens->flSamples[i][0] < max_width  )
            particlesBoundary->width = ConDens->flSamples[i][0];

        if (ConDens->flSamples[i][1] > particlesBoundary->height && ConDens->flSamples[i][0] >= 0 && ConDens->flSamples[i][0] < max_height )
            particlesBoundary->height = ConDens->flSamples[i][1];
	}
    
    particlesBoundary->width = particlesBoundary->width - particlesBoundary->x;   
    particlesBoundary->height = particlesBoundary->height - particlesBoundary->y;   
}


