/*
 * Copyright (C) 2009 Lucas Amorim <lucas@vettalabs.com>
 */

#include "surf.h"

double
compareSURFDescriptors(const float* d1, const float* d2, double best, int length)
{

    int i;
    double total_cost;

    assert(length % 4 == 0);

    for (i = 0, total_cost = 0.0; i < length; i += 4) {
        double t0 = d1[i] - d2[i];
        double t1 = d1[i + 1] - d2[i + 1];
        double t2 = d1[i + 2] - d2[i + 2];
        double t3 = d1[i + 3] - d2[i + 3];
        total_cost += t0 * t0 + t1 * t1 + t2 * t2 + t3 * t3;
        if (total_cost > best)
            break;
    }
    return total_cost;
}

int
naiveNearestNeighbor(const float* vec, const int laplacian, const CvSeq* model_keypoints,
                     const CvSeq* model_descriptors)
{

    CvSeqReader reader, kreader;
    int         length, i, neighbor;
    double      d, dist1, dist2;

    length = (int) (model_descriptors->elem_size / sizeof (float));
    neighbor = -1;
    dist1 = dist2 = 1e6;

    cvStartReadSeq(model_keypoints, &kreader, 0);
    cvStartReadSeq(model_descriptors, &reader, 0);

    for (i = 0; i < model_descriptors->total; i++) {
        const CvSURFPoint* kp = (const CvSURFPoint*) kreader.ptr;
        const float* mvec = (const float*) reader.ptr;
        CV_NEXT_SEQ_ELEM(kreader.seq->elem_size, kreader);
        CV_NEXT_SEQ_ELEM(reader.seq->elem_size, reader);
        if (laplacian != kp->laplacian)
            continue;
        d = compareSURFDescriptors(vec, mvec, dist2, length);
        if (d < dist1) {
            dist2 = dist1;
            dist1 = d;
            neighbor = i;
        } else if (d < dist2)
            dist2 = d;
    }
    if (dist1 < 0.6 * dist2)
        return neighbor;
    return -1;
}

CvSeq*
getMatchPoints(const CvSeq *src, const GArray *pairs, const int PAIR_A_0__PAIR_B_1, CvMemStorage *storage)
{
    CvSeq   *dst;
    gboolean found;
    gint     m, idx;
    guint    n;

    dst = cvCloneSeq(src, storage);

    for (m = dst->total - 1; m >= 0; --m) {
        found = FALSE;

        for (n = 0; n < pairs->len; ++n) {
            IntPair pair = g_array_index(pairs, IntPair, n);
            idx = (!PAIR_A_0__PAIR_B_1) ? pair.a : pair.b;
            if (idx == m) {
                found = TRUE;
                break;
            }
        }

        if (found == FALSE)
            cvSeqRemove(dst, m);
    }

    return dst;
}

void
drawSurfPoints(const CvSeq *seq, CvPoint pt_displacement, IplImage *img,
               CvScalar color, int NOMARK0_MARK1)
{
    CvPoint point;
    int     i;

    if ((seq == NULL) || (seq->total == 0))
        return;

    for (i = 0; i < seq->total; ++i) {
        CvSURFPoint* r = (CvSURFPoint*) cvGetSeqElem(seq, i);
        point.x = cvPointFrom32f(r->pt).x + pt_displacement.x;
        point.y = cvPointFrom32f(r->pt).y + pt_displacement.y;
        cvCircle(img, point, 3, color, -1, 8, 0);
        if (NOMARK0_MARK1) cvCircle(img, point, 1, cvScalarAll(255), -1, 8, 0);
    }
}

CvPoint
surfCentroid(const CvSeq *seq, CvPoint pt_displacement)
{
    CvPoint point = {0, 0};

    if (seq != NULL) {
        int i;
        for (i = 0; i < seq->total; ++i) {
            CvSURFPoint *r = (CvSURFPoint*) cvGetSeqElem(seq, i);
            point.x += cvPointFrom32f(r->pt).x + pt_displacement.x;
            point.y += cvPointFrom32f(r->pt).y + pt_displacement.y;
        }

        point.x /= seq->total;
        point.y /= seq->total;
    }

    return point;
}

CvRect
surfPointsBoundingRect(const CvSeq *seq)
{
    CvSURFPoint *r;
    CvPoint      point_min, point_max;
    int          i;

    if ((seq == NULL) || (seq->total == 0))
        return cvRect(-1, -1, -1, -1);

    r = (CvSURFPoint*) cvGetSeqElem(seq, 0);
    point_min.x = point_max.x = cvPointFrom32f(r->pt).x;
    point_min.y = point_max.y = cvPointFrom32f(r->pt).y;

    for (i = 1; i < seq->total; ++i) {
        CvSURFPoint* r = (CvSURFPoint*) cvGetSeqElem(seq, i);
        if (cvPointFrom32f(r->pt).x < point_min.x) point_min.x = cvPointFrom32f(r->pt).x;
        if (cvPointFrom32f(r->pt).y < point_min.y) point_min.y = cvPointFrom32f(r->pt).y;
        if (cvPointFrom32f(r->pt).x > point_max.x) point_max.x = cvPointFrom32f(r->pt).x;
        if (cvPointFrom32f(r->pt).y > point_max.y) point_max.y = cvPointFrom32f(r->pt).y;
    }

    return cvRect(point_min.x, point_min.y, point_max.x - point_min.x, point_max.y - point_min.y);
}

static gint
sortDoubleArray(gconstpointer a, gconstpointer b)
{
    if (*(double *) a < *(double *) b) return -1;
    else if (*(double *) a > *(double *) b) return 1;
    else return 0;
}

CvRect
rectDisplacement(const CvSeq* objectKeypoints, const CvSeq* imageKeypoints,
                 const GArray* pairs, const CvRect objectRect,
                 const float pairs_perc_considerate)
{
    CvPoint point_obj, point_img;
    gint    real_size;
    guint   n;
    double  lim = 0.0;

    // Calculates the maximum acceptable displacement feature
    if (pairs->len >= 2) {
        GArray *dists = g_array_new(FALSE, FALSE, sizeof (double));

        for (n = 0; n < pairs->len; ++n) {
            IntPair      pair;
            CvSURFPoint *s_point_obj, *s_point_img;
            int          x1, y1, x2, y2;
            double       d;

            pair        = g_array_index(pairs, IntPair, n);
            s_point_obj = (CvSURFPoint*) cvGetSeqElem(objectKeypoints, pair.a);
            s_point_img = (CvSURFPoint*) cvGetSeqElem(imageKeypoints, pair.b);
            x1          = cvPointFrom32f(s_point_obj->pt).x + objectRect.x;
            y1          = cvPointFrom32f(s_point_obj->pt).y + objectRect.y;
            x2          = cvPointFrom32f(s_point_img->pt).x;
            y2          = cvPointFrom32f(s_point_img->pt).y;
            d           = sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));

            g_array_append_val(dists, d);
        }

        g_array_sort(dists, sortDoubleArray);
        lim = g_array_index(dists, double, (int) (dists->len * pairs_perc_considerate));
    }

    real_size = 0;
    point_obj.x = point_obj.y = point_img.x = point_img.y = 0;

    if (pairs->len) {
        for (n = 0; n < pairs->len; ++n) {
            IntPair      pair;
            CvSURFPoint *s_point_obj, *s_point_img;

            pair        = g_array_index(pairs, IntPair, n);
            s_point_obj = (CvSURFPoint*) cvGetSeqElem(objectKeypoints, pair.a);
            s_point_img = (CvSURFPoint*) cvGetSeqElem(imageKeypoints, pair.b);

            // compare with maximum acceptable displacement feature
            if (pairs->len >= 2) {
                int    x1, y1, x2, y2;
                double d;

                x1 = cvPointFrom32f(s_point_obj->pt).x + objectRect.x;
                y1 = cvPointFrom32f(s_point_obj->pt).y + objectRect.y;
                x2 = cvPointFrom32f(s_point_img->pt).x;
                y2 = cvPointFrom32f(s_point_img->pt).y;
                d  = sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
                if (d > lim) continue;
            }

            point_obj.x += cvPointFrom32f(s_point_obj->pt).x;
            point_obj.y += cvPointFrom32f(s_point_obj->pt).y;
            point_img.x += cvPointFrom32f(s_point_img->pt).x;
            point_img.y += cvPointFrom32f(s_point_img->pt).y;

            real_size++;
        }

        point_obj.x /= real_size;
        point_obj.y /= real_size;
        point_img.x /= real_size;
        point_img.y /= real_size;
    }

    return cvRect(point_img.x - point_obj.x, point_img.y - point_obj.y, objectRect.width, objectRect.height);
}

void
findPairs(const CvSeq* objectKeypoints, const CvSeq* objectDescriptors,
          const CvSeq* imageKeypoints, const CvSeq* imageDescriptors, GArray* array)
{
    CvSeqReader reader, kreader;
    int         i;

    cvStartReadSeq(objectKeypoints, &kreader, 0);
    cvStartReadSeq(objectDescriptors, &reader, 0);

    for (i = 0; i < objectDescriptors->total; i++) {
        const CvSURFPoint *kp;
        const float       *descriptor;
        gint               nearest_neighbor;

        kp = (const CvSURFPoint*) kreader.ptr;
        descriptor = (const float*) reader.ptr;
        CV_NEXT_SEQ_ELEM(kreader.seq->elem_size, kreader);
        CV_NEXT_SEQ_ELEM(reader.seq->elem_size, reader);
        nearest_neighbor = naiveNearestNeighbor(descriptor, kp->laplacian, imageKeypoints, imageDescriptors);
        if (nearest_neighbor >= 0) {
            IntPair pair;
            pair.a = i;
            pair.b = nearest_neighbor;
            g_array_append_val(array, pair);
        }
    }
}
