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

class BoxSelectionRepresentation2D;

///////////////////////////////////////////////////////////////////////////////////////////////////
// BoxSelectionWidget class
//
class BoxSelectionWidget
: public vtkAbstractWidget
{
  public:
    static BoxSelectionWidget *New();

    vtkTypeMacro(BoxSelectionWidget,vtkAbstractWidget);

    void PrintSelf(ostream& os, vtkIndent indent);

    /** \brief Specify an instance of vtkWidgetRepresentation used to represent this
     * widget in the scene. Note that the representation is a subclass of vtkProp
     * so it can be added to the renderer independent of the widget.
     * \param[in] representation box representation to set.
     *
     */
    void SetRepresentation(BoxSelectionRepresentation2D *representation)
    {
      this->Superclass::SetWidgetRepresentation(reinterpret_cast<vtkWidgetRepresentation*>(representation));
    }

    /** \brief Returns the representation as a vtkBorderRepresentation.
     *
     */
    BoxSelectionRepresentation2D *GetBorderRepresentation() const
    {
      return reinterpret_cast<BoxSelectionRepresentation2D*>(this->WidgetRep);
    }

    virtual void CreateDefaultRepresentation();

    virtual void SetEnabled(int value);

  protected:
    /** \brief BoxSelectionWidget class constructor.
     *
     */
    BoxSelectionWidget();

    /** \brief BoxSelectionWidget class virtual destructor.
     *
     */
    ~BoxSelectionWidget();

    // processes the registered events
    static void SelectAction(vtkAbstractWidget *widget);
    static void TranslateAction(vtkAbstractWidget *widget);
    static void EndSelectAction(vtkAbstractWidget *widget);
    static void MoveAction(vtkAbstractWidget *widget);

    /** \brief Helper methods for cursor management.
     * \param[in] state widget state.
     *
     */
    virtual void SetCursor(int State);

    enum class WidgetState: char { Start = 0, Define, Manipulate, Selected };
    WidgetState m_state; /** widget state. */

  private:
    BoxSelectionWidget(const BoxSelectionWidget&) = delete;
    void operator=(const BoxSelectionWidget&) = delete;
};

#endif
