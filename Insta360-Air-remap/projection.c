#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <getopt.h>
#include <math.h>

/* Compile with: gcc projection.c -Wall -o project -lm
 *     math.h does not like to be linked directly...
 * Example call: ./project -x test_x.pgm -y test_y.pgm -h 400 -w 400 -r 400 -c 400 -m equirectangular --verbose
 *         ./project -x fly360_x.pgm -y fly360_y.pgm -h 1504 -w 1504 -r 752 -c 1504 -m equirectangular --verbose
 * Example command: ffmpeg -i input.jpg -i test_x.pgm -i test_y.pgm -lavfi remap out.png
 *          ffmpeg -i fly360.mp4 -i fly360_x.pgm -i fly360_y.pgm -lavfi remap out.mp4
 *
 * # Sources
 * - https://trac.ffmpeg.org/wiki/RemapFilter
 * - https://en.wikipedia.org/wiki/Spherical_coordinate_system
 * - https://en.wikipedia.org/wiki/Stereographic_projection
 * - https://en.wikipedia.org/wiki/Equirectangular_projection
 * - http://paulbourke.net/geometry/transformationprojection/
 */

/* Flag set by ‘--verbose’. */
static int verbose_flag;

static double magicnum = .9280; // scaling for theta-lens .8875 .9170 .9260

/** Kodak SP360 4k full 4k frame
static double FOV = 235.0 * M_PI / 180.0;
// The width of the fisheye image as a percentage of the video frame width
static double lens_scale = 0.97;
static double x_shift = 0;
static double y_shift = 0;
*/

/** Kodak SP360 4k HDMI 1080p frame */
static double FOV = 235.0 * M_PI / 180.0;
// The width of the fisheye image as a percentage of the video frame width
static double lens_scale = 0.60;
static double x_shift = 0;
static double y_shift = 0;

typedef struct double2 {
  double x;
  double y;
} double2;

typedef struct double3 {
  double x;
  double y;
  double z;
} double3;

typedef struct polar2 {
  double r;
  double theta;
} polar2;

typedef struct polar3 {
  double r;
  double theta;
  double phi;
} polar3;

enum CameraMode {
  FRONT,
  THETAS,
  EQUIRECTANGULAR
};

typedef struct configuration {
  char* xmap_filename;
  char* ymap_filename; 
  int xmap_set;
  int ymap_set;
  int rows; // target
  int cols; // target
  int height; // source
  int width; // source
  int rows_set;
  int cols_set;
  int height_set;
  int width_set;
  int crop;
  enum CameraMode mode;
} configuration;

/* Store command line options in configuration */
configuration parse_options(int argc, char **argv) {
  
  int c;
  configuration po; // to hold parsed options
  po.xmap_filename = NULL;
  po.ymap_filename = NULL;
  po.xmap_set = 0;
  po.ymap_set = 0;
  po.rows = 0;
  po.cols = 0;
  po.rows_set = 0;
  po.cols_set = 0;
  po.height_set = 0;
  po.width_set = 0;
  po.mode = THETAS; // default

  while (1) {
  
  static struct option long_options[] = {
      
      /* These options set a flag. */
      {"verbose", no_argument,       &verbose_flag, 1},
      {"brief",   no_argument,       &verbose_flag, 0},
      
      /* These options don’t set a flag.
         We distinguish them by their indices. */
      {"help",    no_argument,       0, 'q'},
      
      /* options with arg*/
      {"xmap",    required_argument, 0, 'x'},
      {"ymap",    required_argument, 0, 'y'},
      {"rows",    required_argument, 0, 'r'}, // target
      {"cols",    required_argument, 0, 'c'}, // target
      {"height",  required_argument, 0, 'h'}, // source
      {"width",   required_argument, 0, 'w'}, // source
      {"mode",    required_argument, 0, 'm'},
      {"crop",    required_argument, 0, 'b'},

      {0, 0, 0, 0}
  };
    
    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long (argc, argv, "qx:y:r:c:h:w:m:b:",
             long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c) {
      
      /* If this option set a flag, do nothing else now. */
      case 0:
        if (long_options[option_index].flag != 0)
          break;
        printf("option %s", long_options[option_index].name);
        if (optarg)
          printf (" with arg %s", optarg);
        printf ("\n");
        break;

      case 'x':
        po.xmap_filename = optarg;
        po.xmap_set++;
        break;

      case 'y':
        po.ymap_filename = optarg;
        po.ymap_set++;
        break;
    
      case 'h':
        po.height = atoi(optarg);
        po.height_set++;
        break;
        
      case 'w':
        po.width = atoi(optarg);
        po.width_set++;
        break;
        
      case 'c':
        po.cols = atoi(optarg);
        po.cols_set++;
        break;
        
      case 'r':
        po.rows = atoi(optarg);
        po.rows_set++;
        break;
        
      case 'b':
        po.crop = atoi(optarg);
        break;
        
      case 'm':
        if (strcmp(optarg, "front") == 0) {
        po.mode = FRONT;
      } else if (strcmp(optarg, "theta") == 0) {
        po.mode = THETAS;
      } else if (strcmp(optarg, "equirectangular") == 0) {
        po.mode = EQUIRECTANGULAR;
        } else /* default: */ {
        printf("Mode %s not implemented \n",optarg); exit(1);
        }
        break;

    /* getopt_long already printed an error message. */
      case '?':
      
      case 'q':
        printf ("Usage: %s -x|--xmap FILE_x.pgm -y|--ymap FILE_y.pgm -h|--height 300 -w|--width 400 -r|--rows 600 -c|--cols 800 \n", argv[0]);
        printf ("h,w is source size, r,c is targetsize \n");
        exit(1);
        break;

      default:
        abort();
    }
  }

  /* Instead of reporting ‘--verbose’
     and ‘--brief’ as they are encountered,
     we report the final status resulting from them. */
  if (verbose_flag) {
  switch(po.mode) {
  case FRONT:
    printf("Mode: Front proj\n"); 
    break;
  case THETAS:
    printf("Mode: Ricoh Theta S proj\n"); 
    break;
  case EQUIRECTANGULAR:
    printf("Mode: Equirectangular proj\n");
    break;
  default:
    printf("Mode not in verbose, exiting\n"); 
    exit(1);
  }
  }
  
  /* Print any remaining command line arguments (not options). */
  if (optind < argc) {
    printf ("ERROR: non-option ARGV-elements: ");
    while (optind < argc){
      printf ("%s ", argv[optind++]);
    }
    putchar ('\n');
    exit(1);
  }
  
  if (po.xmap_set != 1 || po.ymap_set != 1) 
  {
    printf("ERROR: Xmap and ymap are mandatory arguments and have to appear only once!\ntry --help for help\n\n ");
    exit(-1);
  }
  if (po.rows_set != 1 || po.cols_set != 1) 
  {
    printf("ERROR: Target Rows and Cols are mandatory arguments and have to appear only once!\ntry --help for help\n\n ");
    exit(-1);
  }
  if (po.height_set != 1 || po.width_set != 1) 
  {
    printf("ERROR: Source Height and Width are mandatory arguments and have to appear only once!\ntry --help for help\n\n ");
    exit(-1);
  }
  
  return po;
}

/* Write to file */
int pgmWrite_ASCII(char* filename, int rows, int cols, int **image, char* comment_string) {
  FILE* file;             /* pointer to the file buffer */
  long nwritten = 0; /* counter for the number of pixels written */
  long x, y;         /* for loop counters */

  int maxval = 65535;  /* maximum value in the image array */

  /* open the file; write header and comments specified by the user. */
  if ((file = fopen(filename, "w")) == NULL) {
    printf("ERROR: file open failed\n");
    return(0);
  }
  
  fprintf(file,"P2\n");

  if (comment_string != NULL) fprintf(file,"# %s \n", comment_string);
  
  /* write the dimensions of the image */   
  fprintf(file,"%i %i \n", cols, rows);

  /* NOTE: MAXIMUM VALUE IS WHITE; COLOURS ARE SCALED FROM 0 - */
  /* MAXVALUE IN A .PGM FILE. */
    
  /* WRITE MAXIMUM VALUE TO FILE */
  fprintf(file, "%d\n", (int)maxval);

  /* Write data */
  for (y = 0; y < rows; y++) {
    for (x = 0; x < cols; x++) {
      fprintf(file,"%i ", image[y][x]);
      nwritten++;
  }
    fprintf(file, "\n");
  }
  fprintf(file, "\n");

  printf ("\nNumber of pixels total (from rows * cols): %i\n", rows * cols);
  printf ("Number of pixels written in file %s: %ld\n\n", filename, nwritten); 

  fclose(file);
  return(1);
}

int ppmWrite_ASCII(char* filename, int rows, int cols, int **image1, int **image2, char* comment_string, int maxrange) {
  FILE* file;             /* pointer to the file buffer */
  long nwritten = 0; /* counter for the number of pixels written */
  long x, y;         /* for loop counters */

  int maxval = maxrange;  /* maximum value in the image array */
  // maxval = 65535; //breaks previews, fixes ffmpeg

  /* open the file; write header and comments specified by the user. */
  if ((file = fopen(filename, "w")) == NULL) {
    printf("ERROR: file open failed\n");
    return(0);
  }
  
  fprintf(file,"P3\n");

  if (comment_string != NULL) fprintf(file,"# %s \n", comment_string);
  
  /* write the dimensions of the image */   
  fprintf(file,"%i %i \n", cols, rows);

  /* NOTE: MAXIMUM VALUE IS WHITE; COLOURS ARE SCALED FROM 0 - */
  /* MAXVALUE IN A .PGM FILE. */
    
  /* WRITE MAXIMUM VALUE TO FILE */
  fprintf(file, "%d\n", (int)maxval);

  /* Write data */
  for (y = 0; y < rows; y++) {
    for (x = 0; x < cols; x++) {
      fprintf(file,"%i ", image1[y][x]);
      fprintf(file,"%i ", image2[y][x]);
      fprintf(file,"%i ", 0);
      nwritten++;
  }
    fprintf(file, "\n");
  }
  fprintf(file, "\n");

  printf ("\nNumber of pixels total (from rows * cols): %i\n", rows * cols);
  printf ("Number of pixels written in file %s: %ld\n\n", filename, nwritten); 

  fclose(file);
  return(1);
}

/* So, to get the x’,y’ position for the circular image we will have to first pass the 
 * coordinates x,y from the rectangular output image to spherical coordinates using the
 * first coordinate system, then those to the second shown spherical coordinate system, 
 * then those to the polar projection and then pass the polar system to cardinal x’,y’.
 */
double2 evaluatePixel_Front(double2 outPos, double2 srcSize) {
  double theta, phi, phi2;
  double x, y, z;
  double theta2;
  double2 inCentered;
  int lens = 0;
  double adj;
  double radius;

  // Convert outcoords to radians on the sphere.
  // Center the frame on 180 degrees and adjust the angles to account for the FOV of the lens
  theta = (2.0 * outPos.x * M_PI - M_PI);
  phi = (outPos.y * M_PI - M_PI_2);
  
  // Convert outcoords to coordinates on a unit sphere
  x = sin(theta) * cos(phi);
  y = -sin(phi);
  z = cos(theta) * cos(phi);

  // Convert to spherical with respect to the lens
  theta2 = atan2(y, x);
  adj = sqrt(x * x + y * y);
  phi2 = atan2(adj, z);
  radius = phi2 / FOV;
  if (radius <= 0.5) {
    inCentered.x = (radius * sin(theta2) * lens_scale + 0.5) * srcSize.x;
    inCentered.y = (radius * cos(theta2) * lens_scale + 0.5) * srcSize.y;
  } else {
    inCentered.x = 0;
    inCentered.y = 0;
  }

  return inCentered;
}

/* So, to get the x’,y’ position for the circular image we will have to first pass the 
 * coordinates x,y from the rectangular output image to spherical coordinates using the
 * first coordinate system, then those to the second shown spherical coordinate system, 
 * then those to the polar projection and then pass the polar system to cardinal x’,y’.
 */
double2 evaluatePixel_Theta(double2 outPos, double2 srcSize) {
  double theta, phi;
  double3 sphericCoords;
  double phi2_over_pi;
  double theta2;
  double2 inCentered;
  
  int lens = 0;

  if(outPos.x>0.5){
    lens++;
  }

  outPos.x*=2;
  
  // Convert outcoords to radians (180 = pi, so half a sphere)
  theta = (1.0 - outPos.x) * M_PI; 
  phi = outPos.y * M_PI;

  // Convert outcoords to spherical (x,y,z on unit sphere)
  sphericCoords.z = cos(theta) * sin(phi);
  sphericCoords.y = sin(theta+ (double)lens*M_PI) * sin(phi);
  sphericCoords.x = -cos(phi+ (double)lens*M_PI);
    
  // Convert spherical to input coordinates...
  theta2 = atan2(-sphericCoords.z, sphericCoords.x);
  phi2_over_pi = acos(sphericCoords.y) / M_PI;
  //phi2_over_pi = sphericCoords.y;
  //###
  inCentered.x = (phi2_over_pi * cos(theta2) * magicnum + 0.5102 + lens) * srcSize.x;
  inCentered.y = (phi2_over_pi * sin(theta2) * magicnum + 0.4990) * srcSize.y;
  
  return inCentered;
}

/* 1. Define cartesian plane
 * 2. Reverse equirectangular projection from cartesian plane to polar coords in sphere
 * 3. Stereographic projection of polar coords from sphere to plane 
 * 4. Convert polar coords to cartesian coords in plane
 * 5. Center and stretch according to source size
 */
double2 evaluatePixel_Equirectangular(double2 outPos, double2 srcSize) {
  
  double2 cartesianCoordsPlane;
  polar3 polarCoordsSphere;
  polar2 polarCoordsPlane;
  double2 result;
  
  // Define cartesianCoordsPlane
  cartesianCoordsPlane.x = 1.0 - outPos.x;
  cartesianCoordsPlane.y = 1.0 - outPos.y;
  
  // Reverse equirectangular projection
  // Convert cartesianCoordsPlane to polarCoordsSphere
  polarCoordsSphere.theta = cartesianCoordsPlane.x * 2.0 * M_PI;
  polarCoordsSphere.phi = cartesianCoordsPlane.y * M_PI/2.0 + M_PI/2.0;
  
  // Stereographic projection
  // Convert polarCoordsSphere to polar coordinates on plane
  polarCoordsPlane.r = sin(polarCoordsSphere.phi)/(1.0-cos(polarCoordsSphere.phi));
  polarCoordsPlane.theta = polarCoordsSphere.theta;
  
  // Convert polarCoordsPlane to cartesian coordinates; center and stretch
  result.x = (polarCoordsPlane.r * cos(polarCoordsPlane.theta) + 1.0)/2.0 * srcSize.x;
  result.y = (polarCoordsPlane.r * sin(polarCoordsPlane.theta) + 1.0)/2.0 * srcSize.y;
  
  // Coordinates of pixel in input which should be mapped onto given pixel in output
  return result;
}

//###
/* Generate maps */
void gen_maps(configuration cfg, int** image_x, int** image_y) {
  int x, y;
  for (y = 0; y < cfg.rows; y++) {
  for (x = 0; x < cfg.cols; x++) {
    double2 outPos = {(double)x / (double)cfg.cols,
                (double)y / (double)cfg.rows}; 
    double2 srcSize = {cfg.width, cfg.height};
    double2 o;
    // TODO crop
    // Map output pixel (x, y) to corresponding input pixel
    switch (cfg.mode) {
      case FRONT:       
      o = evaluatePixel_Front(outPos, srcSize); 
      break;
      case THETAS:       
      o = evaluatePixel_Theta(outPos, srcSize); 
      break;
      case EQUIRECTANGULAR: 
      o = evaluatePixel_Equirectangular(outPos, srcSize); 
      break;
      default: 
      printf("Mode not implemented\n");
      exit(1);
    }

    image_x[y][x] = (int)round(o.x);
    image_y[y][x] = (int)round(o.y);

  }
  }
}

/* Main */
int main (int argc, char **argv) {
  int y;
  int** image_x;
  int** image_y;
  configuration cfg = parse_options(argc, argv);

  if (cfg.xmap_filename) printf("xmapfile: %s\n", cfg.xmap_filename);
  if (cfg.ymap_filename) printf("ymapfile: %s\n", cfg.ymap_filename);
  
  /* Allocate memory for TODO */
  image_x = malloc((cfg.rows) * sizeof(*image_x));
  for (y = 0 ; y < (cfg.rows); y++) image_x[y] = malloc((cfg.cols) * sizeof(*(image_x[y])));
  image_y = malloc((cfg.rows) * sizeof(*image_y));
  for (y = 0; y < (cfg.rows); y++) image_y[y]= malloc((cfg.cols) * sizeof(*(image_y[y])));

  /* Generate the maps */
  printf("Generating maps\n");
  gen_maps(cfg, image_x, image_y);

  /* Write files */
  printf("Writing files\n");
  pgmWrite_ASCII(cfg.ymap_filename, cfg.rows, cfg.cols,image_y, cfg.ymap_filename);
  pgmWrite_ASCII(cfg.xmap_filename, cfg.rows, cfg.cols,image_x, cfg.xmap_filename);
  char* filename = "xy.ppm";
  ppmWrite_ASCII(filename, cfg.rows, cfg.cols,image_x,image_y,filename, cfg.width*2);

  /* Free memory */
  if (image_y) {
    for (y = 0; y < cfg.rows; y++) free(image_y[y]);
    free(image_y);
  }
  if (image_x) {
    for (y = 0; y < cfg.rows; y++) free(image_x[y]);
    free(image_x);
  }

  /* Exit */
  exit (0);
}
