
#pragma once

#include "..\Shared\cppdefs.h"
#include "delaunay.h"

struct vertex {
  int index;
  int degree;
  double edgeSum;
};

typedef std::vector<double> DoubleVec;

class PathOptimizer
{
public:
  PathOptimizer(void);
  ~PathOptimizer(void);
  double mInitialPathLength;
  double mPathLength;
  delaunay* mTriangulation;
  point* mPoints;
  std::vector<DoubleVec> mDistMatrix;
  int OptimizePath(FloatVec &xCen, FloatVec &yCen, IntVec &tourInd);
  const char *returnErrorString(int err);
private:
  bool IsSamePoint(double x1, double y1, float x2, float y2, float tol);
  void SortInteriorVertices(IntVec &indices, IntVec vertexDeg, DoubleVec vertEdgeSum);
};
