#ifndef LIGHTNING_H
#define LIGHTNING_H

#include <glm/glm.hpp>
#include <vector>

class LightningSegment {
  public:
    LightningSegment(float radius, glm::vec3 start, glm::vec3 end);
    std::vector<std::vector<glm::vec3> > getTriangles() {
        return triangles; } 
    glm::vec3 getStart() { return _start; }
    glm::vec3 getEnd() { return _end; }
    float getRadius() { return _radius; }
  private:
    float _radius;
    glm::vec3 _start;
    glm::vec3 _end;
    std::vector<std::vector<glm::vec3> > triangles;
};

#endif
