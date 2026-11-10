/*
 * donut_host — SDL2 engine for the esolang vertex-stream donut (protocol v2).
 *
 * The language program computes an animated torus (50x30 vertices) and writes
 * a token stream to stdout (whitespace-separated, so newline-terminated
 * numeric prints slot straight in):
 *
 *   F                      begin frame
 *   px py z intensity      one VERTEX: four plain numbers per vertex,
 *                          1500 vertices per frame, ordered i*30+j
 *   S                      swap/present
 *
 * The host is a faithful port of the reference renderer (doughnut.c): it
 * generates the torus mesh and the 2-triangles-per-quad indices itself,
 * and rasterizes each frame with the reference's exact draw_triangle logic
 * (bounding box, barycentrics, perspective-correct z and Gouraud intensity,
 * wireframe rule w0/w1/w2 < 0.04 -> 0xFF00FFFF under the 'w' toggle, palette
 * r=I*0.9 g=I*0.4 b=I*0.9 clamped), into an 800x600 framebuffer cleared to
 * 0xFF101010 with a 1e9 zbuffer, presented via a streaming texture.
 *
 * After the producer's stream ends, the host replays the recorded vertex
 * frames forever, so finite (recorded) demos still animate. Headless:
 * --frames N exits after N presented frames (SDL_VIDEODRIVER=dummy works).
 *
 * Build: cc -std=c99 $(sdl2-config --cflags --libs) -o donut_host donut_host.c
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WIDTH 800
#define HEIGHT 600

#define R_MAJOR 2.0f
#define R_MINOR 0.8f
#define U_STEPS 50
#define V_STEPS 30

#define PI_F 3.14159265358979323846f /* M_PI truncated to float, as in doughnut.c */

typedef struct {
    float x, y, z;
} Vector3;

typedef struct {
    int i0, i1, i2;
} Triangle;

/* Global buffers for mesh data */
static Vector3 base_vertices[U_STEPS * V_STEPS];
static Vector3 base_normals[U_STEPS * V_STEPS];
static Triangle triangles[U_STEPS * V_STEPS * 2];
static int total_triangles = 0;

static float zbuffer[WIDTH * HEIGHT];
static Uint32 framebuffer[WIDTH * HEIGHT];
/* ------------------------------------------------------------------ */
/* Canonical per-frame logging — byte-identical format shared by       */
/* doughnut_logged.c and every donut_host. JSON Lines, one object per  */
/* presented frame:                                                    */
/*  {"frame":N,"vertices":N,"stream_checksum":"<16hex>",               */
/*   "vertex_minmax":{"px":[min,max],"py":[..],"z":[..],"i":[..]},     */
/*   "first_vertex":[px,py,z,i],"last_vertex":[px,py,z,i],             */
/*   "z_writes":N,"wireframe_pixels":N,"fb_checksum":"<16hex>",         */
/*   "wall_ms":f}                                                      */
/* Rules: checksums are 64-bit FNV-1a (offset 14695981039346656037,    */
/* prime 1099511628211) printed as 16 lowercase hex chars;              */
/* stream_checksum hashes the raw text of the frame's 6000 vertex      */
/* values in stream order, no separators (here: the token bytes        */
/* exactly as read from stdin); fb_checksum hashes the 800x600x4      */
/* framebuffer bytes in memory; floats use round-trip-shortest %.9g;  */
/* wall_ms is %.3f and covers the render only (clear..raster, no      */
/* present).                                                          */
/* ------------------------------------------------------------------ */
static FILE *log_file = NULL;
static unsigned long long log_z_writes = 0;
static unsigned long long log_wireframe_pixels = 0;

static unsigned long long log_fnv_init(void) { return 14695981039346656037ULL; }
static unsigned long long log_fnv_text(unsigned long long h, const char *s) {
    while (*s) { h ^= (unsigned char)(*s++); h *= 1099511628211ULL; }
    return h;
}
static unsigned long long log_fnv_fb(unsigned long long h) {
    const unsigned char *b = (const unsigned char *)framebuffer;
    size_t n = sizeof(framebuffer);
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}
static double log_wall_ms(Uint64 t0, Uint64 t1) {
    Uint64 freq = SDL_GetPerformanceFrequency();
    return freq ? (double)(t1 - t0) * 1000.0 / (double)freq : 0.0;
}
/* Emit one canonical frame record (see the format comment above).    */
static void log_frame(int frame, int nverts, const float *pxv, const float *pyv,
                      const float *zv, const float *iv,
                      unsigned long long stream_checksum, double wall_ms) {
    if (!log_file || nverts <= 0) return;
    float mn[4] = {pxv[0], pyv[0], zv[0], iv[0]};
    float mx[4] = {pxv[0], pyv[0], zv[0], iv[0]};
    for (int k = 1; k < nverts; k++) {
        float v[4] = {pxv[k], pyv[k], zv[k], iv[k]};
        for (int c = 0; c < 4; c++) {
            if (v[c] < mn[c]) mn[c] = v[c];
            if (v[c] > mx[c]) mx[c] = v[c];
        }
    }
    int last = nverts - 1;
    fprintf(log_file,
        "{\"frame\":%d,\"vertices\":%d,\"stream_checksum\":\"%016llx\","
        "\"vertex_minmax\":{\"px\":[%.9g,%.9g],\"py\":[%.9g,%.9g],"
        "\"z\":[%.9g,%.9g],\"i\":[%.9g,%.9g]},"
        "\"first_vertex\":[%.9g,%.9g,%.9g,%.9g],"
        "\"last_vertex\":[%.9g,%.9g,%.9g,%.9g],"
        "\"z_writes\":%llu,\"wireframe_pixels\":%llu,"
        "\"fb_checksum\":\"%016llx\",\"wall_ms\":%.3f}\n",
        frame, nverts, stream_checksum,
        mn[0], mx[0], mn[1], mx[1], mn[2], mx[2], mn[3], mx[3],
        pxv[0], pyv[0], zv[0], iv[0],
        pxv[last], pyv[last], zv[last], iv[last],
        log_z_writes, log_wireframe_pixels, log_fnv_fb(log_fnv_init()), wall_ms);
}

/* Recorded vertex frames from the token stream */
typedef struct {
    double *verts; /* 4 doubles per vertex: px py z intensity */
    int count;
    unsigned long long stream_checksum; /* FNV-1a of the frame's raw token text */
} VertexFrame;

static VertexFrame *frames = NULL;
static int nframes = 0, frame_cap = 0;

static void generate_torus(void) {
    int idx = 0;
    for (int i = 0; i < U_STEPS; i++) {
        float u = (float)i / U_STEPS * 2.0f * PI_F;
        float cu = cosf(u), su = sinf(u);

        for (int j = 0; j < V_STEPS; j++) {
            float v = (float)j / V_STEPS * 2.0f * PI_F;
            float cv = cosf(v), sv = sinf(v);

            /* Vertex position on torus */
            base_vertices[idx] = (Vector3){
                (R_MAJOR + R_MINOR * cv) * cu,
                (R_MAJOR + R_MINOR * cv) * su,
                R_MINOR * sv
            };

            /* Surface normal */
            base_normals[idx] = (Vector3){
                cv * cu,
                cv * su,
                sv
            };
            idx++;
        }
    }

    /* Generate triangle indices (quads split into 2 triangles) */
    total_triangles = 0;
    for (int i = 0; i < U_STEPS; i++) {
        int next_i = (i + 1) % U_STEPS;
        for (int j = 0; j < V_STEPS; j++) {
            int next_j = (j + 1) % V_STEPS;

            int current = i * V_STEPS + j;
            int right = next_i * V_STEPS + j;
            int down = i * V_STEPS + next_j;
            int diag = next_i * V_STEPS + next_j;

            /* Triangle 1 */
            triangles[total_triangles++] = (Triangle){current, right, diag};
            /* Triangle 2 */
            triangles[total_triangles++] = (Triangle){current, diag, down};
        }
    }
}

static Vector3 rotate_vector(Vector3 v, float ax, float ay, float az) {
    /* Rotate X */
    float y1 = v.y * cosf(ax) - v.z * sinf(ax);
    float z1 = v.y * sinf(ax) + v.z * cosf(ax);
    float x1 = v.x;

    /* Rotate Y */
    float x2 = x1 * cosf(ay) + z1 * sinf(ay);
    float z2 = -x1 * sinf(ay) + z1 * cosf(ay);
    float y2 = y1;

    /* Rotate Z */
    float x3 = x2 * cosf(az) - y2 * sinf(az);
    float y3 = x2 * sinf(az) + y2 * cosf(az);
    float z3 = z2;

    return (Vector3){x3, y3, z3};
}

/* Rasterizer with Gouraud shading interpolation and wireframe overlay */
static void draw_triangle(Vector3 v0, Vector3 v1, Vector3 v2,
                          float int0, float int1, float int2,
                          int wireframe_on) {
    /* Bounding box */
    int min_x = (int)fmaxf(0, fminf(v0.x, fminf(v1.x, v2.x)));
    int max_x = (int)fminf(WIDTH - 1, fmaxf(v0.x, fmaxf(v1.x, v2.x)));
    int min_y = (int)fmaxf(0, fminf(v0.y, fminf(v1.y, v2.y)));
    int max_y = (int)fminf(HEIGHT - 1, fmaxf(v0.y, fmaxf(v1.y, v2.y)));

    float area = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
    if (fabsf(area) < 1e-5f) return;

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;

            /* Barycentric coordinates */
            float w0 = ((v1.y - v2.y) * (px - v2.x) + (v2.x - v1.x) * (py - v2.y)) / area;
            float w1 = ((v2.y - v0.y) * (px - v2.x) + (v0.x - v2.x) * (py - v2.y)) / area;
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                /* Perspective-correct depth interpolation */
                float inv_z = w0 * (1.0f / v0.z) + w1 * (1.0f / v1.z) + w2 * (1.0f / v2.z);
                float z = 1.0f / inv_z;

                int idx = y * WIDTH + x;
                if (z < zbuffer[idx]) {
                    zbuffer[idx] = z;
                    log_z_writes++;

                    /* Perspective-correct Gouraud intensity interpolation */
                    float intensity = (w0 * (int0 / v0.z) + w1 * (int1 / v1.z) + w2 * (int2 / v2.z)) / inv_z;

                    if (wireframe_on && (w0 < 0.04f || w1 < 0.04f || w2 < 0.04f)) {
                        /* Cyan wireframe grid lines */
                        framebuffer[idx] = 0xFF00FFFF;
                        log_wireframe_pixels++;
                    } else {
                        /* Smooth shaded purple-pink gradient */
                        Uint32 r = (Uint32)(fminf(1.0f, intensity * 0.9f) * 255);
                        Uint32 g = (Uint32)(fminf(1.0f, intensity * 0.4f) * 255);
                        Uint32 b = (Uint32)(fminf(1.0f, intensity * 0.9f) * 255);
                        framebuffer[idx] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                    }
                }
            }
        }
    }
}

/* Rasterize one recorded vertex frame exactly as doughnut.c renders it. */
static void render_frame(const VertexFrame *f) {
    /* Clear Z-buffer and Framebuffer */
    log_z_writes = 0;
    log_wireframe_pixels = 0;
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        zbuffer[i] = 1e9f;
        framebuffer[i] = 0xFF101010;
    }

    for (int t = 0; t < total_triangles; t++) {
        Triangle tri = triangles[t];
        Vector3 v0 = {
            (float)f->verts[tri.i0 * 4 + 0],
            (float)f->verts[tri.i0 * 4 + 1],
            (float)f->verts[tri.i0 * 4 + 2]
        };
        Vector3 v1 = {
            (float)f->verts[tri.i1 * 4 + 0],
            (float)f->verts[tri.i1 * 4 + 1],
            (float)f->verts[tri.i1 * 4 + 2]
        };
        Vector3 v2 = {
            (float)f->verts[tri.i2 * 4 + 0],
            (float)f->verts[tri.i2 * 4 + 1],
            (float)f->verts[tri.i2 * 4 + 2]
        };
        float int0 = (float)f->verts[tri.i0 * 4 + 3];
        float int1 = (float)f->verts[tri.i1 * 4 + 3];
        float int2 = (float)f->verts[tri.i2 * 4 + 3];
        draw_triangle(v0, v1, v2, int0, int1, int2, 0);
    }
}

static int close_frame(double *verts, int count, unsigned long long stream_checksum) {
    if (count == 0) { free(verts); return 0; }
    if (nframes == frame_cap) {
        frame_cap = frame_cap ? frame_cap * 2 : 8;
        frames = realloc(frames, (size_t)frame_cap * sizeof(VertexFrame));
        if (!frames) { fprintf(stderr, "donut_host: out of memory\n"); return 1; }
    }
    frames[nframes].verts = verts;
    frames[nframes].count = count / 4;
    frames[nframes].stream_checksum = stream_checksum;
    nframes++;
    return 0;
}

int main(int argc, char **argv) {
    int present_limit = -1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) present_limit = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--log") && i + 1 < argc) {
            log_file = fopen(argv[++i], "a");
            if (!log_file) { fprintf(stderr, "donut_host: cannot open log file\n"); return 1; }
        }
    }

    /* Token-stream protocol: F / <4 numbers per vertex> ... / S. */
    double *verts = NULL;
    int vcount = 0, vcap = 0;
    char tok[64];
    long total_vertices = 0;
    unsigned long long cur_checksum = log_fnv_init(); /* raw-text FNV-1a */
    while (scanf("%63s", tok) == 1) {
        if (tok[0] == 'F' && tok[1] == '\0') {
            vcount = 0;
            cur_checksum = log_fnv_init();
        } else if (tok[0] == 'S' && tok[1] == '\0') {
            if (vcount % 4 != 0) { fprintf(stderr, "donut_host: incomplete vertex at frame close\n"); return 2; }
            if (close_frame(verts, vcount, cur_checksum)) return 1;
            total_vertices += vcount / 4;
            verts = NULL;
            vcount = 0;
            vcap = 0;
        } else {
            char *end = NULL;
            double v = strtod(tok, &end);
            if (end == tok || *end != '\0') { fprintf(stderr, "donut_host: bad token '%s'\n", tok); return 2; }
            if (vcount == vcap) {
                vcap = vcap ? vcap * 2 : U_STEPS * V_STEPS * 4;
                verts = realloc(verts, (size_t)vcap * sizeof(double));
                if (!verts) { fprintf(stderr, "donut_host: out of memory\n"); return 1; }
            }
            cur_checksum = log_fnv_text(cur_checksum, tok);
            verts[vcount++] = v;
        }
    }
    free(verts);
    if (nframes == 0) { fprintf(stderr, "donut_host: no frames in input\n"); return 2; }
    fprintf(stderr, "donut_host: %d frames, %ld vertices\n", nframes, total_vertices);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "donut_host: %s\n", SDL_GetError()); return 1; }
    SDL_Window *window = SDL_CreateWindow("Polygonal Donut - Orbiting Light",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) { fprintf(stderr, "donut_host: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) { fprintf(stderr, "donut_host: %s\n", SDL_GetError()); return 1; }
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    if (!texture) { fprintf(stderr, "donut_host: %s\n", SDL_GetError()); return 1; }

    generate_torus();

    int presented = 0;
    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            }
        }

        VertexFrame *rf = &frames[presented % nframes];
        Uint64 wt0 = SDL_GetPerformanceCounter();
        render_frame(rf);
        double wms = log_wall_ms(wt0, SDL_GetPerformanceCounter());
        {
            /* canonical log record: values as the rasterizer received them (float) */
            static float pxv[U_STEPS * V_STEPS], pyv[U_STEPS * V_STEPS],
                        zv[U_STEPS * V_STEPS], iv[U_STEPS * V_STEPS];
            for (int k = 0; k < rf->count; k++) {
                pxv[k] = (float)rf->verts[k * 4 + 0];
                pyv[k] = (float)rf->verts[k * 4 + 1];
                zv[k] = (float)rf->verts[k * 4 + 2];
                iv[k] = (float)rf->verts[k * 4 + 3];
            }
            log_frame(presented, rf->count, pxv, pyv, zv, iv, rf->stream_checksum, wms);
        }

        SDL_UpdateTexture(texture, NULL, framebuffer, WIDTH * sizeof(Uint32));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        presented++;
        if (present_limit >= 0 && presented >= present_limit) running = 0;
        if (present_limit < 0) SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
