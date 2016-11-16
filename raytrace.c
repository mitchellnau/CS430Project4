/* Project 4 *
 * Mitchell Hewitt*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "3dmath.h"
#define MAX_RECURSION 7

//data type to store pixel rgb values
typedef struct Pixel
{
    unsigned char r, g, b;
} Pixel;

//data type to store objects
typedef struct
{
    int kind; // 0 = camera, 1 = sphere, 2 = plane
    //double reflectivity;
    //double refractivity;
    //double ior;
    double diffuse_color[3];
    double specular_color[3];
    union
    {
        struct
        {
            double center[3];
            double width;
            double height;
        } camera;
        struct
        {
            double center[3];
            double radius;
            double reflectivity;
            double refractivity;
            double ior;
        } sphere;
        struct
        {
            double center[3];
            double normal[3];
        } plane;
    };
} Object;

//data type to store lights
typedef struct
{
    int kind; // 0 = radial, 1 = spotlight
    double color[3];
    double position[3];
    double radial_a2;
    double radial_a1;
    double radial_a0;
    double theta;
    union
    {
        struct
        {
        } radial;
        struct
        {
            double angular_a0;
            double direction[3];
        } spotlight;
    };
} Light;

FILE* outputfp;
int pwidth, pheight, maxcv; //global variables to store p3 header information
int line = 1;               //global variable to store line of json file currently being parsed
int ns = 20;                //global variable to store phong reflectivity

//this function clamps the input value between 0 and 1
double clamp(double input)
{
    if(input < 0.0) return 0.0;
    else if (input > 1.0) return 1.0;
    else return input;
}
//this function calculates the amount of radial attenuation of a light for a given coordinate
//based off of the parameters, calculating the distance between where the camera intersects the relevant object
//and the light's position, then calculating the value to return based off of that and the passed in radial values
double frad(double a0, double a1, double a2, double t, double* Ro, double* Rd, double* pos)
{
    if(t == INFINITY) return 1.0;
    else
    {
        double result, d;
        double intersection[3] = {0, 0, 0};
        double tRd[3] = {0, 0, 0};
        v3_scale(Rd, t, tRd);
        v3_add(Ro, tRd, intersection);
        d = sqrt(pow(pos[0]-intersection[0], 2) + pow(pos[1]-intersection[1], 2) + pow(pos[2]-intersection[2], 2));
        result = 1/(a2*(d*d) + a1*d + a0);
        return result;
    }
}

//this function calculates the amount of angular attenuation of a spotlight for a given coordinate
//returns 1.0 if the input light was a radial light and 0.0 if the coordinate falls outside of the spotlight's cone.
//If the coordinate falls inside the cone, the amount to color by is returned
double fang(int kind, double theta, double* vlight, double* vobject, double angular_a0)
{
    normalize(vlight);
    if(kind != 1) return 1.0;
    else if(v3_dot(vobject, vlight) < cos(theta*(M_PI/180))) return 0.0;
    else
    {
        return pow(v3_dot(vobject, vlight),angular_a0);
    }
    return 1.0;
}

//This function writes data from the pixel buffer passed into the function to the output file in ascii.
int write_p3(Pixel* image)
{
    fprintf(outputfp, "%c%c\n", 'P', '3'); //write out the file header P type
    fprintf(outputfp, "%d %d\n", pwidth, pheight); //write the width and the height
    fprintf(outputfp, "%d\n", maxcv); //write the max color value
    int i;
    for(i = 0; i < pwidth*pheight; i++)   //write each pixel in the image to the output file
    {
        fprintf(outputfp, "%d\n%d\n%d\n", image[i*sizeof(Pixel)].r, //in ascii
                image[i*sizeof(Pixel)].g,
                image[i*sizeof(Pixel)].b);
    }
    return 1;
}

// next_c() wraps the getc() function and provides error checking and line
// number maintenance
int next_c(FILE* json)
{
    int c = fgetc(json);
#ifdef DEBUG
    printf("next_c: '%c'\n", c);
#endif
    if (c == '\n')
    {
        line += 1;
    }
    if (c == EOF)
    {
        fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
        exit(1);
    }
    return c;
}


// expect_c() checks that the next character is d.  If it is not it emits
// an error.
void expect_c(FILE* json, int d)
{
    int c = next_c(json);
    if (c == d) return;
    fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
    exit(1);
}


// skip_ws() skips white space in the file.
void skip_ws(FILE* json)
{
    int c = next_c(json);
    while (isspace(c))
    {
        c = next_c(json);
    }
    ungetc(c, json);
}


// next_string() gets the next string from the file handle and emits an error
// if a string can not be obtained.
char* next_string(FILE* json)
{
    char buffer[129];
    int c = next_c(json);
    if (c != '"')
    {
        fprintf(stderr, "Error: Expected string on line %d.\n", line);
        exit(1);
    }
    c = next_c(json);
    int i = 0;
    while (c != '"')
    {
        if (i >= 128)
        {
            fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported.\n");
            exit(1);
        }
        if (c == '\\')
        {
            fprintf(stderr, "Error: Strings with escape codes are not supported.\n");
            exit(1);
        }
        if (c < 32 || c > 126)
        {
            fprintf(stderr, "Error: Strings may contain only ascii characters.\n");
            exit(1);
        }
        buffer[i] = c;
        i += 1;
        c = next_c(json);
    }
    buffer[i] = 0;
    char* returnString = malloc(sizeof(buffer));
    strcpy(returnString, buffer);
    return returnString;
}

//This function reads the next number in the input json file and returns that number
//if it was indeed a number.  If a number was not found, it exits the program with an error.
double next_number(FILE* json)
{
    double value;
    int f = fscanf(json, "%lf", &value);
    if (f == 1) return value;
    fprintf(stderr, "Error: Expected number on line %d.\n", line);
    exit(1);
}

//this function reads a three dimensional vector from the input json file.
//Its error handling is inside the expect_c and next_number functions.
//It expects a three dimensional vector which is bookended by brackets where
//each value of the vector is a number and each number is separated by a comma.
double* next_vector(FILE* json)
{
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

//this function takes in a json file and memory to store objects from the file.
//After successfully parsing the json file, it will have stored all objects in
//the json file into the memory passed into the function and will return the
//number of objects it found.
int* read_scene(char* filename, Object* objects, Light* lights)
{
    int c;
    FILE* json = fopen(filename, "r");

    if (json == NULL)
    {
        fprintf(stderr, "Error: Could not open file \"%s\"\n", filename);
        exit(1);
    }

    skip_ws(json);
    // Find the beginning of the list
    expect_c(json, '[');
    skip_ws(json);
    // Find the objects
    expect_c(json, '{');
    ungetc('{', json);
    int i = 0;
    int j = 0;
    while (1)
    {
        c = fgetc(json);
        if (c == ']')          //if the list is empty, the file contains no objects
        {
            fprintf(stderr, "Error: Scene file contains no objects.\n");
            fclose(json);
            return -1;
        }
        if (c == '{')         //if an object is found
        {
            skip_ws(json);
            Object temp;      //temporary variable to store the object
            Light templight;

            //default values for reflectivity, refractivity, and index of refraction
            temp.sphere.reflectivity = 0;
            temp.sphere.refractivity = 0;
            temp.sphere.ior = 1;

            int obj_or_light = 0;

            // Parse the object
            char* key = next_string(json);
            if (strcmp(key, "type") != 0) //object type is the first key of an object expected
            {
                fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
                exit(1);
            }

            skip_ws(json);
            expect_c(json, ':');        //colon separates key from value in each key-value pair
            skip_ws(json);
            char* value = next_string(json);

            if (strcmp(value, "camera") == 0)
            {
                temp.kind = 0;         //remember that this object is a camera in the temporary Object
            }
            else if (strcmp(value, "sphere") == 0)
            {
                temp.kind = 1;         //remember that this object is a sphere in the temporary Object
            }
            else if (strcmp(value, "plane") == 0)
            {
                temp.kind = 2;         //remember that this object is a plane in the temporary Object
            }
            else if (strcmp(value, "light") == 0)
            {
                obj_or_light = 1;     //remember that a light was found
            }
            else                       //if a non-camera/sphere/plane/light was found as the the type, print an error message and exit
            {
                fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
                exit(1);
            }

            skip_ws(json);

            //attribute counters for error checking
            int w_attribute_counter = 0;
            int h_attribute_counter = 0;
            int r_attribute_counter = 0;
            int ra0_attribute_counter = 0;
            int ra1_attribute_counter = 0;
            int ra2_attribute_counter = 0;
            int aa0_attribute_counter = 0;
            int t_attribute_counter = 0;

            int dc_attribute_counter = 0;
            int sc_attribute_counter = 0;
            int p_attribute_counter = 0;
            int n_attribute_counter = 0;
            int c_attribute_counter = 0;
            int d_attribute_counter = 0;

            while (1)         //this loop gets each attribute of an object
            {
                // , }
                c = next_c(json);
                if (c == '}') //curly brace means there are no object properties so break out of the loop
                {
                    // stop parsing this object
                    break;
                }
                else if (c == ',') //there is another object property to be read
                {
                    // read another field
                    skip_ws(json);
                    char* key = next_string(json); //get the key of the property
                    skip_ws(json);
                    expect_c(json, ':');           //key-value pair is separated by a colon
                    skip_ws(json);
                    if ((strcmp(key, "width") == 0) ||    //if the key denotes a decimal number
                            (strcmp(key, "height") == 0) ||
                            (strcmp(key, "radius") == 0) ||
                            (strcmp(key, "radial-a2") == 0) ||
                            (strcmp(key, "radial-a1") == 0) ||
                            (strcmp(key, "radial-a0") == 0) ||
                            (strcmp(key, "angular-a0") == 0) ||
                            (strcmp(key, "theta") == 0) ||
                            (strcmp(key, "reflectivity") == 0) ||
                            (strcmp(key, "refractivity") == 0) ||
                            (strcmp(key, "ior") == 0))
                    {
                        double value = next_number(json); //get the decimal number and store it in the relevant struct field
                        if(obj_or_light == 1)
                        {
                            if((strcmp(key, "radial-a2") == 0))
                            {
                                if(value < 0)
                                {
                                    fprintf(stderr, "Error: radial-a2 cannot be negative, \"%f\", on line number %d.\n", value, line);
                                    exit(1);
                                }
                                templight.radial_a2 = value;
                                ra2_attribute_counter++;
                            }
                            else if((strcmp(key, "radial-a1") == 0))
                            {
                                if(value < 0)
                                {
                                    fprintf(stderr, "Error: radial-a1 cannot be negative, \"%f\", on line number %d.\n", value, line);
                                    exit(1);
                                }
                                templight.radial_a1 = value;
                                ra1_attribute_counter++;
                            }
                            else if((strcmp(key, "radial-a0") == 0))
                            {
                                if(value < 0)
                                {
                                    fprintf(stderr, "Error: radial-a0 cannot be negative, \"%f\", on line number %d.\n", value, line);
                                    exit(1);
                                }
                                templight.radial_a0 = value;
                                ra0_attribute_counter++;
                            }
                            else if((strcmp(key, "angular-a0") == 0))
                            {
                                if(value < 0)
                                {
                                    fprintf(stderr, "Error: angular-a0 cannot be negative, \"%f\", on line number %d.\n", value, line);
                                    exit(1);
                                }
                                templight.kind = 1;
                                templight.spotlight.angular_a0 = value;
                                aa0_attribute_counter++;
                            }
                            else if((strcmp(key, "theta") == 0))
                            {
                                if(value != 0) templight.kind = 1;
                                templight.theta = value;
                                t_attribute_counter++;
                            }
                            else
                            {
                                fprintf(stderr, "Error: Unknown property, \"%s\", on line number %d.\n", key, line);
                                exit(1);
                            }
                        }
                        else if(temp.kind == 0)
                        {
                            if((strcmp(key, "width") == 0))
                            {
                                temp.camera.width = value;
                                w_attribute_counter++;
                            }
                            else if(strcmp(key, "height") == 0)
                            {
                                temp.camera.height = value;
                                h_attribute_counter++;
                            }
                            else
                            {
                                fprintf(stderr, "Error: Camera object has unexpected attribute '%s' on line number %d.\n", key, line);
                                exit(1);
                            }
                            //default camera position
                            temp.camera.center[0] = 0.0;
                            temp.camera.center[1] = 0.0;
                            temp.camera.center[2] = 0.0;
                        }
                        else if(temp.kind == 1 && (strcmp(key, "radius") == 0))
                        {
                            temp.sphere.radius = value;
                            r_attribute_counter++;
                            if(value <= 0)           //a sphere cannot have a radius of 0 or less so print an error and exit
                            {
                                fprintf(stderr, "Error: Sphere radius cannot be less than or equal to 0 on line %d.\n", line);
                                exit(1);
                            }
                        }
                        else if(temp.kind == 1)
                        {
                            if((strcmp(key, "reflectivity") == 0))
                            {
                                temp.sphere.reflectivity = value;
                                //w_attribute_counter++;
                            }
                            else if((strcmp(key, "refractivity") == 0))
                            {
                                temp.sphere.refractivity = value;
                                //w_attribute_counter++;
                            }
                            else if((strcmp(key, "ior") == 0))
                            {
                                temp.sphere.ior = value;
                                //w_attribute_counter++;
                            }
                            else
                            {
                                fprintf(stderr, "Error: Unexpected sphere attribute %s on line %d.\n", key, line);
                                exit(1);
                            }
                        }
                        else if(temp.kind == 2)
                        {
                            if((strcmp(key, "reflectivity") == 0))
                            {
                                temp.sphere.reflectivity = value;
                                //w_attribute_counter++;
                            }
                            else if((strcmp(key, "refractivity") == 0))
                            {
                                temp.sphere.refractivity = value;
                                //w_attribute_counter++;
                            }
                            else if((strcmp(key, "ior") == 0))
                            {
                                temp.sphere.ior = value;
                                //w_attribute_counter++;
                            }
                            else
                            {
                                fprintf(stderr, "Error: Unexpected plane attribute %s on line %d.\n", key, line);
                                exit(1);
                            }
                        }
                        else
                        {
                            fprintf(stderr, "Error: Unexpected attribute '%s' on line number %d.\n", key, line);
                            exit(1);
                        }
                    }
                    else if ((strcmp(key, "diffuse_color") == 0) || //if the key denotes a vector
                             (strcmp(key, "specular_color") == 0) ||
                             (strcmp(key, "position") == 0) ||
                             (strcmp(key, "normal") == 0) ||
                             (strcmp(key, "color") == 0) ||
                             (strcmp(key, "direction") == 0))
                    {
                        double* value = next_vector(json); //get the vector and store it in the relevant struct field
                        if(strcmp(key, "diffuse_color") == 0)
                        {
                            if(value[0] > 1 || value[0] < 0 || //color values must be between 0 and 1
                                    value[1] > 1 || value[1] < 0 || //an error is printed and and the program exits otherwise.
                                    value[2] > 1 || value[2] < 0 )
                            {
                                fprintf(stderr, "Error: Color value is not 0.0 to 1.0 on line number %d.\n", line);
                                exit(1);
                            }
                        }
                        if(obj_or_light == 1) //if the current thing being parsed is a light
                        {
                            if((strcmp(key, "color") == 0))
                            {
                                templight.color[0] = value[0];
                                templight.color[1] = value[1];
                                templight.color[2] = value[2];
                                c_attribute_counter++;
                            }
                            else if((strcmp(key, "position") == 0))
                            {
                                templight.position[0] = value[0];
                                templight.position[1] = value[1];
                                templight.position[2] = value[2];
                                p_attribute_counter++;
                            }
                            else if((strcmp(key, "direction") == 0))
                            {
                                templight.kind = 1;
                                templight.spotlight.direction[0] = value[0];
                                templight.spotlight.direction[1] = value[1];
                                templight.spotlight.direction[2] = value[2];
                                d_attribute_counter++;
                            }
                            else
                            {
                                fprintf(stderr, "Error: Unknown property, \"%s\", on line number %d.\n", key, line);
                                exit(1);
                            }
                        }
                        else if (temp.kind == 0 && (strcmp(key, "position") == 0))
                        {
                            temp.camera.center[0] = value[0];
                            temp.camera.center[1] = value[1];
                            temp.camera.center[2] = value[2];
                            p_attribute_counter++;
                        }
                        else if(temp.kind == 1)
                        {
                            if(strcmp(key, "diffuse_color") == 0)
                            {
                                temp.diffuse_color[0] = value[0];
                                temp.diffuse_color[1] = value[1];
                                temp.diffuse_color[2] = value[2];
                                dc_attribute_counter++;
                            }
                            else if(strcmp(key, "specular_color") == 0)
                            {
                                temp.specular_color[0] = value[0];
                                temp.specular_color[1] = value[1];
                                temp.specular_color[2] = value[2];
                                sc_attribute_counter++;
                            }
                            else if(strcmp(key, "position") == 0)
                            {
                                temp.sphere.center[0] = value[0];
                                temp.sphere.center[1] = value[1];
                                temp.sphere.center[2] = value[2];
                                p_attribute_counter++;
                            }
                            else
                            {
                                fprintf(stderr, "Error: Unknown property, \"%s\", on line number %d.\n", key, line);
                                exit(1);
                            }
                        }
                        else if(temp.kind == 2)
                        {
                            if(strcmp(key, "diffuse_color") == 0)
                            {
                                temp.diffuse_color[0] = value[0];
                                temp.diffuse_color[1] = value[1];
                                temp.diffuse_color[2] = value[2];
                                dc_attribute_counter++;
                            }
                            else if(strcmp(key, "specular_color") == 0)
                            {
                                temp.specular_color[0] = value[0];
                                temp.specular_color[1] = value[1];
                                temp.specular_color[2] = value[2];
                                sc_attribute_counter++;
                            }
                            else if(temp.kind == 2 && (strcmp(key, "position") == 0))
                            {
                                temp.plane.center[0] = value[0];
                                temp.plane.center[1] = -value[1];
                                temp.plane.center[2] = value[2];
                                p_attribute_counter++;
                            }
                            else if(temp.kind == 2 && (strcmp(key, "normal") == 0))
                            {
                                temp.plane.normal[0] = value[0];
                                temp.plane.normal[1] = value[1];
                                temp.plane.normal[2] = value[2];
                                n_attribute_counter++;
                            }
                            else
                            {
                                fprintf(stderr, "Error: Unknown property, \"%s\", on line number %d.\n", key, line);
                                exit(1);
                            }
                        }
                        else
                        {
                            if(temp.kind == 0) //if the camera has a vector property that is not a position, print an error and exit
                            {
                                fprintf(stderr, "Error: Camera object has non-position attribute on line %d.\n", line);
                                exit(1);
                            }
                            else if(temp.kind == 1) //if the sphere has a vector property that is not a color or position
                            {
                                fprintf(stderr, "Error: Sphere object has non-color/position attribute on line %d.\n", line);
                                exit(1);
                            }
                            else //if the plane has a vector property that is not a color or position or normal, print an error and exit
                            {
                                fprintf(stderr, "Error: Plane object has non-position/color/normal attribute on line %d.\n", line);
                                exit(1);
                            }
                        }
                    }
                    else //if the input property is unknown, tell the user that property is unknown and exit
                    {
                        fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n",
                                key, line);
                        //char* value = next_string(json);
                    }
                    skip_ws(json);
                }
                else //if junk was found in the file tell the user where it was found
                {
                    fprintf(stderr, "Error: Unexpected value on line %d\n", line);
                    exit(1);
                }
            }
            //error checking for duplicate or missing object attributes
            if(obj_or_light == 0 && temp.kind == 0 && (h_attribute_counter != 1 || w_attribute_counter != 1 || dc_attribute_counter != 0 ||
                    sc_attribute_counter != 0 || n_attribute_counter != 0 || r_attribute_counter != 0))
            {
                fprintf(stderr, "Error: Expecting unique width, height, (or additionally position) attributes for camera object on line %d.\n", line);
                exit(1);
            }
            if(obj_or_light == 0 && temp.kind == 1 && (dc_attribute_counter != 1 || sc_attribute_counter != 1 || r_attribute_counter != 1 ||
                    p_attribute_counter != 1 || h_attribute_counter != 0 || w_attribute_counter != 0 ||
                    n_attribute_counter != 0))
            {
                fprintf(stderr, "Error: Expecting unique color, position, or radius attributes for sphere object on line %d.\n", line);
                exit(1);
            }
            if(obj_or_light == 0 && temp.kind == 2 && (dc_attribute_counter != 1 || sc_attribute_counter != 1 || p_attribute_counter != 1 ||
                    n_attribute_counter != 1  || h_attribute_counter != 0 || w_attribute_counter != 0 ||
                    r_attribute_counter != 0))
            {
                fprintf(stderr, "Error: Expecting unique color, position, or normal attributes for plane object on line %d.\n", line);
                exit(1);
            }
            //error checking for duplicate or missing light attributes
            if(obj_or_light == 1 && templight.kind == 0 && (c_attribute_counter != 1 || ra2_attribute_counter != 1 || ra1_attribute_counter != 1 ||
                    ra0_attribute_counter != 1  || p_attribute_counter != 1 || d_attribute_counter != 0 ||
                    aa0_attribute_counter != 0))
            {
                fprintf(stderr, "Error: Expecting unique color, ra2, ra1, ra0, position properties for light on line %d.\n", line);
                exit(1);
            }
            if(obj_or_light == 1 && templight.kind == 1 && (c_attribute_counter != 1 || ra2_attribute_counter != 1 || ra1_attribute_counter != 1 ||
                    ra0_attribute_counter != 1  || p_attribute_counter != 1 || d_attribute_counter != 1 ||
                    aa0_attribute_counter != 1 || t_attribute_counter != 1))
            {
                fprintf(stderr, "Error: Expecting unique color, ra2, ra1, ra0, position, aa0, direction, theta for light on line %d.\n", line);
                exit(1);
            }
            skip_ws(json);
            c = next_c(json);

            //if the current thing being parsed is not a light, store it object data
            if(obj_or_light == 0)
            {
                *(objects+i*sizeof(Object)) = temp; //allocate the temporary object into a struct of objects at its corresponding position
                i++; //and increment the index of the current object for the memory that holds the object structs
            }
            else //otherwise store it in light data
            {
                *(lights+j*sizeof(Light)) = templight; //allocate the temporary light into a struct of lights at its corresponding position
                j++; //and increment the index of the current light for the memory that holds the light structs
            }



            if (c == ',') //if there is another object to be parsed
            {
                skip_ws(json);
                char d = next_c(json);
                if(d != '{')  //if there is another object to be parsed, the next char should be a curly brace, and if not print an error and exit
                {
                    fprintf(stderr, "Error: Expecting '{' on line %d.\n", line);
                    exit(1);
                }
                ungetc(d, json); //if the next char was a curly brace, unget it
            }
            else if (c == ']') //if there are no more objects to be parsed, close the file and return the number of objects
            {
                fclose(json);
                int* numObjLts = malloc(sizeof(int)*2);
                numObjLts[0] = i;
                numObjLts[1] = j;
                return numObjLts;
            }
            else //if a list separator or list terminator was not found, print an error and exit
            {
                fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
                exit(1);
            }
        }
    }
}

//this function calculates the t-value that the input ray intersects with an object
//based on the sphere's center position and radius that are each passed into the function.
double sphere_intersection(double* Ro, double* Rd,
                           double* C, double r)
{
    double a = (sqr(Rd[0]) + sqr(Rd[1]) + sqr(Rd[2]));
    double b = (2 * (Ro[0] * Rd[0] - Rd[0] * C[0] + Ro[1] * Rd[1] - Rd[1] * C[1] + Ro[2] * Rd[2] - Rd[2] * C[2]));
    double c = sqr(Ro[0]) - 2*Ro[0]*C[0] + sqr(C[0]) + sqr(Ro[1]) - 2*Ro[1]*C[1] + sqr(C[1]) + sqr(Ro[2]) - 2*Ro[2]*C[2] + sqr(C[2]) - sqr(r);

    double det = sqr(b) - 4 * a * c; //use a b and c to calculate the determinant
    if (det < 0) return -1;          //returns -1 if a number not in the viewplane was calculated

    det = sqrt(det);

    double t0 = (-b - det) / (2*a); //find the first t value, which is smaller
    if (t0 > 0) return t0;          //return it if it is positive

    double t1 = (-b + det) / (2*a); //find the larger second t value
    if (t1 > 0) return t1;          //return it if it is positive

    return -1;                      //return -1 if there are no positive points of intersection
}

//this function calculates the t-value of the intersection of an input ray with a plane.
//based on the plane's center and normal values.
double plane_intersection(double* Ro, double* Rd,
                          double* C, double* N)
{
    double t, d;
    //t = -(AX0 + BY0 + CZ0 + D) / (AXd + BYd + CZd);
    //D = distance from the origin to the plane
    d = sqrt(sqr(C[0]-Ro[0])+sqr(C[1]-Ro[1])+sqr(C[2]-Ro[2])); //calculate the d
    t = -(N[0]*Ro[0] + N[1]*Ro[1] + N[2]*Ro[2] + d) / (N[0]*Rd[0] + N[1]*Rd[1] + N[2]*Rd[2]); //calculate the t where the ray intersects the plane
    return t;
}


double* shoot(double* Ro, double* Rd, double best_t, int best_object, int numOfObjects, Object* objects, int extra, int closest_extra)
{
    double t = 0;

    int i;
    for (i=0; i < numOfObjects; i += 1)
    {
        t = 0;

        if (i == closest_extra && extra != 0) continue;

        switch(objects[i*sizeof(Object)].kind)
        {
        case 0: //camera has no physical intersections
            break;
        case 1: //if the object is a sphere, find its minimum intersection
            t = sphere_intersection(Ro, Rd,
                                    objects[i*sizeof(Object)].sphere.center,
                                    objects[i*sizeof(Object)].sphere.radius);
            break;
        case 2: //if the object is a plane, find its point of intersection
            t = plane_intersection(Ro, Rd,
                                   objects[i*sizeof(Object)].plane.center,
                                   objects[i*sizeof(Object)].plane.normal);
            break;
        default:
            fprintf(stderr, "Error: Forbidden object struct type located in memory, intersection could not be calculated.\n");
            exit(1);
        }
        if (t > extra && extra != 0)
        {
            continue;
        }
        if (t > 0 && t < best_t) //if an object is in front of another object, ensure the front-most object is displayed
        {
            best_t = t;
            best_object = i;
        }
    }
    double* returnVals = malloc(sizeof(double)*2);
    returnVals[0] = best_t;
    returnVals[1] = (double)best_object;
    return returnVals;
}

/*color shade(objectid o, position x, vector ur , int level)
{
  if(level > max recursion level) return black;
  else{
        um = reflection_vector(x, o, ur);
        (om,t) = shoot(x,um);
        if(t== INFINITY)
            color = background_color;
        else{
            m_color = shade(om,x+t*um,um, level + 1);
            color = directshade(o,x,ur, m_color, -um);
            }
        for(each light i in the scene)
        {
            if(light i is visible fromx)
            color += directshade(o,x,ur, light[i].color, light[i].direction);
        }
        return color;
  }
}*/


double* shade(double best_t, int best_object, int numOfObjects, Object* objects, int numOfLights, Light* lights,  double* Ro, double* Rd, int level)
{
    double color[3] = {0,0,0}; //ambient lighting is 0
    if(level > MAX_RECURSION){
        double* returnVal = malloc(sizeof(double)*3);
        returnVal[0] = color[0];
        returnVal[1] = color[1];
        returnVal[2] = color[2];
        return returnVal;
    }
    else{
        //Ron = best_t * Rd + Ro;
        double Ron[3] = {0, 0, 0};
        double test[3] = {0, 0, 0};
        v3_scale(Rd, best_t, test);
        v3_add(test, Ro, Ron);

        int j;
        for (j=0; j < numOfLights; j+=1)
        {
            // Shadow test
            double Rdn[3] = {0, 0, 0};
            //Rdn = light_position - Ron;
            v3_subtract(lights[j*sizeof(Light)].position, Ron, Rdn);
            double best_lobjt = INFINITY; //find the minimum best t intersection of any object
            int closest_shadow_object = -1; //keep track of the corresponding object's index
            double distance_to_light = sqrt(sqr(Rdn[0]) + sqr(Rdn[1]) + sqr(Rdn[2]));
            normalize(Rdn);

            //find the closest object to the shadow for shadow omission
            double* ricochet2 = shoot(&Ron, &Rdn, best_lobjt, closest_shadow_object, numOfObjects, &objects[0], distance_to_light, best_object);

            best_lobjt = ricochet2[0];
            closest_shadow_object = (int)ricochet2[1];


            if (closest_shadow_object == -1)
            {

                // N, L, R, V
                double n[3] = {0, 0, 0};
                double l[3] = {0, 0, 0};
                double r[3] = {0, 0, 0};
                double v[3] = {0, 0, 0};
                double diffuse[3] = {0, 0, 0};
                double specular[3] = {0, 0, 0};


                //N = closest_object->normal; // plane
                //N = Ron - closest_object->center; // sphere
                if(objects[best_object*sizeof(Object)].kind  == 0)
                {
                    //camera found, do nothing
                }
                else if(objects[best_object*sizeof(Object)].kind  == 1)
                {
                    v3_subtract(Ron, objects[best_object*sizeof(Object)].sphere.center, n);
                }
                else if(objects[best_object*sizeof(Object)].kind  == 2)
                {
                    v3_scale(objects[best_object*sizeof(Object)].plane.normal, 1.0, n);
                }
                else
                {
                    fprintf(stderr, "Error: Unexpected object struct type located in memory, N could not be calculated.\n");
                    exit(1);
                }
                normalize(n);

                //L = Rdn; // light_position - Ron;
                v3_scale(Rdn, 1.0, l);
                normalize(l);

                //R = reflection of L = (2N dot L)N - L;
                double res[3] = {0, 0, 0};
                double scaleFactor = 0.0;
                v3_scale(n, 2.0, res); //2N
                scaleFactor = v3_dot(res, l); //(2n dot L)
                v3_scale(n, scaleFactor, res); //(2n dot L)N
                v3_subtract(res, l, r); //(2N dot L)N - L = R


                //V = Rd;
                v3_scale(Rd, -1.0, v);

                //calculates the diffuse light on an object based off of the equation
                //Ksubd * IsubL * (N dot L) only if N dot L is greater than 0
                double ndotl = v3_dot(n, l);
                if(ndotl <= 0)
                {
                    ndotl = 0;
                }

                diffuse[0] = ndotl*objects[best_object*sizeof(Object)].diffuse_color[0]*lights[j*sizeof(Light)].color[0];
                diffuse[1] = ndotl*objects[best_object*sizeof(Object)].diffuse_color[1]*lights[j*sizeof(Light)].color[1];
                diffuse[2] = ndotl*objects[best_object*sizeof(Object)].diffuse_color[2]*lights[j*sizeof(Light)].color[2];

                //calculates the specular light on an object based off of the equation
                //Ksubs * IsubL * (V dot R)^ns only if N dot L and V dot R are greater than 0
                double vdotr = v3_dot(v, r);
                if(vdotr <= 0)
                {
                    vdotr = 0;
                }

                if(vdotr > 0 && ndotl > 0)
                {
                    specular[0] = pow(vdotr, ns)*objects[best_object*sizeof(Object)].specular_color[0]*lights[j*sizeof(Light)].color[0];
                    specular[1] = pow(vdotr, ns)*objects[best_object*sizeof(Object)].specular_color[1]*lights[j*sizeof(Light)].color[1];
                    specular[2] = pow(vdotr, ns)*objects[best_object*sizeof(Object)].specular_color[2]*lights[j*sizeof(Light)].color[2];
                }

                double angular_a0;
                double light_dir[3] = {0,0,0};

                //get the light's direction if it has one so that it can be passed into fang
                if(lights[j*sizeof(Light)].kind == 1)
                {
                    light_dir[0] = lights[j*sizeof(Light)].spotlight.direction[0];
                    light_dir[1] = lights[j*sizeof(Light)].spotlight.direction[1];
                    light_dir[2] = lights[j*sizeof(Light)].spotlight.direction[2];
                    angular_a0 = lights[j*sizeof(Light)].spotlight.angular_a0;
                }

                //get vobject so it can be passed into fang
                double vobject[3] = {0, 0, 0};
                v3_scale(Rdn, -1, vobject);
                normalize(vobject);

                //summation of all lights' effect on a given coordinate
                color[0] += fang(lights[j*sizeof(Light)].kind,
                                 lights[j*sizeof(Light)].theta,
                                 light_dir, vobject,
                                 lights[j*sizeof(Light)].spotlight.angular_a0)
                            *frad(lights[j*sizeof(Light)].radial_a0,
                                  lights[j*sizeof(Light)].radial_a1,
                                  lights[j*sizeof(Light)].radial_a2,
                                  best_t, Ro, Rd,
                                  lights[j*sizeof(Light)].position)*(diffuse[0] + specular[0]); //frad() * fang() * (diffuse + specular);
                color[1] += fang(lights[j*sizeof(Light)].kind,
                                 lights[j*sizeof(Light)].theta,
                                 light_dir, vobject,
                                 lights[j*sizeof(Light)].spotlight.angular_a0)
                            *frad(lights[j*sizeof(Light)].radial_a0,
                                  lights[j*sizeof(Light)].radial_a1,
                                  lights[j*sizeof(Light)].radial_a2,
                                  best_t, Ro, Rd,
                                  lights[j*sizeof(Light)].position)*(diffuse[1] + specular[1]);//frad() * fang() * (diffuse + specular);
                color[2] += fang(lights[j*sizeof(Light)].kind,
                                 lights[j*sizeof(Light)].theta,
                                 light_dir, vobject,
                                 lights[j*sizeof(Light)].spotlight.angular_a0)
                            *frad(lights[j*sizeof(Light)].radial_a0,
                                  lights[j*sizeof(Light)].radial_a1,
                                  lights[j*sizeof(Light)].radial_a2,
                                  best_t, Ro, Rd,
                                  lights[j*sizeof(Light)].position)*(diffuse[2] + specular[2]);//frad() * fang() * (diffuse + specular);
            }
        }

        double kr = objects[best_object*sizeof(Object)].sphere.reflectivity;
        double kt = objects[best_object*sizeof(Object)].sphere.refractivity;
        double ior = objects[best_object*sizeof(Object)].sphere.ior;

        if(kr != 0 && kt != 0)
        {
            // N, L, R, V
            double n[3] = {0, 0, 0};
            double l[3] = {0, 0, 0};
            double r[3] = {0, 0, 0};
            double v[3] = {0, 0, 0};

            //N = closest_object->normal; // plane
            //N = Ron - closest_object->center; // sphere
            if(objects[best_object*sizeof(Object)].kind  == 0)
            {
                //camera found, do nothing
            }
            else if(objects[best_object*sizeof(Object)].kind  == 1)
            {
                v3_subtract(Ron, objects[best_object*sizeof(Object)].sphere.center, n);
            }
            else if(objects[best_object*sizeof(Object)].kind  == 2)
            {
                v3_scale(objects[best_object*sizeof(Object)].plane.normal, 1.0, n);
            }
            else
            {
                fprintf(stderr, "Error: Unexpected object struct type located in memory, N could not be calculated.\n");
                exit(1);
            }
            normalize(n);

            double reflection[3] = {0,0,0};
            v3_reflect(n, Rd, reflection);
            normalize(reflection);

            double newbest_t = INFINITY; //find the minimum best t intersection of any object
            int newbest_object = -1; //keep track of the corresponding object's index
            double* newricochet = shoot(&Ron, &reflection, newbest_t, newbest_object, numOfObjects, &objects[0], 0, 0);

            newbest_t = newricochet[0];
            newbest_object = (int)newricochet[1];

            double Roprime[3] = {0,0,0};
            v3_scale(Rd, 0.01, Roprime);
            v3_add(Roprime, Ron, Roprime);

            double* reflected_color = shade(newbest_t, newbest_object, numOfObjects, &objects[0], numOfLights, &lights[0], &Roprime, &reflection, level+1);

            if(newbest_t != INFINITY)
            {
                color[0] = (1-kr-kt)*color[0]+kr*reflected_color[0];//+kr*shade(recursive call to shade the reflection vector)+kt*shade(recursive call to shade the refraction vector)
                color[1] = (1-kr-kt)*color[1]+kr*reflected_color[0];
                color[2] = (1-kr-kt)*color[2]+kr*reflected_color[0];
            }
        }

        double* returnVal = malloc(sizeof(double)*3);
        returnVal[0] = color[0];
        returnVal[1] = color[1];
        returnVal[2] = color[2];
        return returnVal;
    }
}


//this function takes in the number of objects and lights in the input json file, memory where those objects and lights are stored,
//and a buffer to store the data of each pixel.  It then uses the camera information to display the intersections
//of raycasts and the objects those raycasts are hitting to store RGB pixel values for that spot of intersection
//as observed by the camera position.  It also illuminates those objects based on the information in the lights buffer.
void store_pixels(int numOfObjects, int numOfLights, Object* objects, Pixel* data, Light* lights)
{
    double cx, cy, h, w;
    cx = 0;  //default camera values
    cy = 0;  // ||
    h = 1;   // ||
    w = 1;   // ||
    int i;
    int found = 0; //tell whether a camera is found or not
    for (i=0; i < numOfObjects; i += 1) //get the first camera's x/y positions and width/height
    {
        if(objects[i*sizeof(Object)].kind == 0)
        {
            w = objects[i*sizeof(Object)].camera.width;
            h = objects[i*sizeof(Object)].camera.height;
            cx = objects[i*sizeof(Object)].camera.center[0];
            cy = objects[i*sizeof(Object)].camera.center[1];
            found = 1;
            break;
        }
    }
    if(found != 1) //if a camera was not found in the list of objects, print an error but continue with default camera values
    {
        fprintf(stderr, "Error: A camera object was not found in the input json file.\n\tUsing default camera position: (%f,%f)\n\tUsing default camera width: %f\n\tUsing default camera height: %f\n", cx, cy, w, h);
    }

    int M = pheight; //M is equal to the input command line height
    int N = pwidth;  //N is equal to the input command line width

    double pixheight = h / M; //pixel height and width of the area to be raycasted
    double pixwidth = w / N;

    int y, x; //loop control variables

    printf("calculating intersections and storing intersection pixels...\n");
    for (y = 0; y < M; y += 1)
    {
        for (x = 0; x < N; x += 1)
        {
            double Ro[3] = {0, 0, 0};
            // Rd = normalize(P - Ro)
            double Rd[3] =
            {
                cx - (w/2) + pixwidth * (x + 0.5),
                cy - (h/2) + pixheight * (y + 0.5),
                1
            };
            normalize(Rd);


            double best_t = INFINITY; //find the minimum best t intersection of any object
            int best_object = -1; //keep track of the corresponding object's index

            double* ricochet = shoot(&Ro, &Rd, best_t, best_object, numOfObjects, &objects[0], 0, 0);

            best_t = ricochet[0];
            best_object = (int)ricochet[1];

            double color[3] = {0,0,0}; //ambient lighting is 0
            double* resultcolor = shade(best_t, best_object, numOfObjects, &objects[0], numOfLights, &lights[0], &Ro, &Rd, 0);
            color[0] = resultcolor[0];
            color[1] = resultcolor[1];
            color[2] = resultcolor[2];


            if (best_t > 0 && best_t != INFINITY) //if the intersection is in the viewplane and isn't infinity, store its object's color into the buffer
            {
                //at the correct x,y location
                //printf("here. x %d\ty %d\n", x, y);
                Pixel temporary;
                temporary.r = (int)(clamp(color[0])*255);
                temporary.g = (int)(clamp(color[1])*255);
                temporary.b = (int)(clamp(color[2])*255);
                *(data+(sizeof(Pixel)*pheight*pwidth)-(y+1)*pwidth*sizeof(Pixel)+x*sizeof(Pixel)) = temporary;
            }
            else //no point of intersection was found for any object at the given x,y so put black into that x,y pixel into the buffer
            {
                //printf("here. x %3d\ty %3d\n", x, y);
                Pixel temporary;
                temporary.r = 0;
                temporary.g = 0;
                temporary.b = 0;
                *(data+(sizeof(Pixel)*pheight*pwidth)-(y+1)*pwidth*sizeof(Pixel)+x*sizeof(Pixel)) = temporary;
            }
        } //end of x iteration
    } //end of y iteration
}


int main(int argc, char* argv[])
{
    if(argc != 5)
    {
        fprintf(stderr, "Error: Insufficient parameter amount.\nProper input: width height input_filename.json output_filename.ppm\n\n");
        exit(1); //exit the program if there are insufficient arguments
    }
    //echo the command line arguments
    printf("Arg 0: %s\n", argv[0]);
    printf("Arg 1: %s\n", argv[1]);
    printf("Arg 2: %s\n", argv[2]);
    printf("Arg 3: %s\n", argv[3]);
    printf("Arg 4: %s\n", argv[4]);

    outputfp = fopen(argv[4], "wb"); //open output to write to binary
    if (outputfp == 0)
    {
        fprintf(stderr, "Error: Output file \"%s\" could not be opened.\n", argv[3]);
        exit(1); //if the file cannot be opened, exit the program
    }

    Object* objects = malloc(sizeof(Object)*128);
    Light* lights = malloc(sizeof(Light)*128);

    pwidth = atoi(argv[1]);
    pheight = atoi(argv[2]);
    if(pwidth <= 0)
    {
        fprintf(stderr, "Error: Input width '%d' cannot be less than or equal to zero.\n", pwidth);
        exit(1);
    }
    if(pheight <= 0)
    {
        fprintf(stderr, "Error: Input height '%d' cannot be less than or equal to zero.\n", pheight);
        exit(1);
    }
    int* parsedNums = read_scene(argv[3], &objects[0], &lights[0]);  //parse the scene and store the number of objects
    int numOfObjects = parsedNums[0];
    int numOfLights = parsedNums[1];
    printf("# of Objects: %d\n", numOfObjects);           //echo the number of objects
    printf("# of Lights : %d\n", numOfLights);           //echo the number of lights
    Pixel* data = malloc(sizeof(Pixel)*pwidth*pheight*3); //allocate memory to hold all of the pixel data


    store_pixels(numOfObjects, numOfLights, &objects[0], &data[0], &lights[0]);    //store the points of ray intersection and that object's color values into a buffer
    maxcv = 255;
    printf("writing to image file...\n");
    int successfulWrite = write_p3(&data[0]);             //write the pixel buffer to the image file
    if(successfulWrite != 1)
    {
        fprintf(stderr, "Error: Failed to properly write to output image file.\n");
        exit(1);
    }
    fclose(outputfp); //close the output file
    printf("closing...");
    free(lights); //free the memory being used
    free(objects);
    free(data);
    return(0);
}
