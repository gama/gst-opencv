/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 */

#include "draw.h"

void printText(IplImage *dst, CvPoint point, const char *text, CvScalar color, float font_scale, int box1_nobox0) {
    const int labelBorder = 4;
    CvFont font;
    CvSize text_size;
    int baseline;

    // setup font and calculate text size
    cvInitFont(&font, CV_FONT_HERSHEY_DUPLEX, font_scale, font_scale, 0, 1, CV_AA);
    cvGetTextSize(text, &font, &text_size, &baseline);

    
    int x = (text_size.width  + ((box1_nobox0)?(2*labelBorder):0))/2;
    int y = (text_size.height + ((box1_nobox0)?(2*labelBorder):0))/2;
    if(point.x - x < 0) point.x = x;
    if(point.y - y < 0) point.y = y;
    if(point.x + x > dst->width ) point.x = dst->width  - x;
    if(point.y + y > dst->height) point.y = dst->height - y;
    
    // the text is drawn using the given color as background and black as foreground
    if (box1_nobox0) {
        cvRectangle(dst,
                cvPoint((point.x - (text_size.width/2) - labelBorder), (point.y - (text_size.height/2) - labelBorder)),
                cvPoint((point.x + (text_size.width/2) + labelBorder), (point.y + (text_size.height/2) + labelBorder)),
                color, -1, 8, 0);
        color = cvScalarAll(255);
    }
    
    cvPutText(dst, text, cvPoint((point.x - (text_size.width/2)), (point.y + (text_size.height/2))), &font, color);
}

void monitorImage(IplImage *src_bin, IplImage *dst, int scale, int quadrant) {

    int w = dst->width / scale;
    int h = dst->height / scale;

    CvRect rectPos;
    if (quadrant == 1)
        rectPos = cvRect(dst->width - w, 0, w, h);
    else if (quadrant == 2)
        rectPos = cvRect(dst->width - w, dst->height - h, w, h);
    else if (quadrant == 3)
        rectPos = cvRect(0, dst->height - h, w, h);
    else
        rectPos = cvRect(0, 0, w, h);

    IplImage * min_mask = cvCreateImage(cvSize(w, h), 8, 1);
    cvResize(src_bin, min_mask, CV_INTER_LINEAR);

    IplImage *min_maskc = cvCreateImage(cvSize(w, h), 8, 3);
    cvCvtColor(min_mask, min_maskc, CV_GRAY2BGR);

    cvSetImageROI(dst, rectPos);
    cvCopy(min_maskc, dst, 0);
    cvResetImageROI(dst);

    cvReleaseImage(&min_mask);
    cvReleaseImage(&min_maskc);
}

int contourIsFull(CvSeq *contour, IplImage *src) {
    const int desloc = 1;
    CvRect rect = cvBoundingRect(contour, 0);
    if (rect.x <= desloc || rect.y <= desloc ||
            (rect.x + rect.width) >= (src->width - desloc) ||
            (rect.y + rect.height) >= (src->height - desloc))
        return 0;
    else
        return 1;
}

double euclid_dist_points(CvPoint2D32f p1, CvPoint2D32f p2) {
    return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

CvScalar colorIdx(int i){
    CvScalar vet[] = {COLOR_GREEN,COLOR_MAGENTA,COLOR_BLUE,COLOR_RED,COLOR_YELLOW,COLOR_CIAN};
    return vet[i%6];
}

void pr(CvRect ob){
    g_print("RECT\tx1=%i\ty1=%i\tx2=%i\ty2=%i\twidth=%i\theigth=%i\n",ob.x, ob.y, ob.x+ob.width, ob.y+ob.height, ob.width, ob.height);
}

void pp(CvPoint ob){
    g_print("POINT\tx=%i\ty=%i\n",ob.x, ob.y);
}

void pi(int i){
    g_print("INT\t%i\n",i);
}

void ps(int i){
    int j=0;
    while(j<i){
        g_print("\n");
        ++j;
    }
}

void pt(char *s){
    g_print("%s\n",s);
}

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
