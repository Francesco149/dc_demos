/*
 * this dreamcast demo showcases
 * - basic 3D camera
 * - shading implemented only using vertex color interpolation
 * - point lights
 * - fast vertex transformations using the matrix asm macros
 *
 * ![](https://i.imgur.com/vXgC0gR.png)
 *
 * # controls
 * - d-pad (arrow keys in the emulator) to rotate the camera
 * - a (z in the emulator) to go forward
 * - b (x in the emulator) to go backwards
 * - x (a in the emulator) to go up
 * - y (s in the emulator) to go down
 *
 * # license
 * this is free and unencumbered software released into the
 * public domain.
 *
 * refer to the attached UNLICENSE or http://unlicense.org/
 */

#define WHOAMI "point_lights"
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
#define VERSION_STR \
    WHOAMI " " \
    STRINGIFY(VERSION_MAJOR) "." \
    STRINGIFY(VERSION_MINOR) "." \
    STRINGIFY(VERSION_PATCH)

#include <kos.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);

/* assets -------------------------------------------------------------- */

/* I wrote a little tool that converts obj models to headers, see obj2h */
#include "monkey.h"

/* --------------------------------------------------------------------- */

#define ARRAY_LENGTH(a) (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, a, b) MAX(a, MIN(x, b))

static
void angle_sub(float* angle, float amt)
{
    *angle -= amt;
    while (*angle < 0) *angle += F_PI*2;
}

static
void angle_add(float* angle, float amt)
{
    *angle += amt;
    while (*angle > F_PI*2) *angle -= F_PI*2;
}

/* --------------------------------------------------------------------- */

static pvr_poly_hdr_t poly;

static
void init()
{
    pvr_poly_cxt_t cxt;

    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    pvr_poly_compile(&poly, &cxt);
}

static float ambient = .01f;

struct light
{
    vector_t pos;
    float intensity;
    float radius;
    uint32 rgb;
};

typedef struct light light_t;

static
light_t lights[] = {
    { { -3  , -3 , -3 }, 1  , 10, 0xFF2030 },
    { {  0.5, -0.8, 1 }, 0.5,  1, 0xFF9030 },
    { { 0 }, -1, -1, 0 },
};

static
uint32 light(vector_t const* v, vector_t const* n)
{
    light_t* l;
    vector_t ld; /* light direction */
    float amt;
    int r, g, b;
    int tr, tg, tb;
    float d;

    tr = (uint8)(ambient * 255);
    tg = (uint8)(ambient * 255);
    tb = (uint8)(ambient * 255);

    for (l = lights; l->intensity >= 0; ++l)
    {
        vec3f_distance(l->pos.x, l->pos.y, l->pos.z, v->x, v->y, v->z, d);
        if (d > l->radius) {
            continue;
        }

        vec3f_sub_normalize(l->pos.x, l->pos.y, l->pos.z,
            v->x, v->y, v->z, ld.x, ld.y, ld.z);
        vec3f_dot(ld.x, ld.y, ld.z, n->x, n->y, n->z, amt);

        amt *= l->intensity;
        amt *= 1.0f / (1.0f + 0.1f * d + 0.01f * d * d); /* attenuation */

        amt = CLAMP(amt, 0, 1);
        r = (l->rgb & 0xFF0000) >> 16;
        g = (l->rgb & 0x00FF00) >> 8;
        b = (l->rgb & 0x0000FF);

        tr += (int)(r * amt);
        tg += (int)(g * amt);
        tb += (int)(b * amt);
    }

    tr = MIN(tr, 255);
    tg = MIN(tg, 255);
    tb = MIN(tb, 255);

    return
        0xFF000000  |
        ((tr << 16) & 0x00FF0000) |
        ((tg <<  8) & 0x0000FF00) |
        ((tb <<  0) & 0x000000FF);
}

static vector_t camera_pos = { -0.5, -0.8f, 1.5f, 1 };
static vector_t camera_rot = { F_PI/8, F_PI*2-F_PI/6, 0, 1 };

#define SENS (F_PI/60.0f)
#define MOVSPEED (4.0f/60.0f)

static
void update()
{
    maple_device_t* joy;
    cont_state_t* st;

    joy = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    if (!joy) return;

    st = (cont_state_t*)maple_dev_status(joy);
    if (!st) return;

    if (st->buttons & CONT_DPAD_UP   ) angle_sub(&camera_rot.x, SENS);
    if (st->buttons & CONT_DPAD_DOWN ) angle_add(&camera_rot.x, SENS);
    if (st->buttons & CONT_DPAD_RIGHT) angle_sub(&camera_rot.y, SENS);
    if (st->buttons & CONT_DPAD_LEFT ) angle_add(&camera_rot.y, SENS);

    if (st->buttons & (CONT_A|CONT_B|CONT_X|CONT_Y))
    {
        vector_t direction = { 0, 0, 0, 0 };

        if (st->buttons & CONT_A) {
            direction.z -= MOVSPEED;
        }

        if (st->buttons & CONT_B) {
            direction.z += MOVSPEED;
        }

        if (st->buttons & CONT_X) {
            direction.y -= MOVSPEED;
        }

        if (st->buttons & CONT_Y) {
            direction.y += MOVSPEED;
        }

        mat_identity();
        mat_rotate(0, F_PI*2 - camera_rot.y, 0);
        mat_trans_nodiv(direction.x, direction.y, direction.z,
            direction.w);

        camera_pos.x += direction.x;
        camera_pos.y += direction.y;
        camera_pos.z += direction.z;
    }
}

/* buffer for vertex transformations */
static
vector_t tbuf[ARRAY_LENGTH(monkey_vertices)]
__attribute__((aligned(32)));

static
void draw()
{
    size_t i, j;
    pvr_vertex_t pv; /* pvr struct for the vertex */
    face_t* f; /* current face */
    vector_t* v; /* current vertex */
    vector_t* t; /* transformed vertex */
    vector_t* n; /* current normal */

    mat_identity();
    mat_perspective(320, 240, 1, 1, 100);
    mat_rotate(camera_rot.x, camera_rot.y, camera_rot.z);
    mat_translate(-camera_pos.x, -camera_pos.y, -camera_pos.z);
    mat_transform(monkey_vertices, tbuf, ARRAY_LENGTH(tbuf),
        sizeof(vector_t));

    /* TODO: transform into the store queues */

    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&poly, sizeof(poly));

    pv.oargb = 0;
    pv.u = pv.v = 0;

    /* TODO: convert everything to triangle strips */

    for (i = 0; i < ARRAY_LENGTH(monkey_faces); ++i)
    {
        f = &monkey_faces[i];

        for (j = 0; j < 3; ++j)
        {
            t = &tbuf[f->vertex_indices[2-j]];
            v = &monkey_vertices[f->vertex_indices[2-j]];
            n = &monkey_normals[f->normal_indices[2-j]];

            pv.argb = light(v, n);
            pv.x = t->x;
            pv.y = t->y;
            pv.z = t->z;
            pv.flags = j == 2 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            pvr_prim(&pv, sizeof(pv));
        }
    }

    pvr_list_finish();
    pvr_scene_finish();
}

int main()
{
    puts(VERSION_STR);

    pvr_init_defaults();
    init();

    while (1)
    {
        update();
        draw();
    }

    return 0;
}
