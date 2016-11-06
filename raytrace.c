#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define M_PI  3.14159265358979323846

typedef struct {	//Create structure to be used for our object_array
  int kind; // 0 = camera, 1 = sphere, 2 = plane, 3 = light
  union {
    struct {
      double width;
      double height;
    } camera;
    struct {
      double diffuse_color[3];
	  double specular_color[3];
	  double position[3];
      double radius;
    } sphere;
    struct {
      double diffuse_color[3];
	  double specular_color[3];
	  double position[3];
	  double normal[3];
    } plane;
	struct {
	  double color[3];
	  double position[3];
	  double direction[3];
	  double radial_a2;
	  double radial_a1;
	  double radial_a0;
	  double angular_a0;
	  double theta;
	} light;
  };
} Object;

int line = 1;

// next_c() wraps the getc() function and provides error checking and line
// number maintenance
int next_c(FILE* json) {
  int c = fgetc(json);
#ifdef DEBUG
  printf("next_c: '%c'\n", c);
#endif
  if (c == '\n') {
    line += 1;
  }
  if (c == EOF) {
    fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
    exit(1);
  }
  return c;
}


// expect_c() checks that the next character is d.  If it is not it emits
// an error.
void expect_c(FILE* json, int d) {
  int c = next_c(json);
  if (c == d) return;
  fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
  exit(1);    
}


// skip_ws() skips white space in the file.
void skip_ws(FILE* json) {
  int c = next_c(json);
  while (isspace(c)) {
    c = next_c(json);
  }
  ungetc(c, json);
}


// next_string() gets the next string from the file handle and emits an error
// if a string can not be obtained.
char* next_string(FILE* json) {
  char buffer[129];
  int c = next_c(json);
  if (c != '"') {
    fprintf(stderr, "Error: Expected string on line %d.\n", line);
    exit(1);
  }  
  c = next_c(json);
  int i = 0;
  while (c != '"') {
    if (i >= 128) {	//Strings must be shorter than 128 characters
      fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported.\n");
      exit(1);      
    }
    if (c == '\\') {	//No escape characters allowed
      fprintf(stderr, "Error: Strings with escape codes are not supported.\n");
      exit(1);      
    }
    if (c < 32 || c > 126) {	//String characters must be ascii
      fprintf(stderr, "Error: Strings may contain only ascii characters.\n");
      exit(1);
    }
    buffer[i] = c;
    i += 1;
    c = next_c(json);
  }
  buffer[i] = 0;
  return strdup(buffer);	//Return the string in a duplicated buffer
}

double next_number(FILE* json) {	//Parse the next number and return it as a double
	double value;
	int numDigits = 0;
	numDigits = fscanf(json, "%lf", &value);
	if(numDigits == 0){
		fprintf(stderr, "Error: Expected number at line %d\n", line);
		exit(1);
	}
	return value;
}

double* next_vector(FILE* json) {	//parse the next vector, and return it in double form
	double* v = malloc(3*sizeof(double));
	expect_c(json, '[');
	skip_ws(json);
	v[0] = next_number(json);
	skip_ws(json);
	expect_c(json, ',');
	skip_ws(json);
	v[1] = next_number(json);
	skip_ws(json);
	expect_c(json, ',');
	skip_ws(json);
	v[2] = next_number(json);
	skip_ws(json);
	expect_c(json, ']');
	return v;
}

static inline double sqr(double v) {	//Return the square of the number passed in
  return v*v;
}

static inline void normalize(double* v) {	//Normalize any vectors passed into this function
	double len = sqrt(sqr(v[0]) + sqr(v[1]) + sqr(v[2]));
	v[0] /= len;
	v[1] /= len;
	v[2] /= len;
}

double clamp(double input){		//Return 1 if the input is above one, and return 0 if the input is negative
	if(input > 1) return 1;
	if(input < 0) return 0;
	return input;
}

//This function takes an input value or vector, and puts it into our object array
void store_value(Object* input_object, int type_of_field, double input_value, double* input_vector){
	//type_of_field values: 0 = width, 1 = height, 2 = radius, 3 = diffuse_color, 4 = specular_color, 5 = position, 6 = normal
	//7 = radial_a0, 8 = radial_a1, 9 = radial_a2, 10 = angular_a0, 11 = color, 12 = direction, 13 = theta
	//if input_value or input_vector aren't used, a 0 or NULL value should be passed in
	if(input_object->kind == 0){	//If the object is a camera, store the input into its width or height fields
		if(type_of_field == 0){
			if(input_value <= 0){
				fprintf(stderr, "Error: Camera width must be greater than 0, line:%d\n", line);
				exit(1);
			}
			input_object->camera.width = input_value;
		}else if(type_of_field == 1){
			if(input_value <= 0){
				fprintf(stderr, "Error: Camera height must be greater than 0, line:%d\n", line);
				exit(1);
			}
			input_object->camera.height = input_value;
		}else{
			fprintf(stderr, "Error: Camera may only have 'width' or 'height' fields, line:%d\n", line);
			exit(1);
		}
	}else if(input_object->kind == 1){	//If the object is a sphere, store input in the radius, color, or position fields
		if(type_of_field == 2){
			input_object->sphere.radius = input_value;
		}else if(type_of_field == 3){
			if(input_vector[0] > 1 || input_vector[1] > 1 || input_vector[2] > 1){
				fprintf(stderr, "Error: Diffuse color values must be between 0 and 1, line:%d\n", line);
				exit(1);
			}
			if(input_vector[0] < 0 || input_vector[1] < 0 || input_vector[2] < 0){
				fprintf(stderr, "Error: Diffuse color values may not be negative, line:%d\n", line);
				exit(1);
			}
			input_object->sphere.diffuse_color[0] = input_vector[0];
			input_object->sphere.diffuse_color[1] = input_vector[1];
			input_object->sphere.diffuse_color[2] = input_vector[2];
		}else if(type_of_field == 4){
			if(input_vector[0] > 1 || input_vector[1] > 1 || input_vector[2] > 1){
				fprintf(stderr, "Error: Specular color values must be between 0 and 1, line:%d\n", line);
				exit(1);
			}
			if(input_vector[0] < 0 || input_vector[1] < 0 || input_vector[2] < 0){
				fprintf(stderr, "Error: Specular color values may not be negative, line:%d\n", line);
				exit(1);
			}
			input_object->sphere.specular_color[0] = input_vector[0];
			input_object->sphere.specular_color[1] = input_vector[1];
			input_object->sphere.specular_color[2] = input_vector[2];
		}else if(type_of_field == 5){
			input_object->sphere.position[0] = input_vector[0];
			input_object->sphere.position[1] = input_vector[1];
			input_object->sphere.position[2] = input_vector[2];
		}else{
			fprintf(stderr, "Error: Spheres only have 'radius', 'specular_color', 'diffuse_color', or 'position' fields, line:%d\n", line);
			exit(1);
		}
	}else if(input_object->kind == 2){	//If the object is a plane, store input in the radius, color, or normal fields
		if(type_of_field == 3){
			if(input_vector[0] > 1 || input_vector[1] > 1 || input_vector[2] > 1){
				fprintf(stderr, "Error: Diffuse color values must be between 0 and 1, line:%d\n", line);
				exit(1);
			}
			if(input_vector[0] < 0 || input_vector[1] < 0 || input_vector[2] < 0){
				fprintf(stderr, "Error: Diffuse color values may not be negative, line:%d\n", line);
				exit(1);
			}
			input_object->plane.diffuse_color[0] = input_vector[0];
			input_object->plane.diffuse_color[1] = input_vector[1];
			input_object->plane.diffuse_color[2] = input_vector[2];
		}else if(type_of_field == 4){
			if(input_vector[0] > 1 || input_vector[1] > 1 || input_vector[2] > 1){
				fprintf(stderr, "Error: Specular color values must be between 0 and 1, line:%d\n", line);
				exit(1);
			}
			if(input_vector[0] < 0 || input_vector[1] < 0 || input_vector[2] < 0){
				fprintf(stderr, "Error: Specular color values may not be negative, line:%d\n", line);
				exit(1);
			}
			input_object->plane.specular_color[0] = input_vector[0];
			input_object->plane.specular_color[1] = input_vector[1];
			input_object->plane.specular_color[2] = input_vector[2];
		}else if(type_of_field == 5){
			input_object->plane.position[0] = input_vector[0];
			input_object->plane.position[1] = input_vector[1];
			input_object->plane.position[2] = input_vector[2];
		}else if(type_of_field == 6){
			if(input_vector[2] > 0){
				input_object->plane.normal[0] = -input_vector[0];
				input_object->plane.normal[1] = -input_vector[1];
				input_object->plane.normal[2] = -input_vector[2];
			}else{
				input_object->plane.normal[0] = input_vector[0];
				input_object->plane.normal[1] = input_vector[1];
				input_object->plane.normal[2] = input_vector[2];
			}
			normalize(input_object->plane.normal);
		}else{
			fprintf(stderr, "Error: Planes only have 'radius', 'specular_color', 'diffuse_color', or 'normal' fields, line:%d\n", line);
			exit(1);
		}
	}else if(input_object->kind == 3){
		if(type_of_field == 5){
			input_object->light.position[0] = input_vector[0];
			input_object->light.position[1] = input_vector[1];
			input_object->light.position[2] = input_vector[2];
		}else if(type_of_field == 11){
			input_object->light.color[0] = input_vector[0];
			input_object->light.color[1] = input_vector[1];
			input_object->light.color[2] = input_vector[2];
		}else if(type_of_field == 12){
			input_object->light.direction[0] = input_vector[0];
			input_object->light.direction[1] = input_vector[1];
			input_object->light.direction[2] = input_vector[2];
			normalize(input_object->light.direction);
		}else if(type_of_field == 7){
			input_object->light.radial_a0 = input_value;
		}else if(type_of_field == 8){
			input_object->light.radial_a1 = input_value;
		}else if(type_of_field == 9){
			input_object->light.radial_a2 = input_value;
		}else if(type_of_field == 10){
			input_object->light.angular_a0 = input_value;
		}else if(type_of_field == 13){
			input_object->light.theta = input_value;
		}else{
			fprintf(stderr, "Error: Lights must have the fields listed, line:%d\n", line);
			fprintf(stderr, "Color\nPosition\nDirection\nradial-n0\nradial-n1\nradial-n2\nangular-n0\n");
			exit(1);
		}
	}else{
		fprintf(stderr, "Error: Undefined object type, line:%d\n", line);
		exit(1);
	}
}

double degrees_to_radians(double value){	//Converts input from degrees to radians
	return 2*M_PI*value/360;
}

int read_scene(char* filename, Object** object_array) {	//Parses json file, and stores object information into object_array
  int c;
  int num_objects = 0;
  int object_counter = -1;
  int height = 0, width = 0, radius = 0, diffuse_color = 0, specular_color = 0, position = 0, normal = 0;	//These will serve as boolean operators
  int radial_a2 = 0, radial_a1 = 0, radial_a0 = 0, angular_a0 = 0, color = 0, theta = 0;
  FILE* json = fopen(filename, "r");	//Open our json file

  if (json == NULL) {	//If the file does not exist, throw an error
    fprintf(stderr, "Error: Could not open file \"%s\"\n", filename);
    exit(1);
  }
  
  skip_ws(json);
  
  // Find the beginning of the list
  expect_c(json, '[');

  skip_ws(json);

  // Find the objects
  while (1) {
    c = fgetc(json);
    if (c == ']' && num_objects != 0) {		//A ',' must be read before getting here, which means we are expecting more objects
      fprintf(stderr, "Error: End of file reached when expecting more objects, line:%d\n", line);
      exit(1);
    }
	else if(c == ']'){	//If no objects have been parsed and a bracket is found, our file is empty, throw an error
		fprintf(stderr, "Error: JSON file contains no objects\n");
		fclose(json);
		exit(1);
	}
	
    if (c == '{') {	//Start object parsing
	  if(object_counter >= 128){	//If 128 objects have already been scanned, throw an error
		  fprintf(stderr, "Error: Maximum amount of objects allowed (not including the camera) is 128, line:%d\n", line);
		  exit(1);
	  }
	  object_array[++object_counter] = malloc(sizeof(Object)); //Make space for the new object in object_array
      skip_ws(json);
    
      // Parse object type
      char* key = next_string(json);
      if (strcmp(key, "type") != 0) {
	fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
	exit(1);
      }

      skip_ws(json);

      expect_c(json, ':');

      skip_ws(json);

      char* value = next_string(json);

      if (strcmp(value, "camera") == 0) {
		  object_array[object_counter]->kind = 0;	//If camera, set object kind to 0
		  width = 1;
		  height = 1;
      } else if (strcmp(value, "sphere") == 0) {
		  object_array[object_counter]->kind = 1;	//If sphere, set object kind to 1
		  position = 1;
		  radius = 1;
		  specular_color = 1;
		  diffuse_color = 1;
      } else if (strcmp(value, "plane") == 0) {
		  object_array[object_counter]->kind = 2;	//If plane, set object kind to 2
		  position = 1;
		  normal = 1;
		  specular_color = 1;
		  diffuse_color = 1;
      } else if (strcmp(value, "light") == 0){		//If light, set object kind to 3
		  object_array[object_counter]->kind = 3;
		  position = 1;
		  color = 1;
		  radial_a0 = 1;
		  radial_a1 = 1;
		  radial_a2 = 1;
		  angular_a0 = 1;
		  theta = 1;
	  } else {
		  fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
		  exit(1);
      }

      skip_ws(json);

      while (1) {	//Parse the rest of the object
		
		c = next_c(json);
		if (c == '}') {
		  // stop parsing this object
		  //If a required field is missing from an object, throw an error
		  if(height == 1 || width == 1 || position == 1 || normal == 1 || color == 1 || radius == 1 ||
		  diffuse_color == 1 || specular_color == 1 || position == 1){	//If a required value was not in the json file, throw error
			  fprintf(stderr, "Error: Required field missing from object at line:%d\n", line);
			  exit(1);
		  }
		  if(radial_a0 == 1){	//If radial_a0 did not exist in json file, store the default value 1
			  store_value(object_array[object_counter], 7, 1, NULL);
			  radial_a0 = 0;
		  }
		  if(radial_a1 == 1){	//If radial_a1 did not exist in json file, store the default value 0
			  store_value(object_array[object_counter], 8, 0, NULL);
			  radial_a1 = 0;
		  }
		  if(radial_a2 == 1){	//If radial_a2 did not exist in json file, store the default value 0
			  store_value(object_array[object_counter], 9, 0, NULL);
			  radial_a2 = 0;
		  }
		  if(angular_a0 == 1){	//If angular_a0 did not exist in json file, store default value 0
			  store_value(object_array[object_counter], 10, 0, NULL);
			  angular_a0 = 0;
		  }
		  if(theta == 1){	//If theta did not exist in json file, store default value 0
			  store_value(object_array[object_counter], 13, 0, NULL);
			  theta = 0;
		  }
		  break;
		} else if (c == ',') {
		  // read another field
		  skip_ws(json);
		  char* key = next_string(json);
		  skip_ws(json);
		  expect_c(json, ':');
		  skip_ws(json);
		  if (strcmp(key, "width") == 0){	//Based on the field, parse a number or vector
			  double value = next_number(json);
			  store_value(object_array[object_counter], 0, value, NULL);	//And store the value in the object_array
			  width = 0;
		  }else if(strcmp(key, "height") == 0){
			  double value = next_number(json);
			  store_value(object_array[object_counter], 1, value, NULL);
			  height = 0;
		  }else if(strcmp(key, "radius") == 0) {
			  double value = next_number(json);
			  store_value(object_array[object_counter], 2, value, NULL);
			  radius = 0;
		  }else if (strcmp(key, "color") == 0){
			  double* value = next_vector(json);
			  store_value(object_array[object_counter], 11, 0, value);
			  color = 0;
		  }else if(strcmp(key, "position") == 0){
			  double* value = next_vector(json);
			  store_value(object_array[object_counter], 5, 0, value);
			  position = 0;
		  }else if(strcmp(key, "normal") == 0) {
			  double* value = next_vector(json);
			  store_value(object_array[object_counter], 6, 0, value);
			  normal = 0;
		  }else if(strcmp(key, "diffuse_color") == 0){
			  double* value = next_vector(json);
			  store_value(object_array[object_counter], 3, 0, value);
			  diffuse_color = 0;
		  }else if(strcmp(key, "specular_color") == 0){
			  double* value = next_vector(json);
			  store_value(object_array[object_counter], 4, 0, value);
			  specular_color = 0;
		  }else if(strcmp(key, "radial-a0") == 0){
			  double value = next_number(json);
			  store_value(object_array[object_counter], 7, value, NULL);
			  radial_a0 = 0;
		  }else if(strcmp(key, "radial-a1") == 0){
			  double value = next_number(json);
			  store_value(object_array[object_counter], 8, value, NULL);
			  radial_a1 = 0;
		  }else if(strcmp(key, "radial-a2") == 0){
			  double value = next_number(json);
			  store_value(object_array[object_counter], 9, value, NULL);
			  radial_a2 = 0;
		  }else if(strcmp(key, "angular-a0") == 0){
			  double value = next_number(json);
			  store_value(object_array[object_counter], 10, value, NULL);
			  angular_a0 = 0;
		  }else if(strcmp(key, "direction") == 0){
			  double* value = next_vector(json);
			  store_value(object_array[object_counter], 12, 0, value);
		  }else if(strcmp(key, "theta") == 0){
			  double value = next_number(json);
			  store_value(object_array[object_counter], 13, degrees_to_radians(value), NULL);
			  theta = 0;
		  }else{	//If there was an invalid field, throw an error
				fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
				key, line);
				exit(1);
		  }
		  skip_ws(json);
		} else {	//If a ',' or '}' was not received, throw an error
		  fprintf(stderr, "Error: Unexpected value on line %d\n", line);
		  exit(1);
		}
      }
      skip_ws(json);
	  num_objects++;
      c = next_c(json);
      if (c == ',') {	//If there is a comma, parse more objects!
	// noop
	skip_ws(json);
      } else if (c == ']') {	//If there is an ending bracket, it is the end JSON file
	fclose(json);
	return object_counter;
      } else {	//Throw error if we don't encounter a ',' or ']'
	fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
	exit(1);
      }
    }
  }
}

void argument_checker(int c, char** argv){	//Check input arguments for validity
	int i = 0;
	int j = 0;
	char* periodPointer;
	if(c != 5){	//Ensure that five arguments are passed in through command line
		fprintf(stderr, "Error: Incorrect amount of arguments\n");
		exit(1);
	}
	
	while(1){	//Ensure that both the width and height arguments are numbers
		if(*(argv[1] + i) == NULL && *(argv[2] + j) == NULL){
			break;
		}
		else if(*(argv[1] + i) == NULL){
			i--;
		}
		else if(*(argv[2] + j) == NULL){
			j--;
		}
		
		if(!isdigit(*(argv[1] + i)) || !isdigit(*(argv[2] + j))){
			fprintf(stderr, "Error: Width or Height field is not a number\n");
			exit(1);
		}
		i++;
		j++;
	}
	
	periodPointer = strrchr(argv[3], '.');	//Ensure that the input scene file has an extension .json
	if(periodPointer == NULL){
		fprintf(stderr, "Error: Input scene file does not have a file extension\n");
		exit(1);
	}
	if(strcmp(periodPointer, ".json") != 0){
		fprintf(stderr, "Error: Input scene file is not of type JSON\n");
		exit(1);
	}
	
	periodPointer = strrchr(argv[4], '.');	//Ensure that the output picture file has an extension .ppm
	if(periodPointer == NULL){
		fprintf(stderr, "Error: Output picture file does not have a file extension\n");
		exit(1);
	}
	if(strcmp(periodPointer, ".ppm") != 0){
		fprintf(stderr, "Error: Output picture file is not of type PPM\n");
		exit(1);
	}
}

double sphere_intersection(double* Ro, double* Rd, double* C, double radius){ //Calculates the solutions to a sphere intersection
	//Sphere equation is x^2 + y^2 + z^2 = r^2
	//Parameterize: (x-Cx)^2 + (y-Cy)^2 + (z-Cz)^2 - r^2 = 0
	//Substitute with ray:
	//(Rox + t*Rdx - Cx)^2 + (Roy + t*Rdy - Cy)^2 + (Roz + t*Rdz - Cz)^2 - r^2 = 0
	//Solve for t:
	//a = Rdx^2 + Rdy^2 + Rdz^2
	double a = sqr(Rd[0]) + sqr(Rd[1]) + sqr(Rd[2]);
	//b = (2RdxRox - 2RdxCx) + (2RdyRoy - 2RdyCy) + (2RdzRoz - 2RdzCz)
	double b = (2*Rd[0]*Ro[0] - 2*Rd[0]*C[0]) + (2*Rd[1]*Ro[1] - 2*Rd[1]*C[1]) + (2*Rd[2]*Ro[2] - 2*Rd[2]*C[2]);
	//c = (Rox^2 - 2RoxCx + Cx^2) + (Roy^2 - 2RoyCy + Cy^2) + (Roz^2 - 2RozCz + Cz^2) - r^2
	double c = (sqr(Ro[0]) - 2*Ro[0]*C[0] + sqr(C[0])) + (sqr(Ro[1]) - 2*Ro[1]*C[1] + sqr(C[1])) + (sqr(Ro[2]) - 2*Ro[2]*C[2] + sqr(C[2])) - sqr(radius);
	
	double t0;
	double t1;
	double det = sqr(b) - 4*a*c;
	if(det < 0) return 0;	//If there are no real solutions return 0
	
	t0 = (-b - sqrt(det))/(2*a);	//Calculate both solutions
	t1 = (-b + sqrt(det))/(2*a);
	if(t0 <= 0 && t1 <= 0) return 0; //If both solutions are less than 0, return 0
	if(t0 <= 0 && t1 > 0) return t1; //If only t1 is greater than 0, return t1
	if(t1 <= 0 && t0 > 0) return t0; //If only t0 is greater than 0, return t0
	if(t0 > t1) return t1;	//If t0 is greater than t1, return t1
	if(t1 > t0) return t0;	//If t1 is greater than t0, return t0
	
	return t0;	//If both solutions are equal, just return t0
	
}

double plane_intersection(double* Ro, double* Rd, double* C, double* N){ //Calculates the solution of a plane intersection
	//Solve for Plane Equation:
	//Nx(x - Cx) + Ny(y - Cy) + Nz(z - Cz) = 0
	//Plug in our Ray:
	//Nx(Rox + t*Rdx - Cx) + Ny(Roy + t*Rdy - Cy) + Nz(Roz + t*Rdz - Cz) = 0
	//Solve for t:
	//t = ((NxCx - NxRox) + (NyCy - NyRoy) + (NzCz - NzRoz))/(RdxNx + RdyNy + RdzNz)
	double t = ((N[0]*C[0] - N[0]*Ro[0]) + (N[1]*C[1] - N[1]*Ro[1]) + (N[2]*C[2] - N[2]*Ro[2]))/(Rd[0]*N[0] + Rd[1]*N[1] + Rd[2]*N[2]);
	if(t > 0) return t;	//Return solution if it is greater than 0
	return 0;	//else just return 0
}

double fang(double a0, double theta, double* vO, double* vL){	//Return angular attenuation value
	//vO is vector pointing from the light to the object
	//vL is the direction of the light
	if(theta == 0 || a0 == 0) return 1;	//If we aren't using spotlights, we can just return 1
	if(vO[0]*vL[0] + vO[1]*vL[1] + vO[2]*vL[2] < cos(theta)){
		return 0;
	}
	return pow(vO[0]*vL[0] + vO[1]*vL[1] + vO[2]*vL[2], a0);
}

double frad(double a0, double a1, double a2, double distance){	//Return radial attenuation value
	if(distance == 0) return 1;		//If the light is somehow on the object, return 1
	double denominator = (a0 + a1*distance + a2*sqr(distance));	//Calculate denominator of radial attenuation fnc
	if (denominator == 0) return 1;	//If denominator is a faulty value of 0, return 1
	return 1/denominator;	//If everything goes smoothly, return the real radial attenuation value
}

double* diffuse(double* L, double* N, double* Cd, double* Ci){	//Return diffuse color value
	//Cd is diffuse color, and Ci is light color
	double* diffused_color = malloc(sizeof(double)*3);
	//Calculate diffuse color values
	diffused_color[0] = (L[0] * N[0] + L[1] * N[1] + L[2] * N[2])*Cd[0]*Ci[0];
	diffused_color[1] = (L[0] * N[0] + L[1] * N[1] + L[2] * N[2])*Cd[1]*Ci[1];
	diffused_color[2] = (L[0] * N[0] + L[1] * N[1] + L[2] * N[2])*Cd[2]*Ci[2];
	if(diffused_color[0] < 0) diffused_color[0] = 0;	//Diffuse colors cannot be negative
	if(diffused_color[1] < 0) diffused_color[1] = 0;
	if(diffused_color[2] < 0) diffused_color[2] = 0;
	return diffused_color;		//Return diffuse color
}

double* specular(double* R, double* V, double* Cs, double* Ci, double* N, double* L){	//Return specular color value
	double* speculared_color = malloc(sizeof(double)*3);
	if(N[0]*L[0] + N[1]*L[1] + N[2]*L[2] <= 0 || R[0]*V[0] + R[1]*V[1] + R[2]*V[2] <= 0){
		//If our object normal, and vector from the light to object dot product is 0 or negative,
		//return speculared_colors of {0,0,0}
		speculared_color[0] = 0;
		speculared_color[1] = 0;
		speculared_color[2] = 0;
		return speculared_color;
	}
	//Calculate specular color, with ns value hardcoded to 20
	speculared_color[0] = pow((R[0]*V[0] + R[1]*V[1] + R[2]*V[2]), 20)*Cs[0]*Ci[0];
	speculared_color[1] = pow((R[0]*V[0] + R[1]*V[1] + R[2]*V[2]), 20)*Cs[1]*Ci[1];
	speculared_color[2] = pow((R[0]*V[0] + R[1]*V[1] + R[2]*V[2]), 20)*Cs[2]*Ci[2];
	if(speculared_color[0] < 0) speculared_color[0] = 0;	//Specular color may not be negative
	if(speculared_color[1] < 0) speculared_color[1] = 0;
	if(speculared_color[2] < 0) speculared_color[2] = 0;
	return speculared_color;	//Return specular color
}

double calculate_distance(double* input_vector){	//Calculate the magnitude/distance of the input vector
	return sqrt(sqr(input_vector[0]) + sqr(input_vector[1]) + sqr(input_vector[2]));
}

double* reflect(double* L, double* N){	//Reflect vector L across a normal N
	double* reflect_vector = malloc(sizeof(double)*3);
	reflect_vector[0] = L[0] - 2*((L[0] * N[0]) + (L[1] * N[1]) + (L[2] * N[2])) * N[0];
	reflect_vector[1] = L[1] - 2*((L[0] * N[0]) + (L[1] * N[1]) + (L[2] * N[2])) * N[1];
	reflect_vector[2] = L[2] - 2*((L[0] * N[0]) + (L[1] * N[1]) + (L[2] * N[2])) * N[2];
	return reflect_vector;
}

double* render_light(Object** object_array, int object_counter, double best_t,
					int best_index, double* Ro, double* Rd){	//Calculate color values using lights
	double t = 0;
	int parse_count = 1;
	int parse_count1 = 1;
	double Ron[3];
	double Rdn[3];
	double* color = malloc(sizeof(double)*3);
	double* diffused_color;
	double* speculared_color;
	double N[3];
	double L[3];
	double* R;
	double V[3];
	double distance_from_light;
	
	
	Ron[0] = best_t * Rd[0] + Ro[0];	//Calculate the intersection point of the object we hit
	Ron[1] = best_t * Rd[1] + Ro[1];
	Ron[2] = best_t * Rd[2] + Ro[2];
			
	parse_count = 1;
	parse_count1 = 1;
	
	color[0] = 0;	//Set color value to black (for now)
	color[1] = 0;
	color[2] = 0;
	
	while(parse_count < object_counter + 1){	//Iterate through object array and check for lights
		if(object_array[parse_count]->kind == 3){
			//Create vector pointing to light source, originating from our intersection
			Rdn[0] = object_array[parse_count]->light.position[0] - Ron[0];
			Rdn[1] = object_array[parse_count]->light.position[1] - Ron[1];
			Rdn[2] = object_array[parse_count]->light.position[2] - Ron[2];
			distance_from_light = calculate_distance(Rdn);	//Calculate distance from light to intersection
			normalize(Rdn);	//normalize our object to light vector
			
			while(parse_count1 < object_counter + 1){	//Check to see if our point of intersection is in shadow
				if(parse_count1 == best_index){	//The object we intersected cannot overshadow itself!
					parse_count1++;
					continue;
				}
				if(object_array[parse_count1]->kind == 1){	//See if a sphere overshadows our point of intersection
					t = sphere_intersection(Ron, Rdn, object_array[parse_count1]->sphere.position,
									object_array[parse_count1]->sphere.radius);			
					parse_count1++;
				}else if(object_array[parse_count1]->kind == 2){ //See if a plane overshadows our point of intersection
					t = plane_intersection(Ron, Rdn, object_array[parse_count1]->plane.position,
											object_array[parse_count1]->plane.normal);
					parse_count1++;
				}else{	//If a light was found, skip it
					parse_count1++;
					continue;
				}
				
				if(t > 0 && t < distance_from_light){	//If a valid overshadowing object was found, break
					break;
				}else if(t > 0 && t > distance_from_light){ //If object found behind light, it does not cast a shadow
					t = 0;
				}
			}
			
			L[0] = Rdn[0];	//Store object to light vector into L
			L[1] = Rdn[1];
			L[2] = Rdn[2];
			
			V[0] = Rd[0];	//Store vector pointing from camera to object
			V[1] = Rd[1];
			V[2] = Rd[2];
			
			if(t <= 0){
				if(object_array[best_index]->kind == 1){
					//Find normal of sphere
					N[0] = Ron[0] - object_array[best_index]->sphere.position[0];
					N[1] = Ron[1] - object_array[best_index]->sphere.position[1];
					N[2] = Ron[2] - object_array[best_index]->sphere.position[2];
					normalize(N);
					
					R = reflect(L, N);	//Get reflected vector of L
					
					//Calculate diffuse and specular color
					diffused_color = diffuse(L, N, object_array[best_index]->sphere.diffuse_color,
												object_array[parse_count]->light.color);
					speculared_color = specular(R, V, object_array[best_index]->sphere.specular_color,
												object_array[parse_count]->light.color, N, L);
					
				}else if(object_array[best_index]->kind == 2){
					//Find normal of plane
					N[0] = object_array[best_index]->plane.normal[0];
					N[1] = object_array[best_index]->plane.normal[1];
					N[2] = object_array[best_index]->plane.normal[2];
					
					
					R = reflect(L, N);  //Get reflected vector of L
					
					//Calculate diffuse and specular color
					diffused_color = diffuse(L, N, object_array[best_index]->plane.diffuse_color,
												object_array[parse_count]->light.color);
					speculared_color = specular(R, V, object_array[best_index]->plane.specular_color,
												object_array[parse_count]->light.color, N, L);
					
				}
				else{	//If the current object is somehow a light
					fprintf(stderr,"Error: Tried to render light as a shape primitive");
					exit(1);
				}
				
				//Reverse direction of Rdn to be used in angular attenuation calculations
				Rdn[0] = -Rdn[0];
				Rdn[1] = -Rdn[1];
				Rdn[2] = -Rdn[2];
				//Add total light values together
				color[0] += frad(object_array[parse_count]->light.radial_a0,
								object_array[parse_count]->light.radial_a1,
								object_array[parse_count]->light.radial_a2, best_t) *
								fang(object_array[parse_count]->light.angular_a0,
								object_array[parse_count]->light.theta, Rdn,
								object_array[parse_count]->light.direction) *
								(diffused_color[0] + speculared_color[0]);
				color[1] += frad(object_array[parse_count]->light.radial_a0,
								object_array[parse_count]->light.radial_a1,
								object_array[parse_count]->light.radial_a2, best_t) *
								fang(object_array[parse_count]->light.angular_a0,
								object_array[parse_count]->light.theta, Rdn,
								object_array[parse_count]->light.direction) *
								(diffused_color[1] + speculared_color[1]);
				color[2] += frad(object_array[parse_count]->light.radial_a0,
								object_array[parse_count]->light.radial_a1,
								object_array[parse_count]->light.radial_a2, best_t) *
								fang(object_array[parse_count]->light.angular_a0,
								object_array[parse_count]->light.theta, Rdn,
								object_array[parse_count]->light.direction) *
								(diffused_color[2] + speculared_color[2]);
				free(diffused_color);	//free memory
				free(speculared_color);
				free(R);
			}
			t = 0;
			parse_count1 = 1;
		}
		parse_count++;
	}
	//Clamp color values
	color[0] = clamp(color[0]);
	color[1] = clamp(color[1]);
	color[2] = clamp(color[2]);
	
	return color;
}

void raycast_scene(Object** object_array, int object_counter, double** pixel_buffer, int N, int M){	//This raycasts our object_array
	int parse_count = 0;
	int pixel_count = 0;
	int i;
	double Ro[3];
	double Rd[3];
	double* color;
	double cx = 0;
	double cy = 0;
	double w;
	double h;
	double pixwidth;
	double pixheight;
	double t;
	double best_t = INFINITY;
	int best_index;
	
	if(object_array[parse_count]->kind != 0){	//If camera is not present, throw an error
		fprintf(stderr, "Error: You must have one object of type camera\n");
		exit(1);
	}
	
	//Grab camera width and height, and calculate our pixel widths and pixel heights
	w = object_array[parse_count]->camera.width;
	pixwidth = w/N;
	h = object_array[parse_count]->camera.height;
	pixheight = h/M;
	parse_count++;
	
	//Create origin point for our vector
	Ro[0] = 0;
	Ro[1] = 0;
	Ro[2] = 0;
	
	for(int y = 0; y < M; y += 1){	//Raycast every shape for each pixel
		for(int x = 0; x < N; x += 1){
			Rd[0] = cx - (w/2) + pixwidth * (x + .5);	//Create direction vector
			Rd[1] = cy - (h/2) + pixheight * (y + .5);
			Rd[2] = 1;
			normalize(Rd);
			while(parse_count < object_counter + 1){	//Iterate through object array and test for intersections
				if(object_array[parse_count]->kind == 1){	//If sphere, test for sphere intersections
					t = sphere_intersection(Ro, Rd, object_array[parse_count]->sphere.position,
											object_array[parse_count]->sphere.radius);
				}else if(object_array[parse_count]->kind == 2){	//If plane, test for a plane intersection
					t = plane_intersection(Ro, Rd, object_array[parse_count]->plane.position,
											object_array[parse_count]->plane.normal);
				}else{
					parse_count++;
					continue;
				}
				
				if(t < best_t && t > 0){	//Store object index with the closest intersection
					best_t = t;
					best_index = parse_count;
				}
				parse_count++;
			}

			
			if(best_t > 0 && best_t != INFINITY){	//If our closest intersection is valid...
				//render light, and store the outputted colors into our pixel array
				color = render_light(object_array, object_counter, best_t, best_index, Ro, Rd);
				pixel_buffer[(int)((N*M) - (floor(pixel_count/N) + 1)*N)+ pixel_count%N][0] = color[0];
				pixel_buffer[(int)((N*M) - (floor(pixel_count/N) + 1)*N)+ pixel_count%N][1] = color[1];
				pixel_buffer[(int)((N*M) - (floor(pixel_count/N) + 1)*N)+ pixel_count%N][2] = color[2];
				
				free(color);
				parse_count = 1;
			}else{
				parse_count = 1;
			}
			pixel_count++;
			best_t = INFINITY;
		}
	}
}

void create_image(double** pixel_buffer, char* output, int width, int height){	//Stores pixel array info into a .ppm file
	FILE *output_pointer = fopen(output, "wb");	/*Open the output file*/
	char buffer[width*height*3];
	int counter = 0;
	
	while(counter < width*height){	//Iterate through pixel array, and store values into a character buffer
		buffer[counter*3] = (int)(255*pixel_buffer[counter][0]);
		buffer[counter*3 + 1] = (int)(255*pixel_buffer[counter][1]);
		buffer[counter*3 + 2] = (int)(255*pixel_buffer[counter][2]);
		counter++;
	}
	fprintf(output_pointer, "P6\n%d %d\n255\n", width, height);	//Write P6 header to output.ppm
	fwrite(buffer, sizeof(char), width*height*3, output_pointer);	//Write buffer to output.ppm
	
	fclose(output_pointer);
}

void move_camera_to_front(Object** object_array, int object_count){	//Moves camera object to the front of object_array
	Object* temp_object;
	int counter = 0;
	int num_cameras = 0;
	while(counter < object_count + 1){	//Iterate through all objects in object_array
		if(object_array[counter]->kind == 0 && counter == 0){	//If first object is a camera, do nothing
			num_cameras++;
		}else if(object_array[counter]->kind == 0){		//If a camera is found further in the array, switch first object with it
			if((++num_cameras) > 1){	//But, if two cameras are ever found, throw an error
				fprintf(stderr, "Error: You may only have one camera in your .json file\n");
				exit(1);
			}
			temp_object = object_array[0];
			object_array[0] = object_array[counter];
			object_array[counter] = temp_object;
		}
		counter++;
	}
}

int main(int c, char** argv) {	//This recieves our input.json and runs functions on it to create an output.ppm
	Object** object_array = malloc(sizeof(Object*)*130);	//Create array of object pointers
	int width;
	int height;
	double** pixel_buffer;
	int object_counter;
	int counter = 0;
	object_array[129] = NULL;	//Indicate end of object pointer array with a NULL
	
	argument_checker(c, argv);	//Check our arguments to make sure they written correctly
	
	width = atoi(argv[1]);
	height = atoi(argv[2]);
	
	pixel_buffer = malloc(sizeof(double*)*(width*height + 1));	//Create our pixel array to hold color values
	pixel_buffer[width*height] = NULL;
	
	while(counter < width*height){	//Reserve space for each pixel in our pixel array
		pixel_buffer[counter] = malloc(sizeof(double)*3);
		pixel_buffer[counter][0] = 0;
		pixel_buffer[counter][1] = 0;
		pixel_buffer[counter][2] = 0;
		counter++;
	}
	
	object_counter = read_scene(argv[3], object_array);	//Parse .json scene file
	move_camera_to_front(object_array, object_counter);	//Make camera the first object in our object array
	raycast_scene(object_array, object_counter, pixel_buffer, width, height);	//Raycast our scene into the pixel array
	create_image(pixel_buffer, argv[4], width, height);	//Put info from pixel array into a P6 PPM file
	
	return 0;
}