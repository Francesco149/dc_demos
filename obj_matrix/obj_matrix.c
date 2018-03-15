/*
 * basic 3D camera example with imported models and shading
 *
 * ![](https://media.giphy.com/media/24FIhzuq4cZsI9K7Ei/giphy.gif)
 *
 * this version uses the built in matrix transformation functions
 * which are insanely fast compared to doing the perspective divide
 * manually as the CPU has specific accelerated instructions for this
 *
 * further optimization could be achieved by converting models to
 * triangle strips and using mat_transform_sq to transform vertices
 * directly into the store queues like the serpent demo does
 *
 * # controls
 * - d-pad (arrow keys in the emulator) to rotate the camera
 * - a (z in the emulator) to go forward
 * - b (x in the emulator) to go backwards
 * - x, y (a, s in the emulator) to move the light on the x axis
 * - start, start + b to move the light on the z axis
 */

#include <kos.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

extern uint8_t romdisk[];
KOS_INIT_ROMDISK(romdisk);

#define ARRAY_LENGTH(a) (sizeof(a)/sizeof((a)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, a, b) MAX(a, MIN(x, b))

/* --------------------------------------------------------------------- */

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

struct face { int vertex_indices[3], normal_indices[3]; };
typedef struct face face_t;

/* I wrote a little parser that converts obj models to .h files */
#include "monkey.h"

static pvr_poly_hdr_t poly;

static
void init()
{
    pvr_poly_cxt_t cxt;

    pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
    pvr_poly_compile(&poly, &cxt);
}

static vector_t light_pos = { 200, -600, -200, 1 };
static float ambient = .1f;
static vector_t camera_pos = { 1.5f, -2, 2.5f, 1 };
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

    if (st->buttons & CONT_X) light_pos.x += MOVSPEED*100;
    if (st->buttons & CONT_Y) light_pos.x -= MOVSPEED*100;

    if (st->buttons & CONT_START)
    {
        if (st->buttons & CONT_B) {
            light_pos.z -= MOVSPEED*100;
        } else {
            light_pos.z += MOVSPEED*100;
        }

        return;
    }

    if (st->buttons & CONT_DPAD_UP   ) angle_sub(&camera_rot.x, SENS);
    if (st->buttons & CONT_DPAD_DOWN ) angle_add(&camera_rot.x, SENS);
    if (st->buttons & CONT_DPAD_RIGHT) angle_sub(&camera_rot.y, SENS);
    if (st->buttons & CONT_DPAD_LEFT ) angle_add(&camera_rot.y, SENS);

    if (st->buttons & (CONT_A|CONT_B))
    {
        vector_t direction = {0, 0, MOVSPEED, 0};

        if (st->buttons & CONT_A) {
            direction.z *= -1;
        }

        /* movement is buggy at certain y axis rotations. no idea why */
        mat_identity();
        mat_rotate(F_PI*2 - camera_rot.x, F_PI*2 - camera_rot.y,
            F_PI*2 - camera_rot.z);
        mat_trans_nodiv(direction.x, direction.y, direction.z,
            direction.w);

        camera_pos.x += direction.x;
        camera_pos.y += direction.y;
        camera_pos.z += direction.z;
    }
}

static vector_t t[ARRAY_LENGTH(monkey_vertices)];

/*
 * some quirks with the built in matrix functions
 * - mat_perspective seems to look towards negative z by default
 * - mat_perspective also handles transformation to screen coordinates.
 *   just set center_x and center_y to half of the resolution
 * - rotations are the opposite direction as what you would expect from
 *   stuff like glRotate (counter-clockwise?)
 * - matrix transformations must be applied in reverse order (idk what
 *   the technical term for it is)
 */

static
void draw()
{
    size_t i, j;
    pvr_vertex_t pv; /* pvr struct for the vertex */
    face_t* f; /* current face */
    vector_t* v; /* current vertex */
    vector_t* n; /* current normal */

    vector_t ld; /* light direction */
    vector_t* lp = &light_pos;
    float light;

    mat_identity();
    mat_perspective(320, 240, 1, 1, 100);
    mat_rotate(camera_rot.x, camera_rot.y, camera_rot.z);
    mat_translate(-camera_pos.x, -camera_pos.y, -camera_pos.z);
    mat_transform(monkey_vertices, t, ARRAY_LENGTH(t), sizeof(vector_t));

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
            v = &t[f->vertex_indices[2-j]];
            n = &monkey_normals[f->normal_indices[2-j]];

            vec3f_sub_normalize(lp->x, lp->y, lp->z, v->x, v->y, v->z,
                ld.x, ld.y, ld.z);
            vec3f_dot(ld.x, ld.y, ld.z, n->x, n->y, n->z, light);
            light = CLAMP(light, 0, 1);
            light = ambient + light * (1 - ambient);
            pv.argb = 0xFF000000 | 0x00010101 * (uint32)(light * 255);

            pv.x = v->x;
            pv.y = v->y;
            pv.z = v->z;
            pv.flags = j == 2 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            pvr_prim(&pv, sizeof(pv));
        }
    }

    pvr_list_finish();
    pvr_scene_finish();
}

int main()
{
    pvr_init_defaults();
    init();

    while (1)
    {
        update();
        draw();
    }

    return 0;
}
