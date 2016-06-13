///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ContourRepresentation.h
// Purpose: Superclass of ContourRepresentationGlyph
// Notes: Modified from vtkContourRepresentation class in vtk 5.8
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CONTOURREPRESENTATION_H_
#define _CONTOURREPRESENTATION_H_

// vtk includes
#include <vtkContourRepresentation.h>
#include <vtkSmartPointer.h>

// C++
#include <memory>

// forward declarations
class vtkPolyData;

///////////////////////////////////////////////////////////////////////////////////////////////////
// ContourRepresentation class
//
class ContourRepresentationNode
{
	public:
		double WorldPosition[3];   /** node's position in world coordinates.              */
		int    Selected;           /** 0 to select, other value otherwise.                */
};

class ContourRepresentationInternals
{
	public:
		std::vector<ContourRepresentationNode*> Nodes; /** list of nodes composing the contour. */

		/** \brief Method to clear the contour nodes.
		 *
		 */
		void ClearNodes()
		{
			for (unsigned int i = 0; i < this->Nodes.size(); i++)
			{
				delete this->Nodes[i];
			}
			this->Nodes.clear();
		}
};

class ContourRepresentation
: public vtkWidgetRepresentation
{
		friend class ContourWidget;

	public:
		vtkTypeMacro(ContourRepresentation,vtkWidgetRepresentation);

		void PrintSelf(ostream& os, vtkIndent indent);

		virtual int AddNodeAtWorldPosition(double x, double y, double z);
		virtual int AddNodeAtWorldPosition(double worldPos[3]);

		virtual int AddNodeAtDisplayPosition(int X, int Y);
		virtual int AddNodeAtDisplayPosition(int displayPos[2]);

		virtual int ActivateNode(int displayPos[2]);
		virtual int ActivateNode(int X, int Y);

		virtual int SetActiveNodeToWorldPosition(double pos[3]);
		virtual int SetActiveNodeToDisplayPosition(int pos[2]);

		virtual int ToggleActiveNodeSelected();
		virtual int GetActiveNodeSelected();
		virtual int GetNthNodeSelected(int);
		virtual int SetNthNodeSelected(int);

		virtual int GetActiveNodeWorldPosition(double pos[3]);
		virtual int GetActiveNodeDisplayPosition(int pos[2]);

		virtual int GetNumberOfNodes();

		virtual int GetNthNodeDisplayPosition(int n, int pos[2]);
		virtual int GetNthNodeWorldPosition(int n, double pos[3]);

		virtual int SetNthNodeDisplayPosition(int n, int X, int Y);
		virtual int SetNthNodeDisplayPosition(int n, int pos[2]);

		virtual int SetNthNodeWorldPosition(int n, double pos[3]);

		virtual int GetNthNodeSlope(int idx, double slope[3]);

		virtual int DeleteLastNode();
		virtual int DeleteActiveNode();
		virtual int DeleteNthNode(int n);

		virtual void ClearAllNodes();

		virtual int AddNodeOnContour(int X, int Y);

		vtkSetClampMacro(PixelTolerance,int,1,100);
		vtkGetMacro(PixelTolerance,int);

		vtkSetClampMacro(WorldTolerance, double, 0.0, VTK_DOUBLE_MAX);
		vtkGetMacro(WorldTolerance, double);

		enum { Outside = 0, Nearby, Inside, NearContour, NearPoint };

		enum { Inactive = 0, Translate, Shift, Scale };

		vtkGetMacro(CurrentOperation, int);
		vtkSetClampMacro(CurrentOperation, int,ContourRepresentation::Inactive,ContourRepresentation::Scale );

		void SetCurrentOperationToInactive()
		{
			this->SetCurrentOperation(Inactive);
		}
		void SetCurrentOperationToTranslate()
		{
			this->SetCurrentOperation(Translate);
		}
		void SetCurrentOperationToShift()
		{
			this->SetCurrentOperation(Shift);
		}
		void SetCurrentOperationToScale()
		{
			this->SetCurrentOperation(Scale);
		}

		/** \brief These are methods that satisfy vtkWidgetRepresentation's API.
		 *
		 */
		virtual void BuildRepresentation() = 0;
		virtual int  ComputeInteractionState(int X, int Y, int modified = 0) = 0;
		virtual void StartWidgetInteraction(double e[2]) = 0;
		virtual void WidgetInteraction(double e[2]) = 0;

		/** \brief Methods required by vtkProp superclass.
		 *
		 */
		virtual void ReleaseGraphicsResources(vtkWindow *w) = 0;
		virtual int  RenderOverlay(vtkViewport *viewport) = 0;
		virtual int  RenderOpaqueGeometry(vtkViewport *viewport) = 0;
		virtual int  RenderTranslucentPolygonalGeometry(vtkViewport *viewport) = 0;
		virtual int  HasTranslucentPolygonalGeometry()=0;

		void SetClosedLoop(int val);
		vtkGetMacro(ClosedLoop, int);
		vtkBooleanMacro(ClosedLoop, int);

		virtual void SetShowSelectedNodes(int);vtkGetMacro(ShowSelectedNodes, int);
		vtkBooleanMacro(ShowSelectedNodes, int);

		/** \brief Get the points in this contour as a vtkPolyData.
		 *
		 */
		virtual vtkSmartPointer<vtkPolyData> GetContourPolyData() = 0;

    void GetWorldFromDisplay(int displayPos[2], double worldPos[3]);
    void GetDisplayFromWorld(double worldPos[3], int displayPos[2]);

	protected:
		/** \brief ContourRepresentation class constructor.
		 *
		 */
		ContourRepresentation();

		/** \brief ContourRepresentation class virtual destructor.
		 *
		 */
		virtual ~ContourRepresentation();

		int PixelTolerance;    /** pixel tolerance for the handles. */
		double WorldTolerance; /** world tolerance for the handles. */

		int ActiveNode; /** index of the current active node. */

		int CurrentOperation;  /** current operation of the widget. */
		int ClosedLoop;        /** 0 if loop is not closed, and other value otherwise. */
		int ShowSelectedNodes; /** A flag to indicate whether to show the selected nodes. */

		std::shared_ptr<ContourRepresentationInternals> Internal; /** internal representation of the contour. */

		void AddNodeAtPositionInternal(double worldPos[3]);
		void AddNodeAtPositionInternal(int displayPos[2]);
		void SetNthNodeWorldPositionInternal(int n, double worldPos[3]);

		virtual int FindClosestPointOnContour(int X, int Y, double worldPos[3], int *idx);

		/** \brief Builds the lines between nodes.
		 *
		 */
		virtual void BuildLines() = 0;

		vtkTimeStamp ContourBuildTime; /** contour build time stamp to check modifications. */

		/** \brief Compute the middle point between two points.
		 * \param[in] p1 point 1 world coordinates.
		 * \param[in] p2 point 2 world coordinates.
		 * \param[out] mid middle point world coordinates.
		 */
		void Midpoint(double p1[3], double p2[3], double mid[3])
		{
			mid[0] = (p1[0] + p2[0]) / 2;
			mid[1] = (p1[1] + p2[1]) / 2;
			mid[2] = (p1[2] + p2[2]) / 2;
		}

		virtual void Initialize(vtkPolyData *polydata);

		/** \brief Iterates over the contour lines to check if line segments intersect, if so then deletes useless points
		 * and closes the contour in the intersection point. checks for intersection with segment (n-2, n-1)
		 *
		 */
		bool CheckAndCutContourIntersection(void);

		/** \brief Iterates over the contour lines to check if line segments intersect,
		 *  if so then deletes useless points and closes the contour in the intersection point.
		 *  Checks for intersection with segments (n-2, n-1) and (n-1,0)
		 *
		 */
		bool CheckAndCutContourIntersectionInFinalPoint(void);

		/** \brief Same as previous method, but just checks (node-1, node) and (node, node+1).
		 * used only when translating node the node after the contour has been defined.
		 * \param[in] node index of the node to check.
		 *
		 */
		bool CheckContourIntersection(int node);

		/** \brief Returns true if nodes 1 and 2 have the same coordinates.
		 * \param[in] node1 first node index.
		 * \param[in] node2 second node index.
		 *
		 */
		bool CheckNodesForDuplicates(int node1, int node2);

		/** \brief Implements shooting algorithm to know if a point is inside a closed polygon.
		 * \param[in] X x display coordinate.
		 * \param[in] Y y display coordinate.
		 *
		 */
		bool ShootingAlgorithm(int X, int Y);

		/** \brief Returns the shortest distance to the contour given a point.
		 * \param[in] X x point coordinate in display coordinates.
		 * \param[in] Y y point coordinate in display coordinates.
		 *
		 */
		double FindClosestDistanceToContour(int X, int Y);
	private:
		ContourRepresentation(const ContourRepresentation&) = delete;
		void operator=(const ContourRepresentation&) = delete;

		/** \brief Checks if the lines of the node intersect with any other line of the contour.
		 * \param[in] node index of the node to check.
		 * \param[out] intersectionPoints coordinates of the intersection point.
		 * \param[out] intersectionNode intersection node.
		 *
		 */
		bool LineIntersection(int node, double *intersectionPoint, int *intersectionNode);

    /** \brief Removes duplicated nodes of the contour.
     *
     */
		void RemoveDuplicatedNodes();

    /** \brief Checks the intersection between two nodes.
     * \param[in] node1 node index.
     * \param[in] node2 node index.
     *
     */
		bool NodesIntersection(int node1, int node2);

    /** \brief Translates all the nodes of the contour in the plane by the given
     * vector coordinates.
     *
     */
		void TranslatePoints(double *coordinates);
};

#endif // _CONTOURREPRESENTATION_H_
