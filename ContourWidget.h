///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ContourWidget.h
// Purpose: Manages lasso and polygon selection interaction
// Notes: Modified from vtkContourWidget class in vtk 5.8
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CONTOURWIDGET_H_
#define _CONTOURWIDGET_H_

// vtk includes
#include <vtkAbstractWidget.h>

// forward declarations
class ContourRepresentation;
class vtkPolyData;
class Selection;

///////////////////////////////////////////////////////////////////////////////////////////////////
// ContourWidget class
//
class VTK_WIDGETS_EXPORT ContourWidget: public vtkAbstractWidget
{
	public:
		// Description:
		// Instantiate this class.
		static ContourWidget *New();

		// Description:
		// Standard methods for a VTK class.
		vtkTypeMacro(ContourWidget,vtkAbstractWidget);
		void PrintSelf(ostream& os, vtkIndent indent);

		// Description:
		// The method for activiating and deactiviating this widget. This method
		// must be overridden because it is a composite widget and does more than
		// its superclasses' vtkAbstractWidget::SetEnabled() method.
		virtual void SetEnabled(int);

		// Description:
		// Specify an instance of vtkWidgetRepresentation used to represent this
		// widget in the scene. Note that the representation is a subclass of vtkProp
		// so it can be added to the renderer independent of the widget.
		void SetRepresentation(ContourRepresentation *r)
		{
			this->Superclass::SetWidgetRepresentation(reinterpret_cast<vtkWidgetRepresentation*>(r));
		}

		// Description:
		// Return the representation as a ContourRepresentation.
		ContourRepresentation *GetContourRepresentation()
		{
			return reinterpret_cast<ContourRepresentation*>(this->WidgetRep);
		}

		// Description:
		// Create the default widget representation if one is not set.
		void CreateDefaultRepresentation();

		// Description:
		// Convenient method to close the contour loop.
		void CloseLoop();

		// Description:
		// Convenient method to change what state the widget is in.
		vtkSetMacro(WidgetState,int);

		// Description:
		// Convenient method to determine the state of the method
		vtkGetMacro(WidgetState,int);

		// Description:
		// Set / Get the AllowNodePicking value. This ivar indicates whether the nodes
		// and points between nodes can be picked/un-picked by Ctrl+Click on the node.
		void SetAllowNodePicking(int);
		vtkGetMacro( AllowNodePicking, int );
		vtkBooleanMacro( AllowNodePicking, int );

		// Description:
		// Follow the cursor ? If this is ON, during definition, the last node of the
		// contour will automatically follow the cursor, without waiting for the the
		// point to be dropped. This may be useful for some interpolators, such as the
		// live-wire interpolator to see the shape of the contour that will be placed
		// as you move the mouse cursor.
		vtkSetMacro( FollowCursor, int );
		vtkGetMacro( FollowCursor, int );
		vtkBooleanMacro( FollowCursor, int );

		// Description:
		// Define a contour by continuously drawing with the mouse cursor.
		// Press and hold the left mouse button down to continuously draw.
		// Releasing the left mouse button switches into a snap drawing mode.
		// Terminate the contour by pressing the right mouse button.  If you
		// do not want to see the nodes as they are added to the contour, set the
		// opacity to 0 of the representation's property.  If you do not want to
		// see the last active node as it is being added, set the opacity to 0
		// of the representation's active property.
		vtkSetMacro( ContinuousDraw, int );
		vtkGetMacro( ContinuousDraw, int );
		vtkBooleanMacro( ContinuousDraw, int );

		// Description:
		// Initialize the contour widget from a user supplied set of points. The
		// state of the widget decides if you are still defining the widget, or
		// if you've finished defining (added the last point) are manipulating
		// it. Note that if the polydata supplied is closed, the state will be
		// set to manipulate.
		//  State: Define = 0, Manipulate = 1.
		virtual void Initialize(vtkPolyData * poly, int state = 1);
		virtual void Initialize()
		{
			this->Initialize(NULL);
		}

		// orientation of the slice where the widget is (0 = axial, 1 = coronal, 2 = sagittal)
		vtkGetMacro(Orientation,int);
		vtkSetMacro(Orientation,int);

	protected:
		ContourWidget();
		~ContourWidget();

		// The state of the widget
//BTX
		enum
		{
			Start, Define, Manipulate
		};

//ETX

		int WidgetState;
		int CurrentHandle;
		int AllowNodePicking;
		int FollowCursor;
		int ContinuousDraw;
		int ContinuousActive;
		int Orientation;

		// Callback interface to capture events when
		// placing the widget.
		static void SelectAction(vtkAbstractWidget*);
		static void AddFinalPointAction(vtkAbstractWidget*);
		static void MoveAction(vtkAbstractWidget*);
		static void EndSelectAction(vtkAbstractWidget*);
		static void DeleteAction(vtkAbstractWidget*);
		static void TranslateContourAction(vtkAbstractWidget*);
		static void ScaleContourAction(vtkAbstractWidget*);
		static void ResetAction(vtkAbstractWidget*);

		// Internal helper methods
		void SelectNode();
		void AddNode();

		// helper methods for cursor management
		virtual void SetCursor(int State);

		friend class Selection;
	private:
		ContourWidget(const ContourWidget&);  //Not implemented
		void operator=(const ContourWidget&);  //Not implemented
};

#endif // _CONTOURWIDGET_H_
