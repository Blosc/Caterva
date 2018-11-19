/*
 * Copyright (C) 2018  Francesc Alted
 * Copyright (C) 2018  Aleix Alcacer
 */

#include "test_common.h"

void test_roundtrip(caterva_ctx *ctx, size_t ndim, size_t *shape_, size_t *pshape_) {

    caterva_dims shape = caterva_new_dims(shape_, ndim);
    caterva_dims pshape = caterva_new_dims(pshape_, ndim);

    caterva_array *src = caterva_empty_array(ctx, NULL, pshape);

    size_t buf_size = 1;
    for (int i = 0; i < CATERVA_MAXDIM; ++i) {
        buf_size *= (shape.dims[i]);
    }

    /* Create original data */
    double *bufsrc = (double *) malloc(buf_size * sizeof(double));
    fill_buf(bufsrc, buf_size);

    /* Fill empty caterva_array with original data */
    caterva_from_buffer(src, shape, bufsrc);

    /* Fill dest array with caterva_array data */
    double *bufdest = (double *) malloc(buf_size * sizeof(double));
    caterva_to_buffer(src, bufdest);

    /* Testing */
    assert_buf(bufsrc, bufdest, buf_size, 1e-15);

    /* Free mallocs */
    free(bufsrc);
    free(bufdest);
    caterva_free_array(src);
}

LWTEST_DATA(roundtrip) {
    caterva_ctx *ctx;
};

LWTEST_SETUP(roundtrip) {
    data->ctx = caterva_new_ctx(NULL, NULL, BLOSC_CPARAMS_DEFAULTS, BLOSC_DPARAMS_DEFAULTS);
    data->ctx->cparams.typesize = sizeof(double);
}

LWTEST_TEARDOWN(roundtrip) {
    caterva_free_ctx(data->ctx);
}

LWTEST_FIXTURE(roundtrip, 3_dim) {
    const size_t ndim = 3;
    size_t shape_[ndim] = {4, 3, 3};
    size_t pshape_[ndim] = {2, 2, 2};

    test_roundtrip(data->ctx, ndim, shape_, pshape_);
}

LWTEST_FIXTURE(roundtrip, 3_dim_2) {
    const size_t ndim = 3;
    size_t shape_[ndim] = {134, 56, 204};
    size_t pshape_[ndim] = {26, 17, 34};

    test_roundtrip(data->ctx, ndim, shape_, pshape_);
}

LWTEST_FIXTURE(roundtrip, 4_dim) {
    const size_t ndim = 4;
    size_t shape_[ndim] = {4, 3, 8, 5};
    size_t pshape_[ndim] = {2, 2, 3, 3};

    test_roundtrip(data->ctx, ndim, shape_, pshape_);
}

LWTEST_FIXTURE(roundtrip, 4_dim_2) {
    const size_t ndim = 4;
    size_t shape_[ndim] = {78, 85, 34, 56};
    size_t pshape_[ndim] = {13, 32, 18, 12};

    test_roundtrip(data->ctx, ndim, shape_, pshape_);
}

LWTEST_FIXTURE(roundtrip, 5_dim) {
    const size_t ndim = 5;
    size_t shape_[ndim] = {4, 3, 8, 5, 10};
    size_t pshape_[ndim] = {2, 2, 3, 3, 4};

    test_roundtrip(data->ctx, ndim, shape_, pshape_);
}

LWTEST_FIXTURE(roundtrip, 5_dim_2) {
    const size_t ndim = 5;
    size_t shape_[ndim] = {35, 55, 24, 36, 12};
    size_t pshape_[ndim] = {13, 32, 18, 12, 5};

    test_roundtrip(data->ctx, ndim, shape_, pshape_);
}

LWTEST_FIXTURE(roundtrip, 6_dim) {
    const size_t ndim = 6;
    size_t shape_[ndim] = {4, 3, 8, 5, 10, 12};
    size_t pshape_[ndim] = {2, 2, 3, 3, 4, 5};

    test_roundtrip(data->ctx, ndim, shape_, pshape_);
}

LWTEST_FIXTURE(roundtrip, 7_dim) {
    const size_t ndim = 7;
    size_t shape_[ndim] = {12, 15, 24, 16, 12, 8, 7};
    size_t pshape_[ndim] = {5, 7, 9, 8, 5, 3, 7};

    test_roundtrip(data->ctx, ndim, shape_, pshape_);
}

LWTEST_FIXTURE(roundtrip, 8_dim) {
    const size_t ndim = 8;
    size_t shape_[ndim] = {4, 3, 8, 5, 10, 12, 6, 4};
    size_t pshape_[ndim] = {3, 2, 3, 3, 4, 5, 4, 2};

    test_roundtrip(data->ctx, ndim, shape_, pshape_);
}