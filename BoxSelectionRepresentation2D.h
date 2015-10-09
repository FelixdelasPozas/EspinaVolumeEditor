///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: BoxSelectionRepresentation2D.h
// Purpose: used as BoxSelectionWidget representation in one of the planes
// Notes: Ripped from vtkBorderRepresentation class in vtk 5.8. See implementation for more details
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _BOXSELECTIONREPRESENTATION2D_H_
#define _BOXSELECTIONREPRESENTATION2D_H_

// vtk includes
#include <vtkWidgetRepresentation.h>
#include <vtkSmartPointer.h>
#include <vtkCoordinate.h>

// forward declarations
class vtkPolyData;
class vtkPolyDataMapper;
class vtkActor;
class vtkProperty;

///////////////////////////////////////////////////////////////////////////////////////////////////
// BoxSelectionRepresentation2D class
//
class BoxSelectionRepresentation2D
: public vtkWidgetRepresentation
{
	public:
		// Description:
		// Instantiate this class.
		static BoxSelectionRepresentation2D *New();

		// Description:
		// Define standard methods.
		vtkTypeMacro(BoxSelectionRepresentation2D,vtkWidgetRepresentation);
		void PrintSelf(ostream& os, vtkIndent indent);

		// Description:
		// Specify opposite corners of the box defining the boundary of the
		// widget. Position is the lower left of the outline, and Position2 is
		// the upper right, and is not relative to Position.
		// The rectangle (Position,Position2) is a bounding rectangle.
		vtkViewportCoordinateMacro(Position);
		vtkViewportCoordinateMacro(Position2);

		enum
		{
			BORDER_OFF = 0, BORDER_ON, BORDER_ACTIVE
		};

		// Description:
		// Specify the properties of the border.
		vtkGetObjectMacro(_widgetActorProperty,vtkProperty);

		// Description:
		// Specify a minimum and/or maximum size (in world coordinates) that this representation
		// can take. These methods require two values: size values in the x and y directions, respectively.
		vtkSetVector2Macro(MinimumSize,double);
		vtkGetVector2Macro(MinimumSize,double);
		vtkSetVector2Macro(MaximumSize,double);
		vtkGetVector2Macro(MaximumSize,double);

		// Description:
		// This is a modifier of the interaction state. When set, widget interaction
		// allows the border (and stuff inside of it) to be translated with mouse
		// motion.
		vtkSetMacro(Moving,int);
		vtkGetMacro(Moving,int);
		vtkBooleanMacro(Moving,int);

		// minimum and maximum selection limits (the slice size with the spacing border)
		vtkSetVector2Macro(MinimumSelectionSize,double);
		vtkGetVector2Macro(MinimumSelectionSize,double);
		vtkSetVector2Macro(MaximumSelectionSize,double);
		vtkGetVector2Macro(MaximumSelectionSize,double);

		// image spacing
		vtkSetVector2Macro(Spacing,double);
		vtkGetVector2Macro(Spacing,double);

		// Description:
		// Define the various states that the representation can be in.
		enum _InteractionState
		{
			Outside = 0, Inside, AdjustingP0, AdjustingP1, AdjustingP2, AdjustingP3, AdjustingE0, AdjustingE1, AdjustingE2, AdjustingE3
		};


		// Description:
		// Subclasses should implement these methods. See the superclasses'
		// documentation for more information.
		virtual void BuildRepresentation();
		virtual void StartWidgetInteraction(double eventPos[2]);
		virtual void WidgetInteraction(double eventPos[2]);
		virtual int ComputeInteractionState(int X, int Y, int modify = 0);

		// Description:
		// These methods are necessary to make this representation behave as
		// a vtkProp.
		virtual void GetActors2D(vtkPropCollection*);
		virtual void ReleaseGraphicsResources(vtkWindow*);
		virtual int RenderOverlay(vtkViewport*);
		virtual int RenderOpaqueGeometry(vtkViewport*);
		virtual int RenderTranslucentPolygonalGeometry(vtkViewport*);
		virtual int HasTranslucentPolygonalGeometry();
		void TransformToWorldCoordinates(double *, double *);
		void SetBoxCoordinates(int, int, int, int);
	protected:
		BoxSelectionRepresentation2D();
		~BoxSelectionRepresentation2D();

		// auxiliary methods
		double Distance(double, double); // in this method the former value is always smaller then the latter

		// Ivars
		vtkProperty							*_widgetActorProperty;
		int 								Moving;
		double 								_selectionPoint[2];

		// Layout (position of lower left and upper right corners of border)
		vtkCoordinate						*PositionCoordinate;
		vtkCoordinate 						*Position2Coordinate;

		// Keep track of start position when moving border
		double 								_startPosition[2];

		// Border representation.
		vtkSmartPointer<vtkPolyData> 		_widgetPolyData;
		vtkSmartPointer<vtkPolyDataMapper> 	_widgetMapper;
		vtkSmartPointer<vtkActor>			_widgetActor;

		// Constraints on size
		double 								MinimumSize[2];
		double 								MaximumSize[2];

		// Constraints on selection limits
		double 								MinimumSelectionSize[2];
		double 								MaximumSelectionSize[2];

		// image spacing needed for correct widget placing
		double 								Spacing[2];

		// edge tolerance in world coordinates for the edges
		double 								_tolerance;
	private:
		BoxSelectionRepresentation2D(const BoxSelectionRepresentation2D&);   //Not implemented
		void operator=(const BoxSelectionRepresentation2D&);   //Not implemented
};

#endif // _BOXSELECTIONREPRESENTATION2D_H_
