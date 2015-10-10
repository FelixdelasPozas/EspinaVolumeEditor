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
		static BoxSelectionRepresentation2D *New();

		vtkTypeMacro(BoxSelectionRepresentation2D,vtkWidgetRepresentation);

		void PrintSelf(ostream& os, vtkIndent indent);

		/** \brief Specify opposite corners of the box defining the boundary of the
		 * widget. Position is the lower left of the outline, and Position2 is
		 * the upper right, and is not relative to Position.
		 * The rectangle (Position,Position2) is a bounding rectangle.
		 *
		 */
		vtkViewportCoordinateMacro(Position);
		vtkViewportCoordinateMacro(Position2);

		enum class BorderType: char	{ BORDER_OFF = 0, BORDER_ON, BORDER_ACTIVE };

		/** \brief Specify the properties of the border.
		 *
		 */
		vtkGetObjectMacro(m_widgetActorProperty,vtkProperty);

		/** \brief Specify a minimum and/or maximum size (in world coordinates) that this representation
		 * can take. These methods require two values: size values in the x and y directions, respectively.
		 *
		 */
		vtkSetVector2Macro(m_minimumSize,double);
		vtkGetVector2Macro(m_minimumSize,double);
		vtkSetVector2Macro(m_maximumSize,double);
		vtkGetVector2Macro(m_maximumSize,double);

		/** \brief This is a modifier of the interaction state. When set, widget interaction
		 * allows the border (and stuff inside of it) to be translated with mouse motion.
		 *
		 */
		vtkSetMacro(m_moving,int);
		vtkGetMacro(m_moving,int);
		vtkBooleanMacro(m_moving,int);

		/** \brief Minimum and maximum selection limits (the slice size with the spacing border).
		 *
		 */
		vtkSetVector2Macro(m_minimumSelectionSize,double);
		vtkGetVector2Macro(m_minimumSelectionSize,double);
		vtkSetVector2Macro(m_maximumSelectionSize,double);
		vtkGetVector2Macro(m_maximumSelectionSize,double);

		/** \brief Image spacing.
		 *
		 */
		vtkSetVector2Macro(m_spacing,double);
		vtkGetVector2Macro(m_spacing,double);

		// Description:
		// Define the various states that the representation can be in.
		enum WidgetState { Outside = 0, Inside, AdjustingP0, AdjustingP1, AdjustingP2, AdjustingP3, AdjustingE0, AdjustingE1, AdjustingE2, AdjustingE3 };

		/** \brief Subclasses should implement these methods. See the superclasses'
		 * Subclasses should implement these methods. See the superclasses'
		 * documentation for more information.
		 *
		 */
		virtual void BuildRepresentation();
		virtual void StartWidgetInteraction(double eventPos[2]);
		virtual void WidgetInteraction(double eventPos[2]);
		virtual int ComputeInteractionState(int X, int Y, int modify = 0);

		/** \brief These methods are necessary to make this representation behave as
		 * a vtkProp.
		 *
		 */
		virtual void GetActors2D(vtkPropCollection*);
		virtual void ReleaseGraphicsResources(vtkWindow*);
		virtual int RenderOverlay(vtkViewport*);
		virtual int RenderOpaqueGeometry(vtkViewport*);
		virtual int RenderTranslucentPolygonalGeometry(vtkViewport*);
		virtual int HasTranslucentPolygonalGeometry();

		/** \brief Transforms display coords to world coordinates.
		 * \param[inout] x display coordinate x.
		 * \param[inout] y display coordinate y.
		 *
		 */
		void TransformToWorldCoordinates(double *x, double *y);

		/** \brief Sets the box size.
		 * \param[in] x1 first point x coordinate.
     * \param[in] y1 first point y coordinate.
     * \param[in] x2 second point x coordinate.
     * \param[in] y2 second point y coordinate.
		 *
		 */
		void SetBoxCoordinates(const int x1, const int y1, const int x2, const int y2);
	protected:
		/** \brief BoxSelectionRepresentation2D class constructor.
		 *
		 */
		BoxSelectionRepresentation2D();

		/** \brief BoxSelectionRepresentation2D class destructor.
		 *
		 */
		virtual ~BoxSelectionRepresentation2D();

		/** \brief Helper method to get the distance between two double numbers.
		 * In this method the former value is always smaller then the latter.
		 *
		 */
		double Distance(double, double);

		vtkProperty	*m_widgetActorProperty; /** properties of the box actor. */
		int          m_moving;              /** state modifier when moving the widget. */
		double       m_selectionPoint[2];   /** selection point when clicking on the widget. */
		double       m_startPosition[2];    /** Keep track of start position when moving border. */

		vtkCoordinate *PositionCoordinate;  /** lower left. */
		vtkCoordinate *Position2Coordinate; /** upper right. */

		vtkSmartPointer<vtkPolyData>       m_polyData; /** border polydata. */
		vtkSmartPointer<vtkPolyDataMapper> m_mapper;   /** border actor mapper. */
		vtkSmartPointer<vtkActor>          m_actor;    /** border actor. */

		double m_minimumSize[2]; /** minimum box size. */
		double m_maximumSize[2]; /** maximum box size. */

		double m_minimumSelectionSize[2]; /** minimum box selection size. */
		double m_maximumSelectionSize[2]; /** maximum box selection size. */

		double m_spacing[2]; /** image spacing needed for correct widget placing. */
		double m_tolerance;  /** edge tolerance in world coordinates for the edges. */
	private:
		BoxSelectionRepresentation2D(const BoxSelectionRepresentation2D&) = delete;
		void operator=(const BoxSelectionRepresentation2D&) = delete;
};

#endif // _BOXSELECTIONREPRESENTATION2D_H_
