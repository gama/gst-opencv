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

CvSeq * getMatchPoints(const CvSeq *src,
        const GArray *pairs,
        const int PAIR_A_0__PAIR_B_1,
        CvMemStorage *mem_storage) {

    CvSeq *dst;
    dst = cvCloneSeq(src, mem_storage);

    int m, n, idx, find;
    for (m = dst->total - 1; m >= 0; --m) {

        find = 0;
        for (n = 0; n < pairs->len; ++n) {
            IntPair pair = g_array_index(pairs, IntPair, n);
            idx = (!PAIR_A_0__PAIR_B_1) ? pair.a : pair.b;

            if (idx == m) {
                find = 1;
                break;
            }
        }

        if (!find) cvSeqRemove(dst, m);
    }

    return dst;
}

void drawSurfPoints(const CvSeq *seq,
        CvPoint pt_displacement,
        IplImage *img,
        CvScalar color,
        int NOMARK0_MARK1) {

    if (seq == NULL || !seq->total) return;

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

    if (seq != NULL) {
        int i;
        for (i = 0; i < seq->total; ++i) {
            CvSURFPoint* r = (CvSURFPoint*) cvGetSeqElem(seq, i);
            point.x += cvPointFrom32f(r->pt).x + pt_displacement.x;
            point.y += cvPointFrom32f(r->pt).y + pt_displacement.y;
        }

        point.x /= seq->total;
        point.y /= seq->total;
    }

    return point;
}

CvRect surfPointsBoundingRect(const CvSeq *seq) {

    if (seq == NULL || !seq->total) return cvRect(-1, -1, -1, -1);

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

CvRect rectDisplacement(const CvSeq* objectKeypoints,
        const CvSeq* imageKeypoints,
        const GArray* pairs,
        const CvRect objectRect) {

    CvPoint point_obj, point_img;
    point_obj.x = point_obj.y = point_img.x = point_img.y = 0;

    if (pairs->len) {
        int n;
        for (n = 0; n < pairs->len; ++n) {
            IntPair pair = g_array_index(pairs, IntPair, n);
            CvSURFPoint* s_point_obj = (CvSURFPoint*) cvGetSeqElem(objectKeypoints, pair.a);
            CvSURFPoint* s_point_img = (CvSURFPoint*) cvGetSeqElem(imageKeypoints, pair.b);
            point_obj.x += cvPointFrom32f(s_point_obj->pt).x;
            point_obj.y += cvPointFrom32f(s_point_obj->pt).y;
            point_img.x += cvPointFrom32f(s_point_img->pt).x;
            point_img.y += cvPointFrom32f(s_point_img->pt).y;
        }

        point_obj.x /= pairs->len;
        point_obj.y /= pairs->len;
        point_img.x /= pairs->len;
        point_img.y /= pairs->len;
    }

    return cvRect(point_img.x - point_obj.x, point_img.y - point_obj.y, objectRect.width, objectRect.height);
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
