#ifdef WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <vector>
#include <thread>
#include <utility>
#include <functional>
#include <tuple>
#include <mutex>
#include "raytracing.h"
#include "mesh.h"
#include "traqueboule.h"
#include "imageWriter.h"
#include "helper.h"
#include "squeue.h"

Vec3Dd MyCameraPosition;

//MyLightPositions stores all the light positions to use
//for the ray tracing. Please notice, the light that is 
//used for the real-time rendering is NOT one of these, 
//but following the camera instead.
std::vector<Vec3Dd> MyLightPositions;

//Main mesh 
Mesh MyMesh; 

unsigned int WindowSize_X = 800;  // resolution X
unsigned int WindowSize_Y = 800;  // resolution Y


// helper variables for adaptive Anti-Aliasing.
// SET EPSILON TO 0.001 FOR FINAL RENDER!
#define EPSILON  0.05
#define MAX_SAMPLES  5 

// Worker threads.
unsigned threads, wthreads = 0;
std::mutex wmutex;
synchronized_queue<std::tuple<unsigned, unsigned>> wqueue;


/**
 * Main function, which is drawing an image (frame) on the screen
*/
void drawFrame( )
{
	yourDebugDraw();
}

//animation is called for every image on the screen once
void animate()
{
	MyCameraPosition=getCameraPosition();
	glutPostRedisplay();
}


void display(void);
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void startRaytracing();
void doThreadTrace(Image &result, Vec3Dd &origin00, Vec3Dd &dest00, Vec3Dd &origin01, Vec3Dd &dest01, Vec3Dd &origin10, Vec3Dd &dest10, Vec3Dd &origin11, Vec3Dd &dest11, int id, unsigned minY, unsigned maxY);
Vec3Dd calculateCorners(Vec3Dd &origin00, Vec3Dd &dest00, Vec3Dd &origin01, Vec3Dd &dest01, Vec3Dd &origin10, Vec3Dd &dest10, Vec3Dd &origin11, Vec3Dd &dest11,
	double x, double y, double space, double amountofsamples);
Vec3Dd calculateRay(Vec3Dd &origin00, Vec3Dd &dest00, Vec3Dd &origin01, Vec3Dd &dest01, Vec3Dd &origin10, Vec3Dd &dest10, Vec3Dd &origin11, Vec3Dd &dest11, double x_offset, double y_offset,
	double x, double y);
bool compareColor(Vec3Dd color1, Vec3Dd color2, Vec3Dd color3, Vec3Dd color4);
bool compareWithEpsilon(Vec3Dd vec1, Vec3Dd vec2);

/**
 * Main Programme
 */
int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    for (; *argv; argv++)
        if (!strcmp(*argv, "-p")) {
            double x, y, z;
            sscanf(*++argv, "%lf,%lf,%lf", &x, &y, &z);
            Vec3Dd pos(x, y, z);
            std::cout << "Command-line camera position: " << pos << std::endl;
            MyCameraPosition = pos;
        } else if (!strcmp(*argv, "-l")) {
            double x, y, z;
            char *p = *++argv;
            do {
                char *e = strchr(p, ':');
                if (e) *e++ = 0;
                sscanf(p, "%lf,%lf,%lf", &x, &y, &z); 
                Vec3Dd pos(x, y, z);
                std::cout << "Command-line light source: " << pos << std::endl;
                MyLightPositions.push_back(pos);
                p = e;
            } while (p);
        }

    //framebuffer setup
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH );

    // positioning and size of window
    glutInitWindowPosition(200, 100);
    glutInitWindowSize(WindowSize_X,WindowSize_Y);
    glutCreateWindow(argv[0]);  

    //initialize viewpoint
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0,0,-4);
    tbInitTransform();     // This is for the trackball, please ignore
    tbHelp();             // idem
    MyCameraPosition=getCameraPosition();

    //activate the light following the camera
      glEnable( GL_LIGHTING );
      glEnable( GL_LIGHT0 );
      glEnable(GL_COLOR_MATERIAL);
      int LightPos[4] = {0,0,2,0};
      int MatSpec [4] = {1,1,1,1};
      glLightiv(GL_LIGHT0,GL_POSITION,LightPos);

    //normals will be normalized in the graphics pipeline
    glEnable(GL_NORMALIZE);
      //clear color of the background is black.
    glClearColor (0.0, 0.0, 0.0, 0.0);

    
    // Activate rendering modes
      //activate depth test
    glEnable( GL_DEPTH_TEST ); 
      //draw front-facing triangles filled
    //and back-facing triangles as wires
      glPolygonMode(GL_FRONT,GL_FILL);
      glPolygonMode(GL_BACK,GL_LINE);
      //interpolate vertex colors over the triangles
    glShadeModel(GL_SMOOTH);


    // glut setup... to ignore
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutDisplayFunc(display);
    glutMouseFunc(tbMouseFunc);    // trackball
    glutMotionFunc(tbMotionFunc);  // uses mouse
    glutIdleFunc( animate);


    // thread setup.
    threads = getThreadCount();
    printf("Detected %u threads.\n", threads);

    init();

#if defined(FIFTYRAYS_ONLYTRACE)
    reshape(WindowSize_X,WindowSize_Y);
    display();
    animate();
    startRaytracing();
#else    
	//main loop for glut... this just runs your application
    glutMainLoop();
#endif
   
    return 0;  // execution never reaches this point
}

/**
 * OpenGL setup - functions do not need to be changed! 
 * you can SKIP AHEAD TO THE KEYBOARD FUNCTION
 */
//what to do before drawing an image
 void display(void)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);//store GL state
    // Effacer tout
    glClear( GL_COLOR_BUFFER_BIT  | GL_DEPTH_BUFFER_BIT); // clear image
    
    glLoadIdentity();  

    tbVisuTransform(); // init trackball

    drawFrame( );    //actually draw

    glutSwapBuffers();//glut internal switch
	glPopAttrib();//return to old GL state
}
//Window changes size
void reshape(int w, int h)
{
    glViewport(0, 0, (GLsizei) w, (GLsizei) h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    //glOrtho (-1.1, 1.1, -1.1,1.1, -1000.0, 1000.0);
    gluPerspective (50, (float)w/h, 0.01, 10);
    glMatrixMode(GL_MODELVIEW);
}


//transform the x, y position on the screen into the corresponding 3D world position
void produceRay(int x_I, int y_I, Vec3Dd * origin, Vec3Dd * dest)
{
		int viewport[4];
		double modelview[16];
		double projection[16];
		glGetDoublev(GL_MODELVIEW_MATRIX, modelview); //recuperer matrices
		glGetDoublev(GL_PROJECTION_MATRIX, projection); //recuperer matrices
		glGetIntegerv(GL_VIEWPORT, viewport);//viewport
		int y_new = viewport[3] - y_I;

		double x, y, z;
		
		gluUnProject(x_I, y_new, 0, modelview, projection, viewport, &x, &y, &z);
		origin->p[0]=float(x);
		origin->p[1]=float(y);
		origin->p[2]=float(z);
		gluUnProject(x_I, y_new, 1, modelview, projection, viewport, &x, &y, &z);
		dest->p[0]=float(x);
		dest->p[1]=float(y);
		dest->p[2]=float(z);
}

// react to keyboard input
void keyboard(unsigned char key, int x, int y)
{
    printf("key %d pressed at %d,%d\n",key,x,y);
    fflush(stdout);
    switch (key)
    {
	//add/update a light based on the camera position.
	case 'L':
		MyLightPositions.push_back(getCameraPosition());
		break;
	case 'l':
		MyLightPositions[MyLightPositions.size()-1]=getCameraPosition();
		break;
	case 'r':
	{
		//Pressing r will launch the raytracing.
		cout<<"Raytracing..."<<endl;
    startRaytracing();
		
		break;
	}
	case 27:     // touche ESC
        exit(0);
    }

	
	//produce the ray for the current mouse position
	Vec3Dd testRayOrigin, testRayDestination;
	produceRay(x, y, &testRayOrigin, &testRayDestination);

	yourKeyboardFunc(key,x,y, testRayOrigin, testRayDestination);
}


void startRaytracing() {
  //Setup an image with the size of the current image.
  Image result(WindowSize_X,WindowSize_Y);
  
  //produce the rays for each pixel, by first computing
  //the rays for the corners of the frustum.
  Vec3Dd origin00, dest00;
  Vec3Dd origin01, dest01;
  Vec3Dd origin10, dest10;
  Vec3Dd origin11, dest11;

  produceRay(0,0, &origin00, &dest00);
  produceRay(0,WindowSize_Y-1, &origin01, &dest01);
  produceRay(WindowSize_X-1,0, &origin10, &dest10);
  produceRay(WindowSize_X-1,WindowSize_Y-1, &origin11, &dest11);

  std::vector<std::thread> t;
  for (unsigned n = 0; n < threads; n++)
    t.push_back(std::thread(std::bind(doThreadTrace, std::ref(result), std::ref(origin00), std::ref(dest00), std::ref(origin01), std::ref(dest01), std::ref(origin10), std::ref(dest10), std::ref(origin11), std::ref(dest11), n + 1, (unsigned)((float)n / threads * WindowSize_Y), (unsigned)((float)(n + 1) / threads * WindowSize_Y))));
  for (unsigned n = 0; n < threads; n++)
    t[n].join();

  char filename[64];
#if defined(_MSC_VER)
  _snprintf(filename, sizeof(filename), "result-%ld.bmp", time(NULL));
#else    
  snprintf(filename, sizeof(filename), "result-%ld.bmp", time(NULL));
#endif
  result.writeImage(filename);
}

void doThreadTrace(Image &result, Vec3Dd &origin00, Vec3Dd &dest00, Vec3Dd &origin01, Vec3Dd &dest01, Vec3Dd &origin10, Vec3Dd &dest10, Vec3Dd &origin11, Vec3Dd &dest11, int id, unsigned minY, unsigned maxY) {
  Vec3Dd origin, dest;
  while (true) {
    for (unsigned int y = minY; y < maxY; y++) {
      unsigned divider = (maxY - minY < 20) ? 5 : 20;
      if ((y - minY) % ((maxY - minY) / divider) == 0) {
        double perc = (float)(y - minY) / (float)(maxY - minY);
        std::cout << "[thread #" << id << "] " << 100 * perc << "%" << std::endl;

        wmutex.lock();
        if (wthreads && maxY - y >= 25) {
          unsigned diff = (maxY - y) / 2;
          maxY -= diff;
          wqueue.push(std::tuple<unsigned, unsigned>(maxY, maxY + diff));

          std::cout << "[thread #" << id << "] Rebalanced " << diff << " lines to waiting thread." << std::endl;
        }
        wmutex.unlock();
      }

	  for (unsigned int x = 0; x<WindowSize_X;++x) {
		  Vec3Dd rgb = nullVector();
		  //produce the rays for each pixel, by interpolating
		  Vec3Dd color1 = nullVector();
		  Vec3Dd color2 = nullVector();
		  Vec3Dd color3 = nullVector();
		  Vec3Dd color4 = nullVector();
		  double start_space = 0.5;
		  double samples = 0;
		  //launch raytracing for the corners.
		  rgb = calculateCorners(origin00, dest00, origin01, dest01, origin10, dest10, origin11, dest11, x, y, start_space, samples);

		  //store the result in an image
		  result.setPixel(x, y, RGBValue(rgb[0], rgb[1], rgb[2]));
	  }
    }

    // Wait for new assignments.
    std::cout << "[thread #" << id << "] Waiting... [" << wthreads + 1 << "]" << std::endl;
    wmutex.lock();
    wthreads++;
    wmutex.unlock();

    if (wthreads == threads) {
      std::cout << "[thread #" << id << "] Done!" << std::endl;
      for (unsigned n = 0; n < threads - 1; n++)
        wqueue.push(std::tuple<unsigned, unsigned>(0, 0));
      break;
    }


    std::tuple<unsigned, unsigned> t = wqueue.front();
    wqueue.pop();

    wmutex.lock();
    wthreads--;
    wmutex.unlock();

    // Got new assignment, let's go.
    minY = std::get<0>(t);
    maxY = std::get<1>(t);
    if (minY == 0 && maxY == 0)
      break;

    std::cout << "[thread #" << id << "] Got new work: " << minY << " - " << maxY << std::endl;

  }
}

/*
Calculate the four corners of a pixel. When they are not equal, repeat the process with adapative supersampling.
*/
Vec3Dd calculateCorners(Vec3Dd &origin00, Vec3Dd &dest00, Vec3Dd &origin01, Vec3Dd &dest01, Vec3Dd &origin10, Vec3Dd &dest10, Vec3Dd &origin11, Vec3Dd &dest11, 
	double x, double y,double space,double amountofsamples ) {
	Vec3Dd newcolor = nullVector();
	Vec3Dd color1, color2, color3, color4;
	/*
	Calculate for x - space & y - space
	x - space & y + space
	x + space & y - space
	x + space & y + space

	*/
	color1 = calculateRay(origin00, dest00, origin01, dest01, origin10, dest10, origin11, dest11, -space, -space, x, y);
	color2 = calculateRay(origin00, dest00, origin01, dest01, origin10, dest10, origin11, dest11, space, -space, x, y);
	color3 = calculateRay(origin00, dest00, origin01, dest01, origin10, dest10, origin11, dest11, -space, space, x, y);
	color4 = calculateRay(origin00, dest00, origin01, dest01, origin10, dest10, origin11, dest11, space, space, x, y);
	
	// corner colors are equal
	if ( amountofsamples > MAX_SAMPLES || compareColor(color1, color2, color3, color4)) {
		// get the average of these colors.
		newcolor = ((color1 + color2 + color3 + color4) / 4);
	}
	else {
		// divide the space since we are searching in a smaller square of the pixel now.
		double newspace = space / 2;
		amountofsamples = amountofsamples + 1;

		// the color of the pixel is equal to the average of four smaller squares in the pixel.
		newcolor = 0.25 * calculateCorners(origin00, dest00, origin01, dest01, origin10, dest10, origin11, dest11,  (x - newspace), (y - newspace),newspace,amountofsamples );
		newcolor = newcolor + 0.25 *  calculateCorners(origin00, dest00, origin01, dest01, origin10, dest10, origin11, dest11, (x + newspace), (y - newspace), newspace, amountofsamples);
		newcolor = newcolor + 0.25 *  calculateCorners(origin00, dest00, origin01, dest01, origin10, dest10, origin11, dest11, (x - newspace), (y + newspace), newspace, amountofsamples);
		newcolor = newcolor + 0.25 *  calculateCorners(origin00, dest00, origin01, dest01, origin10, dest10, origin11, dest11, (x + newspace), (y + newspace), newspace, amountofsamples);
	}
	return newcolor;
}

/**
Cast a ray with a x offset and y offset from the points (x,y) and get his color.
*/
Vec3Dd calculateRay(Vec3Dd &origin00, Vec3Dd &dest00, Vec3Dd &origin01, Vec3Dd &dest01, Vec3Dd &origin10, Vec3Dd &dest10, Vec3Dd &origin11, Vec3Dd &dest11, double x_offset, double y_offset,
	double x, double y) {
	Vec3Dd origin, dest;

	double xscale = 1.0 - (x) / (WindowSize_X - (1.0 + x_offset));
	double yscale = 1.0 - (y) / (WindowSize_Y - (1.0 + y_offset));

	origin = yscale*(xscale*origin00 + (1 - xscale)*origin10) +
		(1 - yscale)*(xscale*origin01 + (1 - xscale)*origin11);
	dest = yscale*(xscale*dest00 + (1 - xscale)*dest10) +
		(1 - yscale)*(xscale*dest01 + (1 - xscale)*dest11);
	
	return performRayTracing(origin, dest);
}

/**
Compare 4 Vec3D with each other, only return true if they are all the same.
*/
bool compareColor(Vec3Dd color1, Vec3Dd color2, Vec3Dd color3, Vec3Dd color4) {
	bool equal = false;
	if ( compareWithEpsilon(color1,color2) && compareWithEpsilon(color2, color3) && compareWithEpsilon(color3, color4) && compareWithEpsilon(color4,color1) && compareWithEpsilon(color1,color3) && compareWithEpsilon(color2,color4) ){
		equal = true;
	}
	return equal;
}

/*
 Compare the value of two Vec3D with a Epsilon factor.
*/
bool compareWithEpsilon(Vec3Dd vec1, Vec3Dd vec2) {
	bool equal = false;
	if ( abs(vec1[0] - vec2[0]) < EPSILON && abs(vec1[1] - vec2[1]) < EPSILON && abs(vec1[2] - vec2[2]) < EPSILON) {
		equal = true;
	}
	return equal;
}



