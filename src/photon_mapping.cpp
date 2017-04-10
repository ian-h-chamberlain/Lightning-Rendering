#include "glCanvas.h"

#include <iostream>
#include <algorithm>
#include <set>

#include "argparser.h"
#include "photon_mapping.h"
#include "mesh.h"
#include "face.h"
#include "primitive.h"
#include "kdtree.h"
#include "utils.h"
#include "raytracer.h"


// ==========
// DESTRUCTOR
PhotonMapping::~PhotonMapping() {
  // cleanup all the photons
  delete kdtree;
}


// ========================================================================
// Recursively trace a single photon

void PhotonMapping::TracePhoton(const glm::vec3 &position, const glm::vec3 &direction, 
				const glm::vec3 &energy, int iter) {

  // find the first hit location
  Ray r(position, direction);
  Hit h;
  if(!raytracer->CastRay(r, h, false)) {
    // no hit, we're done
    return;
  }

  glm::vec3 hitLoc = r.pointAtParameter(h.getT());

  // get the material of the spot where the photon hit
  Material* mat = h.getMaterial();
  glm::vec3 dif = mat->getDiffuseColor(h.get_s(), h.get_t());
  glm::vec3 spec = mat->getReflectiveColor();
  glm::vec3 new_energy, new_dir;

  // store the photon here
  kdtree->AddPhoton(Photon(hitLoc, direction, energy, iter));

  /*
  // ====================
  // ALWAYS-REFLECT IMPLEMENTATION

  if (glm::length(spec) > 0.01) {
    // reflect specularly
    new_dir = MirrorDirection(h.getNormal(), direction);
    new_energy = energy * spec;
  }
  else {
    // diffuse reflection
    new_energy = energy * dif;
    new_dir = RandomDiffuseDirection(h.getNormal());
  }

  if (iter < args->num_bounces) {
    TracePhoton(hitLoc, new_dir, new_energy, iter+1);
  }
  */

  // =====================
  // RUSSIAN ROULETTE IMPLEMENTATION
  // calculate the probability of reflecting, and how to reflect
  float prob_reflection = std::max({dif.x + spec.x, dif.y + spec.y, dif.z + spec.z});

  float prob_dif = prob_reflection * (dif.x + dif.y + dif.z) /
                   (dif.x + dif.y + dif.z + spec.x + spec.y + spec.z);
  float prob_spec = prob_reflection * (spec.x + spec.y + spec.z) /
                    (dif.x + dif.y + dif.z + spec.x + spec.y + spec.z);

  // russian roulette based on diffuse/reflective
  float choice = args->rand();

  // if some kind of reflection happens
  if (choice <= prob_reflection) {

    if (choice <= prob_dif) {
      // diffusely reflect random direction
      new_dir = RandomDiffuseDirection(h.getNormal());
      new_energy = energy * mat->getDiffuseColor() / prob_dif;
    }
    else {
      // specularly reflect across the normal
      new_dir = MirrorDirection(h.getNormal(), direction);
      new_energy = energy * mat->getReflectiveColor() / prob_spec;
    }

    if (iter < args->num_bounces) {
      // actually trace the reflected photon
      TracePhoton(hitLoc, new_dir, new_energy, iter+1);
    }
  }
  // otherwise absorb, do nothing
}


// ========================================================================
// Trace the specified number of photons through the scene

void PhotonMapping::TracePhotons() {
  std::cout << "trace photons" << std::endl;

  // first, throw away any existing photons
  delete kdtree;

  // consruct a kdtree to store the photons
  BoundingBox *bb = mesh->getBoundingBox();
  glm::vec3 min = bb->getMin();
  glm::vec3 max = bb->getMax();
  glm::vec3 diff = max-min;
  min -= 0.001f*diff;
  max += 0.001f*diff;
  kdtree = new KDTree(BoundingBox(min,max));

  // photons emanate from the light sources
  const std::vector<Face*>& lights = mesh->getLights();

  // compute the total area of the lights
  float total_lights_area = 0;
  for (unsigned int i = 0; i < lights.size(); i++) {
    total_lights_area += lights[i]->getArea();
  }

  // shoot a constant number of photons per unit area of light source
  // (alternatively, this could be based on the total energy of each light)
  for (unsigned int i = 0; i < lights.size(); i++) {  
    float my_area = lights[i]->getArea();
    int num = args->num_photons_to_shoot * my_area / total_lights_area;
    // the initial energy for this photon
    glm::vec3 energy = my_area/float(num) * lights[i]->getMaterial()->getEmittedColor();
    glm::vec3 normal = lights[i]->computeNormal();
    for (int j = 0; j < num; j++) {
      glm::vec3 start = lights[i]->RandomPoint();
      // the initial direction for this photon (for diffuse light sources)
      glm::vec3 direction = RandomDiffuseDirection(normal);
      TracePhoton(start,direction,energy,0);
    }
  }
}


// ======================================================================

// helper function
bool closest_photon(const std::pair<Photon,float> &a, const std::pair<Photon,float> &b) {
  return (a.second < b.second);
}


// ======================================================================
glm::vec3 PhotonMapping::GatherIndirect(const glm::vec3 &point, const glm::vec3 &normal, const glm::vec3 &direction_from) const {

  if (kdtree == NULL) { 
    std::cout << "WARNING: Photons have not been traced throughout the scene." << std::endl;
    return glm::vec3(0,0,0); 
  }

  // first get initial selection
  std::vector<Photon> collected;
  // start with smallest possible radius, increasing as needed
  float radius = EPSILON;
  while (collected.size() < args->num_photons_to_collect) {
    glm::vec3 min(point.x - radius, point.y - radius, point.z - radius);
    glm::vec3 max(point.x + radius, point.y + radius, point.z + radius);

    BoundingBox bbox(min, max);
  
    collected.clear();
    // first get all photons in box
    kdtree->CollectPhotonsInBox(bbox, collected);

    // increase the radius in case we don't get enough
    radius *= 2.0f;
    // and skip the extra computation if we have too few photons
    if (collected.size() < args->num_photons_to_collect) {
      continue;
    }

    // comparator to sort by distance from point
    struct photonCmp {
      glm::vec3 pt;
      photonCmp(glm::vec3 _pt) { pt = _pt; }
      bool operator() (const Photon& a, const Photon& b) const {
        return glm::distance(a.getPosition(), pt) < glm::distance(b.getPosition(), pt);
      }
    };

    photonCmp cmp(point);

    // sort the photons collected thus far
    std::vector<Photon> sortedPhotons(collected);
    std::sort(sortedPhotons.begin(), sortedPhotons.end(), cmp);
    collected.clear();
    
    Hit h;
    for (int i=0; i<sortedPhotons.size(); i++) {
      // first remove if outside the radius
      if (glm::distance(sortedPhotons[i].getPosition(), point) < radius) { 
        // and check that it's not occluded from the ray being cast
        Ray r(sortedPhotons[i].getPosition(), -direction_from);
        if (!raytracer->CastRay(r, h, false)) {
          collected.push_back(sortedPhotons[i]);
          // once we have enough, finish up
          if (collected.size() >= args->num_photons_to_collect) {
            break;
          }
        }
      }
    }
  }

  // shrink radius to furthest out photon
  radius = glm::distance(collected.back().getPosition(), point);

  // now divide the photon colors out and return the result
  glm::vec3 result(0, 0, 0);
  for (int i=0; i<collected.size(); i++) {
    float weight = glm::dot(-collected[i].getDirectionFrom(), normal);
    result += collected[i].getEnergy() * weight;
  }

  // divide by area of sphere projected onto surface
  result /= M_PI * radius * radius; 

  // return the color
  return result;
}


// ======================================================================
// PHOTON VISUALIZATION FOR DEBUGGING
// ======================================================================

void PhotonMapping::initializeVBOs() {
  HandleGLError("enter photonmapping initializevbos()");
  glGenBuffers(1, &photon_direction_verts_VBO);
  glGenBuffers(1, &photon_direction_indices_VBO);
  glGenBuffers(1, &kdtree_verts_VBO);
  glGenBuffers(1, &kdtree_edge_indices_VBO);
  HandleGLError("leave photonmapping initializevbos()");
}

void PhotonMapping::setupVBOs() {
  HandleGLError("enter photonmapping setupvbos()");

  photon_direction_verts.clear();
  photon_direction_indices.clear();
  kdtree_verts.clear();
  kdtree_edge_indices.clear();

  // initialize the data
  BoundingBox *bb = mesh->getBoundingBox();
  float max_dim = bb->maxDim();

  if (kdtree == NULL) return;
  std::vector<const KDTree*> todo;  
  todo.push_back(kdtree);
  while (!todo.empty()) {
    const KDTree *node = todo.back();
    todo.pop_back(); 
    if (node->isLeaf()) {

      // initialize photon direction vbo
      const std::vector<Photon> &photons = node->getPhotons();
      int num_photons = photons.size();
      for (int i = 0; i < num_photons; i++) {
	const Photon &p = photons[i];
	glm::vec3 energy = p.getEnergy()*float(args->num_photons_to_shoot);
        glm::vec4 color(energy.x,energy.y,energy.z,1);
	const glm::vec3 &position = p.getPosition();
	glm::vec3 other = position - p.getDirectionFrom()*0.02f*max_dim;
        addEdgeGeometry(photon_direction_verts,photon_direction_indices,
                        position,other,color,color,max_dim*0.0005f,0);
      }

      // initialize kdtree vbo
      float thickness = 0.001*max_dim;
      glm::vec3 A = node->getMin();
      glm::vec3 B = node->getMax();
      glm::vec4 black(1,0,0,1);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(A.x,A.y,A.z),glm::vec3(A.x,A.y,B.z),black,black,thickness,thickness);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(A.x,A.y,B.z),glm::vec3(A.x,B.y,B.z),black,black,thickness,thickness);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(A.x,B.y,B.z),glm::vec3(A.x,B.y,A.z),black,black,thickness,thickness);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(A.x,B.y,A.z),glm::vec3(A.x,A.y,A.z),black,black,thickness,thickness);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(B.x,A.y,A.z),glm::vec3(B.x,A.y,B.z),black,black,thickness,thickness);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(B.x,A.y,B.z),glm::vec3(B.x,B.y,B.z),black,black,thickness,thickness);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(B.x,B.y,B.z),glm::vec3(B.x,B.y,A.z),black,black,thickness,thickness);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(B.x,B.y,A.z),glm::vec3(B.x,A.y,A.z),black,black,thickness,thickness);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(A.x,A.y,A.z),glm::vec3(B.x,A.y,A.z),black,black,thickness,thickness);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(A.x,A.y,B.z),glm::vec3(B.x,A.y,B.z),black,black,thickness,thickness);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(A.x,B.y,B.z),glm::vec3(B.x,B.y,B.z),black,black,thickness,thickness);
      addEdgeGeometry(kdtree_verts,kdtree_edge_indices,glm::vec3(A.x,B.y,A.z),glm::vec3(B.x,B.y,A.z),black,black,thickness,thickness);

    } else {
      todo.push_back(node->getChild1());
      todo.push_back(node->getChild2());
    } 
  }



  // copy the data to each VBO
  if (photon_direction_verts.size() > 0) {
    glBindBuffer(GL_ARRAY_BUFFER,photon_direction_verts_VBO); 
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(VBOPosNormalColor) * photon_direction_verts.size(),
                 &photon_direction_verts[0],
                 GL_STATIC_DRAW); 
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,photon_direction_indices_VBO); 
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(VBOIndexedTri) * photon_direction_indices.size(),
                 &photon_direction_indices[0], GL_STATIC_DRAW);
  }
  if (kdtree_verts.size() > 0) {
    glBindBuffer(GL_ARRAY_BUFFER,kdtree_verts_VBO); 
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(VBOPosNormalColor) * kdtree_verts.size(),
                 &kdtree_verts[0],
                 GL_STATIC_DRAW); 
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,kdtree_edge_indices_VBO); 
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(VBOIndexedTri) * kdtree_edge_indices.size(),
                 &kdtree_edge_indices[0], GL_STATIC_DRAW);
  }

  HandleGLError("leave photonmapping setupvbos()");
}

void PhotonMapping::drawVBOs() {
  HandleGLError("enter photonmapping drawvbos()");

  glUniform1i(GLCanvas::colormodeID, 1);
  if (args->render_photons && photon_direction_verts.size() > 0) {
    glBindBuffer(GL_ARRAY_BUFFER,photon_direction_verts_VBO); 
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,photon_direction_indices_VBO); 
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor),(void*)sizeof(glm::vec3) );
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor), (void*)(sizeof(glm::vec3)*2));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor), (void*)(sizeof(glm::vec3)*2 + sizeof(glm::vec4)));
    glDrawElements(GL_TRIANGLES,
                   photon_direction_indices.size()*3,
                   GL_UNSIGNED_INT, 0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
  }

  if (args->render_kdtree && kdtree_edge_indices.size() > 0) {
    glBindBuffer(GL_ARRAY_BUFFER,kdtree_verts_VBO); 
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,kdtree_edge_indices_VBO); 
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor),(void*)sizeof(glm::vec3) );
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor), (void*)(sizeof(glm::vec3)*2));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor), (void*)(sizeof(glm::vec3)*2 + sizeof(glm::vec4)));
    glDrawElements(GL_TRIANGLES,
                   kdtree_edge_indices.size()*3,
                   GL_UNSIGNED_INT, 0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
  }

  HandleGLError("leave photonmapping drawvbos()");
}

void PhotonMapping::cleanupVBOs() {
  glDeleteBuffers(1, &photon_direction_verts_VBO);
  glDeleteBuffers(1, &photon_direction_indices_VBO);
  glDeleteBuffers(1, &kdtree_verts_VBO);
  glDeleteBuffers(1, &kdtree_edge_indices_VBO);
}

