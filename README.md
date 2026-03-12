# Program 4 - Swan Lake Scene

**Name:** Kyrene Angelica Tigas

## 1. Program Description

For this project, I created a theatrical ballet scene inspired by Swan Lake. There's a ballerina spinning gracefully at center stage with two swans positioned on either side facing inward toward her. I added eight flowers arranged in a semicircle around the stage that sway and spin, plus a little bunny sitting off to the side watching the performance like a guest in the audience. The whole scene sits on an ice-blue textured stage under a dark starry night sky positioned high above. You can move around to view the performance from different angles.

## 2. Controls

A / D - Rotate view left/right  
Q / E - Move the light source left/right  
M - Changes the ballerina's dress from light pink to dark velvet  
Z - Switches between solid and wireframe mode  
G - Starts/stops an automated camera tour that follows a path around the scene  
W / S - Move camera forward/backward (only when not in tour mode)

## 3. Features I Implemented

- **Blinn-Phong Lighting** with ambient, diffuse, and specular components to make everything look more realistic  
- **Custom Materials** with colors inspired by the white swan/black swan contrast from the ballet  
- **Manual Normal Calculation** for the bunny mesh since the original file didn't include normals (did this in main.cpp)  
- **Texture Mapping** on the ground using a repeating ice pattern and on the stage using a painted backdrop texture  
- **Skybox** with a starry night texture that stays fixed above the scene instead of following the camera  
- **Hierarchical Animation** for the flowers - the stems sway back and forth while the flower heads spin around  
- **Scene Layout** with everything placed intentionally - ballerina centered, swans symmetrically flanking her, flowers in a semicircle, bunny off to the side  
- **Bezier Camera Path** that flies smoothly around the whole scene so you can see everything without controlling it yourself  

## 4. Files I Modified or Added

- simple_vert.glsl  
- simple_frag.glsl  
- tex_vert.glsl  
- tex_frag0.glsl  
- main.cpp