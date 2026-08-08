/* d1d_trace.c - decode a raw AV1 stream with instrumented dav1d, dumping
 * the msac trace to stderr. Usage: d1d_trace <file.av1> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dav1d/dav1d.h>

static void buf_free(const uint8_t *buf, void *cookie) {
    (void)buf; (void)cookie;
    free(cookie);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <file.av1>\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *buf = (unsigned char *)malloc((size_t)sz);
    if (!buf) return 1;
    fread(buf, 1, (size_t)sz, f);
    fclose(f);

    Dav1dSettings st;
    dav1d_default_settings(&st);
    st.n_threads = 1;
    st.max_frame_delay = 1;
    st.apply_grain = 0;

    Dav1dContext *c;
    if (dav1d_open(&c, &st) < 0) { fprintf(stderr, "open failed\n"); return 1; }

    Dav1dData data;
    if (dav1d_data_wrap(&data, buf, (size_t)sz, buf_free, buf) < 0) return 1;
    fprintf(stderr, "[D1D] send_data sz=%ld\n", sz);
    if (dav1d_send_data(c, &data) < 0) { fprintf(stderr, "send failed\n"); return 1; }

    Dav1dPicture pic;
    int r = dav1d_get_picture(c, &pic);
    fprintf(stderr, "[D1D] get_picture ret=%d\n", r);
    if (r == 0) {
        fprintf(stderr, "[D1D] pic %dx%d layout=%d bpc=%d\n",
                pic.p.w, pic.p.h, pic.p.layout, pic.p.bpc);
        {
            FILE *fy = fopen("/tmp/d1d_y.raw", "wb");
            FILE *fu = fopen("/tmp/d1d_u.raw", "wb");
            FILE *fv = fopen("/tmp/d1d_v.raw", "wb");
            int y, x;
            for (y = 0; y < pic.p.h; y++)
                fwrite(pic.data[0] + y * pic.stride[0], 1, pic.p.w, fy);
            for (y = 0; y < (pic.p.h + 1) / 2; y++)
                fwrite(pic.data[1] + y * pic.stride[1], 1, (pic.p.w + 1) / 2, fu);
            for (y = 0; y < (pic.p.h + 1) / 2; y++)
                fwrite(pic.data[2] + y * pic.stride[1], 1, (pic.p.w + 1) / 2, fv);
            fclose(fy); fclose(fu); fclose(fv);
        }
        dav1d_picture_unref(&pic);
    }

    /* drain any remaining pictures */
    while (r == 0) {
        r = dav1d_get_picture(c, &pic);
        if (r == 0) { fprintf(stderr, "[D1D] extra pic\n"); dav1d_picture_unref(&pic); }
    }

    dav1d_close(&c);
    free(buf);
    return 0;
}
