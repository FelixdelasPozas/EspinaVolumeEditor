///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ContourRepresentation.h
// Purpose: Manages lasso and polygon manual selection representations
// Notes: Modified from vtkOrientedGlyphContourRepresentation class in vtk 5.8
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CONTOURREPRESENTATIONGLYPH_H_
#define _CONTOURREPRESENTATIONGLYPH_H_

// project includes
#include "ContourRepresentation.h"

// vtk
#include <vtkSmartPointer.h>

// forward declarations
class vtkProperty;
class vtkActor;
class vtkPolyDataMapper;
class vtkPolyData;
class vtkGlyph3D;
class vtkPoints;

///////////////////////////////////////////////////////////////////////////////////////////////////
// ContourRepresentation class
//
class ContourRepresentationGlyph
: public ContourRepresentation
{
	public:
		static ContourRepresentationGlyph *New();

		vtkTypeMacro(ContourRepresentationGlyph,ContourRepresentation);
		void PrintSelf(ostream& os, vtkIndent indent);

		/** \brief Specify the cursor shape. Keep in mind that the shape will be
		 * aligned with the  constraining plane by orienting it such that
		 * the x axis of the geometry lies along the normal of the plane.
		 * \param[in] cursorShape shape polydata.
		 *
		 */
		void SetCursorShape(vtkSmartPointer<vtkPolyData> cursorShape);

		/** \brief Returns a pointer to the cursor shape polydata.
		 *
		 */
		vtkSmartPointer<vtkPolyData> GetCursorShape() const;

		/** \brief Specify the shape of the cursor (handle) when it is active.
		 * This is the geometry that will be used when the mouse is
		 * close to the handle or if the user is manipulating the handle.
     * \param[in] activeShape shape polydata.
		 *
		 */
		void SetActiveCursorShape(vtkSmartPointer<vtkPolyData> activeShape);

		/** \brief Returns a pointer to the active shape polydata.
		 *
		 */
		vtkSmartPointer<vtkPolyData> GetActiveCursorShape() const;

		/** \brief Property used when the handle is not active.
		 * (the mouse is not near the handle)
		 *
		 */
		vtkGetObjectMacro(Property,vtkProperty);

		/** \brief Property used when the user is interacting with the handle.
		 *
		 */
		vtkGetObjectMacro(ActiveProperty,vtkProperty);

		/** \brief Property used by the lines.
		 *
		 */
		vtkGetObjectMacro(LinesProperty,vtkProperty);

		/** \brief Subclasses of ContourRepresentationGlyph must implement these methods. These
		 * are the methods that the widget and its representation use to
		 * communicate with each other.
		 *
		 */
		virtual void SetRenderer(vtkRenderer *ren);
		virtual void BuildRepresentation();
		virtual void StartWidgetInteraction(double eventPos[2]);
		virtual void WidgetInteraction(double eventPos[2]);
		virtual int ComputeInteractionState(int X, int Y, int modified = 0);

		/** \brief Methods to make this class behave as a vtkProp.
		 *
		 */
		virtual void GetActors(vtkPropCollection *);
		virtual void ReleaseGraphicsResources(vtkWindow *);
		virtual int RenderOverlay(vtkViewport *viewport);
		virtual int RenderOpaqueGeometry(vtkViewport *viewport);
		virtual int RenderTranslucentPolygonalGeometry(vtkViewport *viewport);
		virtual int HasTranslucentPolygonalGeometry();

		/** \brief Returns the contour as a polydata.
		 *
		 */
		virtual vtkPolyData *GetContourRepresentationAsPolyData();

		/** \brief Controls whether the contour widget should always appear on top
		 * of other actors in the scene. (In effect, this will disable OpenGL
		 * Depth buffer tests while rendering the contour).
		 * Default is to set it to false.
		 *
		 */
		vtkSetMacro( AlwaysOnTop, int );
		vtkGetMacro( AlwaysOnTop, int );
		vtkBooleanMacro( AlwaysOnTop, int );

		/** \brief Sets the colour of the contour's lines.
		 * \param[in] r red color value in [0,1]
     * \param[in] g green color value in [0,1]
     * \param[in] b blue color value in [0,1]
		 */
		void SetLineColor(double r, double g, double b);

		/** \brief A flag to indicate whether to show the Selected nodes
		 * Default is to set it to false.
		 * \parma[in] value 0 for false and other value for otherwise.
		 *
		 */
		virtual void SetShowSelectedNodes(int value);

		/** \brief Returns the bounds of the representation.
		 *
		 */
		virtual double *GetBounds() const;

		/** \brief Spacing is needed for interaction calculations, together with bounds.
		 *
		 */
		vtkSetVector2Macro(Spacing, double);

	protected:
		/** \brief ContourRepresentationGlyph class constructor.
		 *
		 */
		ContourRepresentationGlyph();

		vtkSmartPointer<vtkGlyph3D>        Glypher;
		vtkSmartPointer<vtkPolyDataMapper> Mapper;
		vtkSmartPointer<vtkActor>          Actor;

		vtkSmartPointer<vtkGlyph3D>        ActiveGlypher;
		vtkSmartPointer<vtkPolyDataMapper> ActiveMapper;
		vtkSmartPointer<vtkActor>          ActiveActor;

		vtkSmartPointer<vtkPolyData>       CursorShape;
		vtkSmartPointer<vtkPolyData>       ActiveCursorShape;

	  vtkSmartPointer<vtkPoints>         FocalPoint;
		vtkSmartPointer<vtkPolyData>       FocalData;
		vtkSmartPointer<vtkPoints>         ActiveFocalPoint;
		vtkSmartPointer<vtkPolyData>       ActiveFocalData;

		vtkSmartPointer<vtkPolyData>       SelectedNodesData;
		vtkSmartPointer<vtkPoints>         SelectedNodesPoints;
		vtkSmartPointer<vtkActor>          SelectedNodesActor;
		vtkSmartPointer<vtkPolyDataMapper> SelectedNodesMapper;
		vtkSmartPointer<vtkGlyph3D>        SelectedNodesGlypher;
		vtkSmartPointer<vtkPolyData>       SelectedNodesCursorShape;

		/** \brief Creates the representation for the selectend contour points.
		 *
		 */
		void CreateSelectedNodesRepresentation();

		vtkSmartPointer<vtkPolyData>       Lines;
		vtkSmartPointer<vtkPolyDataMapper> LinesMapper;
		vtkSmartPointer<vtkActor>          LinesActor;

		double LastPickPosition[3];  /** last pick position in display coordinates. */
		double LastEventPosition[2]; /** last event position in display coordinates. */

		/** \brief Methods to manipulate the contour.
		 *
		 */
		void Translate(double eventPos[2]);
		void Scale(double eventPos[2]);
		void ShiftContour(double eventPos[2]);
		void ScaleContour(double eventPos[2]);

		/** \brief Computes the centroid of the contour.
		 * \param[out] centroid centroid coordinates.
		 *
		 */
		void ComputeCentroid(double *centroid);

		vtkProperty *Property;           /** properties of the contour actor. */
		vtkProperty *ActiveProperty;     /** properties of the active nodes actor. */
		vtkProperty *LinesProperty;      /** properties of the lines actor. */

		/** \brief Sets the default properties for the actors.
		 *
		 */
		void SetDefaultProperties();

		double InteractionOffset[2]; /** distance between the point starting the interaction and the current point. */

		int AlwaysOnTop;    /** 0 to false, other value for otherwise. */
		double Spacing[2];  /** spacing of the image needed to center the contour nodes. */

		virtual void BuildLines();

	private:
		ContourRepresentationGlyph(const ContourRepresentationGlyph&) = delete;
		void operator=(const ContourRepresentationGlyph&) = delete;
};

#endif // _CONTOURREPRESENTATIONGLYPH_H_
