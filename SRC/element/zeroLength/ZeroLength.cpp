/* ****************************************************************** **
**    OpenSees - Open System for Earthquake Engineering Simulation    **
**          Pacific Earthquake Engineering Research Center            **
**                                                                    **
**                                                                    **
** (C) Copyright 1999, The Regents of the University of California    **
** All Rights Reserved.                                               **
**                                                                    **
** Commercial use of this program without express permission of the   **
** University of California, Berkeley, is strictly prohibited.  See   **
** file 'COPYRIGHT'  in main directory for information on usage and   **
** redistribution,  and for a DISCLAIMER OF ALL WARRANTIES.           **
**                                                                    **
** Developed by:                                                      **
**   Frank McKenna (fmckenna@ce.berkeley.edu)                         **
**   Gregory L. Fenves (fenves@ce.berkeley.edu)                       **
**   Filip C. Filippou (filippou@ce.berkeley.edu)                     **
**                                                                    **
** ****************************************************************** */
                                                                        
// $Revision: 1.21 $
// $Date: 2007-11-07 23:24:07 $
// $Source: /usr/local/cvs/OpenSees/SRC/element/zeroLength/ZeroLength.cpp,v $
                                                                        
                                                                        
// File: ~/element/ZeroLength/ZeroLength.C
// 
// Written: GLF
// Created: 12/99
// Revision: A
//
// Description: This file contains the implementation for the ZeroLength class.
//
// What: "@(#) ZeroLength.C, revA"

#include "ZeroLength.h"
#include <Information.h>

#include <Domain.h>
#include <Node.h>
#include <Channel.h>
#include <FEM_ObjectBroker.h>
#include <UniaxialMaterial.h>
#include <Renderer.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <ElementResponse.h>

// initialise the class wide variables
Matrix ZeroLength::ZeroLengthM2(2,2);
Matrix ZeroLength::ZeroLengthM4(4,4);
Matrix ZeroLength::ZeroLengthM6(6,6);
Matrix ZeroLength::ZeroLengthM12(12,12);
Vector ZeroLength::ZeroLengthV2(2);
Vector ZeroLength::ZeroLengthV4(4);
Vector ZeroLength::ZeroLengthV6(6);
Vector ZeroLength::ZeroLengthV12(12);

//  Constructor:
//  responsible for allocating the necessary space needed by each object
//  and storing the tags of the ZeroLength end nodes.

//  Construct element with one unidirectional material (numMaterials1d=1)
ZeroLength::ZeroLength(int tag,
		       int dim,
		       int Nd1, int Nd2, 
		       const Vector &x, const Vector &yp,
		       UniaxialMaterial &theMat,
		       int direction )
 :Element(tag,ELE_TAG_ZeroLength),     
  connectedExternalNodes(2),
  dimension(dim), numDOF(0), transformation(3,3),
  theMatrix(0), theVector(0),
  numMaterials1d(1), theMaterial1d(0), dir1d(0), t1d(0)
{
  // allocate memory for numMaterials1d uniaxial material models
  theMaterial1d = new UniaxialMaterial*  [numMaterials1d];
  dir1d	  = new ID(numMaterials1d);
  
  if ( theMaterial1d == 0 || dir1d == 0 ) {
    opserr << "FATAL ZeroLength::ZeroLength - failed to create a 1d  material or direction array\n";
    exit(-1);
  }

  // initialize uniaxial materials and directions and check for valid values
  (*dir1d)(0) = direction;
  this->checkDirection( *dir1d );
  
  // get a copy of the material and check we obtained a valid copy
  theMaterial1d[0] = theMat.getCopy();
  if (theMaterial1d[0] == 0) {
    opserr << "FATAL ZeroLength::ZeroLength - failed to get a copy of material " << theMat.getTag() << endln;
    exit(-1);
  }

  // establish the connected nodes and set up the transformation matrix for orientation
  this->setUp( Nd1, Nd2, x, yp);
}


//  Construct element with multiple unidirectional materials
ZeroLength::ZeroLength(int tag,
		       int dim,
		       int Nd1, int Nd2, 
		       const Vector& x, const Vector& yp,
		       int n1dMat,
		       UniaxialMaterial** theMat,
		       const ID& direction )
 :Element(tag,ELE_TAG_ZeroLength),     
  connectedExternalNodes(2),
  dimension(dim), numDOF(0), transformation(3,3),
  theMatrix(0), theVector(0),
  numMaterials1d(n1dMat), theMaterial1d(0), dir1d(0), t1d(0)
{

    // allocate memory for numMaterials1d uniaxial material models
    theMaterial1d = new UniaxialMaterial*  [numMaterials1d];
    dir1d	  = new ID(numMaterials1d);
    
    if ( theMaterial1d == 0 || dir1d == 0 ) {
      opserr << "FATAL ZeroLength::ZeroLength - failed to create a 1d  material or direction array\n";
      exit(-1);
    }
    
    // initialize uniaxial materials and directions and check for valid values
    *dir1d = direction;
    this->checkDirection( *dir1d );
    
    // get a copy of the material objects and check we obtained a valid copy
    for (int i=0; i<numMaterials1d; i++) {
      theMaterial1d[i] = theMat[i]->getCopy();
      if (theMaterial1d[i] == 0) {
	opserr << "FATAL ZeroLength::ZeroLength - failed to get a copy of material " <<theMat[i]->getTag() << endln;
	exit(-1);
      }
     }
	
    // establish the connected nodes and set up the transformation matrix for orientation
    this->setUp( Nd1, Nd2, x, yp);
}


//   Constructor:
//   invoked by a FEM_ObjectBroker - blank object that recvSelf needs
//   to be invoked upon
ZeroLength::ZeroLength(void)
  :Element(0,ELE_TAG_ZeroLength),     
  connectedExternalNodes(2),
  dimension(0), numDOF(0), transformation(3,3),
  theMatrix(0), theVector(0),
  numMaterials1d(0), theMaterial1d(0),
  dir1d(0), t1d(0)
{
    // ensure the connectedExternalNode ID is of correct size 
    if (connectedExternalNodes.Size() != 2)
      opserr << "FATAL ZeroLength::ZeroLength - failed to create an ID of correct size\n";
}


//  Destructor:
//  delete must be invoked on any objects created by the object
//  and on the matertial object.
ZeroLength::~ZeroLength()
{
    // invoke the destructor on any objects created by the object
    // that the object still holds a pointer to

    // invoke destructors on material objects
    for (int mat=0; mat<numMaterials1d; mat++) 
	delete theMaterial1d[mat];

    // delete memory of 1d materials    
    if (theMaterial1d != 0)
	delete [] theMaterial1d;

    if (t1d != 0)
	delete t1d;
    if (dir1d != 0 )
	delete dir1d;
}


int
ZeroLength::getNumExternalNodes(void) const
{
    return 2;
}


const ID &
ZeroLength::getExternalNodes(void) 
{
    return connectedExternalNodes;
}



Node **
ZeroLength::getNodePtrs(void) 
{
  return theNodes;
}

int
ZeroLength::getNumDOF(void) 
{
    return numDOF;
}


// method: setDomain()
//    to set a link to the enclosing Domain and to set the node pointers.
//    also determines the number of dof associated
//    with the ZeroLength element, we set matrix and vector pointers,
//    allocate space for t matrix and define it as the basic deformation-
//    displacement transformation matrix.
void
ZeroLength::setDomain(Domain *theDomain)
{
    // check Domain is not null - invoked when object removed from a domain
    if (theDomain == 0) {
	theNodes[0] = 0;
	theNodes[1] = 0;
	return;
    }

    // set default values for error conditions
    numDOF = 2;
    theMatrix = &ZeroLengthM2;
    theVector = &ZeroLengthV2;
    
    // first set the node pointers
    int Nd1 = connectedExternalNodes(0);
    int Nd2 = connectedExternalNodes(1);
    theNodes[0] = theDomain->getNode(Nd1);
    theNodes[1] = theDomain->getNode(Nd2);	

    // if can't find both - send a warning message
    if ( theNodes[0] == 0 || theNodes[1] == 0 ) {
      if (theNodes[0] == 0) 
        opserr << "WARNING ZeroLength::setDomain() - Nd1: " << Nd1 << " does not exist in ";
      else
        opserr << "WARNING ZeroLength::setDomain() - Nd2: " << Nd2 << " does not exist in ";

      opserr << "model for ZeroLength ele: " << this->getTag() << endln;

      return;
    }

    // now determine the number of dof and the dimension    
    int dofNd1 = theNodes[0]->getNumberDOF();
    int dofNd2 = theNodes[1]->getNumberDOF();	

    // if differing dof at the ends - print a warning message
    if ( dofNd1 != dofNd2 ) {
      opserr << "WARNING ZeroLength::setDomain(): nodes " << Nd1 << " and " << Nd2 <<
	"have differing dof at ends for ZeroLength " << this->getTag() << endln;
      return;
    }	

    // Check that length is zero within tolerance
    const Vector &end1Crd = theNodes[0]->getCrds();
    const Vector &end2Crd = theNodes[1]->getCrds();	
    Vector diff = end1Crd - end2Crd;
    double L  = diff.Norm();
    double v1 = end1Crd.Norm();
    double v2 = end2Crd.Norm();
    double vm;
    
    vm = (v1<v2) ? v2 : v1;


    if (L > LENTOL*vm)
      opserr << "WARNING ZeroLength::setDomain(): Element " << this->getTag() << " has L= " << L << 
	", which is greater than the tolerance\n";
        
    // call the base class method
    this->DomainComponent::setDomain(theDomain);
    
    // set the number of dof for element and set matrix and vector pointer
    if (dimension == 1 && dofNd1 == 1) {
	numDOF = 2;    
	theMatrix = &ZeroLengthM2;
	theVector = &ZeroLengthV2;
	elemType  = D1N2;
    }
    else if (dimension == 2 && dofNd1 == 2) {
	numDOF = 4;
	theMatrix = &ZeroLengthM4;
	theVector = &ZeroLengthV4;
	elemType  = D2N4;
    }
    else if (dimension == 2 && dofNd1 == 3) {
	numDOF = 6;	
	theMatrix = &ZeroLengthM6;
	theVector = &ZeroLengthV6;
	elemType  = D2N6;
    }
    else if (dimension == 3 && dofNd1 == 3) {
	numDOF = 6;	
	theMatrix = &ZeroLengthM6;
	theVector = &ZeroLengthV6;
	elemType  = D3N6;
    }
    else if (dimension == 3 && dofNd1 == 6) {
	numDOF = 12;	    
	theMatrix = &ZeroLengthM12;
	theVector = &ZeroLengthV12;
	elemType  = D3N12;
    }
    else {
      opserr << "WARNING ZeroLength::setDomain cannot handle " << dimension << 
	"dofs at nodes in " << dofNd1 << " d problem\n"; 
      return;
    }

    // create the basic deformation-displacement transformation matrix for the element
    // for 1d materials (uniaxial materials)
    if ( numMaterials1d > 0 )
	this->setTran1d( elemType, numMaterials1d );
}   	 


int
ZeroLength::commitState()
{
    int code=0;

    // call element commitState to do any base class stuff
    if ((code = this->Element::commitState()) != 0) {
      opserr << "ZeroLength::commitState () - failed in base class";
    }    

    // commit 1d materials
    for (int i=0; i<numMaterials1d; i++) 
	code += theMaterial1d[i]->commitState();

    return code;
}

int
ZeroLength::revertToLastCommit()
{
    int code=0;
    
    // revert state for 1d materials
    for (int i=0; i<numMaterials1d; i++)
	code += theMaterial1d[i]->revertToLastCommit();
    
    return code;
}


int
ZeroLength::revertToStart()
{   
    int code=0;
    
    // revert to start for 1d materials
    for (int i=0; i<numMaterials1d; i++)
	code += theMaterial1d[i]->revertToStart();
    
    return code;
}


int
ZeroLength::update(void)
{
    double strain;
    double strainRate;

    // get trial displacements and take difference
    const Vector& disp1 = theNodes[0]->getTrialDisp();
    const Vector& disp2 = theNodes[1]->getTrialDisp();
    Vector  diff  = disp2-disp1;
    const Vector& vel1  = theNodes[0]->getTrialVel();
    const Vector& vel2  = theNodes[1]->getTrialVel();
    Vector  diffv = vel2-vel1;
    
    // loop over 1d materials
    
    //    Matrix& tran = *t1d;
    int ret = 0;
    for (int mat=0; mat<numMaterials1d; mat++) {
	// compute strain and rate; set as current trial for material
	strain     = this->computeCurrentStrain1d(mat,diff );
        strainRate = this->computeCurrentStrain1d(mat,diffv);
	ret += theMaterial1d[mat]->setTrialStrain(strain,strainRate);
    }

    return ret;
}

const Matrix &
ZeroLength::getTangentStiff(void)
{
    double E;

    // stiff is a reference to the matrix holding the stiffness matrix
    Matrix& stiff = *theMatrix;
    
    // zero stiffness matrix
    stiff.Zero();
    
    // loop over 1d materials
    
    Matrix& tran = *t1d;;
    for (int mat=0; mat<numMaterials1d; mat++) {
      
      // get tangent for material
      E = theMaterial1d[mat]->getTangent();
      
      // compute contribution of material to tangent matrix
      for (int i=0; i<numDOF; i++)
	for(int j=0; j<i+1; j++)
	  stiff(i,j) +=  tran(mat,i) * E * tran(mat,j);
      
    }

    // end loop over 1d materials 
    
    // complete symmetric stiffness matrix
    for (int i=0; i<numDOF; i++)
      for(int j=0; j<i; j++)
	stiff(j,i) = stiff(i,j);

    return stiff;
}


const Matrix &
ZeroLength::getInitialStiff(void)
{
    double E;

    // stiff is a reference to the matrix holding the stiffness matrix
    Matrix& stiff = *theMatrix;
    
    // zero stiffness matrix
    stiff.Zero();
    
    // loop over 1d materials
    
    Matrix& tran = *t1d;;
    for (int mat=0; mat<numMaterials1d; mat++) {
      
      // get tangent for material
      E = theMaterial1d[mat]->getInitialTangent();
      
      // compute contribution of material to tangent matrix
      for (int i=0; i<numDOF; i++)
	for(int j=0; j<i+1; j++)
	  stiff(i,j) +=  tran(mat,i) * E * tran(mat,j);
      
    }

    // end loop over 1d materials 
    
    // complete symmetric stiffness matrix
    for (int i=0; i<numDOF; i++)
      for(int j=0; j<i; j++)
	stiff(j,i) = stiff(i,j);

    return stiff;
}
    

const Matrix &
ZeroLength::getDamp(void)
{
    double eta;

    // damp is a reference to the matrix holding the damping matrix
    Matrix& damp = *theMatrix;
 
    // zero stiffness matrix
    damp.Zero();
    
    // loop over 1d materials
    Matrix& tran = *t1d;;
    for (int mat=0; mat<numMaterials1d; mat++) {
	
	// get tangent for material
	eta = theMaterial1d[mat]->getDampTangent();
	
        // compute contribution of material to tangent matrix
        for (int i=0; i<numDOF; i++)
	    for(int j=0; j<i+1; j++)
		damp(i,j) +=  tran(mat,i) * eta * tran(mat,j);

    } // end loop over 1d materials 

    // complete symmetric stiffness matrix
    for (int i=0; i<numDOF; i++)
	for(int j=0; j<i; j++)
	    damp(j,i) = damp(i,j);

    return damp;
}


const Matrix &
ZeroLength::getMass(void)
{
    // no mass 
    theMatrix->Zero();    
    return *theMatrix; 
}


void 
ZeroLength::zeroLoad(void)
{
  // does nothing now
}

int 
ZeroLength::addLoad(ElementalLoad *theLoad, double loadFactor)
{
  opserr << "ZeroLength::addLoad - load type unknown for truss with tag: " << this->getTag() << endln;
  
  return -1;
}

int 
ZeroLength::addInertiaLoadToUnbalance(const Vector &accel)
{
  // does nothing as element has no mass yet!
  return 0;
}


const Vector &
ZeroLength::getResistingForce()
{
    double force;
    
    // zero the residual
    theVector->Zero();
    
    // loop over 1d materials
    for (int mat=0; mat<numMaterials1d; mat++) {
	
	// get resisting force for material
	force = theMaterial1d[mat]->getStress();
	
        // compute residual due to resisting force
        for (int i=0; i<numDOF; i++)
	    (*theVector)(i)  += (*t1d)(mat,i) * force;
	
    } // end loop over 1d materials 

    return *theVector;
}


const Vector &
ZeroLength::getResistingForceIncInertia()
{	
    // there is no mass, so return
    
    return this->getResistingForce();
}


int
ZeroLength::sendSelf(int commitTag, Channel &theChannel)
{
	int res = 0;

	// note: we don't check for dataTag == 0 for Element
	// objects as that is taken care of in a commit by the Domain
	// object - don't want to have to do the check if sending data
	int dataTag = this->getDbTag();

	// ZeroLength packs its data into an ID and sends this to theChannel
	// along with its dbTag and the commitTag passed in the arguments

	// Make one size bigger so not a multiple of 3, otherwise will conflict
	// with classTags ID
	static ID idData(6+1);

	idData(0) = this->getTag();
	idData(1) = dimension;
	idData(2) = numDOF;
	idData(3) = numMaterials1d;
	idData(4) = connectedExternalNodes(0);
	idData(5) = connectedExternalNodes(1);

	res += theChannel.sendID(dataTag, commitTag, idData);
	if (res < 0) {
	  opserr << "ZeroLength::sendSelf -- failed to send ID data\n";
	  return res;
	}

	// Send the 3x3 direction cosine matrix, have to send it since it is only set
	// in the constructor and not setDomain()
	res += theChannel.sendMatrix(dataTag, commitTag, transformation);
	if (res < 0) {
	  opserr <<  "ZeroLength::sendSelf -- failed to send transformation Matrix\n";
	  return res;
	}

	if (numMaterials1d < 1)
	  return res;
	else {
	  ID classTags(numMaterials1d*3);
	  
	  int i;
	  // Loop over the materials and send them
	  for (i = 0; i < numMaterials1d; i++) {
	    int matDbTag = theMaterial1d[i]->getDbTag();
	    if (matDbTag == 0) {
	      matDbTag = theChannel.getDbTag();
	      if (matDbTag != 0)
		theMaterial1d[i]->setDbTag(matDbTag);
	    }
	    classTags(i) = matDbTag;
	    classTags(numMaterials1d+i) = theMaterial1d[i]->getClassTag();
	    classTags(2*numMaterials1d+i) = (*dir1d)(i);
	  }
	  
	  res += theChannel.sendID(dataTag, commitTag, classTags);
	  if (res < 0) {
	    opserr << " ZeroLength::sendSelf -- failed to send classTags ID\n";
	    return res;
	  }
	
	  for (i = 0; i < numMaterials1d; i++) {
	    res += theMaterial1d[i]->sendSelf(commitTag, theChannel);
	    if (res < 0) {
	      opserr << "ZeroLength::sendSelf -- failed to send Material1d " << i << endln;
		
	      return res;
	  }
		}
	}

	return res;
}

int
ZeroLength::recvSelf(int commitTag, Channel &theChannel, FEM_ObjectBroker &theBroker)
{
  int res = 0;
  
  int dataTag = this->getDbTag();

  // ZeroLength creates an ID, receives the ID and then sets the 
  // internal data with the data in the ID

  static ID idData(6+1);

  res += theChannel.recvID(dataTag, commitTag, idData);
  if (res < 0) {
    opserr << "ZeroLength::recvSelf -- failed to receive ID data\n";
			    
    return res;
  }

  res += theChannel.recvMatrix(dataTag, commitTag, transformation);
  if (res < 0) {
    opserr << "ZeroLength::recvSelf -- failed to receive transformation Matrix\n";
			    
    return res;
  }

  this->setTag(idData(0));
  dimension = idData(1);
  numDOF = idData(2);
  connectedExternalNodes(0) = idData(4);
  connectedExternalNodes(1) = idData(5);
  
  if (idData(3) < 1) {
    numMaterials1d = 0;
    if (dir1d != 0) {
      delete dir1d;
      dir1d = 0;
    }
    return res;
  }
  else {
    // Check that there is correct number of materials, reallocate if needed
    if (numMaterials1d != idData(3)) {
      int i;
      if (theMaterial1d != 0) {
	for (i = 0; i < numMaterials1d; i++)
	  delete theMaterial1d[i];
	delete [] theMaterial1d;
	theMaterial1d = 0;
      }
      
      numMaterials1d = idData(3);
      
      theMaterial1d = new UniaxialMaterial *[numMaterials1d];
      if (theMaterial1d == 0) {
	opserr << "ZeroLength::recvSelf -- failed to new Material1d array\n";
	return -1;
      }
      
      for (i = 0; i < numMaterials1d; i++)
	theMaterial1d[i] = 0;
      
      // Allocate ID array for directions
      if (dir1d != 0)
	delete dir1d;
      dir1d = new ID(numMaterials1d);
      if (dir1d == 0) {
	opserr << "ZeroLength::recvSelf -- failed to new dir ID\n";
				
	return -1;
      }
    }
    
    ID classTags(3*numMaterials1d);
    res += theChannel.recvID(dataTag, commitTag, classTags);
    if (res < 0) {
      opserr << "ZeroLength::recvSelf -- failed to receive classTags ID\n";
      return res;
    }
    
    for (int i = 0; i < numMaterials1d; i++) {
      int matClassTag = classTags(numMaterials1d+i);
      
      // If null, get a new one from the broker
      if (theMaterial1d[i] == 0)
	theMaterial1d[i] = theBroker.getNewUniaxialMaterial(matClassTag);
      
      // If wrong type, get a new one from the broker
      if (theMaterial1d[i]->getClassTag() != matClassTag) {
	delete theMaterial1d[i];
	theMaterial1d[i] = theBroker.getNewUniaxialMaterial(matClassTag);
      }
      
      // Check if either allocation failed from broker
      if (theMaterial1d[i] == 0) {
	opserr << "ZeroLength::recvSelf  -- failed to allocate new Material1d " << i << endln;
	return -1;
      }
      
      // Receive the materials
      theMaterial1d[i]->setDbTag(classTags(i));
      res += theMaterial1d[i]->recvSelf(commitTag, theChannel, theBroker);
      if (res < 0) {
	opserr << "ZeroLength::recvSelf  -- failed to receive new Material1d " << i << endln;
	return res;
      }
      
      // Set material directions
      (*dir1d)(i) = classTags(2*numMaterials1d+i);
    }
  }

  return res;
}


int
ZeroLength::displaySelf(Renderer &theViewer, int displayMode, float fact)
{
    // ensure setDomain() worked
    if (theNodes[0] == 0 || theNodes[1] == 0 )
       return 0;

    // first determine the two end points of the ZeroLength based on
    // the display factor (a measure of the distorted image)
    // store this information in 2 3d vectors v1 and v2
    const Vector &end1Crd = theNodes[0]->getCrds();
    const Vector &end2Crd = theNodes[1]->getCrds();	
    const Vector &end1Disp = theNodes[0]->getDisp();
    const Vector &end2Disp = theNodes[1]->getDisp();    

    if (displayMode == 1 || displayMode == 2) {
	Vector v1(3);
	Vector v2(3);
	for (int i=0; i<dimension; i++) {
	    v1(i) = end1Crd(i)+end1Disp(i)*fact;
	    v2(i) = end2Crd(i)+end2Disp(i)*fact;    
	}
	
	// don't display strain or force
	double strain = 0.0;
	double force  = 0.0;
    
	if (displayMode == 2) // use the strain as the drawing measure
	    return theViewer.drawLine(v1, v2, strain, strain);	
	else { // otherwise use the axial force as measure
	    return theViewer.drawLine(v1,v2, force, force);
	}
    }
    return 0;
}


void
ZeroLength::Print(OPS_Stream &s, int flag)
{
    // compute the strain and axial force in the member
    double strain=0.0;
    double force =0.0;
    
    for (int i=0; i<numDOF; i++)
	(*theVector)(i) = (*t1d)(0,i)*force;

    if (flag == 0) { // print everything
	s << "Element: " << this->getTag(); 
	s << " type: ZeroLength  iNode: " << connectedExternalNodes(0);
	s << " jNode: " << connectedExternalNodes(1) << endln;
	for (int j = 0; j < numMaterials1d; j++) {
		s << "\tMaterial1d, tag: " << theMaterial1d[j]->getTag() 
			<< ", dir: " << (*dir1d)(j) << endln;
		s << *(theMaterial1d[j]);
	}
    } else if (flag == 1) {
	s << this->getTag() << "  " << strain << "  ";
    }
}

Response*
ZeroLength::setResponse(const char **argv, int argc, OPS_Stream &output)
{
  Response *theResponse = 0;

  output.tag("ElementOutput");
  output.attr("eleType","ZeroLength");
  output.attr("eleTag",this->getTag());
  output.attr("node1",connectedExternalNodes[0]);
  output.attr("node2",connectedExternalNodes[1]);

  char outputData[10];

  if (strcmp(argv[0],"force") == 0 || strcmp(argv[0],"forces") == 0) {

    for (int i=0; i<numMaterials1d; i++) {
      sprintf(outputData,"P%d",i+1);
      output.tag("ResponseType",outputData);
    }

    theResponse =  new ElementResponse(this, 1, Vector(numMaterials1d));
  
  }  else if (strcmp(argv[0],"defo") == 0 || strcmp(argv[0],"deformations") == 0 ||
	      strcmp(argv[0],"deformation") == 0) {

    for (int i=0; i<numMaterials1d; i++) {
      sprintf(outputData,"e%d",i+1);
      output.tag("ResponseType",outputData);
    }

    theResponse =  new ElementResponse(this, 2, Vector(numMaterials1d));
  
  }  else if ((strcmp(argv[0],"defoANDforce") == 0) ||
	   (strcmp(argv[0],"deformationANDforces") == 0) ||
	      (strcmp(argv[0],"deformationsANDforces") == 0)) {
	int i;
    for (i=0; i<numMaterials1d; i++) {
      sprintf(outputData,"e%d",i+1);
      output.tag("ResponseType",outputData);
    }
    for (i=0; i<numMaterials1d; i++) {
      sprintf(outputData,"P%d",i+1);
      output.tag("ResponseType",outputData);
    }

    theResponse =  new ElementResponse(this, 4, Vector(2*numMaterials1d));

  }  else if (strcmp(argv[0],"material") == 0) {
    if (argc > 2) {
      int matNum = atoi(argv[1]);
      if (matNum >= 1 && matNum <= numMaterials1d)
	theResponse =  theMaterial1d[matNum-1]->setResponse(&argv[2], argc-2, output);
    }
  }

  output.endTag();

  return theResponse;
}

int 
ZeroLength::getResponse(int responseID, Information &eleInformation)
{
  const Vector& disp1 = theNodes[0]->getTrialDisp();
  const Vector& disp2 = theNodes[1]->getTrialDisp();
  const Vector  diff  = disp2-disp1;
  
  switch (responseID) {
  case -1:
    return -1;
    
  case 1:
    if (eleInformation.theVector != 0) {
      for (int i = 0; i < numMaterials1d; i++)
	(*(eleInformation.theVector))(i) = theMaterial1d[i]->getStress();
    }
    return 0;
    
  case 2:
    if (eleInformation.theVector != 0) {
      for (int i = 0; i < numMaterials1d; i++)
	(*(eleInformation.theVector))(i) = theMaterial1d[i]->getStrain();
    }
    return 0;      
    
  case 4:
    if (eleInformation.theVector != 0) {
      for (int i = 0; i < numMaterials1d; i++) {
	(*(eleInformation.theVector))(i) = theMaterial1d[i]->getStrain();
	(*(eleInformation.theVector))(i+numMaterials1d) = theMaterial1d[i]->getStress();
      }
    }
    return 0;      
    
  case 3:
    if (eleInformation.theMatrix != 0) {
      for (int i = 0; i < numMaterials1d; i++)
	(*(eleInformation.theMatrix))(i,i) = theMaterial1d[i]->getTangent();
    }
    return 0;      
    
  default:
    return -1;
  }
}

int
ZeroLength::setParameter(const char **argv, int argc, Parameter &param)
{
  int result = -1;  

  if (argc < 1)
    return -1;


  for (int i=0; i<numMaterials1d; i++) {
    int res = theMaterial1d[i]->setParameter(argv, argc, param);
	if (res != -1) {
      result = res;
	}
  }  
  return result;
}

int
ZeroLength::updateParameter (int parameterID, Information &info)
{
  return 0;
}

int
ZeroLength::activateParameter(int passedParameterID)
{
  
  return 0;
}



// Private methods


// Establish the external nodes and set up the transformation matrix
// for orientation
void
ZeroLength::setUp( int Nd1, int Nd2,
		   const Vector &x,
		   const Vector &yp )
{ 
    // ensure the connectedExternalNode ID is of correct size & set values
    if (connectedExternalNodes.Size() != 2)
      opserr << "FATAL ZeroLength::setUp - failed to create an ID of correct size\n";
    
    connectedExternalNodes(0) = Nd1;
    connectedExternalNodes(1) = Nd2;

	int i;
    for (i=0; i<2; i++)
      theNodes[i] = 0;

    // check that vectors for orientation are correct size
    if ( x.Size() != 3 || yp.Size() != 3 )
	opserr << "FATAL ZeroLength::setUp - incorrect dimension of orientation vectors\n";

    // establish orientation of element for the tranformation matrix
    // z = x cross yp
    Vector z(3);
    z(0) = x(1)*yp(2) - x(2)*yp(1);
    z(1) = x(2)*yp(0) - x(0)*yp(2);
    z(2) = x(0)*yp(1) - x(1)*yp(0);

    // y = z cross x
    Vector y(3);
    y(0) = z(1)*x(2) - z(2)*x(1);
    y(1) = z(2)*x(0) - z(0)*x(2);
    y(2) = z(0)*x(1) - z(1)*x(0);

    // compute length(norm) of vectors
    double xn = x.Norm();
    double yn = y.Norm();
    double zn = z.Norm();

    // check valid x and y vectors, i.e. not parallel and of zero length
    if (xn == 0 || yn == 0 || zn == 0) {
      opserr << "FATAL ZeroLength::setUp - invalid vectors to constructor\n";
    }
    
    // create transformation matrix of direction cosines
    for ( i=0; i<3; i++ ) {
	transformation(0,i) = x(i)/xn;
	transformation(1,i) = y(i)/yn;
	transformation(2,i) = z(i)/zn;
     }

}


// Check that direction is in the range of 0 to 5
void
ZeroLength::checkDirection( ID &dir ) const
{
    for ( int i=0; i<dir.Size(); i++)
	if ( dir(i) < 0 || dir(i) > 5 ) {
	  opserr << "WARNING ZeroLength::checkDirection - incorrect direction " << dir(i) << " is set to 0\n";
	  dir(i) = 0;
	}
}


// Set basic deformation-displacement transformation matrix for 1d
// uniaxial materials
void
ZeroLength::setTran1d( Etype elemType,
		       int   numMat )
{
    enum Dtype { TRANS, ROTATE };
    
    int   indx, dir;
    Dtype dirType;
    
    // Create 1d transformation matrix
    t1d = new Matrix(numMat,numDOF);
    
    if (t1d == 0)
	opserr << "FATAL ZeroLength::setTran1d - can't allocate 1d transformation matrix\n";
    
    // Use reference for convenience and zero matrix.
    Matrix& tran = *t1d;
    tran.Zero();
    
    // loop over materials, setting row in tran for each material depending on dimensionality of element
    
    for ( int i=0; i<numMat; i++ ) {
	
	dir  = (*dir1d)(i);	// direction 0 to 5;
	indx = dir % 3;		// direction 0, 1, 2 for axis of translation or rotation
	
	// set direction type to translation or rotation
	dirType = (dir<3) ? TRANS : ROTATE;
	
	// now switch on dimensionality of element
	
	switch (elemType) {
	  	    
	  case D1N2:
	    if (dirType == TRANS)
		tran(i,1) = transformation(indx,0);
	    break;
		 
	  case D2N4:
	    if (dirType == TRANS) {
		tran(i,2) = transformation(indx,0);  
	        tran(i,3) = transformation(indx,1);
	    }
	    break;
		 
          case D2N6: 
	    if (dirType == TRANS) {
		tran(i,3) = transformation(indx,0);  
	        tran(i,4) = transformation(indx,1);
	        tran(i,5) = 0.0;
	    } else if (dirType == ROTATE) {
		tran(i,3) = 0.0;
		tran(i,4) = 0.0;
		tran(i,5) = transformation(indx,2);
	    }
	    break;
		    
	  case D3N6:
	    if (dirType == TRANS) {
		tran(i,3) = transformation(indx,0);  
	        tran(i,4) = transformation(indx,1);
	        tran(i,5) = transformation(indx,2);
	    }
	    break;
		 
	  case D3N12:
	    if (dirType == TRANS) {
		tran(i,6)  = transformation(indx,0);  
	        tran(i,7)  = transformation(indx,1);
	        tran(i,8)  = transformation(indx,2);
		tran(i,9)  = 0.0;
		tran(i,10) = 0.0;
		tran(i,11) = 0.0;
	    } else if (dirType == ROTATE) {
		tran(i,6)  = 0.0;
	        tran(i,7)  = 0.0;
	        tran(i,8)  = 0.0;
		tran(i,9)  = transformation(indx,0);
		tran(i,10) = transformation(indx,1);
		tran(i,11) = transformation(indx,2);
	    }
	    break;
		 
	} // end switch
	
	// fill in first half of transformation matrix with
	// negative sign
	
	for (int j=0; j < numDOF/2; j++ )
	    tran(i,j) = -tran(i,j+numDOF/2);
	
    } // end loop over 1d materials
}
		     

// Compute current strain for 1d material mat
// dispDiff are the displacements of node 2 minus those
// of node 1
double
ZeroLength::computeCurrentStrain1d( int mat,
				    const Vector& dispDiff ) const
{
    double strain = 0.0;

    for (int i=0; i<numDOF/2; i++){
	strain += -dispDiff(i) * (*t1d)(mat,i);
    }

    return strain;
}
	      
void
ZeroLength::updateDir(const Vector& x, const Vector& y)
{
	this->setUp(connectedExternalNodes(0), connectedExternalNodes(1), x, y);
	this->setTran1d(elemType, numMaterials1d);
}
