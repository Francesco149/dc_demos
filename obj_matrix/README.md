
basic 3D camera example with imported models and shading

![](https://media.giphy.com/media/24FIhzuq4cZsI9K7Ei/giphy.gif)

this version uses the built in matrix transformation functions
which are insanely fast compared to doing the perspective divide
manually as the CPU has specific accelerated instructions for this

further optimization could be achieved by converting models to
triangle strips and using mat_transform_sq to transform vertices
directly into the store queues like the serpent demo does

# controls
- d-pad (arrow keys in the emulator) to rotate the camera
- a (z in the emulator) to go forward
- b (x in the emulator) to go backwards
- x, y (a, s in the emulator) to move the light on the x axis
- start, start + b to move the light on the z axis

