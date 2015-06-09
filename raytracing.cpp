#include <stdio.h>
#ifdef WIN32
#include <windows.h>
#endif
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "raytracing.h"


//temporary variables
//these are only used to illustrate 
//a simple debug drawing. A ray 
Vec3Df testRayOrigin;
Vec3Df testRayDestination;


//use this function for any preprocessing of the mesh.
void init()
{
	//load the mesh file
	//please realize that not all OBJ files will successfully load.
	//Nonetheless, if they come from Blender, they should, if they 
	//are exported as WavefrontOBJ.
	//PLEASE ADAPT THE LINE BELOW TO THE FULL PATH OF THE dodgeColorTest.obj
	//model, e.g., "C:/temp/myData/GraphicsIsFun/dodgeColorTest.obj", 
	//otherwise the application will not load properly

  
  MyMesh.loadMesh(FILE_LOCATION, true);
	MyMesh.computeVertexNormals();

	//one first move: initialize the first light source
	//at least ONE light source has to be in the scene!!!
	//here, we set it to the current location of the camera
	MyLightPositions.push_back(MyCameraPosition);
}

//return the color of your pixel.
Vec3Df performRayTracing(const Vec3Df & origin, const Vec3Df & dest)
{
  float depth;
  Vec3Df color = Vec3Df(0,0,0);
  for(int i = 0; i < MyMesh.triangles.size(); i++){
    Triangle triangle = MyMesh.triangles.at(i);
    Vertex v0 = MyMesh.vertices.at(triangle.v[0]);
    Vertex v1 = MyMesh.vertices.at(triangle.v[1]);
    Vertex v2 = MyMesh.vertices.at(triangle.v[2]);
    

  
    bool intersectTest = rayTriangleIntersect(origin, dest, v0.p, v1.p, v2.p, depth);
    if(intersectTest){
      // save color and depth
      unsigned int triMat = MyMesh.triangleMaterials.at(i);
      color=MyMesh.materials.at(triMat).Kd() + MyMesh.materials.at(triMat).Ka() + MyMesh.materials.at(triMat).Ks();
    }
    

  }
  
  return color;

	//  if(intersect(level, ray, max, &hit)) {
	//    Shade(level, hit, &color);
	//  }
	//  else
	//    color=BackgroundColor

	//return Vec3Df(dest[0],dest[1],dest[2]);
}

// bool intersect(level, ray, max, &hit) {
	// compute intersection between rays and planes
// }
bool rayTriangleIntersect(const Vec3Df &orig, const Vec3Df &dir, const Vec3Df v0, const Vec3Df v1, const Vec3Df v2,float &t)
{
  // compute plane's normal
  Vec3Df v0v1 = v1 - v0;
  Vec3Df v0v2 = v2 - v0;
  // no need to normalize
  Vec3Df N = Vec3Df::crossProduct(v0v1, v0v2); // N
  float area2 = N.getLength();
  
  // Step 1: finding P
  
  // check if ray and plane are parallel ?
  float NdotRayDirection = Vec3Df::dotProduct(N, dir);
  if (fabs(NdotRayDirection) < 0.000000001) // almost 0
    return false; // they are parallel so they don't intersect !
  
  // compute d parameter using equation 2
  float d = Vec3Df::dotProduct(N, v0);
  
  // compute t (equation 3)
  t = (Vec3Df::dotProduct(N, orig) + d) / NdotRayDirection;
  // check if the triangle is in behind the ray
  if (t < 0) return false; // the triangle is behind
  
  // compute the intersection point using equation 1
  Vec3Df P = orig + t * dir;
  
  // Step 2: inside-outside test
  Vec3Df C; // vector perpendicular to triangle's plane
  
  // edge 0
  Vec3Df edge0 = v1 - v0;
  Vec3Df vp0 = P - v0;
  C = Vec3Df::crossProduct(edge0, vp0);
  if (Vec3Df::dotProduct(N, C) < 0) return false; // P is on the right side
  
  // edge 1
  Vec3Df edge1 = v2 - v1;
  Vec3Df vp1 = P - v1;
  C = Vec3Df::crossProduct(edge1, vp1);
  if (Vec3Df::dotProduct(N, C) < 0)  return false; // P is on the right side
  
  // edge 2
  Vec3Df edge2 = v0 - v2;
  Vec3Df vp2 = P - v2;
  C = Vec3Df::crossProduct(edge2, vp2);
  if (Vec3Df::dotProduct(N,C) < 0) return false; // P is on the right side;
  
  return true; // this ray hits the triangle
}

//Shade(level, hit, &color){
//  for each lightsource
//    ComputeDirectLight(hit, &directColor);
//  if(material reflects && (level < maxLevel))
//    computeReflectedRay(hit, &reflectedray);
//    Trace(level+1, reflectedRay, &reflectedColor);
//  color = directColor + reflection * reflectedcolor + transmission * refractedColor;
//}



void yourDebugDraw()
{
	//draw open gl debug stuff
	//this function is called every frame

	//let's draw the mesh
	MyMesh.draw();
	
	//let's draw the lights in the scene as points
	glPushAttrib(GL_ALL_ATTRIB_BITS); //store all GL attributes
	glDisable(GL_LIGHTING);
	glColor3f(1,1,1);
	glPointSize(10);
	glBegin(GL_POINTS);
	for (int i=0;i<MyLightPositions.size();++i)
		glVertex3fv(MyLightPositions[i].pointer());
	glEnd();
	glPopAttrib();//restore all GL attributes
	//The Attrib commands maintain the state. 
	//e.g., even though inside the two calls, we set
	//the color to white, it will be reset to the previous 
	//state after the pop.


	//as an example: we draw the test ray, which is set by the keyboard function
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glDisable(GL_LIGHTING);
	glBegin(GL_LINES);
	glColor3f(0,1,1);
	glVertex3f(testRayOrigin[0], testRayOrigin[1], testRayOrigin[2]);
	glColor3f(0,0,1);
	glVertex3f(testRayDestination[0], testRayDestination[1], testRayDestination[2]);
	glEnd();
	glPointSize(10);
	glBegin(GL_POINTS);
	glVertex3fv(MyLightPositions[0].pointer());
	glEnd();
	glPopAttrib();
	
	//draw whatever else you want...
	////glutSolidSphere(1,10,10);
	////allows you to draw a sphere at the origin.
	////using a glTranslate, it can be shifted to whereever you want
	////if you produce a sphere renderer, this 
	////triangulated sphere is nice for the preview
}


//yourKeyboardFunc is used to deal with keyboard input.
//t is the character that was pressed
//x,y is the mouse position in pixels
//rayOrigin, rayDestination is the ray that is going in the view direction UNDERNEATH your mouse position.
//
//A few keys are already reserved: 
//'L' adds a light positioned at the camera location to the MyLightPositions vector
//'l' modifies the last added light to the current 
//    camera position (by default, there is only one light, so move it with l)
//    ATTENTION These lights do NOT affect the real-time rendering. 
//    You should use them for the raytracing.
//'r' calls the function performRaytracing on EVERY pixel, using the correct associated ray. 
//    It then stores the result in an image "result.ppm".
//    Initially, this function is fast (performRaytracing simply returns 
//    the target of the ray - see the code above), but once you replaced 
//    this function and raytracing is in place, it might take a 
//    while to complete...
void yourKeyboardFunc(char t, int x, int y, const Vec3Df & rayOrigin, const Vec3Df & rayDestination)
{

	//here, as an example, I use the ray to fill in the values for my upper global ray variable
	//I use these variables in the debugDraw function to draw the corresponding ray.
	//try it: Press a key, move the camera, see the ray that was launched as a line.
	testRayOrigin=rayOrigin;	
	testRayDestination=rayDestination;
	
	// do here, whatever you want with the keyboard input t.
		
//      Trace(0, ray, &color);
//      PutPixel(x, y, color);
	
	std::cout<<t<<" pressed! The mouse was in location "<<x<<","<<y<<"!"<<std::endl;	
}



// Pseudocode From slides

//RayTrace (view)
//{
//  for (y=0; y<view.yres; ++y){
//    for(x=0; x<view.xres; ++x){
//      ComputeRay(x, y, view, &ray);
//      Trace(0, ray, &color);
//      PutPixel(x, y, color);
//    }
//  }
//}
//
//Trace( level, ray, &color){
//  if(intersect(level, ray, max, &hit)) {
//    Shade(level, hit, &color);
//  }
//  else
//    color=BackgroundColor
//}
//
//Shade(level, hit, &color){
//  for each lightsource
//    ComputeDirectLight(hit, &directColor);
//  if(material reflects && (level < maxLevel))
//    computeReflectedRay(hit, &reflectedray);
//    Trace(level+1, reflectedRay, &reflectedColor);
//  color = directColor + reflection * reflectedcolor + transmission * refractedColor;
//}
