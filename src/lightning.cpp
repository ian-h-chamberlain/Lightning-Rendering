#include "mesh.h"
#include "utils.h"
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
  float dist = glm::distance(start_pos, glm::vec3(0,1.5,0));
  float branch_probability = 0.2;
  float mean_branch_length = 1.2;
  float start_radius = 0.05;
  addBranch(start_pos, dir, dist, start_radius, branch_probability, 
            mean_branch_length);

}


void Mesh::addBranch(glm::vec3 start_pos, glm::vec3 dir, float dist,
                     float start_radius, float branch_probability, 
                     float mean_branch_length) {

  // Branch properties
  //float start_radius = 0.05;
  float mean_seg_length = 0.08;
  float max_seg_angle_degrees = 40.0;
  float max_branch_angle_degrees = 20.0;
  glm::vec3 rotation_normal(0,0,1);

  // Variables to track branch construction
  float angle, seglength, radius, branch_angle, branch_dist, radius_delta;
  glm::vec3 last, next, branch;

  // Initialize
  next = start_pos;
  last = start_pos;
  radius = start_radius;
  radius_delta = start_radius / (dist / mean_seg_length);

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
      addBranch(next, branch, branch_dist, radius, branch_probability*0.2, 
                mean_branch_length*0.5);
    }
    // Update for next iteration
    last = next;
    radius -= radius_delta;
    if (radius < 0.02) radius = 0.02;
  }

}

void Mesh::initializeLightningVBOs() {
  glGenBuffers(1,&lightning_tri_verts_VBO);
  glGenBuffers(1,&lightning_tri_indices_VBO);
}


void Mesh::setupLightningVBOs() {
  glm::vec4 lightning_color(1.0, 0.0, 0.0, 1.0);
  for (LightningSegment segment : lightning_segments) {
    for (std::vector<glm::vec3> triangle : segment.getTriangles()) {
      glm::vec3 a = triangle[0];
      glm::vec3 b = triangle[2];
      glm::vec3 c = triangle[1];
      glm::vec3 n = ComputeTriNormal(a,b,c);
      int start = lightning_tri_verts.size();
      lightning_tri_verts.push_back(VBOPosNormalColor(a,n,lightning_color));
      lightning_tri_verts.push_back(VBOPosNormalColor(b,n,lightning_color));
      lightning_tri_verts.push_back(VBOPosNormalColor(c,n,lightning_color));
      lightning_tri_indices.push_back(VBOIndexedTri(start,start+1,start+2));
    }
  }
  glBindBuffer(GL_ARRAY_BUFFER,lightning_tri_verts_VBO);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(VBOPosNormalColor) * lightning_tri_verts.size(),
               &lightning_tri_verts[0],
               GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,lightning_tri_indices_VBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               sizeof(VBOIndexedTri) * lightning_tri_indices.size(),
               &lightning_tri_indices[0],
               GL_STATIC_DRAW);
}


void Mesh::drawLightningVBOs() {
  HandleGLError("enter draw lightning");
  glBindBuffer(GL_ARRAY_BUFFER,lightning_tri_verts_VBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,lightning_tri_indices_VBO);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor),(void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor),(void*)sizeof(glm::vec3) );
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor), (void*)(sizeof(glm::vec3)*2));
  glDrawElements(GL_TRIANGLES, lightning_tri_indices.size()*3,GL_UNSIGNED_INT, 0);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  HandleGLError("leaving draw lightning");
}


void Mesh::cleanupLightningVBOs() {
  glDeleteBuffers(1,&lightning_tri_verts_VBO);
  glDeleteBuffers(1,&lightning_tri_indices_VBO);
}
