#include "glCanvas.h"
#include "argparser.h"
#include "utils.h"
#include "mesh.h"
#include "lightningsegment.h"
#include <sys/stat.h>


bool matrixToPPM(unsigned int dimx, unsigned int dimy,
                 unsigned char*** matrix, const char* filename) {
  unsigned int i, j;
  FILE *fp = fopen(filename, "wb");
  if (!fp) return false;
  fprintf(fp, "P6\n%d %d\n255\n", dimx, dimy);
  for (i = 0; i < dimx; i++) {
    for (j = 0; j < dimy; j++) 
      fwrite(matrix[j][dimy-i-1], 1, 3, fp);
  }
  fclose(fp);
  return true;
}


unsigned int linearToByte(float linearColor) {
  unsigned int byte = 255 * linear_to_srgb(linearColor);
  if (byte > 255) byte = 255;
  return byte;
}


void GLCanvas::renderImage(const char* filename, bool status) {
  if (status) printf("Rendering image %s\n", filename);

  int dimx = args->width;
  int dimy = args->height;

  unsigned char*** image = new unsigned char**[dimx];
  for (int i = 0; i < dimx; i++) {
    image[i] = new unsigned char*[dimy];
    for (int j = 0; j < dimy; j++) {
      image[i][j] = new unsigned char[3];
      glm::vec3 color = TraceRay((double)i, (double)j);
      image[i][j][0] = linearToByte(color.x);
      image[i][j][1] = linearToByte(color.y);
      image[i][j][2] = linearToByte(color.z);
    }
    if (i % 30 == 0 && status) {
      printf("%.1f%% done\n", (float)i * 100.0 / (float)dimx);
    }
  }

  if (!matrixToPPM(dimx, dimy, image, filename)) 
    printf("Could not write to file\n");
  else if (status)
    printf("Done writing image %s\n", filename);
}


void GLCanvas::renderSequence(const char* dirname) {
  
  printf("Rendering lightning sequence\n");

  // Create directory for files
  const int err = mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (err == -1) {
    printf("Could not create directory\n");
    return;
  } 

  std::vector<LightningSegment> segments(mesh->lightning_segments);
  mesh->lightning_segments.clear(); 
  char filebuf[64];
  int segments_per_image = 10;
  int c_index = 0;
  
  printf("Writing %lu files\n", segments.size() / segments_per_image);
  
  for (unsigned int i = 0; i < segments.size() / segments_per_image; i++) {
    c_index = i * segments_per_image;
    snprintf(filebuf, 64, "%s/out%d.ppm", dirname, i);
    renderImage(filebuf, false);
    printf("File %s written\n", filebuf);
    std::vector<LightningSegment> added(&segments[c_index], 
                                        &segments[c_index+segments_per_image]);
    mesh->lightning_segments.insert(mesh->lightning_segments.end(),
                                    added.begin(), added.end());
  }

  printf("Done writing images\n");
  
}
