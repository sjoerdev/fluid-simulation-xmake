#ifndef PARTICLE_H
#define PARTICLE_H

#include <glm/glm.hpp>

struct Particle
{
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec2 force;
    float density;
    float pressure;
};

Particle CreateParticle(float x, float y);

#endif // PARTICLE_H