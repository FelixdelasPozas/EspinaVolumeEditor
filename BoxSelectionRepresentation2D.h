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
#include <vtkCoordinate.h>

// forward declarations
class vtkPoints;
class vtkPolyData;
class vtkCellArray;
class vtkPolyDataMapper;
class vtkActor;
class vtkProperty;

///////////////////////////////////////////////////////////////////////////////////////////////////
// BoxSelectionRepresentation class
//
class VTK_WIDGETS_EXPORT BoxSelectionRepresentation2D: public vtkWidgetRepresentation
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
		// widget. By default, these coordinates are in the normalized viewport
		// coordinate system, with Position the lower left of the outline, and
		// Position2 relative to Position. Note that using these methods are
		// affected by the ProportionalResize flag. That is, if the aspect ratio of
		// the representation is to be preserved (e.g., ProportionalResize is on),
		// then the rectangle (Position,Position2) is a bounding rectangle. Also,
		vtkViewportCoordinateMacro(Position);
		vtkViewportCoordinateMacro(Position2);

//BTX
		enum
		{
			BORDER_OFF = 0, BORDER_ON, BORDER_ACTIVE
		};
//ETX
		// Border is always on, see vtkBorderRepresentation for details of what has changed
		vtkSetClampMacro(ShowBorder,int,BORDER_OFF,BORDER_ACTIVE);
		vtkGetMacro(ShowBorder,int);

		// Description:
		// Specify the properties of the border.
		vtkGetObjectMacro(BorderProperty,vtkProperty);

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

		// minimum and maximum selection limits (the slice size with the spacing border
		vtkSetVector2Macro(MinimumSelection,double);
		vtkGetVector2Macro(MinimumSelection,double);
		vtkSetVector2Macro(MaximumSelection,double);
		vtkGetVector2Macro(MaximumSelection,double);

		// image spacing
		vtkSetVector2Macro(Spacing,double);
		vtkGetVector2Macro(Spacing,double);

//BTX
		// Description:
		// Define the various states that the representation can be in.
		enum _InteractionState
		{
			Outside = 0, Inside, AdjustingP0, AdjustingP1, AdjustingP2, AdjustingP3, AdjustingE0, AdjustingE1, AdjustingE2, AdjustingE3
		};
//ETX

		// Description:
		// Subclasses should implement these methods. See the superclasses'
		// documentation for more information.
		virtual void BuildRepresentation();
		virtual void StartWidgetInteraction(double eventPos[2]);
		virtual void WidgetInteraction(double eventPos[2]);
		virtual void GetSize(double size[2])
		{
			size[0] = 1.0;
			size[1] = 1.0;
		}
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

		// aux methods
		double Distance(double, double); // in this method the former value is always smaller then the latter

		// Ivars
		int ShowBorder;
		vtkProperty *BorderProperty;
		int Moving;
		double SelectionPoint[2];

		// Layout (position of lower left and upper right corners of border)
		vtkCoordinate *PositionCoordinate;
		vtkCoordinate *Position2Coordinate;

		// Sometimes subclasses must negotiate with their superclasses
		// to achieve the correct layout.
		int Negotiated;
		virtual void NegotiateLayout();

		// Keep track of start position when moving border
		double StartPosition[2];

		// Border representation. Subclasses may use the BWTransform class
		// to transform their geometry into the region surrounded by the border.
		vtkPolyData *BWPolyData;
		vtkPolyDataMapper *BWMapper;
		vtkActor *BWActor;

		// Constraints on size
		double MinimumSize[2];
		double MaximumSize[2];

		// Constraints on selection limits
		double MinimumSelection[2];
		double MaximumSelection[2];

		double Spacing[2];
		double _tolerance;

	private:
		BoxSelectionRepresentation2D(const BoxSelectionRepresentation2D&);   //Not implemented
		void operator=(const BoxSelectionRepresentation2D&);   //Not implemented
};

#endif // _BOXSELECTIONREPRESENTATION2D_H_
