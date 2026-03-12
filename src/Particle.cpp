//
// sueda - geometry edits Z. Wood
// 3/16
//

#include <iostream>
#include "Particle.h"
#include "GLSL.h"
#include "MatrixStack.h"
#include "Program.h"
#include "Texture.h"


float randFloat(float l, float h)
{
	float r = rand() / (float) RAND_MAX;
	return (1.0f - r) * l + r * h;
}

Particle::Particle(vec3 start) :
	charge(1.0f),
	m(1.0f),
	d(0.0f),
	x(start),
	v(0.0f, 0.0f, 0.0f),
	lifespan(1.0f),
	tEnd(0.0f),
	scale(1.0f),
	color(1.0f, 1.0f, 1.0f, 1.0f)
{
}

Particle::~Particle()
{
}

void Particle::load(vec3 start)
{
	// Random initialization
	rebirth(0.0f, start);
}

/* all particles born at the origin */
void Particle::rebirth(float t, vec3 start)
{
	charge = randFloat(0.0f, 1.0f) < 0.5 ? -1.0f : 1.0f;	
	m = 1.0f;
  	d = randFloat(0.0f, 0.02f);
	x = start;
	v.x = randFloat(-0.8f, 0.8f);   
	v.y = randFloat(0.5f, 2.5f);    
	v.z = randFloat(-0.8f, 0.8f); 
	lifespan = randFloat(80.0f, 100.0f); 
	tEnd = t + lifespan;
	scale = randFloat(0.5f, 1.5f);
   	color.r = randFloat(0.6f, 1.0f);
   	color.g = randFloat(0.5f, 0.9f);
   	color.b = randFloat(0.9f, 1.0f);
	color.a = 1.0f;
}

void Particle::update(float t, float h, const vec3 &g, const vec3 start)
{
	if(t > tEnd) {
		rebirth(t, start);
	}

	//To do - how do you want to update the forces?

	// Gravity
    vec3 gravity = vec3(0.0f, -0.4f, 0.0f);

    // Soft lift
    vec3 lift = vec3(0.0f, 0.25f, 0.0f);

    // Swirl around Y axis
    vec3 swirl;
    swirl.x = -x.z;
    swirl.z =  x.x;
    swirl.y =  0.0f;

    vec3 accel = (gravity + lift + swirl * 0.6f) * 1.5f;

    v += accel * h;

    // Drag
    v *= 0.985f;

    x += v * h;

    float lifeRatio = (tEnd - t) / lifespan;
    color.a = lifeRatio * lifeRatio;
}
