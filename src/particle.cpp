#include "Particle.h"

Particle CreateParticle(float x, float y)
{
    Particle particle = {};

    particle.position = glm::vec2(x, y);
    particle.velocity = glm::vec2(0.f, 0.f);
    particle.force = glm::vec2(0.f, 0.f);
    particle.density = 0;
    particle.pressure = 0.f;

    return particle;
}