/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 * Copyright (C) 2009 Erickson Nascimento <erickson@vettalabs.com>
 */

#include "draw.h"

CvScalar colorIdx(int i){
    CvScalar vet[] = {COLOR_GREEN,COLOR_MAGENTA,COLOR_BLUE,COLOR_RED,COLOR_YELLOW,COLOR_CIAN};
    return vet[i%6];
}

void pr(CvRect ob){g_print("RECT\tx1=%i\ty1=%i\tx2=%i\ty2=%i\twidth=%i\theigth=%i\n",ob.x, ob.y, ob.x+ob.width, ob.y+ob.height, ob.width, ob.height);}
void pp(CvPoint ob){g_print("POINT\tx=%i\ty=%i\n",ob.x, ob.y);}
void pi(int i){g_print("INT\t%i\n",i);}
void ps(int i){int j=0; while(j<i){g_print("\n");++j;}}
void pt(char *s){g_print("%s\n",s);}

void drawFaceIdentify(IplImage *dst, gchar *idFace, CvRect rectFace, CvScalar color, int drawRect){

    CvPoint a,b;
    a.x = rectFace.x;
    a.y = rectFace.y;
    b.x = rectFace.x+rectFace.width;
    b.y = rectFace.y+rectFace.height;

    // Face box
    if(drawRect)
        cvRectangle(dst, a, b, color, 1,1,0);

    // Text box
    if(idFace != NULL){
        if(!drawRect) a.x = a.x+(rectFace.width/2)-((strlen(idFace)*PIXELSIZECHAR)/2);

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

float distRectToPoint(CvRect rect, CvPoint point){
    return (float)sqrt(pow(((rect.x+(rect.width/2)) - point.x), 2) + pow(((rect.y+(rect.height/2)) - point.y), 2));
}

int pointIntoRect(CvRect rect, CvPoint point){
    if(point.x < rect.x || point.x > (rect.x+rect.width) || point.y < rect.y || point.y > (rect.y+rect.height)) return 0;
    else return 1;
}

int rectIntercept(CvRect *a, CvRect *b){
    CvRect rect_sum = cvMaxRect(a,b);
    if(rect_sum.width > (a->width + b->width)) return 0;
    if(rect_sum.height > (a->height + b->height)) return 0;
    return 1;
}
