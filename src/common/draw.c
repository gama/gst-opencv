/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 */

#include "draw.h"
#include <glib.h>

#define DISPLACEMENT 1

void
printText(IplImage *dst, CvPoint point, const char *text, CvScalar color,
          float font_scale, gint box1_nobox0)
{
    CvFont font;
    CvSize text_size;
    gint   baseline, x, y;

    // setup font and calculate text size
    cvInitFont(&font, CV_FONT_HERSHEY_DUPLEX, font_scale, font_scale, 0, 1, CV_AA);
    cvGetTextSize(text, &font, &text_size, &baseline);

    x = (text_size.width  + ((box1_nobox0)?(2*LABEL_BORDER):0))/2;
    y = (text_size.height + ((box1_nobox0)?(2*LABEL_BORDER):0))/2;

    if (point.x - x < 0) point.x = x;
    if (point.y - y < 0) point.y = y;
    if (point.x + x > dst->width ) point.x = dst->width  - x;
    if (point.y + y > dst->height) point.y = dst->height - y;

    // the text is drawn using the given color as background and black as foreground
    if (box1_nobox0) {
        cvRectangle(dst,
                    cvPoint((point.x - (text_size.width/2) - LABEL_BORDER), (point.y - (text_size.height/2) - LABEL_BORDER)),
                    cvPoint((point.x + (text_size.width/2) + LABEL_BORDER), (point.y + (text_size.height/2) + LABEL_BORDER)),
                    color, -1, 8, 0);
        color = cvScalarAll(255);
    }

    cvPutText(dst, text, cvPoint((point.x - (text_size.width/2)), (point.y + (text_size.height/2))), &font, color);
}

void
monitorImage(IplImage *src_bin, IplImage *dst, gint scale, gint quadrant)
{
    IplImage *min_mask, *min_maskc;
    CvRect    rectPos;
    gint      w, h;

    w = dst->width  / scale;
    h = dst->height / scale;

    if (quadrant == 1)
        rectPos = cvRect(dst->width - w, 0, w, h);
    else if (quadrant == 2)
        rectPos = cvRect(dst->width - w, dst->height - h, w, h);
    else if (quadrant == 3)
        rectPos = cvRect(0, dst->height - h, w, h);
    else
        rectPos = cvRect(0, 0, w, h);

    min_mask = cvCreateImage(cvSize(w, h), 8, 1);
    cvResize(src_bin, min_mask, CV_INTER_LINEAR);

    min_maskc = cvCreateImage(cvSize(w, h), 8, 3);
    cvCvtColor(min_mask, min_maskc, CV_GRAY2BGR);

    cvSetImageROI(dst, rectPos);
    cvCopy(min_maskc, dst, 0);
    cvResetImageROI(dst);

    cvReleaseImage(&min_mask);
    cvReleaseImage(&min_maskc);
}

int
contourIsFull(CvSeq *contour, IplImage *src)
{
    CvRect rect = cvBoundingRect(contour, 0);

    if ((rect.x <= DISPLACEMENT)                                ||
        (rect.y <= DISPLACEMENT)                                ||
        ((rect.x + rect.width) >=  (src->width - DISPLACEMENT)) ||
        ((rect.y + rect.height) >= (src->height - DISPLACEMENT)))
        return 0;
    else
        return 1;
}

double
euclid_dist_points(CvPoint2D32f p1, CvPoint2D32f p2)
{
    return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

CvScalar
colorIdx(int i)
{
    CvScalar vet[] = {COLOR_GREEN,COLOR_MAGENTA,COLOR_BLUE,COLOR_RED,COLOR_YELLOW,COLOR_CIAN};
    return vet[i%6];
}

void
pr(CvRect ob)
{
    g_print("RECT\tx1=%i\ty1=%i\tx2=%i\ty2=%i\twidth=%i\theigth=%i\n",ob.x, ob.y, ob.x+ob.width, ob.y+ob.height, ob.width, ob.height);
}

void
pp(CvPoint ob)
{
    g_print("POINT\tx=%i\ty=%i\n",ob.x, ob.y);
}

void
pi(int i)
{
    g_print("INT\t%i\n",i);
}

void
ps(int i)
{
    int j = 0;
    while (j<i) {
        g_print("\n");
        ++j;
    }
}

void
pt(char *s)
{
    g_print("%s\n",s);
}

void
draw_face_id(IplImage *dst, const gchar *face_id, const CvRect *face_rect,
             CvScalar color, float font_scale, gboolean draw_face_box)
{
    CvFont font;
    CvSize text_size;
    gint   baseline;

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

float
distRectToPoint(CvRect rect, CvPoint point)
{
    return (float) sqrt(pow(((rect.x+(rect.width/2)) - point.x), 2) +
                        pow(((rect.y+(rect.height/2)) - point.y), 2));
}

int
pointIntoRect(CvRect rect, CvPoint point)
{
    if ((point.x < rect.x)                ||
        (point.x > (rect.x + rect.width)) ||
        (point.y < rect.y)                ||
        (point.y > (rect.y + rect.height)))
        return 0;
    else
        return 1;
}

CvRect
rectIntersection(const CvRect *a, const CvRect *b)
{
    CvRect r = cvRect(MAX(a->x, b->x), MAX(a->y, b->y), 0, 0);
    r.width  = MIN(a->x + a->width,  b->x + b->width)  - r.x;
    r.height = MIN(a->y + a->height, b->y + b->height) - r.y;
    return r;
}

float
rectIntercept(const CvRect *a, const CvRect *b)
{
    CvRect rect = cvMaxRect(a, b);

    if (rect.width > (a->width + b->width))
        return 0;

    if (rect.height > (a->height + b->height))
        return 0;

    rect = rectIntersection(a, b);
    return (float) (rect.height * rect.width) / (a->height * a->width);
}
void
draw_mesh(IplImage *dst, const CvPoint q1, const CvPoint q2, const CvPoint q3,
        const CvPoint q4, const int w_lines, const int h_lines, CvScalar color)
{
    int i;
    float m, c;
    CvPoint p_0, p_1;

    // Contours
    cvLine(dst, q1, q2, color, 1, 8, 0);
    cvLine(dst, q2, q3, color, 1, 8, 0);
    cvLine(dst, q3, q4, color, 1, 8, 0);
    cvLine(dst, q4, q1, color, 1, 8, 0);

    // Horizontal
    for (i = 0; i < h_lines; ++i) {
        p_0.y = ((i + 1) * ((float) (q3.y - q4.y) / (float) (h_lines + 1))) + q4.y;

        if ((q3.x - q4.x) == 0) {
            p_0.x = q4.x;
        } else {
            m = (float) (q3.y - q4.y) / (float) (q3.x - q4.x);
            c = q4.y - q4.x*m;
            p_0.x = (p_0.y - c) / m;
        }

        p_1.y = ((i + 1) * ((float) (q2.y - q1.y) / (float) (h_lines + 1))) + q1.y;

        if ((q2.x - q1.x) == 0) {
            p_1.x = q1.x;
        } else {
            m = (float) (q2.y - q1.y) / (float) (q2.x - q1.x);
            c = q1.y - q1.x*m;
            p_1.x = (p_1.y - c) / m;
        }

        cvLine(dst, p_0, p_1, color, 1, 8, 0);
    }

    // Vertical
    for (i = 0; i < w_lines; ++i) {
        p_0.x = ((i + 1) * ((float) (q1.x - q4.x) / (float) (w_lines + 1))) + q4.x;

        m = (float) (q1.y - q4.y) / (float) (q1.x - q4.x);
        c = q4.y - q4.x*m;
        p_0.y = (p_0.x * m) + c;

        p_1.x = ((i + 1) * ((float) (q2.x - q3.x) / (float) (w_lines + 1))) + q3.x;

        m = (float) (q2.y - q3.y) / (float) (q2.x - q3.x);
        c = q3.y - q3.x*m;
        p_1.y = (p_1.x * m) + c;

        cvLine(dst, p_0, p_1, color, 1, 8, 0);
    }
}

void
set_proportion_and_border(CvRect *rect, float scale, int border_x, int border_y)
{
    rect->x *= scale;
    rect->y *= scale;
    rect->width *= scale;
    rect->height *= scale;
    rect->x += border_x;
    rect->y += border_y;
}

CvPoint
medium_point(CvPoint a, CvPoint b)
{
    return cvPoint(((b.x - a.x) / 2) + a.x, ((b.y - a.y) / 2) + a.y);
}

int
inner_circle(int r, CvPoint c, CvPoint p)
{
    return ((((p.x - c.x)*(p.x - c.x)) + ((p.y - c.y)*(p.y - c.y))) <= (r * r)) ? 1 : 0;
}

float
euclid_dist_cvpoints(CvPoint p1, CvPoint p2)
{
    return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

double
linear_projection(const double min_real, const double min_projected,
        const double max_real, const double max_projected,
        const double known_projected_value)
{
    double p = (max_real - min_real) / (max_projected - min_projected);
    return ((known_projected_value - min_projected) * p)+min_real; // return 'real' value
}

void
print_rect(IplImage *image, CvRect rect, int thickness)
{
    cvRectangle(image, cvPoint(rect.x, rect.y), cvPoint(rect.x + rect.width, rect.y + rect.height), CV_RGB(0, 0, 255), thickness, 8, 0);
    cvLine(image, cvPoint(rect.x, rect.y), cvPoint(rect.x + rect.width, rect.y + rect.height), CV_RGB(0, 0, 255), thickness, 8, 0);
    cvLine(image, cvPoint(rect.x + rect.width, rect.y), cvPoint(rect.x, rect.y + rect.height), CV_RGB(0, 0, 255), thickness, 8, 0);
}