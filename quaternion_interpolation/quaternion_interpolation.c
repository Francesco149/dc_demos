/*
 * this dreamcast demo showcases
 * - basic 3D camera
 * - shading implemented only using vertex color interpolation
 * - fast vertex transformations using the matrix asm macros
 * - quaternion interpolation (can be used for skeletal animations)
 *
 * ![](https://media.giphy.com/media/NsKiniCfQ2IOD0kBnb/giphy.gif)
 *
 * # controls
 * - d-pad (arrow keys in the emulator) to rotate the camera
 * - a (z in the emulator) to go forward
 * - b (x in the emulator) to go backwards
 * - x (a in the emulator) to go up
 * - y (s in the emulator) to go down
 * - start (enter in the emulator) to cycle between rotations
 *
 * # license
 * this is free and unencumbered software released into the
 * public domain.
 *
 * refer to the attached UNLICENSE or http://unlicense.org/
 */

#define WHOAMI "quaternion_interpolation"
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
#include <kmg/kmg.h>
#include <math.h>

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

struct quaternion { float x, y, z, w; };
typedef struct quaternion quaternion_t;

static
void quat_axis_angle(quaternion_t* q, vector_t const* v, float angle)
{
    float half_angle = 0.5f * angle;
    float sin_ = fsin(half_angle);
    q->x = v->x * sin_;
    q->y = v->y * sin_;
    q->z = v->z * sin_;
    q->w = fcos(half_angle);
}

static
void quat_matrix(quaternion_t const* q, matrix_t* m)
{
    float wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

    x2 = q->x + q->x;
    y2 = q->y + q->y;
    z2 = q->z + q->z;

    xx = q->x * x2, xy = q->x * y2, xz = q->x * z2;
    yy = q->y * y2, yz = q->y * z2, zz = q->z * z2;
    wx = q->w * x2, wy = q->w * y2, wz = q->w * z2;

    (*m)[0][0] = 1.0f - (yy + zz);
    (*m)[0][1] = xy - wz;
    (*m)[0][2] = xz + wy;
    (*m)[0][3] = 0;

    (*m)[1][0] = xy + wz;
    (*m)[1][1] = 1.0f - (xx + zz);
    (*m)[1][2] = yz - wx;
    (*m)[1][3] = 0;

    (*m)[2][0] = xz - wy;
    (*m)[2][1] = yz + wx;
    (*m)[2][2] = 1.0f - (xx + yy);
    (*m)[2][3] = 0;

    (*m)[3][0] = 0;
    (*m)[3][1] = 0;
    (*m)[3][2] = 0;
    (*m)[3][3] = 1;
}

#define SLERP_EPSILON (1.0f/32)

void quat_slerp(quaternion_t const* a, quaternion_t const* b, float t,
    quaternion_t* c)
{
    float cos_, sin_;
    float angle;
    float scale0, scale1;

    *c = *b;
    cos_ = a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w;

    if (cos_ < 0)
    {
        c->x *= -1;
        c->y *= -1;
        c->z *= -1;
        c->w *= -1;
    }

    if (1.0f - cos_ > SLERP_EPSILON)
    {
        angle = acosf(cos_);
        sin_ = fsin(angle);
        scale0 = fsin((1 - t) * angle) / sin_;
        scale1 = fsin(t * angle) / sin_;
    }

    else
    {
        scale0 = 1 - t;
        scale1 = t;
    }

    c->x = a->x * scale0 + c->x * scale1;
    c->y = a->y * scale0 + c->y * scale1;
    c->z = a->z * scale0 + c->z * scale1;
    c->w = a->w * scale0 + c->w * scale1;
}

/* --------------------------------------------------------------------- */

static pvr_poly_hdr_t poly;
static quaternion_t rotations[3];

static
void init()
{
    vector_t v;
    pvr_poly_cxt_t cxt;

    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    pvr_poly_compile(&poly, &cxt);

    v.x = 0;
    v.y = 1;
    v.z = 0;
    v.w = 0;
    quat_axis_angle(&rotations[0], &v, 0);

    v.x = 1;
    quat_axis_angle(&rotations[1], &v, F_PI / 4);

    v.x = -1;
    v.z = -1;
    vec3f_normalize(v.x, v.y, v.z);
    quat_axis_angle(&rotations[2], &v, F_PI);
}

static float ambient = .01f;
static vector_t light_direction = { .5f, 1, 0, 0 };

static vector_t camera_pos = { -.5, -.8f, 1.5f, 1 };
static vector_t camera_rot = { F_PI/8, F_PI*2-F_PI/6, 0, 1 };

#define SENS (F_PI/60.0f)
#define MOVSPEED (4.0f/60.0f)

static int rotation_index = ARRAY_LENGTH(rotations) - 1;
static float lerp_amt = 1;

static
void input();

static
void update()
{
    input();

    lerp_amt = MIN(1, lerp_amt + 1.0f / 60);
}

static uint32 old_buttons = 0;

static
void input()
{
    maple_device_t* joy;
    cont_state_t* st;

    joy = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    if (!joy) return;

    st = (cont_state_t*)maple_dev_status(joy);
    if (!st) return;

    if ((st->buttons & CONT_START) && !(old_buttons & CONT_START))
    {
        ++rotation_index;
        rotation_index %= ARRAY_LENGTH(rotations);
        lerp_amt = 0;
    }

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

    old_buttons = st->buttons;
}

/* buffer for vertex transformations */
static
vector_t tbuf[ARRAY_LENGTH(monkey_vertices)]
__attribute__((aligned(32)));

static
vector_t nbuf[ARRAY_LENGTH(monkey_normals)]
__attribute__((aligned(32)));

static
void draw()
{
    size_t i, j;
    pvr_vertex_t pv; /* pvr struct for the vertex */
    face_t* f; /* current face */
    vector_t* t; /* transformed vertex */
    vector_t* n; /* current normal */
    vector_t* ld = &light_direction;

    int q0, q1; /* quaternion indices into the rotations array */
    quaternion_t q; /* interpolated quaternion */
    matrix_t qm; /* interpolated quaternion converted to matrix */

    q0 = rotation_index;
    q1 = rotation_index + 1;
    q1 %= ARRAY_LENGTH(rotations);
    quat_slerp(&rotations[q0], &rotations[q1], lerp_amt, &q);
    quat_matrix(&q, &qm);

    mat_identity();
    mat_perspective(320, 240, 1, 1, 100);
    mat_rotate(camera_rot.x, camera_rot.y, camera_rot.z);
    mat_translate(-camera_pos.x, -camera_pos.y, -camera_pos.z);
    mat_apply(&qm);
    mat_transform(monkey_vertices, tbuf, ARRAY_LENGTH(tbuf),
        sizeof(vector_t));

    /* transform normals according to the model's local transform */
    mat_load(&qm);
    mat_transform(monkey_normals, nbuf, ARRAY_LENGTH(nbuf),
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
            float dot;

            t = &tbuf[f->vertex_indices[2-j]];
            n = &nbuf[f->normal_indices[2-j]];

            vec3f_dot(ld->x, ld->y, ld->z, n->x, n->y, n->z, dot);
            dot = CLAMP(dot, 0, 1);
            dot = ambient + dot * (1 - ambient);

            pv.argb = 0xFF000000 | 0x010101 * (uint32)(dot * 255 + 0.5f);
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
