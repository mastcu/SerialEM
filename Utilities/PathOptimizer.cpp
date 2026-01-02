/*
*  PathOptimizer.cpp - Class for sorting points into more optimal data collection path
*
*  Author: Leo Crowder   email: lcrowder101@gmail.com
*
*/

#include "stdafx.h"
#include "PathOptimizer.h"
#include "b3dutil.h"
#include "nn.h"
#include "delaunay.h"
#include <algorithm>
#include <unordered_set>

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

// Error reporting
enum {
  ERR_MISMATCH_SIZES = 1, ERR_MATCHING_HULL_POINTS
};

const char *errStrings[] = {"",
"Size of x and y vectors do not match",
"Convex hull points do not match with triangulation points"
};

//Function used for sorting pairs of vertices. True if v1 is before v2, false otherwise
static bool VertexCompare(vertex v1, vertex v2) {
  if (v1.degree != v2.degree) {
    return v1.degree < v2.degree;
  }
  else {
    return v1.edgeSum > v2.edgeSum;
  }
}

PathOptimizer::PathOptimizer(void)
{
  mInitialPathLength = NULL;
  mPathLength = NULL;
  mTriangulation = NULL;
  mPoints = NULL;
}

PathOptimizer::~PathOptimizer(void)
{
  delaunay_destroy(mTriangulation);
  delete[] mPoints;
  hullCleanup();
  hullchCleanup();
}

//Return error
const char *PathOptimizer::returnErrorString(int err)
{
  if (err < 0 || err >= sizeof(errStrings) / sizeof(char *))
    err = 0;
  return errStrings[err];
}

//Checks that the x and y coordinates of two points match within a relative error
bool PathOptimizer::IsSamePoint(double x1, double y1, float x2, float y2,
  float tol = 1.e-12) {
  return (B3DABS((x1 - x2)) / B3DMIN(B3DABS(x1), B3DABS(x2)) < tol &&
   B3DABS((y1 -y2)) / B3DMIN(B3DABS(y1), B3DABS(y2)) < tol);
}

//Sort the interior point indices in the desired order for the insertion algorithm
void PathOptimizer::SortInteriorVertices(IntVec &indices, IntVec vertexDeg,
 DoubleVec vertEdgeSum) {
  std::vector<vertex> vertices;
  int i;
  vertex vi;

  //Set up vector of each vertex, with index, degree, and edge length sum.
  for (i = 0; i < (int) indices.size(); i++) {
    vi.index = indices[i];
    vi.degree = vertexDeg[vi.index];
    vi.edgeSum = vertEdgeSum[vi.index];
    vertices.push_back(vi);
  }

  std::sort(vertices.begin(), vertices.end(), &VertexCompare);

  for (i = 0; i < (int) indices.size(); i++) {
    indices[i] = vertices[i].index;
  }
}

//Sort the holes into a more optimized path using Delaunay triangulation insertion method
int PathOptimizer::OptimizePath(FloatVec &xCen, FloatVec &yCen, IntVec &tourInd)
{
  int nPts = (int) xCen.size();
  if (nPts != (int)yCen.size()) {
    return ERR_MISMATCH_SIZES;
  }

  mPoints = new point[nPts];
  int nHull, pid, i, j, v1, v2, insertInd, numMatches, startInd = 0;
  FloatVec xHull, yHull;
  DoubleVec vertEdgeSum;
  point ipt;
  IntVec interiorInd, vertexDeg;
  bool reverse = false, inHull = false, noPtsNearCenter = true;
  double x, y, xdif, ydif, changeInLength, minChange, longestEdgeLength;
  float xcen, ycen;
  std::vector<bool> nearCenter;
  double distToHull, distToCenter;

  //Initialize and resize vectors
  tourInd.clear();
  if (nPts < 3) {
    for (i = 0; i < nPts; i++) {
      tourInd.push_back(i);
    }
    return 0;
  }
  xHull.resize(nPts);
  yHull.resize(nPts);
  mDistMatrix.resize(nPts);
  nearCenter.resize(nPts, false);
  for (i = 0; i < nPts; i++) {
    ipt.x = (double)xCen[i];
    ipt.y = (double)yCen[i];
    mPoints[i] = ipt;
    interiorInd.push_back(i);
    mDistMatrix[i].resize(nPts, 0);
  }

  //Compute Delaunay triangulation of the points
  mTriangulation = delaunay_build(nPts, mPoints, 0, NULL, 0, NULL);

  //Populate distance matrix so we can look up values later
  //AND Compute the length of the acquisition path if not optimized
  mInitialPathLength = 0.;
  for (i = 0; i < nPts; i++) {
    for (j = i; j < nPts; j++) {
      xdif = mTriangulation->points[i].x - mTriangulation->points[j].x;
      ydif = mTriangulation->points[i].y - mTriangulation->points[j].y;
      mDistMatrix[i][j] = mDistMatrix[j][i] = sqrt(xdif * xdif + ydif * ydif);

      //Add distance between consecutive points in default path
      if (j == i + 1) {
        mInitialPathLength += mDistMatrix[i][j];
      }
    }
  }

  //Find convex hull of the points, which will be used for the initial tour
  convexBound(&xCen[0], &yCen[0], nPts, 0., 0., &xHull[0], &yHull[0],
    &nHull, &xcen, &ycen, nPts);

  //match points from convexBound with points in the Delaunay Triangulation
  for (i = 0; i < nHull; i++) {
    inHull = false;
    numMatches = 0;
    for (j = 0; j < nPts; j++) {
      if (IsSamePoint(mTriangulation->points[j].x, mTriangulation->points[j].y,
       xHull[i], yHull[i])) {
        pid = j;
        tourInd.push_back(j);
        inHull = true;
        numMatches++;
      }
    }

    // After convexBound point matched with DT point, remove from interior points
    if (inHull && numMatches == 1)
      interiorInd.erase(find(interiorInd.begin(), interiorInd.end(), pid));
    else
      return ERR_MATCHING_HULL_POINTS;
  }

  //Get x and y mid point of bounding box
  xdif = (mTriangulation->xmin + mTriangulation->xmax) / 2.;
  ydif = (mTriangulation->ymin + mTriangulation->ymax) / 2.;

  //Check if each interior point is closer to the center than the hull
  for (i = 0; i < (int)interiorInd.size(); i++) {
    x = mTriangulation->points[i].x;
    y = mTriangulation->points[i].y;

    distToHull = DBL_MAX;
    for (j = 0; j < nHull; j++) {
      if (mDistMatrix[interiorInd[i]][tourInd[j]] < distToHull)
        distToHull = mDistMatrix[interiorInd[i]][tourInd[j]];
    }

    distToCenter = sqrt((x - xdif) * (x - xdif) + (y - ydif) * (y - ydif));
    if (distToCenter < distToHull) {
      nearCenter[interiorInd[i]] = true;
      noPtsNearCenter = false;
    }
  }

  //Find the degree of each vertex in the DT
  vertexDeg.resize(mTriangulation->npoints, 0);
  vertEdgeSum.resize(mTriangulation->npoints, 0.);

  // Use a set to track unique edges and avoid double-counting
  std::unordered_set<int64_t> uniqueEdges;

  for (int i = 0; i < mTriangulation->ntriangles; i++) {
    // Process each edge of the triangle
    for (int j = 0; j < 3; j++) {
      v1 = mTriangulation->triangles[i].vids[j];
      v2 = mTriangulation->triangles[i].vids[(j + 1) % 3];

      // Create a unique key for the edge (always store with smaller index first)
      int64_t edgeKey;
      if (v1 < v2) {
        edgeKey = (static_cast<int64_t>(v1) << 32) | v2;
      }
      else {
        edgeKey = (static_cast<int64_t>(v2) << 32) | v1;
      }

      // If this edge hasn't been counted yet, increment degrees for both endpoints
      if (uniqueEdges.insert(edgeKey).second) {
        vertexDeg[v1]++;
        vertexDeg[v2]++;
        vertEdgeSum[v1] += mDistMatrix[v1][v2];
        vertEdgeSum[v2] += mDistMatrix[v1][v2];
      }
    }
  }

  //Sort the interior points into order of insertion
  SortInteriorVertices(interiorInd, vertexDeg, vertEdgeSum);

  for (i = 0; i < (int)interiorInd.size(); i++) {
    //Get the index from the sorted list of interior points
    pid = interiorInd[i];

    // Initialize by inserting the point between the last and first in the tour
    insertInd = 0;
    minChange = mDistMatrix[pid][tourInd[0]] + mDistMatrix[pid][tourInd.back()]
      - mDistMatrix[tourInd[0]][tourInd.back()];

    //Try inserting the current interior point between each pair of consecutive points
    //in the current tour.
    for (j = 0; j < (int)tourInd.size() - 1; j++) {

      //Insert point from the interior between point [j] and [j+1] in the tour
      //Compute the distance fromthe current point to point [j] then to point [j+1]
      //subtract the old edge from point [j] to point [j+1]
      changeInLength = mDistMatrix[pid][tourInd[j]] + mDistMatrix[pid][tourInd[j+1]]
        - mDistMatrix[tourInd[j]][tourInd[j+1]];

      if (changeInLength <  minChange) {
        minChange = changeInLength;
        insertInd = j + 1;
      }
    }

    //update the tour with the current interior point [j] in the desired location
    tourInd.insert(tourInd.begin() + insertInd, pid);
  }

  //Delete the longest edge inside the center region of points to find the start point
  longestEdgeLength = 0;
  for (i = 0; i < (int) tourInd.size() - 1; i++) {

    //Ensure at least one edge point near center, unless there are no points near center
    if ((noPtsNearCenter || nearCenter[tourInd[i]] || nearCenter[tourInd[i + 1]]) &&
     mDistMatrix[tourInd[i]][tourInd[i + 1]] > longestEdgeLength) {
      longestEdgeLength = mDistMatrix[tourInd[i]][tourInd[i + 1]];
      startInd = i + 1;

      //Reverse the order so that the starting point will be near the center
      if (!nearCenter[tourInd[i + 1]])
        reverse = true;
    }
  }

  //Shift the tour indices so the starting point is first
  std::rotate(tourInd.begin(), tourInd.begin() + startInd, tourInd.end());

  if (reverse)
    std::reverse(tourInd.begin(), tourInd.end());

  //Get new path length
  mPathLength = 0.;
  for (i = 0; i < (int)tourInd.size() - 1; i++) {
    mPathLength += mDistMatrix[tourInd[i]][tourInd[i + 1]];
  }

  return 0;
}
