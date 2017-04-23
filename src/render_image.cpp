#include "glCanvas.h"
#include "argparser.h"
#include "utils.h"


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



void GLCanvas::renderImage(const char* filename) {
  printf("Rendering image\n");

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
    if (i % 30 == 0) {
      printf("%.1f%% done\n", (float)i * 100.0 / (float)dimx);
    }
  }

  if (!matrixToPPM(dimx, dimy, image, filename)) 
    printf("Could not write to file\n");
  else 
    printf("Done writing image\n");
}


