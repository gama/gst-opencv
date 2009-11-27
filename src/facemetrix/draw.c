/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 * Copyright (C) 2009 Erickson Nascimento <erickson@vettalabs.com>
 */

#include "draw.h"

#define LABEL_BORDER 4

CvScalar colorIdx(int i){
    CvScalar vet[] = {COLOR_GREEN,COLOR_MAGENTA,COLOR_BLUE,COLOR_RED,COLOR_YELLOW,COLOR_CIAN};
    return vet[i%6];
}

void pr(CvRect ob){g_print("RECT\tx1=%i\ty1=%i\tx2=%i\ty2=%i\twidth=%i\theigth=%i\n",ob.x, ob.y, ob.x+ob.width, ob.y+ob.height, ob.width, ob.height);}
void pp(CvPoint ob){g_print("POINT\tx=%i\ty=%i\n",ob.x, ob.y);}
void pi(int i){g_print("INT\t%i\n",i);}
void ps(int i){int j=0; while(j<i){g_print("\n");++j;}}
void pt(char *s){g_print("%s\n",s);}

void draw_face_id(IplImage *dst, const gchar *face_id, const CvRect *face_rect,
                  CvScalar color, float font_scale, gboolean draw_face_box) {
    CvFont font;
    CvSize text_size;
    int    baseline;

    // sanity checks
    g_return_if_fail(dst       != NULL);
    g_return_if_fail(face_id   != NULL);
    g_return_if_fail(face_rect != NULL);

    // draw a box around the face, if requested
    if (draw_face_box)
        cvRectangle(dst,
                    cvPoint(face_rect->x, face_rect->y),
                    cvPoint(face_rect->x + face_rect->width, face_rect->y + face_rect->height),
                    color, 1, 8, 0);

    // setup font and calculate text size
    cvInitFont(&font, CV_FONT_HERSHEY_DUPLEX, font_scale, font_scale, 0, 1, CV_AA);
    cvGetTextSize(face_id, &font, &text_size, &baseline);

    // the text is drawn using the given color as background and black as foreground
    cvRectangle(dst,
                cvPoint(face_rect->x - LABEL_BORDER, face_rect->y - text_size.height - LABEL_BORDER),
                cvPoint(face_rect->x + text_size.width + LABEL_BORDER, face_rect->y + LABEL_BORDER),
                color, -1, 8, 0);
    cvPutText(dst, face_id , cvPoint(face_rect->x, face_rect->y), &font, cvScalarAll(0));
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
