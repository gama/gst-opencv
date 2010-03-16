/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 */

#include "contour.h"

void
makeContour(const gchar *str, CvSeq **seq, CvMemStorage *storage)
{
    CvContour  header;
    CvSeqBlock block;
    CvMat     *vector;
    GArray    *array;
    guint      n, ln;
    gchar    **str_pt;

    g_assert(str != NULL);

    // Parser string to points array
    array = g_array_new(FALSE, FALSE, sizeof(CvPoint));
    str_pt = g_strsplit(str, ",", -1);
    ln = 0;

    if (str_pt == NULL) {
        g_warning("unable to parse contour string: \"%s\"", str);
        g_strfreev(str_pt);
        return;
    }

    for (; str_pt[ln] != NULL; ++ln) {
        if (strlen(str_pt[ln]) > 1) {
            gchar **str_pt_coord;
            CvPoint point;

            str_pt_coord = g_strsplit(str_pt[ln], "x", 2);
            if (str_pt_coord != NULL) {
                g_warning("unable to parse contour string: \"%s\"", str);
                continue;
            }

            point = cvPoint(atoi(str_pt_coord[0]), atoi(str_pt_coord[1]));
            g_array_append_val(array, point);
            g_strfreev(str_pt_coord);
        }
    }
    g_strfreev(str_pt);

    // Convert points array in MAT
    vector = cvCreateMat(1, array->len, CV_32SC2);
    for (n = 0; n < array->len; ++n)
        CV_MAT_ELEM(*vector, CvPoint, 0, n) = g_array_index(array, CvPoint, n);
    g_array_free(array, TRUE);

    // Convert MAT in SEQ
    *seq = cvCloneSeq(cvPointSeqFromMat(CV_SEQ_KIND_CURVE + CV_SEQ_FLAG_CLOSED, vector, &header, &block), storage);
}

void
calcInterception(const InstanceObjectAreaContour *a,
                 const InstanceObjectAreaContour *b,
                 InstanceObjectAreaContour       *dst)
{
    CvRect rect_a, rect_b;

    dst->contour = NULL;
    rect_a       = cvBoundingRect(a->contour, 1);
    rect_b       = cvBoundingRect(b->contour, 1);

    if (rectIntercept(&rect_a, &rect_b)) {
        CvContourScanner scanner;
        CvRect           rect_sum;
        IplImage        *img_a, *img_b;

        rect_sum = cvMaxRect(&rect_a, &rect_b);
        img_a = cvCreateImage(cvSize(rect_sum.x + rect_sum.width, rect_sum.y + rect_sum.height), 8, 1);
        img_b = cvCreateImage(cvSize(rect_sum.x + rect_sum.width, rect_sum.y + rect_sum.height), 8, 1);

        cvZero(img_a);
        cvZero(img_b);
        cvDrawContours(img_a, a->contour, cvScalarAll(255), cvScalarAll(255), 0, -1, 8, cvPoint(0, 0));
        cvDrawContours(img_b, b->contour, cvScalarAll(255), cvScalarAll(255), 0, -1, 8, cvPoint(0, 0));

        cvAnd(img_a, img_b, img_a, NULL);

        dst->mem_storage = cvCreateMemStorage(0);
        scanner          = cvStartFindContours(img_a, dst->mem_storage, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cvPoint(0, 0));
        dst->contour     = cvFindNextContour(scanner);

        cvReleaseImage(&img_a);
        cvReleaseImage(&img_b);
    }
}
