///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: BoxSelectionRepresentation3D.h
// Purpose: used as representation for the box selection volume in the render view
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _BOXSELECTIONREPRESENTATION3D_H_
#define _BOXSELECTIONREPRESENTATION3D_H_

// vtk includes
#include <vtkSmartPointer.h>

// vtk classes forward declarations
class vtkActor;
class vtkPolyDataMapper;
class vtkProperty;
class vtkPolyData;
class vtkPoints;
class vtkRenderer;

///////////////////////////////////////////////////////////////////////////////////////////////////
// BoxSelectionRepresentation3D class
//
class BoxSelectionRepresentation3D
{
	public:
		BoxSelectionRepresentation3D();
		~BoxSelectionRepresentation3D();

		// Description:
		void PlaceBox(double bounds[6]);
		double *GetBounds();
		void SetRenderer(vtkRenderer *);
	private:
		// generate the box
		void GenerateOutline();

		// attributes
		//
		// wireframe outline
		vtkSmartPointer<vtkPoints> 				Points;
		vtkSmartPointer<vtkActor> 				HexOutline;
		vtkSmartPointer<vtkPolyDataMapper> 		OutlineMapper;
		vtkSmartPointer<vtkPolyData> 			OutlinePolyData;
		//
		// outline properties
		vtkSmartPointer<vtkProperty> 			OutlineProperty;
		//
		// current renderer
		vtkSmartPointer<vtkRenderer> 			Renderer;
};

#endif // _BOXSELECTIONREPRESENTATION3D_H_
