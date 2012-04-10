///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: BoxSelectionWidget.h
// Purpose: widget that manages interaction of a box in one of the planes
// Notes: Ripped from vtkBorderWidget class. No selectable.
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _BOXSELECTIONWIDGET_H_
#define _BOXSELECTIONWIDGET_H_

// vtk includes
#include <vtkAbstractWidget.h>

// forward declarations
class BoxSelectionRepresentation2D;

///////////////////////////////////////////////////////////////////////////////////////////////////
// BoxSelectionWidget class
//
class VTK_WIDGETS_EXPORT BoxSelectionWidget: public vtkAbstractWidget
{
	public:
		// Description:
		// Method to instantiate class.
		static BoxSelectionWidget *New();

		// Description;
		// Standard methods for class.
		vtkTypeMacro(BoxSelectionWidget,vtkAbstractWidget);
		void PrintSelf(ostream& os, vtkIndent indent);

		// Description:
		// Specify an instance of vtkWidgetRepresentation used to represent this
		// widget in the scene. Note that the representation is a subclass of vtkProp
		// so it can be added to the renderer independent of the widget.
		void SetRepresentation(BoxSelectionRepresentation2D *r)
		{
			this->Superclass::SetWidgetRepresentation(reinterpret_cast<vtkWidgetRepresentation*>(r));
		}

		// Description:
		// Return the representation as a vtkBorderRepresentation.
		BoxSelectionRepresentation2D *GetBorderRepresentation()
		{
			return reinterpret_cast<BoxSelectionRepresentation2D*>(this->WidgetRep);
		}

		// Description:
		// Create the default widget representation if one is not set.
		virtual void CreateDefaultRepresentation();

		// Description:
		// Enables/Disables widget interaction
		virtual void SetEnabled(int);
	protected:
		BoxSelectionWidget();
		~BoxSelectionWidget();

		// processes the registered events
		static void SelectAction(vtkAbstractWidget*);
		static void TranslateAction(vtkAbstractWidget*);
		static void EndSelectAction(vtkAbstractWidget*);
		static void MoveAction(vtkAbstractWidget*);

		// helper methods for cursor management
		virtual void SetCursor(int State);

		//widget state
		int WidgetState;
		enum _WidgetState
		{
			Start = 0, Define, Manipulate, Selected
		};

	private:
		BoxSelectionWidget(const BoxSelectionWidget&);   //Not implemented
		void operator=(const BoxSelectionWidget&);   //Not implemented
};

#endif
