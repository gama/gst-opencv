#include "draw.h"

CvScalar colorIdx(int i){
    CvScalar vet[] = {COLOR_GREEN,COLOR_MAGENTA,COLOR_BLUE,COLOR_RED,COLOR_YELLOW,COLOR_CIAN};
    return vet[i%6];
}

void pr(CvRect ob){printf("RECT\tx1=%i\ty1=%i\tx2=%i\ty2=%i\twidth=%i\theigth=%i\n",ob.x, ob.y, ob.x+ob.width, ob.y+ob.height, ob.width, ob.height);}
void pp(CvPoint ob){printf("POINT\tx=%i\ty=%i\n",ob.x, ob.y);}
void pi(int i){printf("INT\t%i\n",i);}


void drawFaceIdentify(IplImage *dst, gchar *idFace, CvRect rectFace, CvScalar color){

    //const CvScalar COLORRECT = CV_RGB(0,255,0);
    const int PIXELSIZECHAR = 8;

    CvPoint a,b;
    a.x = rectFace.x;
    a.y = rectFace.y;
    b.x = rectFace.x+rectFace.width;
    b.y = rectFace.y+rectFace.height;

    // Face box
    cvRectangle(dst, a, b, color, 1,1,0);

    // Text box
    if(idFace != NULL){
        cvRectangle(dst,
            cvPoint(a.x, a.y-PIXELSIZECHAR),
            cvPoint((strlen(idFace)*PIXELSIZECHAR)+a.x, a.y),
            color, -1, 1, 0 );
        CvFont font;
        cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 1.0, .5, 0, 1, CV_AA);
        cvPutText(dst, idFace , cvPoint(a.x, a.y), &font, cvScalarAll(0));
    }
    return;
}

