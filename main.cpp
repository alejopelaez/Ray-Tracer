#ifdef __APPLE__
#include <GLUT/glut.h>
#include <OpenGL/glu.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#else
#include "GL/glut.h"
#include "GL/gl.h"
#include "GL/glu.h"
#endif
#ifdef WIN
#include <GL/openglut.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <cstring>
#include <string>
#include <sstream>
#include <climits> 

#include "PhotonMap.h"
#include "math/Vector3.h"
#include "math/Ray.h"
#include "objects/Sphere.h"
#include "objects/Wall.h"
#include "objects/Cylinder.h"
#include "Util/Color.h"
#include "Util/Texture.h"
#include "Util/Light.h"
#include "Util/Camera.h"
#include "Util/Timer.h"

using namespace std;

//Maximum recursion
const int MAX_RECURSION = 5;

//Window dimensions
int width = 800, height = 600;

//Scene needs to be redrawn
bool redraw = false;

//Scene objects
vector<Object*> objects;

//Pixel information
Color *pixels;

//Keyboard
bool keyN[256];
bool keyS[21];

//Timer
Timer timer;

//Number of rays
int nrays;

//Light
vector<Light> lights;
//Global ambient light
Color gAmbient;

//Cameras, corners of the plane, and camera pos
vector<Camera> cameras;
int currCamera;

//Keyboard functions
void keyDown(unsigned char key, int x, int y)
{
    keyN[key] = true;
}
void keyUp(unsigned char key, int x, int y)
{
    keyN[key] = false;
}
void keySDown(int key, int x, int y)
{
    keyS[key] = true;
}
void keySUp(int key, int x, int y)
{
    keyS[key] = false;
}

/*
  Writes the image into a ppm file
 */
void writeImage()
{
    freopen("imagen.ppm","w",stdout);
    printf("P3\n# Created by Alejandro Pelaez, Nicolas Hock Cristian Isaza\n");
    printf("%d %d\n255\n", width, height);
    for(int i = 0; i< height;++i)
    {
        for(int j = 0; j < width; ++j)
        {
            Color c = pixels[(height-i)*width+j];
            printf("%.0f %.0f %.0f ",c.r*255, c.g*255, c.b*255);
        }
        puts("");
    }
}

/*
  Reads the keyboard state and updates
  the simulation state*/
void keyboard()
{
    if(keyN[27])exit(0);

    if(keyN['p'] || keyN['P'])
    {
        writeImage();
        keyN['p']=false;
        keyN['P']=false;
    }

    if(keyN['c'] || keyN['C'])
	{
	    redraw=true;
	    currCamera=(currCamera+1)%cameras.size();
	    keyN['c'] = keyN['C'] = false;
	}
}

/*
  Cast a ray and return the color of the intersected object
*/
Color castRay(Ray ray, int recursive, double &dst)
{
    //Increase the ray counter
    nrays++;
    
    //Was there an intersection?
    bool intersect = false;
    //Is this object receiving light?
    bool shadow = false;
    
    //Main color, reflection and refraction color
    Color c(0.0, 0.0, 0.0), c2(0.0, 0.0, 0.0), c3(0.0, 0.0, 0.0);

    //Pointer to the interseted object
    Object *o;

    //Variable use to calculate the beers law;
    double dist;
    
    //Gets the minimum intersection
    //T2 is used in shadow calculation
    double mint = INT_MAX, t = mint, t2 = -1;
    
    //Check the nearest object intersected by the ray
    for(int i = 0; i < objects.size(); ++i)
	{
	    t = objects[i]->rayIntersection(ray);
	    
	    //We had an intersection!!
	    if(t > 0 && t < mint)
		{
		    mint = t;
		    intersect = true;
		    o = objects[i];
		}
	}
    //There wasn�t any intersection, we return the background color
    if(!intersect)
        return c;
    
    //Point of intersection
    Vector3 p = ray.getPoint(mint);
    //Distance of intersection
    dst = mint;
    
    //Normal of the object at the specified point (used in upcoming calculations)
    Vector3 norm = o->getNorm(p);
    
    //Global ambient calculation, ga = Global ambient, oa = Object ambient
    Color ga = gAmbient;
    Color oa = (o->hasTex())?o->getColor(p):o->getMat().color;
    c = ga*oa;
    
    //Local ilumination model, for each light source
    for(int i = 0; i < lights.size(); ++i, shadow = false)
	{
	    Vector3 d = (lights[i].pos - p).normalize();
	    //Ray from the intersection point towards the light source
        Ray ray2(p + d*eps, d);
	    //Now check if this object is receiving light
	    //t value where the light source is
	    double tt = ray2.getT(lights[i].pos);
	    for(int j = 0; !shadow && j < objects.size(); ++j)
		{
		    t2 = objects[j]->rayIntersection(ray2);
		    //The object is in shadows
		    if(t2 > 0 && t2 <= tt)
            {
                shadow = true;
                break;
            }
		}   
	    if(!shadow)
		{
		    //Ambient color calculation, la = light ambient
		    Color la = lights[i].ambient;
		    c = c + la*oa;
            
		    //Diffuse color calculation, ld = light diffuse, od = object diffuse
		    Color ld = lights[i].diffuse;
		    Color od = (o->hasTex())?o->getColor(p):o->getMat().color*o->getMat().diff;
            
		    //dot is the dot product between the normal and the ray
		    double dot = norm.dot(ray2.getDir());
		    double att = 15/(lights[i].pos - p).magnitudeSquared();
		    //double att = 1.0;
		    if(dot > 0)
			{
			    c = c + ld*od*att*dot;
			    //Specular color calculation
			    Color ls = lights[i].specular;
			    Vector3 v = ray.getDir();
			    Vector3 l = ray2.getDir();
			    Vector3 r = l - norm*2*l.dot(norm);
			    double k = v.dot(r);
			    if(k > 0)
                c = c + ls*pow(k, 20.0)*o->getMat().spec;
			}
		}
	}
    
    //Calculates the reflection color
    double refl = o->getMat().refl;
    if(recursive < MAX_RECURSION && refl > 0.01)
	{
	    Vector3 dir2 = (ray.getDir() - norm*2*(ray.getDir().dot(norm)));
        Ray ray3(p+dir2*eps, dir2);
	    c2 = castRay(ray3, recursive+1, dist);
	}
    //Calculates the refraction color
    double refr = o->getMat().refr;
    if(recursive < MAX_RECURSION && refr > 0.01)
	{
	    double index = 1.0;

	    double rIndex = o->getMat().rIndex;
	    double n = index/rIndex;

	    double cosI = (norm).dot(ray.getDir());
	    double sinT2 = n*n*(1.0 - cosI * cosI);

	    Vector3 dir3;
	    //Total internal reflection
	    if(sinT2 > 1.0)
		    dir3 = ray.getDir() - (-norm)*2*cosI;
	    else
            dir3 = ray.getDir()*n - (norm)*(n*cosI + sqrt(1.0 - sinT2));

        Ray ray4(p+dir3*eps, dir3);
	    c3 = castRay(ray4, recursive+1, dist);
		c3 = c3*exp(-dist*0.065);
    }
    
    //Adds the three colors together
    c = c+c2*refl+c3*refr;
    return c;
}

void draw()
{
    //The scene doesn't need to be redrawn
    if(!redraw) return;

    glClear(GL_COLOR_BUFFER_BIT);
    glutSwapBuffers();

    glRasterPos2f(0.0,0.0);

    //Current time
    timer.start();
    double start = timer.getElapsedTime();
    
    //Calculates all the rays for every pixel in the scene
    for(int i = 0; i < height; ++i)
	{
	    for(int j = 0; j < width; ++j)
		{
		    Ray ray = cameras[currCamera].getRay(j, i);
		    double dist;
		    pixels[i*width + j] = castRay(ray, 1, dist);
		}
	}

    //Final time
    double finish = timer.getElapsedTime();

    glDrawPixels(width,height,GL_RGB,GL_FLOAT,pixels); 
    glutSwapBuffers();

    //Set the window title
    double t = (finish-start)/1e6;
    stringstream ss;
    ss << "Width: " << width;
    ss << " Height: "<< height;
    ss << " Number of primitives: " << objects.size();
    ss << " Rays casted: " << nrays;
    ss << " Render time: " << t << "s";
    glutSetWindowTitle(ss.str().c_str());
    
    redraw = false;
    nrays = 0;
    timer.stop();
}

/*
  Check for user input
 */
void update()
{
    keyboard();
    draw();
}

/*
  Handles the resize events
*/
void resize(int w, int h)
{
    // Prevent a divide by zero, when window is too short
    // (you cant make a window of zero width).
    if(h == 0)
        h = 1;
        
    width = w, height = h;

    //Updates the cameras
    for(int i = 0; i < cameras.size(); ++i)
        cameras[i].changeDim(width, height);

    pixels = new Color[width*height];
    memset(pixels, 0, sizeof(GLfloat)*3*width*height);
  
    //Resets the matrices. We don�ta want any transormations
    //So I create an orthographic matrix with the size of the screen
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0,(GLfloat)width, 0.0,(GLfloat)height);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    redraw = true;
}   

/*
  Initializes some stuff
*/
void init()
{
    pixels = new Color[width*height];
    
    //Resets the pixels
    memset(pixels, 0, sizeof(GLfloat)*3*width*height);

    //Initializes some cameras
    cameras.push_back(Camera(PI/4, (double)width, (double)height, Vector3(0,0,10), 
                             Vector3(0,0,0), Vector3(0,1,0)));
    cameras.push_back(Camera(PI/4, (double)width, (double)height, Vector3(25,0,0), 
                             Vector3(0,0,-10), Vector3(0,1,0)));
    cameras.push_back(Camera(PI/4, (double)width, (double)height, Vector3(25,0,-10), 
                             Vector3(0,0,-10), Vector3(0,1,0)));
    
    currCamera = 0;

    //SET 1
    //Add some spheres
    /*objects.push_back(new Sphere(1, Vector3(-3, -4, -11), 
                                 Material(Color(1.0,0,0), 0.95, 0.8, 0.05, 0.0, 1.2)));
    objects.push_back(new Sphere(1.1, Vector3(-1.2, -3.5, -7), 
                                 Material(Color(0,0,1.0), 0.0, 0.3, 0.0, 1.0, 1.51714)));
    objects.push_back(new Sphere(0.8, Vector3(0, -1, -9), 
                                 Material(Color(1.0,1.0,0.0), 0.4, 0.7, 0.8, 0.0, 1.2)));
    objects.push_back(new Sphere(0.8, Vector3(2, -4.2, -11), 
                                 Material(Color(1.0,1.0,1.0), 0.0, 0.1, 1, 0.0, 1.2)));

    objects.push_back(new Cylinder(0.8, Vector3(5, 0, -7), Vector3(5, -4, -9), 
                                 Material(Color(0.5,0.5,1.0), 0.7, 0.7, 0.3, 0.0, 1.2)));

    
    //Add a wall
    objects.push_back(new Wall(Vector3(-7.0f, -5.0f, -17.0f), Vector3(7.0f, -5.0f, -5.0f), 
                               Vector3(-7.0f, -5.0f, -5.0f), 
                               Material(Color(0,0,0.0), 0.7, 0.4, 0.4, 0.7, 1.2),
                               Texture()));
    objects.push_back(new Wall(Vector3(-6.0f, -4.5f, -17.0f), Vector3(8.0f, 5.0f, -17.0f), 
                               Vector3(8.0f, -4.5f, -17.0f),
                               Material(Color(0.7,0.7,0.7), 0.7, 0.4, 0.6, 0.5, 1.2)));
    
    objects.push_back(new Wall(Vector3(-7.5f, -5.0f, -18.0f), Vector3(-7.5f, 5.0f, -5.0f), 
                               Vector3(-7.5f, 5.0f, -18.0f),
                               Material(Color(0.3,0.3,0.7), 0.7, 0.4, 0.6, 0.5, 1.2)));

   //Lights, and global ambient
    lights.push_back(Light(Vector3(-5, 3.0, 2.0), Color(0.0, 0.0, 0.0), Color(0.6f, 0.6f, 0.6f), Color(0.7f, 0.7f, 0.7f), GLOBAL));
    lights.push_back(Light(Vector3(5, 1.0, 3.0), Color(0.0, 0.0, 0.0), Color(0.6f, 0.6f, 0.8f), Color(0.6f, 0.6f, 0.8f), GLOBAL));*/

    

    /* Cornell Box */
    objects.push_back(new Wall(Vector3(-2.0f, -2.0f, -2.0f), Vector3(2.0f, 2.0f, -2.0f), 
                               Vector3(2.0f, -2.0f, -2.0f), Material(Color(0.8,0.8,0.8), 
                               1.0, 0.1, 0.0, 0.0, 1.2), false, true));
    objects.push_back(new Wall(Vector3(-2.0f, -2.0f, -2.0f), Vector3(-2.0f, 2.0f, 2.0f), 
                               Vector3(-2.0f, 2.0f, -2.0f), Material(Color(1,0.0,0.0), 
                               1.0, 0.1, 0.0, 0.0, 1.2), false, true));
    objects.push_back(new Wall(Vector3(2.0f, -2.0f, -2.0f), Vector3(2.0f, 2.0f, 2.0f), 
                               Vector3(2.0f, -2.0f, 2.0f), Material(Color(0.0,1,0.0), 
                               1.0, 0.1, 0.0, 0.0, 1.2), false, true));
    objects.push_back(new Wall(Vector3(-2.0f, 2.0f, -2.0f), Vector3(2.0f, 2.0f, 2.0f), 
                               Vector3(2.0f, 2.0f, -2.0f), Material(Color(0.8,0.8,0.8), 
                               1.0, 0.1, 0.0, 0.0, 1.2), false, true));
    objects.push_back(new Wall(Vector3(-2.0f, -2.0f, -2.0f), Vector3(2.0f, -2.0f, 2.0f), 
                               Vector3(-2.0f, -2.0f, 2.0f), Material(Color(0.8,0.8,0.8), 
                               1.0, 0.1, 0.0, 0.0, 1.2), false, true));

    objects.push_back(new Sphere(0.6, Vector3(-0.7, -1.4, 0), 
                                 Material(Color(0.0,0.0,1.0), 0.0, 0.1, 1.0, 0.0, 1.2)));

    objects.push_back(new Sphere(0.5, Vector3(1, 0, 1), 
                                 Material(Color(0.0,0.0,1.0), 1.0, 0.7, 0.0, 0.0, 1.2)));

    objects.push_back(new Cylinder(0.4, Vector3(1.1, -1.0, 1.7), Vector3(1.1, -2.0, 1.7), 
                                 Material(Color(0.0,0.0,1.0), 0.15, 0.7, 0.85, 0.0, 1.2)));
    
                                 lights.push_back(Light(Vector3(0, 1, 2), Color(0.0, 0.0, 0.0), Color(0.6f, 0.6f, 0.8f), Color(0.6f, 0.6f, 0.8f), GLOBAL));


    //THIS IS THE DEFAULT = COLOR(0,0,0); DO NOT CHANGE UNLESS NECESARY
    gAmbient = Color(0.1, 0.1, 0.1);

    nrays = 0;
}

/*
  Initializes OpenGL|
*/
void initGl()
{
    glShadeModel (GL_SMOOTH);
    glEnable(GL_CULL_FACE);
    
    //Resets the matrices. We don�t want any transformations
    //So we created an orthographic matrix with the size of the screen
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0,(GLfloat)width, 0.0,(GLfloat)height);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char *argv[])
{
    glutInitWindowSize(width,height);
    glutInitWindowPosition(40,40);
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);

    glutCreateWindow("Ray Tracer");

    //Reshape and draw
    glutReshapeFunc(resize);
    glutDisplayFunc(draw);

    //Keyboard Functions
    glutKeyboardFunc(keyDown);
    glutSpecialFunc(keySDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialUpFunc(keySUp);
    
    glutIdleFunc(update);
    initGl();
    init();

    glutMainLoop();

    return 0;
}
