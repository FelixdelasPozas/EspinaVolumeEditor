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

// Qt includes
#include <QCursor>

// forward declarations
class ContourRepresentation;
class vtkPolyData;
class Selection;

///////////////////////////////////////////////////////////////////////////////////////////////////
// ContourWidget class
//
class ContourWidget
: public vtkAbstractWidget
{
  public:
    static ContourWidget *New();

    vtkTypeMacro(ContourWidget,vtkAbstractWidget);

    void PrintSelf(ostream& os, vtkIndent indent);

    virtual void SetEnabled(int enabling);

    /** \brief Specify an instance of vtkWidgetRepresentation used to represent this
     * widget in the scene. Note that the representation is a subclass of vtkProp
     * so it can be added to the renderer independent of the widget.
     * \param[in] representation instance of contour representation.
     *
     */
    void SetRepresentation(ContourRepresentation *representation)
    {
      this->Superclass::SetWidgetRepresentation(reinterpret_cast<vtkWidgetRepresentation*>(representation));
    }

    /** \brief Return the representation as a ContourRepresentation.
     *
     */
    ContourRepresentation *GetContourRepresentation() const
    {
      return reinterpret_cast<ContourRepresentation*>(this->WidgetRep);
    }

    void CreateDefaultRepresentation();

    /** \brief Convenient method to close the contour loop.
     *
     */
    void CloseLoop();

    /** \brief The state of the widget
     *
     */
    enum ContourWidgetState { Start = 0, Define, Manipulate };

    /** \brief Convenient methods to change/get what state the widget is in.
     *
     */
    vtkSetMacro(WidgetState,ContourWidgetState);
    vtkGetMacro(WidgetState,ContourWidgetState);

    /** \brief Set / Get the AllowNodePicking value. This ivar indicates whether the nodes
     * and points between nodes can be picked/un-picked by Ctrl+Click on the node.
     * \param[in] value 0 to enable and other value otherwise.
     *
     */
    void SetAllowNodePicking(int value);

    vtkGetMacro(AllowNodePicking, int);
    vtkBooleanMacro(AllowNodePicking, int);

    /** \brief If this is ON, during definition, the last node of the
     * contour will automatically follow the cursor, without waiting for the the
     * point to be dropped. This may be useful for some interpolators, such as the
     * live-wire interpolator to see the shape of the contour that will be placed
     * as you move the mouse cursor.
     *
     */
    vtkSetMacro(FollowCursor, int);
    vtkGetMacro(FollowCursor, int);
    vtkBooleanMacro( FollowCursor, int);

    /** \brief Define a contour by continuously drawing with the mouse cursor.
     * Press and hold the left mouse button down to continuously draw.
     * Releasing the left mouse button switches into a snap drawing mode.
     * Terminate the contour by pressing the right mouse button.  If you
     * do not want to see the nodes as they are added to the contour, set the
     * opacity to 0 of the representation's property.  If you do not want to
     * see the last active node as it is being added, set the opacity to 0
     * of the representation's active property.
     *
     */
    vtkSetMacro(ContinuousDraw, int);
    vtkGetMacro(ContinuousDraw, int);
    vtkBooleanMacro(ContinuousDraw, int);

    /** \brief Initialize the contour widget from a user supplied set of points. The
     * state of the widget decides if you are still defining the widget, or
     * if you've finished defining (added the last point) are manipulating
     * it. Note that if the polydata supplied is closed, the state will be
     * set to manipulate.
     *  \param[in] polydata polydata object with the contour points.
     *  \param[in] state Define = 0 or Manipulate = 1.
     *
     */
    virtual void Initialize(vtkPolyData * poly, int state = 1);
    virtual void Initialize()
    {
      this->Initialize(nullptr);
    }

    /** \brief Orientation of the slice where the widget is (0 = axial, 1 = coronal, 2 = sagittal)
     *
     */
    vtkGetMacro(Orientation,int);
    vtkSetMacro(Orientation,int);

  protected:
    /** \brief ContourWidget class constructor.
     *
     */
    ContourWidget();

    /** \brief ContourWidget class virtual destructor.
     *
     */
    virtual ~ContourWidget();

    ContourWidgetState WidgetState;      /** current widget state. */

    int CurrentHandle;    /** current node handle. */
    int AllowNodePicking; /** node picking enabled state. */
    int FollowCursor;     /** last node follows cursor state. */
    int ContinuousDraw;   /** continuous contour drawn state. */
    int ContinuousActive; /** continuous active state. */
    int Orientation;      /** orientation of the contour. */

    /** \brief Widget's callbacks to events.
     *
     */
    static void SelectAction(vtkAbstractWidget *widget);
    static void AddFinalPointAction(vtkAbstractWidget *widget);
    static void MoveAction(vtkAbstractWidget *widget);
    static void EndSelectAction(vtkAbstractWidget *widget);
    static void DeleteAction(vtkAbstractWidget *widget);
    static void TranslateContourAction(vtkAbstractWidget *widget);
    static void ScaleContourAction(vtkAbstractWidget *widget);
    static void ResetAction(vtkAbstractWidget *widget);
    static void KeyPressAction(vtkAbstractWidget *widget);

    /** \brief Internal method to select the closest node to the cursor.
     *
     */
    void SelectNode();

    /** \brief Internal method to add a node at the cursor position.
     *
     */
    void AddNode();

    /** \brief Helper method for cursor management.
     * \param[in] state widget state.
     *
     */
    virtual void SetCursor(int State);

    friend class Selection;
  private:
    ContourWidget(const ContourWidget&) = delete;
    void operator=(const ContourWidget&) = delete;

    QCursor crossMinusCursor, crossPlusCursor; /** cursor bitmaps to delete/add a node. */
};

#endif // _CONTOURWIDGET_H_
