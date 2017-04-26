#include "glCanvas.h"

#include "raytracer.h"
#include "material.h"
#include "argparser.h"
#include "raytree.h"
#include "utils.h"
#include "mesh.h"
#include "face.h"
#include "primitive.h"
#include "photon_mapping.h"


// ===========================================================================
// casts a single ray through the scene geometry and finds the closest hit
bool RayTracer::CastRay(const Ray &ray, Hit &h, bool use_rasterized_patches) const {
  bool answer = false;

  // intersect each of the quads
  for (int i = 0; i < mesh->numOriginalQuads(); i++) {
    Face *f = mesh->getOriginalQuad(i);
    if (f->intersect(ray,h,args->intersect_backfacing)) answer = true;
  }

  // intersect each of the primitives (either the patches, or the original primitives)
  if (use_rasterized_patches) {
    for (int i = 0; i < mesh->numRasterizedPrimitiveFaces(); i++) {
      Face *f = mesh->getRasterizedPrimitiveFace(i);
      if (f->intersect(ray,h,args->intersect_backfacing)) answer = true;
    }
  } else {
    int num_primitives = mesh->numPrimitives();
    for (int i = 0; i < num_primitives; i++) {
      if (mesh->getPrimitive(i)->intersect(ray,h)) answer = true;
    }
  }
  return answer;
}

// ===========================================================================
// does the recursive (shadow rays & recursive rays) work
glm::vec3 RayTracer::TraceRay(Ray &ray, Hit &hit, int bounce_count) const {

  // First cast a ray and see if we hit anything.
  hit = Hit();
  bool intersect = CastRay(ray,hit,false);
    
  glm::vec3 answer, normal, point;
  Material *m;

  // if there is no intersection, simply return the background color
  if (intersect == false) {
    answer = glm::vec3(srgb_to_linear(mesh->background_color.r),
                     srgb_to_linear(mesh->background_color.g),
                     srgb_to_linear(mesh->background_color.b));

    // need to fake some other values for the sake of rendering lightning
    normal = glm::vec3(0.0f);
    point = ray.pointAtParameter(1.0f);

    // NOTE: need to delete m if intersect == false at end
    m = new Material("", glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f), 0.0f);
  }
  else {
    normal = hit.getNormal();
    point = ray.pointAtParameter(hit.getT());
    m = hit.getMaterial();
  }

  assert (m != NULL);

  // rays coming from the light source are set to white, don't bother to ray trace further.
  if (glm::length(m->getEmittedColor()) > 0.001) {
    return glm::vec3(1,1,1);
  } 
  
  // ----------------------------------------------
  //  start with the indirect light (ambient light)
  glm::vec3 diffuse_color = m->getDiffuseColor(hit.get_s(),hit.get_t());
  if (args->gather_indirect && intersect) {
    // photon mapping for more accurate indirect light
    answer = diffuse_color * (photon_mapping->GatherIndirect(point, normal, ray.getDirection()) + args->ambient_light);
  } else if (intersect) {
    // the usual ray tracing hack for indirect light
    answer = diffuse_color * args->ambient_light;
  }      

  // ---------------------------------
  // render the lightning segment by segment

  const int numSegments = mesh->lightning_segments.size();

  // some parameters of the lightning
  glm::vec3 lightColor(0.6f, 1.0f, 0.7f);
  float lightningWidth = 0.05f;
  float glowWidth = 0.15f;
  float sharpness = 6.0f;
  float maxChannelContribution = 1.0f;
  float maxGlowContribution = 0.08f;

  // find the plane which the lightning lies in. Assumes at least 2 segments exist
  glm::vec3 p0 = mesh->lightning_segments[0].getStart();
  glm::vec3 p1 = mesh->lightning_segments[0].getEnd();
  glm::vec3 p2 = mesh->lightning_segments[1].getStart();
  glm::vec3 p3 = mesh->lightning_segments[1].getEnd();
  glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p3 - p2));

  // and solve for the intersection of ray and plane
  float t = glm::dot(p0 - ray.getOrigin(), n) / glm::dot(ray.getDirection(), n);

  glm::vec3 plane_point = ray.getOrigin() + t * ray.getDirection();

  for (int i=0; i<numSegments; i++) {

    glm::vec3 startPoint = mesh->lightning_segments[i].getStart();
    glm::vec3 endPoint = mesh->lightning_segments[i].getEnd();
    lightningWidth = mesh->lightning_segments[i].getRadius();
    glowWidth = lightningWidth * 3.0;
    if (glowWidth < 0.08) glowWidth = 0.08;

    // -------------------------------------------------
    // change color based on distance from segment

    glm::vec3 segment_dir = glm::normalize(endPoint - startPoint);

    // get the closest point on the segment to the line
    float line_t = glm::dot(plane_point - startPoint, segment_dir);
    line_t = std::max(line_t, 0.0f);
    line_t = std::min(line_t, glm::length(endPoint - startPoint));

    // calculate the distance from plane intersection to that point
    float dist = glm::distance(plane_point, startPoint + line_t * segment_dir);

    // now add the contribution based on distance
    float contribution = std::exp(-std::pow(2.0 * dist / lightningWidth, sharpness));

    // clamp each color separately to the max contribution
    glm::vec3 result = maxChannelContribution * contribution * lightColor;

    answer += result;

    // now add the glow component
    contribution = std::exp(-std::pow(dist / glowWidth, 2.0f));
    result = maxGlowContribution * contribution * lightColor;

    answer += result;

    // ------------------------------------------------
    // add lighting contribution from each segment as a point light

    if (intersect) {

      glm::vec3 myLightColor;

      // get the midpoint of the segment to use as a light
      glm::vec3 lightPoint = 0.5f * (startPoint + endPoint);
      glm::vec3 dirToLightPoint = glm::normalize(lightPoint - point);
      float distToLightPoint = glm::length(lightPoint - point);
      
      // soft shadows by uniformly sampling along the segment
      if (args->num_shadow_samples >= 1) {
        glm::vec3 shadedColor(0.0f);

        for (int j=0; j<args->num_shadow_samples; j++) {

          // random sampling for soft shadows
          if (args->num_shadow_samples > 1) {
            float alpha = args->rand();
            lightPoint = alpha * startPoint + (1 - alpha) * endPoint;
          }

          distToLightPoint = glm::length(lightPoint - point);
          dirToLightPoint = glm::normalize(lightPoint - point);

          // cast a ray towards the sample light point
          Hit shadowHit;
          Ray shadowRay(point, dirToLightPoint);
          bool didHit = CastRay(shadowRay, shadowHit, false);

          float distToLightPoint = glm::length(lightPoint-point);

          if (didHit && shadowHit.getT() < distToLightPoint) {
            // we got a hit in the direction of shadowRay, keep in shadow
            RayTree::AddShadowSegment(shadowRay, 0.0f, shadowHit.getT());
          }
          else {
            // no hit, add light contribution

            myLightColor = lightColor / float(M_PI*distToLightPoint*distToLightPoint);
            
            // add the lighting contribution from this particular light at this point
            shadedColor += m->Shade(ray,hit,dirToLightPoint,myLightColor,args);
          }
        }

        answer += shadedColor / (float) args->num_shadow_samples;
      }
      else {
        // just do the normal lighting without shadows
        myLightColor = lightColor / float(M_PI*distToLightPoint*distToLightPoint);
        
        // add the lighting contribution from this segment
        answer += m->Shade(ray,hit,dirToLightPoint,myLightColor,args);
      }
    }
  }

  // ---------------------------------
  // end lightning rendering
    



  // ----------------------------------------------
  // add contribution from reflection, if the surface is shiny
  glm::vec3 reflectiveColor = m->getReflectiveColor();

  // ignore if the reflective color contribution is too low
  // or we've reached the recursive limit
  if (intersect && glm::length(reflectiveColor) > EPSILON && bounce_count > 0) {
    // trace a ray recursively to get the reflected color
    glm::vec3 dir = MirrorDirection(normal, ray.getDirection());
    Ray reflectRay(point, dir);
    Hit reflectHit;

    glm::vec3 reflectedColor = TraceRay(reflectRay, reflectHit, bounce_count - 1);

    // draw the debug ray
    RayTree::AddReflectedSegment(reflectRay, 0.0f, reflectHit.getT());
  
    answer += reflectedColor * reflectiveColor;
  }

  // cleanup for fake material if needed
  if (!intersect) {
    delete m;
  }
  
  return answer; 

}



void RayTracer::initializeVBOs() {
  glGenBuffers(1, &pixels_a_VBO);
  glGenBuffers(1, &pixels_b_VBO);
  glGenBuffers(1, &pixels_indices_a_VBO);
  glGenBuffers(1, &pixels_indices_b_VBO);
  render_to_a = true;
}


void RayTracer::resetVBOs() {

  pixels_a.clear();
  pixels_b.clear();

  pixels_indices_a.clear();
  pixels_indices_b.clear();

  render_to_a = true;
}

void RayTracer::setupVBOs() {

  glBindBuffer(GL_ARRAY_BUFFER,pixels_a_VBO); 
  glBufferData(GL_ARRAY_BUFFER,sizeof(VBOPosNormalColor)*pixels_a.size(),&pixels_a[0],GL_STATIC_DRAW); 
  glBindBuffer(GL_ARRAY_BUFFER,pixels_b_VBO); 
  glBufferData(GL_ARRAY_BUFFER,sizeof(VBOPosNormalColor)*pixels_b.size(),&pixels_b[0],GL_STATIC_DRAW); 

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,pixels_indices_a_VBO); 
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
	       sizeof(VBOIndexedTri) * pixels_indices_a.size(),
	       &pixels_indices_a[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,pixels_indices_b_VBO); 
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
	       sizeof(VBOIndexedTri) * pixels_indices_b.size(),
	       &pixels_indices_b[0], GL_STATIC_DRAW);

}

void RayTracer::drawVBOs() {
  // turn off lighting
  glUniform1i(GLCanvas::colormodeID, 0);
  // turn off depth buffer
  glDisable(GL_DEPTH_TEST);

  if (render_to_a) {
    drawVBOs_b();
    drawVBOs_a();
  } else {
    drawVBOs_a();
    drawVBOs_b();
  }

  glEnable(GL_DEPTH_TEST);
}

void RayTracer::drawVBOs_a() {
  if (pixels_a.size() == 0) return;
  glBindBuffer(GL_ARRAY_BUFFER, pixels_a_VBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,pixels_indices_a_VBO); 
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor),(void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor),(void*)sizeof(glm::vec3) );
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor), (void*)(sizeof(glm::vec3)*2));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 3, GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor), (void*)(sizeof(glm::vec3)*2 + sizeof(glm::vec4)));
  glDrawElements(GL_TRIANGLES,
                 pixels_indices_a.size()*3,
                 GL_UNSIGNED_INT, 0);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glDisableVertexAttribArray(3);
}

void RayTracer::drawVBOs_b() {
  if (pixels_b.size() == 0) return;
  glBindBuffer(GL_ARRAY_BUFFER, pixels_b_VBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,pixels_indices_b_VBO); 
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor),(void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor),(void*)sizeof(glm::vec3) );
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor), (void*)(sizeof(glm::vec3)*2));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 3, GL_FLOAT,GL_FALSE,sizeof(VBOPosNormalColor), (void*)(sizeof(glm::vec3)*2 + sizeof(glm::vec4)));
  glDrawElements(GL_TRIANGLES,
                 pixels_indices_b.size()*3,
                 GL_UNSIGNED_INT, 0);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glDisableVertexAttribArray(3);
}


void RayTracer::cleanupVBOs() {
  glDeleteBuffers(1, &pixels_a_VBO);
  glDeleteBuffers(1, &pixels_b_VBO);
}
