#pragma once

#include "SFML/OpenGL.hpp"
#include <vector>
#include <map>
#include "../Body/Body.h"
#include "../Camera/Camera.h"
using namespace std;

namespace CGEngine {
	/// <summary>
	/// Responsible for ordering Bodies for rendering. Allows for default ordering (children render on top of parents)
	/// modified with per-object Z-Order
	/// </summary>
	class Renderer {
	public:
		/// <summary>
		/// Add the Body to the renderOrder and and it and its transform to the bodyTransform map for this frame
		/// </summary>
		/// <param name="body">The Body to add to the Renderer</param>
		/// <param name="transform">The transform of the Body</param>
		void add(Body* body, Transform transform);
		/// <summary>
		/// Calculate the greatest Z-Order of Bodies
		/// </summary>
		/// <returns>The greatest Z-Order among Bodies in the renderOrder</returns>
		int zMax();
		/// <summary>
		/// Calculate the minimum Z-Order of Bodies
		/// </summary>
		/// <returns>The minimum Z-Order among Bodies in the renderOrder</returns>
		int zMin();
		/// <summary>
		/// Get a vector of all bodies with the indicated Z-Order
		/// </summary>
		/// <param name="zIndex">The Z index to get Bodies at</param>
		/// <returns>A vector of Body pointers at the indicated Z index</returns>
		vector<Body*> getZBodies(int zIndex);
		/// <summary>
		/// Get a vector of all bodies below the indicated Z-Order
		/// </summary>
		/// <param name="zIndex">The Z index to get Bodies below</param>
		/// <returns>A vector of Body pointers below the indicated Z index</returns>
		vector<Body*> getLowerZBodies(int zIndex);
		/// <summary>
		/// Get a vector of all bodies above the indicated Z-Order
		/// </summary>
		/// <param name="zIndex">The Z index to get Bodies above</param>
		/// <returns>A vector of Body pointers above the indicated Z index</returns>
		vector<Body*> getHigherZBodies(int zIndex);

		void initializeOpenGL();
		bool setGLWindowState(bool state);
		bool clearGL(GLbitfield mask);
		void commitGL();
		void pullGL();

		bool processRender();
		void setWindow(RenderWindow* window);
		Camera* getCurrentCamera();
		void setCurrentCamera(Camera* camera);
	private:
		friend class World;
		RenderWindow* window = nullptr;
		/// <summary>
		/// Clear the renderOrder and bodyTransform map
		/// </summary>
		void clear();
		/// <summary>
		/// Sort the Bodies by their Z-Order
		/// </summary>
		void sortZ();
		/// <summary>
		/// Draw the Bodies based on their Z-Order (or default order)
		/// </summary>
		/// <param name="window">The RenderTarget to draw the Body in</param>
		void render(RenderTarget* window);

		Camera* currentCamera = nullptr;
		void renderCamera();
		/// <summary>
		/// The order in which to draw bodies, with Bodies further back in the vector drawn on top of other Bodies. This is cleared and re-calculated each frame
		/// </summary>
		vector<Body*> renderOrder;
	};
}