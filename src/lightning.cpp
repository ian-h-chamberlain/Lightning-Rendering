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
 
  // Center branch goes from starting position to ground plane 
  glm::vec3 dir(0,-1,0);
  float dist = glm::distance(start_pos, glm::vec3(0,0,0));
  float branch_probability = 0.2;
  float mean_branch_length = 2.0;
  addBranch(start_pos, dir, dist, branch_probability, mean_branch_length);

}


void Mesh::addBranch(glm::vec3 start_pos, glm::vec3 dir, float dist,
                     float branch_probability, float mean_branch_length) {

  // Branch properties
  float start_radius = 0.04;
  float mean_seg_length = 0.08;
  float max_seg_angle_degrees = 40.0;
  float max_branch_angle_degrees = 20.0;
  glm::vec3 rotation_normal(0,0,1);

  // Variables to track branch construction
  float angle, seglength, radius, branch_angle, branch_dist;
  glm::vec3 last, next, branch;

  // Initialize
  next = start_pos;
  last = start_pos;
  radius = start_radius;

  // Create segments
  while (glm::distance(next, start_pos) < dist) {
    // Random segment angle
    angle = (0.5 - rand_float()) * 2.0 * max_seg_angle_degrees;         
    angle = angle * (M_PI / 180.0);
    // Random segment length
    seglength = rand_float() * 2.0 * mean_seg_length;
    // Get new point
    next = glm::rotate(dir, angle, rotation_normal);
    next = next * seglength;
    next = next + last;
    // Create segment and add to mesh
    lightning_segments.push_back(LightningSegment(radius, last, next));
    // Recursively add branches
    if (rand_float() < branch_probability && branch_probability > 0.01) {
      branch_angle = (0.5 - rand_float())  * max_branch_angle_degrees;
      branch_angle = branch_angle * (M_PI / 180.0);
      branch_dist = rand_float() * 2.0 * mean_branch_length;
      branch = glm::rotate(next-last, branch_angle, rotation_normal);
      branch = glm::normalize(branch);
      addBranch(next, branch, branch_dist, branch_probability*0.2, 
                mean_branch_length*0.5);
    }
    // Update for next iteration
    last = next;
  }

}
