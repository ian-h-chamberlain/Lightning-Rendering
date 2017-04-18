#include "mesh.h"
#include <random>
#include <glm/gtx/rotate_vector.hpp>


float rand_float() {
  // Return a random float between 0 and 1
  std::random_device rd;
  std::mt19937 e2(rd());
  std::uniform_real_distribution<> dist(0, 1);
  return dist(e2);
}


void Mesh::addLightning(glm::vec3 start_pos) {
  
  // Lightning properties 
  float mean_seg_length = 0.08;
  float start_radius = 0.04;
  float max_angle_degrees = 50.0;
  glm::vec3 dir(0,-1,0);
  glm::vec3 rotation_normal(0,0,1);

  // Variables to track lightning construction
  float height, angle, seglength, radius;
  glm::vec3 last, next;

  // Initialize 
  height = start_pos.y;
  radius = start_radius;
  last = start_pos;

  //lightning_segments.push_back(LightningSegment(radius, last, last + dir));

  // Create segments
  while (height > 0) {
    // Random segment angle 
    angle = (0.5 - rand_float()) * 2.0 * max_angle_degrees;
    angle = angle * (M_PI / 180.0);
    // Random segment length
    seglength = rand_float() * 2.0 * mean_seg_length;
    // Get new point
    next = glm::rotate(dir, angle, rotation_normal);
    next = next * seglength;
    next = next + last;
    // Create segment and add to mesh
    lightning_segments.push_back(LightningSegment(radius, last, next));
    // Update for next iteration
    height -= (last.y - next.y); 
    last = next;
    radius -= 0.0004;
  }

}
