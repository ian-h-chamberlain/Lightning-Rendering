#include "utils.h"
#include "material.h"
#include "argparser.h"
#include "sphere.h"
#include "vertex.h"
#include "mesh.h"
#include "ray.h"
#include "hit.h"

// ====================================================================
// ====================================================================

bool Sphere::intersect(const Ray &r, Hit &h) const {

  // use the quadratic formula to solve intersection of ray and sphere
  float b = 2 * glm::dot(r.getDirection(), (r.getOrigin() - center));
  float c = std::pow(glm::distance(r.getOrigin(), center), 2) - (radius * radius);

  float d;
  if (b * b - 4 * c >= 0.0f) {
    // we have at least 1 real root, so calculate hit info
    d = std::sqrt(b * b - 4 * c);
  }
  else {
    // no real root, no intersection
    return false;
  }

  float t_minus = (-b - d) / 2.0f;
  float t_plus = (-b + d) / 2.0f;

  // ignore all results where the origin is inside the sphere
  // or the ray points away from the sphere
  if (t_minus < -EPSILON || t_plus < -EPSILON) {
    return false;
  }

  float t = std::min(t_plus, t_minus);

  glm::vec3 hit = r.getOrigin() + r.getDirection() * t;

  // calculate the normal
  glm::vec3 normal = glm::normalize(hit - center);

  // give the hit object info about the intersection 
  h.set(t, material, normal);

  return true;
} 

// ====================================================================
// ====================================================================

// helper function to place a grid of points on the sphere
glm::vec3 ComputeSpherePoint(float s, float t, const glm::vec3 center, float radius) {
  float angle = 2*M_PI*s;
  float y = -cos(M_PI*t);
  float factor = sqrt(1-y*y);
  float x = factor*cos(angle);
  float z = factor*-sin(angle);
  glm::vec3 answer = glm::vec3(x,y,z);
  answer *= radius;
  answer += center;
  return answer;
}

void Sphere::addRasterizedFaces(Mesh *m, ArgParser *args) {
  
  // and convert it into quad patches for radiosity
  int h = args->sphere_horiz;
  int v = args->sphere_vert;
  assert (h % 2 == 0);
  int i,j;
  int va,vb,vc,vd;
  Vertex *a,*b,*c,*d;
  int offset = m->numVertices(); //vertices.size();

  // place vertices
  m->addVertex(center+radius*glm::vec3(0,-1,0));  // bottom
  for (j = 1; j < v; j++) {  // middle
    for (i = 0; i < h; i++) {
      float s = i / float(h);
      float t = j / float(v);
      m->addVertex(ComputeSpherePoint(s,t,center,radius));
    }
  }
  m->addVertex(center+radius*glm::vec3(0,1,0));  // top

  // the middle patches
  for (j = 1; j < v-1; j++) {
    for (i = 0; i < h; i++) {
      va = 1 +  i      + h*(j-1);
      vb = 1 + (i+1)%h + h*(j-1);
      vc = 1 +  i      + h*(j);
      vd = 1 + (i+1)%h + h*(j);
      a = m->getVertex(offset + va);
      b = m->getVertex(offset + vb);
      c = m->getVertex(offset + vc);
      d = m->getVertex(offset + vd);
      m->addRasterizedPrimitiveFace(a,b,d,c,material);
    }
  }

  for (i = 0; i < h; i+=2) {
    // the bottom patches
    va = 0;
    vb = 1 +  i;
    vc = 1 + (i+1)%h;
    vd = 1 + (i+2)%h;
    a = m->getVertex(offset + va);
    b = m->getVertex(offset + vb);
    c = m->getVertex(offset + vc);
    d = m->getVertex(offset + vd);
    m->addRasterizedPrimitiveFace(d,c,b,a,material);
    // the top patches
    va = 1 + h*(v-1);
    vb = 1 +  i      + h*(v-2);
    vc = 1 + (i+1)%h + h*(v-2);
    vd = 1 + (i+2)%h + h*(v-2);
    a = m->getVertex(offset + va);
    b = m->getVertex(offset + vb);
    c = m->getVertex(offset + vc);
    d = m->getVertex(offset + vd);
    m->addRasterizedPrimitiveFace(b,c,d,a,material);
  }
}

glm::vec3 Sphere::closestPoint(glm::vec3 point) {
    glm::vec3 dir = glm::normalize(point - center);
    return center + (dir * radius);
}
