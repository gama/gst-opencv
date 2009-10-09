#include "identifier.h"


int learnBackground(IplImage* image, CvBGCodeBookModel* model, IplImage* background){
    background = cvCloneImage(image);
    cvCvtColor( image, background, CV_BGR2YCrCb );
    cvBGCodeBookUpdate( model, background, cvRect(0,0,0,0), 0 );

    return 0;
}



/*
int codeBookTraining(CvCapture* capture, CvBGCodeBookModel* model,
        IplImage* yuvImage, int nframesToLearnBG = 50){

    // Faz o treinamento com os nframesToLearnBG proximos frames
    IplImage* rawImage = 0;
    for(int i=0; i<=nframesToLearnBG; i++){
        rawImage = cvQueryFrame( capture );
        if(!rawImage) return 1;
        cvCvtColor( rawImage, yuvImage, CV_BGR2YCrCb );
        cvBGCodeBookUpdate( model, yuvImage );
    }
    cvBGCodeBookClearStale( model, model->t/2 );
    return 0;
}
*/



float onlyBiggerObject(IplImage* frameBW){

    CvSeq *c, *cBig;
    //TODO: ver se este parametro da funcao atrapalha em algo
    CvMemStorage* tempStorage = cvCreateMemStorage(0);
    CvMat mstub;
    //TODO: ver se este parametro da funcao atrapalha em algo
    CvMat *mask = cvGetMat(frameBW, &mstub, 0, 0);
    CvPoint offset = cvPoint(0,0);
    float percentualOcupado = 0.;

    CvContourScanner scanner = cvStartFindContours(mask, tempStorage,
        sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE,offset );

    // Identifica objeto de maior area
    CvSlice cvslice = { 0, 0x3fffffff };
    double area, areaBig = -1;
    while( (c = cvFindNextContour( scanner )) != 0 ){
        area = fabs(cvContourArea(c,cvslice));
        if(area > areaBig){
            areaBig = area;
            cBig = c;
        }
    }

    // Se existir algum objeto, deixa apenas ele na imagem
    if(areaBig != -1){
        cvZero( mask );
        cvDrawContours( mask, cBig, cvScalarAll(255), cvScalarAll(0), -1,
                CV_FILLED, 8, cvPoint(-offset.x,-offset.y));

        // Retorna o percentual de area ocupada pelo objeto
        percentualOcupado = (float) areaBig/(frameBW->height*frameBW->width);
    }

    // TODO: verificar se e necessario fazer mais alguma RELEASE
    cvReleaseMemStorage(&tempStorage);

    return percentualOcupado;
}


/*
IplImage* segObjectBookBGDiff(CvBGCodeBookModel* model, IplImage* rawImage, IplImage* background){
    int i, j;
    IplImage* temp = cvCreateImage( cvGetSize(rawImage), IPL_DEPTH_8U, 1 );
    cvCvtColor( rawImage, background, CV_BGR2YCrCb );//YUV For codebook method
    cvBGCodeBookDiff( model, background, temp, cvRect(0,0,0,0) );
    cvSegmentFGMask( temp, 0, 4.f, 0, cvPoint(0,0) ); 

    int BGCOLOR_DEL = 0;
    CvScalar BGR;
    for(i = 0; i < temp->height; i++){
        for(j = 0; j < temp->width; j++){
            BGR = cvGet2D(temp,i,j);
            if(BGR.val[0] != BGCOLOR_DEL)
                cvSet2D(temp,i,j,cvGet2D(rawImage,i,j));
        }
    }
    return temp;
}
*/


CvRect segObjectBookBGDiff(CvBGCodeBookModel* model, IplImage* rawImage,
        IplImage* yuvImage){

    int i, j;
    //bool withConvexHull = false;

    // Limpa possiveis ROI da imagem
    //cvResetImageROI(rawImage);

    CvRect rectRoi = cvRect(0, 0, 0, 0);
    
    // Cria imagem binaria que representa fundo X objeto
    IplImage* temp = cvCreateImage(cvGetSize(rawImage), IPL_DEPTH_8U, 1);
    
    // Faz a subtracao do fundo
    cvCvtColor( rawImage, yuvImage, CV_BGR2YCrCb ); //YUV For codebook method
    //TODO: ver se este parametro da funcao atrapalha em algo
    cvBGCodeBookDiff( model, yuvImage, temp, cvRect(0,0,0,0) );

    // Faz a sementacao e aplica algoritmo convexHull
    //cvSegmentFGMask(temp, ((withConvexHull)?0:1));
    //nao usa CH:1
    //TODO: ver se este parametro da funcao atrapalha em algo
    cvSegmentFGMask(temp, 1, 4.f, 0, cvPoint(0,0));

    // Da imagem segmentada, deixa apenas o objeto de maior area
    float percentualOcupado = onlyBiggerObject(temp);

    //Definindo ROI limiar do maior objeto encontrado
    if(percentualOcupado){
        int max_x = 0;
        int max_y = 0;
        int min_x = temp->width;
        int min_y = temp->height;
        for(i = 0; i < temp->height; i++){
            for(j = 0; j < temp->width; j++){
                if(cvGet2D(temp,i,j).val[0] != 0){
                    if(min_x > j) min_x = j;
                    if(min_y > i) min_y = i;
                    if(max_x < j) max_x = j;
                    if(max_y < i) max_y = i;
                }
            }
        }
        //cvSetImageROI(rawImage, cvRect(min_x, min_y, max_x, max_y));
 
        rectRoi = cvRect(min_x, min_y, max_x-min_x, max_y-min_y);

//        cvRectangle(rawImage,
//            cvPoint(min_x, min_y),
//            cvPoint(max_x, max_y),
//            CV_RGB(255, 0, 0), 1, 0, 0 );
    }

    // Cola pixeis de objeto na imagem retornada
//    for(int i = 0; i < temp->height; i++){
//        for(int j = 0; j < temp->width; j++){
//            if(cvGet2D(temp,i,j).val[0] == 0)
//                cvSet2D(rawImage,i,j,cvScalarAll(0));
//        }
//    }
    
    cvReleaseImage(&temp);
    return rectRoi;
}
