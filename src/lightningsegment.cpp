#include "lightningsegment.h"

LightningSegment::LightningSegment(float radius, glm::vec3 start, glm::vec3 end) {

  _start = start;
  _end = end;
  _radius = radius;
 
  // To start, represent each segment with a quad 
  // ============================================

  glm::vec3 dir = glm::normalize(end-start);
  glm::vec3 tmp = glm::cross(dir, glm::vec3(1,0,0));
  if (glm::length(tmp) < 0.1)
    tmp = glm::cross(dir, glm::vec3(0,0,1));
  tmp = glm::normalize(tmp);
  glm::vec3 one = glm::cross(dir, tmp);
  glm::vec3 two = glm::cross(dir, one);

  // 1st triangle
  std::vector<glm::vec3> t_1;
  t_1.push_back(glm::vec3(start-one*radius+two*radius));
  t_1.push_back(glm::vec3(end-one*radius+two*radius));
  t_1.push_back(glm::vec3(end+one*radius+two*radius));

  // 2nd triangle
  std::vector<glm::vec3> t_2;
  t_2.push_back(glm::vec3(start-one*radius+two*radius));
  t_2.push_back(glm::vec3(end+one*radius+two*radius));
  t_2.push_back(glm::vec3(start+one*radius+two*radius));

  triangles.push_back(t_1);
  triangles.push_back(t_2);

}
