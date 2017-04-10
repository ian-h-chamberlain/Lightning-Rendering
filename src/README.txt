HOMEWORK 3: RAY TRACING, RADIOSITY, & PHOTON MAPPING

NAME:  Ian Chamberlain



ESTIMATE OF # OF HOURS SPENT ON THIS ASSIGNMENT:  12



COLLABORATORS AND OTHER RESOURCES: 

I used the lecture slides and several other internet-based references (stackoverflow, etc.)
to complete this assignment.


RAYTRACING:

To select random points on the pixel, I used the provided ArgParser::rand() method.
For selecting random light source points, I used Face::RandomPoint(), which also uses
this method. It seems to be fairly successful, as it utilizes a uniform distribution.

RADIOSITY:

The form factor must be computed for all pairs of patches, making the initial form factor
calculation O(n^2). The same calculation can be re-used for F_i,j as for F_j,i, reducing
the complexity by about half, but still O(n^2). normalizeFormFactors() is also O(n),
so using it for each face results in O(n^2) time.

Using several samples for calculating form factor, rather than just the centroid of each
face, increases time complexity to O(n^2 * s), where s is the number of samples.

PHOTON MAPPING:

I use Russian roulette to decide whether or not to terminate a photon's reflection,
with probability based on the hit surface's reflective and diffuse colors.
(based on https://graphics.stanford.edu/courses/cs348b-00/course8.pdf)

The photon may be reflected diffusely, specularly, or absorbed. In each case,
its energy is multiplied by the reflecting color. Additionally, the energy
is divided by the probability for that reflection, reducing the energy as a photon bounces.

For diffuse reflection, I use the utils.h implementation of RandomDiffuseDirection(),
using the normal of the surface.

My results seem to be significantly lighter than the example images, particularly in the
shadow of the ring. The caustics are a bit less noticeable due to this lighter color.

OTHER NEW FEATURES OR EXTENSIONS FOR EXTRA CREDIT:

None
