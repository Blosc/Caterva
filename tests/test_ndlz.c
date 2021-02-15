/*
    Copyright (C) 2014  Francesc Alted
    http://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    Example program demonstrating use of the Blosc filter from C code.

    To compile this program:

    $ gcc -O many_compressors.c -o many_compressors -lblosc2

    To run:

    $ ./test_ndlz
    Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
    Using 4 threads (previously using 1)
    Using blosclz compressor
    Compression: 4000000 -> 57577 (69.5x)
    Succesful roundtrip!
    Using lz4 compressor
    Compression: 4000000 -> 97276 (41.1x)
    Succesful roundtrip!
    Using lz4hc compressor
    Compression: 4000000 -> 38314 (104.4x)
    Succesful roundtrip!
    Using zlib compressor
    Compression: 4000000 -> 21486 (186.2x)
    Succesful roundtrip!
    Using zstd compressor
    Compression: 4000000 -> 10692 (374.1x)
    Succesful roundtrip!

 */

#include <stdio.h>
#include <blosc2.h>
#include <ndlz4x4.h>
#include "test_common.h"

#define SHAPE1 32
#define SHAPE2 32
#define SIZE SHAPE1 * SHAPE2
#define SHAPE {SHAPE1, SHAPE2}
#define OSIZE (17 * SIZE / 16) + 9 + 8 + BLOSC_MAX_OVERHEAD

static int test_ndlz(void *data, int nbytes, int typesize, int ndim, caterva_params_t params, caterva_storage_t storage) {

    uint8_t *data2 = (uint8_t*) data;
    caterva_array_t *array;
    caterva_context_t *ctx;
    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    caterva_context_new(&cfg, &ctx);
    CATERVA_ERROR(caterva_array_from_buffer(ctx, data2, nbytes, &params, &storage, &array));

    int nchunks = array->nchunks;
    int chunksize = (int) array->extchunknitems * typesize;
    int isize = nchunks * chunksize;
    uint8_t *data_in = malloc(isize);
    int decompressed;
    for (int ci = 0; ci < nchunks; ci++) {
      decompressed = blosc2_schunk_decompress_chunk(array->sc, ci, &data_in[ci * chunksize], chunksize);
      if (decompressed < 0) {
        printf("Error decompressing chunk \n");
        return -1;
      }
    }

    int32_t *blockshape = storage.properties.blosc.blockshape;
    int osize = isize + BLOSC_MAX_OVERHEAD;
    int dsize = isize;
    int csize;
    uint8_t *data_out = malloc(osize);
    uint8_t *data_dest = malloc(dsize);
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;

    /* Create a context for compression */
    cparams.typesize = typesize;
    cparams.compcode = BLOSC_NDLZ;
    cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
    cparams.clevel = 5;
    cparams.nthreads = 1;
    cparams.ndim = ndim;
    cparams.blockshape = blockshape;
    cparams.blocksize = blockshape[0] * blockshape[1] * typesize;
    if (cparams.blocksize < BLOSC_MIN_BUFFERSIZE) {
        printf("\n Blocksize is letter than min \n");
    }

    /* Create a context for decompression */
    dparams.nthreads = 1;
    dparams.schunk = NULL;

    blosc2_context *cctx;
    blosc2_context *dctx;
    cctx = blosc2_create_cctx(cparams);
    dctx = blosc2_create_dctx(dparams);
    /*
    printf("\n data \n");
    for (int i = 0; i < nbytes; i++) {
    printf("%u, ", data2[i]);
    }

    printf("\n ----------------------------------------------------------------------------- TEST NDLZ ----------"
           "----------------------------------------------------------------------- \n");
*/
    blosc_timestamp_t start, comp, end;
    blosc_set_timestamp(&start);

  /* Compress with clevel=5 and shuffle active  */
    csize = blosc2_compress_ctx(cctx, data_in, isize, data_out, osize);
    if (csize == 0) {
        printf("Buffer is uncompressible.  Giving up.\n");
        return 0;
    }
    else if (csize < 0) {
        printf("Compression error.  Error code: %d\n", csize);
        return csize;
    }
    blosc_set_timestamp(&comp);

    printf("Compression: %d -> %d (%.1fx)\n", isize, csize, (1. * isize) / csize);
/*
    printf("\n data_in \n");
    for (int i = 0; i < isize; i++) {
      printf("%u, ", data_in[i]);
    }
    printf("\n output \n");
    for (int i = 0; i < osize; i++) {
      if ((i - 16) % 65 == 0) {
        printf("\n");
      }
      if (data_out[i - 52] == 137) {
        printf("\n");
      }
      printf("%u, ", data_out[i]);
    }
    /* Decompress  */
    dsize = blosc2_decompress_ctx(dctx, data_out, osize, data_dest, dsize);
    if (dsize <= 0) {
        printf("Decompression error.  Error code: %d\n", dsize);
        return dsize;
    }

    blosc_set_timestamp(&end);
    double ctime = blosc_elapsed_nsecs(start, comp);
    double dtime = blosc_elapsed_nsecs(comp, end);

/*
    printf("\n dest \n");
    for (int i = 0; i < dsize; i++) {
        printf("%u, ", data_dest[i]);
    }
*/
    for (int i = 0; i < isize; i++) {
        if (data_in[i] != data_dest[i]) {
            printf("i: %d, data %u, dest %u", i, data_in[i], data_dest[i]);
            printf("\n Decompressed data differs from original!\n");
            return -1;
        }
    }

    caterva_array_free(ctx, &array);
    caterva_context_free(&ctx);
    blosc2_free_ctx(cctx);
    blosc2_free_ctx(dctx);
    free(data_in);
    free(data_out);
    free(data_dest);

    printf("Succesful roundtrip!\n");
    printf("\n Test time: \n Compression: %f secs \n Decompression: %f secs \n", ctime / 1000000000, dtime / 1000000000);
    return dsize - csize;
}

int no_matches() {
    int ndim = 2;
    int typesize = 1;
    int32_t shape[8] = {1024, 512};
    int32_t chunkshape[8] = {32, 32};
    int32_t blockshape[8] = {32, 32};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint8_t *data = malloc(nbytes);
    for (int i = 0; i < isize; i++) {
        data[i] = i;
    }
    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int no_matches_pad() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {19, 21};
    int32_t chunkshape[8] = {14, 16};
    int32_t blockshape[8] = {11, 13};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int i = 0; i < isize; i++) {
        data[i] = (-i^2) * 111111 - (-i^2) * 11111 + i * 1111 - i * 110 + i;
    }
    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int all_elem_eq() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {64, 64};
    int32_t chunkshape[8] = {32, 32};
    int32_t blockshape[8] = {16, 16};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int i = 0; i < isize; i++) {
        data[i] = 1;
    }
    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int all_elem_pad() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {29, 31};
    int32_t chunkshape[8] = {24, 21};
    int32_t blockshape[8] = {12, 14};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int i = 0; i < isize; i++) {
        data[i] = 1;
    }
    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int same_cells() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {32, 32};
    int32_t chunkshape[8] = {24, 24};
    int32_t blockshape[8] = {16, 16};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int i = 0; i < isize; i += 4) {
        data[i] = 0;
        data[i + 1] = 1111111;
        data[i + 2] = 2;
        data[i + 3] = 1111111;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int same_cells_pad() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {26, 27};
    int32_t chunkshape[8] = {26, 22};
    int32_t blockshape[8] = {13, 11};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int i = 0; i < (isize / 4); i++) {
        data[i * 4] = (uint32_t *) 11111111;
        data[i * 4 + 1] = (uint32_t *) 99999999;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int same_cells_pad_tam1() {
    int ndim = 2;
    int typesize = 1;
    int32_t shape[8] = {30, 24};
    int32_t chunkshape[8] = {26, 22};
    int32_t blockshape[8] = {13, 11};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint8_t *data = malloc(nbytes);
    for (int i = 0; i < (isize / 4); i++) {
        data[i * 4] = (uint32_t *) 111;
        data[i * 4 + 1] = (uint32_t *) 99;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int matches_2_rows() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {13, 13};
    int32_t chunkshape[8] = {13, 13};
    int32_t blockshape[8] = {13, 13};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int i = 0; i < isize; i += 4) {
        if ((i <= 20) || ((i >= 48) && (i <= 68)) || ((i >= 96) && (i <= 116))) {
            data[i] = 0;
            data[i + 1] = 1;
            data[i + 2] = 2;
            data[i + 3] = 3;
        } else if (((i >= 24) && (i <= 44)) || ((i >= 72) && (i <= 92)) || ((i >= 120) && (i <= 140))){
            data[i] = i;
            data[i + 1] = i + 1;
            data[i + 2] = i + 2;
            data[i + 3] = i + 3;
        } else {
            data[i] = i;
        }
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int matches_3_rows() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {32, 32};
    int32_t chunkshape[8] = {32, 32};
    int32_t blockshape[8] = {16, 16};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int i = 0; i < isize; i += 4) {
        if ((i % 12 == 0) && (i != 0)) {
            data[i] = 1111111;
            data[i + 1] = 3;
            data[i + 2] = 11111;
            data[i + 3] = 4;
        } else {
            data[i] = 0;
            data[i + 1] = 1111111;
            data[i + 2] = 2;
            data[i + 3] = 1111;
        }
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int matches_2_couples() {
    int ndim = 2;
    int typesize = 1;
    int32_t shape[8] = {12, 12};
    int32_t chunkshape[8] = {12, 12};
    int32_t blockshape[8] = {12, 12};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint8_t *data = malloc(nbytes);
    for (int i = 0; i < isize / 4; i++) {
        if (i % 4 == 0) {
            data[i * 4] = 0;
            data[i * 4 + 1] = 1;
            data[i * 4 + 2] = 2;
            data[i * 4 + 3] = 3;
        } else if (i % 4 == 1){
            data[i * 4] = 10;
            data[i * 4 + 1] = 11;
            data[i * 4 + 2] = 12;
            data[i * 4 + 3] = 13;
        } else if (i % 4 == 2){
            data[i * 4] = 20;
            data[i * 4 + 1] = 21;
            data[i * 4 + 2] = 22;
            data[i * 4 + 3] = 23;
        } else {
            data[i * 4] = 30;
            data[i * 4 + 1] = 31;
            data[i * 4 + 2] = 32;
            data[i * 4 + 3] = 33;
        }
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int some_matches() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {256, 256};
    int32_t chunkshape[8] = {128, 128};
    int32_t blockshape[8] = {64, 64};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int i = 0; i < (isize / 2); i++) {
        data[i] = i;
    }
    for (int i = (isize / 2); i < isize; i++) {
        data[i] = 1;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int padding_some() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {215, 233};
    int32_t chunkshape[8] = {128, 128};
    int32_t blockshape[8] = {64, 64};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int i = 0; i < 2 * isize / 3; i++) {
        data[i] = 0;
    }
    for (int i = 2 * isize / 3; i < isize; i++) {
        data[i] = i;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int pad_some_32() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {37, 29};
    int32_t chunkshape[8] = {18, 24};
    int32_t blockshape[8] = {12, 12};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int i = 0; i < 2 * isize / 3; i++) {
        data[i] = 0;
    }
    for (int i = 2 * isize / 3; i < isize; i++) {
        data[i] = i;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image1() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {300, 450};
    int32_t chunkshape[8] = {150, 150};
    int32_t blockshape[8] = {50, 50};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/examples/res.bin", "rb");
    int err = fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image2() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {800, 1200};
    int32_t chunkshape[8] = {400, 400};
    int32_t blockshape[8] = {40, 40};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/examples/res2.bin", "rb");
    int err = fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image3() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {256, 256};
    int32_t chunkshape[8] = {64, 128};
    int32_t blockshape[8] = {32, 32};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/examples/res3.bin", "rb");
    int err = fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image4() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {64, 64};
    int32_t chunkshape[8] = {32, 32};
    int32_t blockshape[8] = {16, 16};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/examples/res4.bin", "rb");
    int err = fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image5() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {641, 1140};
    int32_t chunkshape[8] = {256, 512};
    int32_t blockshape[8] = {256, 256};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/examples/res5.bin", "rb");
    int err = fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image6() {
    int ndim = 2;
    int typesize = 3;
    int32_t shape[8] = {256, 256};
    int32_t chunkshape[8] = {128, 128};
    int32_t blockshape[8] = {64, 64};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/examples/res6.bin", "rb");
    int err = fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image7() {
    int ndim = 2;
    int typesize = 3;
    int32_t shape[8] = {2506, 5000};
    int32_t chunkshape[8] = {512, 1024};
    int32_t blockshape[8] = {128, 512};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/examples/res7.bin", "rb");
    int err = fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image8() {
    int ndim = 2;
    int typesize = 3;
    int32_t shape[8] = {1575, 2400};
    int32_t chunkshape[8] = {1575, 2400};
    int32_t blockshape[8] = {256, 256};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/examples/res8.bin", "rb");
    int err = fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image9() {
    int ndim = 2;
    int typesize = 3;
    int32_t shape[8] = {675, 1200};
    int32_t chunkshape[8] = {675, 1200};
    int32_t blockshape[8] = {256, 256};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/examples/res9.bin", "rb");
    int err = fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image10() {
    int ndim = 2;
    int typesize = 3;
    int32_t shape[8] = {2045, 3000};
    int32_t chunkshape[8] = {2045, 3000};
    int32_t blockshape[8] = {256, 256};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint8_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/examples/res10.bin", "rb");
    int err = fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndlz(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}


int main(void) {

    int result;
/*
    result = no_matches();
    printf("no_matches: %d obtained \n \n", result);
    result = no_matches_pad();
    printf("no_matches_pad: %d obtained \n \n", result);
    result = all_elem_eq();
    printf("all_elem_eq: %d obtained \n \n", result);
    result = all_elem_pad();
    printf("all_elem_pad: %d obtained \n \n", result);
    result = same_cells();
    printf("same_cells: %d obtained \n \n", result);
    result = same_cells_pad();
    printf("same_cells_pad: %d obtained \n \n", result);
    result = same_cells_pad_tam1();
    printf("same_cells_pad_tam1: %d obtained \n \n", result);
    result = matches_2_rows();
    printf("matches_2_rows: %d obtained \n \n", result);
    result = matches_3_rows();
    printf("matches_3_rows: %d obtained \n \n", result);
    result = matches_2_couples();
    printf("matches_2_couples: %d obtained \n \n", result);
    result = some_matches();
    printf("some_matches: %d obtained \n \n", result);
    result = padding_some();
    printf("pad_some: %d obtained \n \n", result);
    result = pad_some_32();
    printf("pad_some_32: %d obtained \n \n", result);
*/
    printf("TEST NDLZ-ZLIB \n");
 /*   result = image1();
    printf("image1 with padding: %d obtained \n \n", result);
    result = image2();
    printf("image2 with  padding: %d obtained \n \n", result);
    result = image3();
    printf("image3 with NO padding: %d obtained \n \n", result);
    result = image4();
    printf("image4 with NO padding: %d obtained \n \n", result);
  */  result = image5();
    printf("image5 with padding: %d obtained \n \n", result);
 /*   result = image6();
    printf("image6 with NO padding: %d obtained \n \n", result);
 */   result = image7();
    printf("image7 with NO padding: %d obtained \n \n", result);
   /* result = image8();
    printf("image8 with NO padding: %d obtained \n \n", result);
   */ result = image9();
    printf("image9 with NO padding: %d obtained \n \n", result);
    result = image10();
    printf("image10 with NO padding: %d obtained \n \n", result);

}