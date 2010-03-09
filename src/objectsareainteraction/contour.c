/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 */

#include "contour.h"

void makeContour(const gchar *str, CvSeq **seq, CvMemStorage* storage) {

    g_assert(str);

    // Parser string to points array
    GArray *array;
    array = g_array_new(FALSE, FALSE, sizeof (CvPoint));
    gchar **str_pt = NULL;
    str_pt = g_strsplit(str, ",", -1);
    g_assert(str_pt);
    gint ln = 0;
    while (str_pt[ln]) {
        if (strlen(str_pt[ln]) > 1) {
            gchar **str_pt_coord = NULL;
            str_pt_coord = g_strsplit(str_pt[ln], "x", 2);
            g_assert(str_pt_coord);
            CvPoint point_temp = cvPoint(atoi(str_pt_coord[0]), atoi(str_pt_coord[1]));
            g_array_append_val(array, point_temp);
            g_strfreev(str_pt_coord);
        }
        ln++;
    }
    g_strfreev(str_pt);

    // Convert points array in MAT
    CvContour header;
    CvSeqBlock block;
    CvMat* vector = cvCreateMat(1, array->len, CV_32SC2);
    int n;
    for (n = 0; n < array->len; ++n)
        CV_MAT_ELEM(*vector, CvPoint, 0, n) = g_array_index(array, CvPoint, n);
    g_array_free(array, TRUE);

    // Convert MAT in SEQ
    *seq = cvCloneSeq(cvPointSeqFromMat(CV_SEQ_KIND_CURVE + CV_SEQ_FLAG_CLOSED, vector, &header, &block), storage);

    return;
}

void calcInterception(const InstanceObjectAreaContour *a, const InstanceObjectAreaContour *b, InstanceObjectAreaContourResult *dst) {

    dst->contour = NULL;

    CvRect rect_a = cvBoundingRect(a->contour, 1);
    CvRect rect_b = cvBoundingRect(b->contour, 1);

    if (rectIntercept(&rect_a, &rect_b)) {

        CvRect rect_sum = cvMaxRect(&rect_a, &rect_b);
        IplImage* img_a = cvCreateImage(cvSize(rect_sum.x + rect_sum.width, rect_sum.y + rect_sum.height), 8, 1);
        IplImage* img_b = cvCreateImage(cvSize(rect_sum.x + rect_sum.width, rect_sum.y + rect_sum.height), 8, 1);
        cvZero(img_a);
        cvZero(img_b);
        cvDrawContours(img_a, a->contour, cvScalarAll(255), cvScalarAll(255), 0, -1, 8, cvPoint(0, 0));
        cvDrawContours(img_b, b->contour, cvScalarAll(255), cvScalarAll(255), 0, -1, 8, cvPoint(0, 0));

        cvAnd(img_a, img_b, img_a, NULL);

        dst->mem_storage = cvCreateMemStorage(0);
        CvContourScanner scanner = cvStartFindContours(img_a, dst->mem_storage, sizeof (CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cvPoint(0, 0));
        dst->contour = cvFindNextContour(scanner);
        cvReleaseImage(&img_a);
        cvReleaseImage(&img_b);

        if(dst->contour != NULL){
            dst->id_a = a->id;
            dst->id_b = b->id;
            dst->perc_a = (gdouble) cvContourArea(dst->contour, CV_WHOLE_SEQ) / cvContourArea(a->contour, CV_WHOLE_SEQ);
            dst->perc_b = (gdouble) cvContourArea(dst->contour, CV_WHOLE_SEQ) / cvContourArea(b->contour, CV_WHOLE_SEQ);
        }
    }
}
