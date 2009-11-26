/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 * Copyright (C) 2009 Erickson Nascimento <erickson@vettalabs.com>
 */

#include "kmeans.h"

void floatDoKmeans(int nClusters, int nPoints, float **vetPoints, int *vetPointsClusterIdx){
    int k;
    CvMat* points = cvCreateMat( nPoints, 1, CV_32FC2 );
    CvMat* clusters = cvCreateMat( nPoints, 1, CV_32SC1 );

    for( k = 0; k < nPoints; k++ ){
        points->data.fl[k*2] = vetPoints[k][0];
        points->data.fl[k*2+1] = vetPoints[k][1];
    }

    cvKMeans2( points, nClusters, clusters,
            cvTermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 10, 1.0 ),
            5, 0, 0, 0, 0 );

    for( k = 0; k < nPoints; k++ ){
        vetPointsClusterIdx[k] = clusters->data.i[k];
    }

    cvReleaseMat( &points );
    cvReleaseMat( &clusters );

    return;
}

void doKmeans(int nClusters, int nPoints, CvPoint2D32f *vetPoints, int *vetPointsClusterIdx){
    int k;
    CvMat* points = cvCreateMat( nPoints, 1, CV_32FC2 );
    CvMat* clusters = cvCreateMat( nPoints, 1, CV_32SC1 );

    for( k = 0; k < nPoints; k++ ){
        points->data.fl[k*2] = vetPoints[k].x;
        points->data.fl[k*2+1] = vetPoints[k].y;
    }

    cvKMeans2( points, nClusters, clusters,
            cvTermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 10, 1.0 ),
            5, 0, 0, 0, 0 );

    for( k = 0; k < nPoints; k++ ){
        vetPointsClusterIdx[k] = clusters->data.i[k];
    }

    cvReleaseMat( &points );
    cvReleaseMat( &clusters );

    return;
}

CvRect rectBoudingIdx(CvPoint2D32f *measurement, int size, int idx, int *vetPointsClusterIdx){
    int x_min = -1;
    int y_min = -1;
    int x_max = -1;
    int y_max = -1;
    int i;
    for(i = 0; i < size; i++){
        if(idx != -1 && vetPointsClusterIdx[i] != idx) continue;
        if(x_min == -1 || measurement[i].x < x_min) x_min = measurement[i].x;
        if(y_min == -1 || measurement[i].y < y_min) y_min = measurement[i].y;
        if(x_max == -1 || measurement[i].x > x_max) x_max = measurement[i].x;
        if(y_max == -1 || measurement[i].y > y_max) y_max = measurement[i].y;
    }
    return cvRect(x_min, y_min, x_max-x_min, y_max-y_min);
}

CvRect floatRectBoudingIdx(float **measurement, int size, int idx, int *vetPointsClusterIdx){
    int x_min = -1;
    int y_min = -1;
    int x_max = -1;
    int y_max = -1;
    int i;
    for(i = 0; i < size; i++){
        if(idx != -1 && vetPointsClusterIdx[i] != idx) continue;
        if(x_min == -1 || measurement[i][0] < x_min) x_min = measurement[i][0];
        if(y_min == -1 || measurement[i][1] < y_min) y_min = measurement[i][1];
        if(x_max == -1 || measurement[i][0] > x_max) x_max = measurement[i][0];
        if(y_max == -1 || measurement[i][1] > y_max) y_max = measurement[i][1];
    }
    return cvRect(x_min, y_min, x_max-x_min, y_max-y_min);
}

/*
int findVetFaceIdOfPoint(InstanceFace *vet_faces, int nFaces, CvPoint point){
    int closerIdx = 0;
    int i;
    float minDistTemp, minDist = -1;
    for(i = 0; i < nFaces; ++i){
        minDistTemp = sqrt(pow((vet_faces[i]->rect.x - point.x), 2) + pow((vet_faces[i]->rect.y - point.y), 2));
        if(minDist == -1 || minDistTemp < minDist){
            minDist = minDistTemp;
            closerIdx = i;
        }
    }
    return closerIdx;
}
*/
