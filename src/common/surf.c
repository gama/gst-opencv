#include <surf.h>

double compareSURFDescriptors(const float* d1,
        const float* d2,
        double best,
        int length) {

    int i;
    double total_cost = 0;
    assert(length % 4 == 0);
    for (i = 0; i < length; i += 4) {
        double t0 = d1[i] - d2[i];
        double t1 = d1[i + 1] - d2[i + 1];
        double t2 = d1[i + 2] - d2[i + 2];
        double t3 = d1[i + 3] - d2[i + 3];
        total_cost += t0 * t0 + t1 * t1 + t2 * t2 + t3*t3;
        if (total_cost > best)
            break;
    }
    return total_cost;
}

int naiveNearestNeighbor(const float* vec,
        int laplacian,
        const CvSeq* model_keypoints,
        const CvSeq* model_descriptors) {

    int length = (int) (model_descriptors->elem_size / sizeof (float));
    int i, neighbor = -1;
    double d, dist1 = 1e6, dist2 = 1e6;
    CvSeqReader reader, kreader;
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

gint sort_report_entry_b(gconstpointer _a, gconstpointer _b) {
    const IntPair *a = _a;
    const IntPair *b = _b;
    if (a->b < b->b) return -1;
    return 1;
}

gint sort_report_entry_a(gconstpointer _a, gconstpointer _b) {
    const IntPair *a = _a;
    const IntPair *b = _b;
    if (a->a < b->a) return -1;
    return 1;
}

CvSeq * getMatchPoints(const CvSeq *src,
        const GArray *pairs,
        const int PAIR_A_0__PAIR_B_1) {

    if (!PAIR_A_0__PAIR_B_1) g_array_sort(pairs, sort_report_entry_a);
    else g_array_sort(pairs, sort_report_entry_b);

    CvSeq *dst;
    dst = cvCloneSeq(src, NULL);

    int i, j;
    for (i = pairs->len - 1, j = dst->total - 1; j >= 0;) {
        IntPair pair = g_array_index(pairs, IntPair, i);
        int idx = (!PAIR_A_0__PAIR_B_1) ? pair.a : pair.b;

        if (idx != j || i < 0) cvSeqRemove(dst, j);
        else --i;

        --j;
    }
    return dst;
}

void drawSurfPoints(const CvSeq *seq,
        CvPoint pt_displacement,
        IplImage *img,
        CvScalar color,
        int NOMARK0_MARK1) {

    int i;
    CvPoint point;
    for (i = 0; i < seq->total; ++i) {
        CvSURFPoint* r = (CvSURFPoint*) cvGetSeqElem(seq, i);
        point.x = cvPointFrom32f(r->pt).x + pt_displacement.x;
        point.y = cvPointFrom32f(r->pt).y + pt_displacement.y;
        cvCircle(img, point, 3, color, -1, 8, 0);
        if (NOMARK0_MARK1) cvCircle(img, point, 1, cvScalarAll(255), -1, 8, 0);
    }
}

CvPoint surfCentroid(const CvSeq *seq,
        CvPoint pt_displacement) {

    CvPoint point;
    point.x = point.y = 0;

    int i;
    for (i = 0; i < seq->total; ++i) {
        CvSURFPoint* r = (CvSURFPoint*) cvGetSeqElem(seq, i);
        point.x += cvPointFrom32f(r->pt).x + pt_displacement.x;
        point.y += cvPointFrom32f(r->pt).y + pt_displacement.y;
    }

    point.x /= seq->total;
    point.y /= seq->total;

    return point;
}

CvRect surfPointsBoundingRect(const CvSeq *seq) {

    CvPoint point_min, point_max;

    CvSURFPoint* r = (CvSURFPoint*) cvGetSeqElem(seq, 0);
    point_min.x = point_max.x = cvPointFrom32f(r->pt).x;
    point_min.y = point_max.y = cvPointFrom32f(r->pt).y;

    int i;
    for (i = 1; i < seq->total; ++i) {
        CvSURFPoint* r = (CvSURFPoint*) cvGetSeqElem(seq, i);
        if (cvPointFrom32f(r->pt).x < point_min.x) point_min.x = cvPointFrom32f(r->pt).x;
        if (cvPointFrom32f(r->pt).y < point_min.y) point_min.y = cvPointFrom32f(r->pt).y;
        if (cvPointFrom32f(r->pt).x > point_max.x) point_max.x = cvPointFrom32f(r->pt).x;
        if (cvPointFrom32f(r->pt).y > point_max.y) point_max.y = cvPointFrom32f(r->pt).y;
    }

    return cvRect(point_min.x, point_min.y, point_max.x - point_min.x, point_max.y - point_min.y);
}

/*
// Surf centroid with index
CvPoint surfCentroid(const CvSeq *seq,
       CvPoint pt_displacement,
       GArray *pairs,
       const int PAIR_OFF_0__PAIR_A_1__PAIR_B_2) {

   CvPoint point;
   point.x = point.y = -1;

   if (seq == NULL || seq->total <= 0 || (PAIR_OFF_0__PAIR_A_1__PAIR_B_2 && (pairs == NULL || pairs->len <= 0)))
       return point;

   point.x = point.y = 0;

   int i;
   int size = (PAIR_OFF_0__PAIR_A_1__PAIR_B_2) ? pairs->len : seq->total;
   for (i = 0; i < size; ++i) {

       int idx;
       if (PAIR_OFF_0__PAIR_A_1__PAIR_B_2) {
           IntPair pair = g_array_index(pairs, IntPair, i);
           idx = (PAIR_OFF_0__PAIR_A_1__PAIR_B_2 == 2) ? pair.b : pair.a;
       } else {
           idx = i;
       }

       CvSURFPoint* r = (CvSURFPoint*) cvGetSeqElem(seq, idx);

       point.x += cvPointFrom32f(r->pt).x + pt_displacement.x;
       point.y += cvPointFrom32f(r->pt).y + pt_displacement.y;
   }

   point.x /= size;
   point.y /= size;

   return point;
}
 */


CvRect rectDisplacement(const CvSeq* objectKeypoints,
        const CvSeq* imageKeypoints,
        const GArray* pairs,
        const CvRect objectRect) {

    CvSeq* obj;
    CvSeq* img;
    obj = cvCloneSeq(objectKeypoints, NULL);
    img = cvCloneSeq(imageKeypoints, NULL);
    obj = getMatchPoints(obj, pairs, 0);
    img = getMatchPoints(img, pairs, 1);
    CvPoint p_obj = surfCentroid(obj, cvPoint(objectRect.x, objectRect.y));
    CvPoint p_img = surfCentroid(img, cvPoint(0, 0));
    cvClearSeq(obj);
    cvClearSeq(img);

    return cvRect(objectRect.x + p_img.x - p_obj.x, objectRect.y + p_img.y - p_obj.y, objectRect.width, objectRect.height);

}

void findPairs(const CvSeq* objectKeypoints,
        const CvSeq* objectDescriptors,
        const CvSeq* imageKeypoints,
        const CvSeq* imageDescriptors,
        GArray* array) {

    int i;
    CvSeqReader reader, kreader;

    cvStartReadSeq(objectKeypoints, &kreader, 0);
    cvStartReadSeq(objectDescriptors, &reader, 0);

    for (i = 0; i < objectDescriptors->total; i++) {
        const CvSURFPoint* kp = (const CvSURFPoint*) kreader.ptr;
        const float* descriptor = (const float*) reader.ptr;
        CV_NEXT_SEQ_ELEM(kreader.seq->elem_size, kreader);
        CV_NEXT_SEQ_ELEM(reader.seq->elem_size, reader);
        int nearest_neighbor = naiveNearestNeighbor(descriptor, kp->laplacian, imageKeypoints, imageDescriptors);
        if (nearest_neighbor >= 0) {
            IntPair pair;
            pair.a = i;
            pair.b = nearest_neighbor;
            g_array_append_val(array, pair);
        }
    }
}
