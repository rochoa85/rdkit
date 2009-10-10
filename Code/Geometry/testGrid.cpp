//  $Id$
// 
//   Copyright (C) 2005-2008 Greg Landrum and Rational Discovery LLC
//
//   @@ All Rights Reserved  @@
//
#include "UniformGrid3D.h"
#include <DataStructs/DiscreteValueVect.h>
#include "point.h"
#include <RDGeneral/Invariant.h>
#include <RDGeneral/utils.h>
#include "GridUtils.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <ios>
#include <stdlib.h>

using namespace RDGeom;
using namespace RDKit;

void testUniformGrid1() {
  UniformGrid3D grd(6.0, 5.0, 4.0);
  CHECK_INVARIANT(grd.getSize() == 960, "");
  CHECK_INVARIANT(RDKit::feq(grd.getSpacing(), .5), "");
  CHECK_INVARIANT(grd.getNumX()==12, "");
  CHECK_INVARIANT(grd.getNumY()==10, "");
  CHECK_INVARIANT(grd.getNumZ()==8, "");
  
  grd.setSphereOccupancy(Point3D(0.0, 0.0, 0.0), 1.5, 0.25);
  CHECK_INVARIANT(grd.getOccupancyVect()->getTotalVal() == 523, "" ); 
  //writeGridToFile(grd, "junk.grd");

  UniformGrid3D grd2(grd);
  CHECK_INVARIANT(grd2.getSize() == 960, "");
  CHECK_INVARIANT(RDKit::feq(grd2.getSpacing(), .5), "");
  CHECK_INVARIANT(grd2.getNumX()==12, "");
  CHECK_INVARIANT(grd2.getNumY()==10, "");
  CHECK_INVARIANT(grd2.getNumZ()==8, "");
  CHECK_INVARIANT(grd2.getOccupancyVect()->getTotalVal() == 523, "" ); 

  // make sure the data are actually decoupled:
  grd.setSphereOccupancy(Point3D(1.0, 1.0, 0.0), 1.5, 0.25);
  CHECK_INVARIANT(grd.getOccupancyVect()->getTotalVal()>523, "" ); 
  CHECK_INVARIANT(grd2.getOccupancyVect()->getTotalVal() == 523, "" ); 
  
}

void testUniformGrid2() {
  // test distance metrics:
  UniformGrid3D grd(10.0, 10.0, 10.0);
  grd.setSphereOccupancy(Point3D(-2.0, -2.0, 0.0), 1.5, 0.25);
  grd.setSphereOccupancy(Point3D(-2.0, 2.0, 0.0), 1.5, 0.25);
  grd.setSphereOccupancy(Point3D(2.0, -2.0, 0.0), 1.5, 0.25);
  grd.setSphereOccupancy(Point3D(2.0, 2.0, 0.0), 1.5, 0.25);
  writeGridToFile(grd, "junk.grd");
#if 1
  std::ofstream outS;
  outS.open("junk.bin",std::ios_base::binary);
  std::string pkl=grd.toString();
  unsigned int sz=pkl.size();
  outS<<sz;
  outS<<pkl;
  outS.close();
#endif  
  double dist = tanimotoDistance(grd, grd);
  CHECK_INVARIANT(RDKit::feq(dist, 0.0), "");

  UniformGrid3D grd2(10.0, 10.0, 10.0);
  grd2.setSphereOccupancy(Point3D(-2.0, -2.0, 0.0), 1.5, 0.25);
  grd2.setSphereOccupancy(Point3D(-2.0, 2.0, 0.0), 1.5, 0.25);
  grd2.setSphereOccupancy(Point3D(2.0, -2.0, 0.0), 1.5, 0.25);
  dist = tanimotoDistance(grd, grd2);
  CHECK_INVARIANT(RDKit::feq(dist, 0.25), "");
  dist = protrudeDistance(grd, grd2);
  CHECK_INVARIANT(RDKit::feq(dist, 0.25), "");
  dist = protrudeDistance(grd2, grd);
  CHECK_INVARIANT(RDKit::feq(dist, 0.0), "");

  UniformGrid3D grd3(10.0, 10.0, 10.0);
  grd3.setSphereOccupancy(Point3D(-2.0, -2.0, 0.0), 1.5, 0.25);
  grd3.setSphereOccupancy(Point3D(-2.0, 2.0, 0.0), 1.5, 0.25);
  dist = tanimotoDistance(grd, grd3);
  CHECK_INVARIANT(RDKit::feq(dist, 0.5), "");
  dist = protrudeDistance(grd, grd3);
  CHECK_INVARIANT(RDKit::feq(dist, 0.5), "");
  dist = protrudeDistance(grd3, grd);
  CHECK_INVARIANT(RDKit::feq(dist, 0.0), "");
  
  UniformGrid3D grd4(10.0, 10.0, 10.0);
  grd4.setSphereOccupancy(Point3D(2.0, 2.0, 0.0), 1.5, 0.25);
  grd4.setSphereOccupancy(Point3D(2.0, -2.0, 0.0), 1.5, 0.25);
  dist = tanimotoDistance(grd3, grd4);
  CHECK_INVARIANT(RDKit::feq(dist, 1.0), "");


  UniformGrid3D grd5(10.0, 10.0, 10.0);
  grd5.setSphereOccupancy(Point3D(-2.0, -2.0, 0.0), 1.5, 0.25);
  dist = tanimotoDistance(grd, grd5);
  CHECK_INVARIANT(RDKit::feq(dist, 0.75), "");
  dist = protrudeDistance(grd, grd5);
  CHECK_INVARIANT(RDKit::feq(dist, 0.75), "");
  dist = protrudeDistance(grd5, grd);
  CHECK_INVARIANT(RDKit::feq(dist, 0.00), "");
  
 
}

void testUniformGridPickling() {
  {
    // test tanimoto distance 
    UniformGrid3D grd(10.0, 10.0, 10.0);
    grd.setSphereOccupancy(Point3D(-2.0, -2.0, 0.0), 1.5, 0.25);
    grd.setSphereOccupancy(Point3D(-2.0, 2.0, 0.0), 1.5, 0.25);
    grd.setSphereOccupancy(Point3D(2.0, -2.0, 0.0), 1.5, 0.25);
    grd.setSphereOccupancy(Point3D(2.0, 2.0, 0.0), 1.5, 0.25);
    UniformGrid3D grd2(grd.toString());
    double dist = tanimotoDistance(grd, grd2);
    CHECK_INVARIANT(RDKit::feq(dist, 0.0), "");
  }

  {
    std::string dirName = getenv("RDBASE");
    dirName+="/Code/Geometry/testData/";
    std::string pklName = dirName+"grid1.bin";
    std::ifstream inS;
    inS.open(pklName.c_str(),std::ios_base::binary);
    unsigned int length;
    inS >> length;
    char *buff = new char[length];
    unsigned int nRead=0;
    while(nRead<length){
      nRead+=inS.readsome(buff+nRead,length-nRead);
    }
    inS.close();
    std::string pkl(buff,length);
    delete [] buff;
    UniformGrid3D grd(pkl);
    
    UniformGrid3D grd2(10.0, 10.0, 10.0);
    grd2.setSphereOccupancy(Point3D(-2.0, -2.0, 0.0), 1.5, 0.25);
    grd2.setSphereOccupancy(Point3D(-2.0, 2.0, 0.0), 1.5, 0.25);
    grd2.setSphereOccupancy(Point3D(2.0, -2.0, 0.0), 1.5, 0.25);
    grd2.setSphereOccupancy(Point3D(2.0, 2.0, 0.0), 1.5, 0.25);

    std::string pkl2=grd2.toString();
    TEST_ASSERT(pkl2.length()==pkl.length());
    TEST_ASSERT(pkl2==pkl);
    
    TEST_ASSERT(grd.getSize()==grd2.getSize());
    TEST_ASSERT(grd.getNumX()==grd2.getNumX());
    TEST_ASSERT(grd.getNumY()==grd2.getNumY());
    TEST_ASSERT(grd.getNumZ()==grd2.getNumZ());
    TEST_ASSERT(grd.compareParams(grd2));
    double dist = tanimotoDistance(grd, grd2);
    TEST_ASSERT(RDKit::feq(dist, 0.0));
  }
}

void testUniformGridOps() {
  UniformGrid3D grd(10.0, 10.0, 10.0);
  grd.setSphereOccupancy(Point3D(-2.0, -2.0, 0.0), 1.0, 0.25);
  grd.setSphereOccupancy(Point3D(-2.0, 2.0, 0.0), 1.0, 0.25);

  UniformGrid3D grd2(10.0, 10.0, 10.0);
  grd2.setSphereOccupancy(Point3D(2.0, -2.0, 0.0), 1.0, 0.25);
  grd2.setSphereOccupancy(Point3D(2.0, 2.0, 0.0), 1.0, 0.25);

  double dist = tanimotoDistance(grd, grd2);
  CHECK_INVARIANT(RDKit::feq(dist, 1.0), "");

  UniformGrid3D grd3(grd);
  grd3 |= grd2;

  dist = tanimotoDistance(grd3, grd);
  CHECK_INVARIANT(RDKit::feq(dist, 0.5), "");
  dist = tanimotoDistance(grd3, grd2);
  CHECK_INVARIANT(RDKit::feq(dist, 0.5), "");

  UniformGrid3D grd4(10.0, 10.0, 10.0);
  grd4.setSphereOccupancy(Point3D(-2.0, -2.0, 0.0), 1.0, 0.25);
  grd4.setSphereOccupancy(Point3D(-2.0, 2.0, 0.0), 1.0, 0.25);
  grd4.setSphereOccupancy(Point3D(2.0, -2.0, 0.0), 1.0, 0.25);

  UniformGrid3D grd5(grd4);
  grd5 &= grd2;
  
  dist = tanimotoDistance(grd5, grd);
  CHECK_INVARIANT(RDKit::feq(dist, 1.0), "");
  dist = tanimotoDistance(grd5, grd2);
  CHECK_INVARIANT(RDKit::feq(dist, 0.5), "");
}


int main() {
  std::cout << "***********************************************************\n";
  std::cout << "Testing Grid\n";

  std::cout << "\t---------------------------------\n";
  std::cout << "\t testUniformGrid1 \n\n";
  testUniformGrid1();

  std::cout << "\t---------------------------------\n";
  std::cout << "\t testUniformGrid2 \n\n";
  testUniformGrid2();

  std::cout << "\t---------------------------------\n";
  std::cout << "\t testUniformGridPickling \n\n";
  testUniformGridPickling();

  std::cout << "\t---------------------------------\n";
  std::cout << "\t testGridOps \n\n";
  testUniformGridOps();


  return 0;
}