/*
 * very rough tool that converts a .obj model into hardcoded C arrays
 *
 * ```sh
 * gcc obj2h.c -o obj2h
 * obj2h model.obj > model.h
 * ```
 *
 * arrays will be named according to the obj filename:
 * model_vertices, model_normals, model_uvs, model_faces
 *
 * any unspecified index is -1
 *
 * the vector_t type is assumed to exist (already defined if using kos)
 *
 * # license
 * this is free and unencumbered software released into the
 * public domain.
 *
 * refer to the attached UNLICENSE or http://unlicense.org/
 */

#define WHOAMI "obj2h"
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>

static
void* push_back(size_t tsize, void** pp, size_t* n, size_t* cap)
{
    if (*n >= *cap)
    {
        if (!*cap) {
            *cap = 512;
        } else {
            *cap *= 2;
        }

        *pp = realloc(*pp, *cap * tsize);
        if (!*pp) {
            perror("realloc");
            return 0;
        }
    }

    return (char*)*pp + tsize * (*n)++;
}

/*
 * skip until the first space or tab and move right after it
 * 123123meme asd -> 123123meme asd
 * ^                            ^
 */
static
char const* skip_after_space(char const* s)
{
    s += strcspn(s, " \t");
    s += strspn(s, " \t");
    return s;
}

/* --------------------------------------------------------------------- */

struct face { int vertex_indices[3], uv_indices[3], normal_indices[3]; };
typedef struct face face_t;

struct vec3 { float x, y, z; };
typedef struct vec3 vec3_t;

struct uv { float u, v; };
typedef struct uv uv_t;

static size_t nvertices = 0;
static size_t vertices_cap = 0;
static vec3_t* vertices = 0;

static
int parse_vertex(char const* line)
{
    vec3_t* v;

    v = push_back(sizeof(vec3_t), (void**)&vertices, &nvertices,
        &vertices_cap);
    if (!v) return 1;

    line = skip_after_space(line);
    v->x = atof(line);
    line = skip_after_space(line);
    v->y = -atof(line);
    line = skip_after_space(line);
    v->z = atof(line);

    return 0;
}

static size_t nnormals = 0;
static size_t normals_cap = 0;
static vec3_t* normals = 0;

static
int parse_normal(char const* line)
{
    vec3_t* n;

    n = push_back(sizeof(vec3_t), (void**)&normals, &nnormals,
        &normals_cap);
    if (!n) return 1;

    line = skip_after_space(line);
    n->x = atof(line);
    line = skip_after_space(line);
    n->y = atof(line);
    line = skip_after_space(line);
    n->z = atof(line);

    return 0;
}

static size_t nuvs = 0;
static size_t uvs_cap = 0;
static uv_t* uvs = 0;

static
int parse_uv(char const* line)
{
    uv_t* uv;

    uv = push_back(sizeof(vec3_t), (void**)&uvs, &nuvs, &uvs_cap);
    if (!uv) return 1;

    line = skip_after_space(line);
    uv->u = atof(line);
    line = skip_after_space(line);
    uv->v = atof(line);

    return 0;
}

static size_t faces_cap = 0;
static size_t nfaces = 0;
static face_t* faces = 0;

/* f vertex_indices/texcoord_indices/normal_indices */
static
int parse_face(char const* line)
{
    face_t* f;
    size_t i;

    f = push_back(sizeof(face_t), (void**)&faces, &nfaces, &faces_cap);
    if (!f) return 1;

    for (i = 0; i < 3; ++i)
    {
        line = skip_after_space(line);
        f->vertex_indices[i] = atoi(line) - 1;

        line += strcspn(line, "/");

        if (*line == '/' && isdigit(line[1])) {
            f->uv_indices[i] = atoi(line + 1) - 1;
        } else {
            f->uv_indices[i] = -1;
        }

        ++line;
        line += strcspn(line, "/");

        if (*line == '/' && isdigit(line[1])) {
            f->normal_indices[i] = atoi(line + 1) - 1;
        } else {
            f->normal_indices[i] = -1;
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    FILE* f;
    char* line = 0;
    size_t n = 0;
    char* array_name;

    if (argc != 2)
    {
        fprintf(stderr, VERSION_STR "\n");
        fprintf(stderr, "converts a .obj model into c arrays\n\n");
        fprintf(stderr, "usage: %s model.obj > model.h\n", argv[0]);
        return 1;
    }

    f = fopen(argv[1], "r");
    if (!f) {
        perror("fopen");
        return 1;
    }

    while (getline(&line, &n, f) != -1)
    {
        if (isalpha(line[1]))
        {
            if (!strncmp(line, "vn", 2) && parse_normal(line)) return 1;
            if (!strncmp(line, "vt", 2) && parse_uv(line)    ) return 1;
            continue;
        }

        switch (*line)
        {
        case 'v': if (parse_vertex(line)) return 1; break;
        case 'f': if (parse_face(line)  ) return 1; break;
        }
    }

    if (!feof(f)) {
        perror("getline");
        return 1;
    }

    /* use filename without extension as the base array name */
    array_name = basename(argv[1]);

    for (n = 0; n < strlen(array_name); ++n)
    {
        char* p = &array_name[n];

        switch (*p)
        {
        case '.':
            *p = 0;
            goto print_output;

        case ' ':
        case '\t':
        case '-':
            *p = '_';
            break;
        }
    }

print_output:
    fprintf(stderr, "%zd vertices\n%zd normals\n%zd uvs\n%zd faces\n",
        nvertices, nnormals, nuvs, nfaces);

    puts("/* this file was generated by " VERSION_STR " */\n");
    puts("#ifndef OBJ2H_TYPES");
    puts("#define OBJ2H_TYPES");
    printf("struct face { int vertex_indices[3], uv_indices[3], "
        "normal_indices[3]; };\n");
    puts("typedef struct face face_t;");
    puts("struct uv { float u, v; };");
    puts("typedef struct uv uv_t;");
    puts("#endif /* !OBJ2H_TYPES */\n");

    printf("static vector_t %s_vertices[]\n"
        "__attribute__((aligned(32))) = {\n", array_name);

    for (n = 0; n < nvertices; ++n)
    {
        vec3_t* v = &vertices[n];
        printf("    { %.17g, %.17g, %.17g, 1 },\n", v->x, v->y, v->z);
    }

    puts("};\n");

    printf("static vector_t %s_normals[]\n"
        "__attribute__((aligned(32))) = {\n", array_name);

    for (n = 0; n < nnormals; ++n)
    {
        vec3_t* v = &normals[n];
        printf("    { %.17g, %.17g, %.17g, 0 },\n", v->x, v->y, v->z);
    }

    puts("};\n");

    printf("static uv_t %s_uvs[]\n"
        "__attribute__((aligned(32))) = {\n", array_name);

    for (n = 0; n < nuvs; ++n)
    {
        uv_t* uv = &uvs[n];
        printf("    { %.17g, %.17g  },\n", uv->u, uv->v);
    }

    puts("};\n");

    printf("static face_t %s_faces[]\n"
        "__attribute__((aligned(32))) = {\n", array_name);

    for (n = 0; n < nfaces; ++n)
    {
        face_t* f = &faces[n];
        int* pi;

        printf("    { ");
        pi = f->vertex_indices;
        printf("{ %d, %d, %d }, ", pi[0], pi[1], pi[2]);
        pi = f->uv_indices;
        printf("{ %d, %d, %d }, ", pi[0], pi[1], pi[2]);
        pi = f->normal_indices;
        printf("{ %d, %d, %d } ", pi[0], pi[1], pi[2]);
        printf("},\n");
    }

    puts("};\n");

    return 0;
}
