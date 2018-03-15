
very rough tool that converts a .obj model into hardcoded C arrays

```sh
gcc obj2h.c -o obj2h
obj2h model.obj > model.h
```

arrays will be named according to the obj filename:
model_vertices, model_normals, model_uvs, model_faces

any unspecified index is -1

the vector_t type is assumed to exist (already defined if using kos)

# license
this is free and unencumbered software released into the
public domain.

refer to the attached UNLICENSE or http://unlicense.org/

