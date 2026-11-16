// -*- c++ -*-
// Threed: Three dimensional viewer.

#ifndef __THREED_H_
#define __THREED_H_

#include "vector.h"

// manifest constants
const int WorldWidth = 1000;       // Current World size in X
const int WorldHeight = 1000;      // Current World size in Y
const int WorldDepth = 1000;       // Current World size in Z

class wxPoint;

// Three dimensional viewing class
class TThreeD
{
public:
    TThreeD();
    void SetAt();
    void SetUp();
    void SetClip(double angle, double _Zmin, double _Zmax);
    void SetFrom();
    void SetEye();
    void SetVolume();
    void SetLight();
    bool TransformSegment(const VECTOR & from3d, const VECTOR & to3d, wxPoint & from2d, wxPoint & to2d);
    bool TransformPoint(const VECTOR & point3d, wxPoint & point2d);

private:
    void WORLDtoPC(double xw, double yw, wxPoint& pc);
    void WorldPointToEyePoint(const VECTOR & worldPoint, VECTOR & eyePoint);
    bool Clip3DSegment(VECTOR & p1, VECTOR & p2, wxPoint & from2d, wxPoint & to2d);
    bool Clip3DPoint(const VECTOR & point3d, wxPoint & point2d);
    unsigned int Code(const VECTOR & p) const;

protected:
    VECTOR From;
    VECTOR At;
    VECTOR Up;
    VECTOR Light;

    VECTOR A1;
    VECTOR A2;
    VECTOR A3;

    double DVal;
    VECTOR Offset;

    double A;
    double B;
    double C;
    double D;

private:
    double Angle;
    double Zmin;
    double Zmax;
};

#endif //  __THREED_H_
