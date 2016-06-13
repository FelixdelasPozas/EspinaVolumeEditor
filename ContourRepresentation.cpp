///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ContourRepresentation.cpp
// Purpose: Superclass of ContourRepresentationGlyph
// Notes: Modified from vtkContourRepresentation class in vtk 5.8
///////////////////////////////////////////////////////////////////////////////////////////////////

// project includes
#include "ContourRepresentation.h"

// c++ includes
#include <cmath>

// vtk includes
#include <vtkCamera.h>
#include <vtkCoordinate.h>
#include <vtkHandleRepresentation.h>
#include <vtkIntArray.h>
#include <vtkInteractorObserver.h>
#include <vtkLine.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkObjectFactory.h>
#include <vtkPolyData.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkWindow.h>

// C++
#include <memory>

///////////////////////////////////////////////////////////////////////////////////////////////////
// ContourRepresentation class
//

///////////////////////////////////////////////////////////////////////////////////////////////////
ContourRepresentation::ContourRepresentation()
: vtkWidgetRepresentation()
, PixelTolerance   {15}
, WorldTolerance   {0.004}
, ActiveNode       {-1}
, CurrentOperation {ContourRepresentation::Inactive}
, ClosedLoop       {0}
, ShowSelectedNodes{0}
, Internal         {std::make_shared<ContourRepresentationInternals>()}
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
ContourRepresentation::~ContourRepresentation()
{
  this->Internal->ClearNodes();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::ClearAllNodes()
{
  this->Internal->ClearNodes();

  this->BuildLines();
  this->NeedToRender = 1;
  this->Modified();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::AddNodeAtPositionInternal(double worldPos[3])
{
  // Add a new point at this position
  auto node = new ContourRepresentationNode();
  node->WorldPosition[0] = worldPos[0];
  node->WorldPosition[1] = worldPos[1];
  node->WorldPosition[2] = 0;
  node->Selected = 0;

  this->Internal->Nodes.push_back(node);

  auto numNodes = this->GetNumberOfNodes();

  if(numNodes > 3)
  {
    // avoid inserting duplicated nodes. the fact that the last node (n-1) is the cursor and not yet in
    // the contour just complicates all this a little bit more.
    if (this->CheckNodesForDuplicates(numNodes - 2, numNodes - 3))
    {
      this->DeleteNthNode(numNodes - 3);
    }
  }

  this->NeedToRender = 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::AddNodeAtPositionInternal(int displayPos[2])
{
  double worldPos[3];
  this->GetWorldFromDisplay(displayPos, worldPos);

  this->AddNodeAtPositionInternal(worldPos);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::AddNodeAtWorldPosition(double worldPos[3])
{
  this->AddNodeAtPositionInternal(worldPos);

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::AddNodeAtWorldPosition(double x, double y, double z)
{
  double worldPos[3] { x, y, z };
  return this->AddNodeAtWorldPosition(worldPos);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::AddNodeAtDisplayPosition(int displayPos[2])
{
  double worldPos[3];
  this->GetWorldFromDisplay(displayPos, worldPos);

  this->AddNodeAtPositionInternal(worldPos);

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::AddNodeAtDisplayPosition(int X, int Y)
{
  int displayPos[2]{X,Y};

  return this->AddNodeAtDisplayPosition(displayPos);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::ActivateNode(int displayPos[2])
{
  // Find closest node to this display pos that
  // is within PixelTolerance
  auto closestDistance2 = VTK_DOUBLE_MAX;

  int selected = VTK_INT_MAX;
  for(int i = 0; i < GetNumberOfNodes(); ++i)
  {
    int pointPos[2];
    GetNthNodeDisplayPosition(i, pointPos);

    auto distance = ((displayPos[0] - pointPos[0]) * (displayPos[0] - pointPos[0])) +
                    ((displayPos[1] - pointPos[1]) * (displayPos[1] - pointPos[1]));

    if(distance < closestDistance2)
    {
      closestDistance2 = distance;
      selected = i;
    }
  }

  if(closestDistance2 > this->PixelTolerance * this->PixelTolerance)
  {
    this->ActiveNode = -1;
    this->NeedToRenderOn();
  }
  else
  {
    if ((selected < VTK_INT_MAX) && (selected != this->ActiveNode))
    {
      this->ActiveNode = selected;
      this->NeedToRender = 1;
    }
  }

  return (this->ActiveNode >= 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::ActivateNode(int X, int Y)
{
  int displayPos[2]{X,Y};

  return this->ActivateNode(displayPos);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::SetActiveNodeToWorldPosition(double worldPos[3])
{
  if (this->ActiveNode < 0 || static_cast<unsigned int>(this->ActiveNode) >= this->Internal->Nodes.size()) return 0;

  this->SetNthNodeWorldPositionInternal(this->ActiveNode, worldPos);

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::SetActiveNodeToDisplayPosition(int displayPos[2])
{
  if (this->ActiveNode < 0 || static_cast<unsigned int>(this->ActiveNode) >= this->Internal->Nodes.size()) return 0;

  double worldPos[3];
  this->GetWorldFromDisplay(displayPos, worldPos);

  this->SetNthNodeWorldPositionInternal(this->ActiveNode, worldPos);

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::ToggleActiveNodeSelected()
{
  if (this->ActiveNode < 0 || static_cast<unsigned int>(this->ActiveNode) >= this->Internal->Nodes.size())
  {
    // Failed to toggle the value
    return 0;
  }

  this->Internal->Nodes[this->ActiveNode]->Selected = this->Internal->Nodes[this->ActiveNode]->Selected ? 0 : 1;
  this->NeedToRender = 1;
  this->Modified();
  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::GetNthNodeSelected(int n)
{
  if (n < 0 || static_cast<unsigned int>(n) >= this->Internal->Nodes.size())
  {
    // This case is considered not Selected.
    return 0;
  }

  return this->Internal->Nodes[n]->Selected;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::SetNthNodeSelected(int n)
{
  if (n < 0 || static_cast<unsigned int>(n) >= this->Internal->Nodes.size())
  {
    // Failed.
    return 0;
  }

  auto val = n > 0 ? 1 : 0;

  if (this->Internal->Nodes[n]->Selected != val)
  {
    this->Internal->Nodes[n]->Selected = val;
    this->NeedToRender = 1;
    this->Modified();
  }

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::GetActiveNodeSelected()
{
  return this->GetNthNodeSelected(this->ActiveNode);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::GetActiveNodeWorldPosition(double pos[3])
{
  return this->GetNthNodeWorldPosition(this->ActiveNode, pos);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::GetActiveNodeDisplayPosition(int pos[2])
{
  return this->GetNthNodeDisplayPosition(this->ActiveNode, pos);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::GetNumberOfNodes()
{
  return static_cast<int>(this->Internal->Nodes.size());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::GetNthNodeDisplayPosition(int n, int displayPos[2])
{
  if (n < 0 || static_cast<unsigned int>(n) >= this->Internal->Nodes.size()) return 0;

  double worldPos[3];
  this->GetNthNodeWorldPosition(n, worldPos);
  this->GetDisplayFromWorld(worldPos, displayPos);

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::GetNthNodeWorldPosition(int n, double worldPos[3])
{
  if (n < 0 || static_cast<unsigned int>(n) >= this->Internal->Nodes.size()) return 0;

  memcpy(worldPos, this->Internal->Nodes[n]->WorldPosition, 3* sizeof(double));

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::SetNthNodeWorldPositionInternal(int n, double worldPos[3])
{
  memcpy(this->Internal->Nodes[n]->WorldPosition, worldPos, 3*sizeof(double));

  this->NeedToRender = 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::SetNthNodeWorldPosition(int n, double worldPos[3])
{
  if (n < 0 || static_cast<unsigned int>(n) >= this->Internal->Nodes.size()) return 0;

  this->SetNthNodeWorldPositionInternal(n, worldPos);

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::SetNthNodeDisplayPosition(int n, int displayPos[2])
{
  if (n < 0 || static_cast<unsigned int>(n) >= this->Internal->Nodes.size()) return 0;

  double worldPos[3];
  this->GetWorldFromDisplay(displayPos, worldPos);
  this->SetNthNodeWorldPosition(n, worldPos);

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::SetNthNodeDisplayPosition(int n, int X, int Y)
{
  int displayPos[2]{X,Y};

  return this->SetNthNodeDisplayPosition(n, displayPos);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::FindClosestPointOnContour(int X, int Y, double closestWorldPos[3], int *idx)
{
  // Make a line out of this viewing ray
  double p1[4], p2[4], *p3 = nullptr, *p4 = nullptr;

  double tmp1[4]{ static_cast<double>(X), static_cast<double>(Y), 0.0, 0.0 };
  double tmp2[4];

  this->Renderer->SetDisplayPoint(tmp1);
  this->Renderer->DisplayToWorld();
  this->Renderer->GetWorldPoint(p1);

  tmp1[2] = 1.0;
  this->Renderer->SetDisplayPoint(tmp1);
  this->Renderer->DisplayToWorld();
  this->Renderer->GetWorldPoint(p2);

  auto closestDistance2 = VTK_DOUBLE_MAX;
  auto closestNode = 0;

  // compute a world tolerance based on pixel
  // tolerance on the focal plane
  double fp[4];
  this->Renderer->GetActiveCamera()->GetFocalPoint(fp);
  fp[3] = 1.0;
  this->Renderer->SetWorldPoint(fp);
  this->Renderer->WorldToDisplay();
  this->Renderer->GetDisplayPoint(tmp1);

  tmp1[0] = 0;
  tmp1[1] = 0;
  this->Renderer->SetDisplayPoint(tmp1);
  this->Renderer->DisplayToWorld();
  this->Renderer->GetWorldPoint(tmp2);

  tmp1[0] = this->PixelTolerance;
  this->Renderer->SetDisplayPoint(tmp1);
  this->Renderer->DisplayToWorld();
  this->Renderer->GetWorldPoint(tmp1);

  auto wt2 = vtkMath::Distance2BetweenPoints(tmp1, tmp2);

  // Now loop through all lines and look for closest one within tolerance
  for (unsigned int i = 0; i < this->Internal->Nodes.size(); i++)
  {
    p3 = this->Internal->Nodes[i]->WorldPosition;
    if (i < this->Internal->Nodes.size() - 1)
    {
      p4 = this->Internal->Nodes[i + 1]->WorldPosition;
    }
    else
    {
      if (this->ClosedLoop)
      {
        p4 = this->Internal->Nodes[0]->WorldPosition;
      }
    }

    // Now we have the four points - check closest intersection
    double u, v;

    if (vtkLine::Intersection(p1, p2, p3, p4, u, v))
    {
      double p5[3], p6[3];
      p5[0] = p1[0] + u * (p2[0] - p1[0]);
      p5[1] = p1[1] + u * (p2[1] - p1[1]);
      p5[2] = p1[2] + u * (p2[2] - p1[2]);

      p6[0] = p3[0] + v * (p4[0] - p3[0]);
      p6[1] = p3[1] + v * (p4[1] - p3[1]);
      p6[2] = p3[2] + v * (p4[2] - p3[2]);

      auto d = vtkMath::Distance2BetweenPoints(p5, p6);

      if (d < wt2 && d < closestDistance2)
      {
        closestWorldPos[0] = p6[0];
        closestWorldPos[1] = p6[1];
        closestWorldPos[2] = p6[2];
        closestDistance2 = d;
        closestNode = static_cast<int>(i);
      }
    }
    else
    {
      auto d = vtkLine::DistanceToLine(p3, p1, p2);
      if (d < wt2 && d < closestDistance2)
      {
        closestWorldPos[0] = p3[0];
        closestWorldPos[1] = p3[1];
        closestWorldPos[2] = p3[2];
        closestDistance2 = d;
        closestNode = static_cast<int>(i);
      }

      d = vtkLine::DistanceToLine(p4, p1, p2);
      if (d < wt2 && d < closestDistance2)
      {
        closestWorldPos[0] = p4[0];
        closestWorldPos[1] = p4[1];
        closestWorldPos[2] = p4[2];
        closestDistance2 = d;
        closestNode = static_cast<int>(i);
      }
    }
  }

  if (closestDistance2 < VTK_DOUBLE_MAX)
  {
    if (closestNode < this->GetNumberOfNodes() - 1)
    {
      *idx = closestNode + 1;
      return 1;
    }
    else if (this->ClosedLoop)
    {
      *idx = 0;
      return 1;
    }
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::AddNodeOnContour(int X, int Y)
{
  int idx;
  double worldPos[3];

  // Compute the world position from the display position based on the concrete representation's constraints
  // If this is not a valid display location return 0
  double displayPos[2]{ static_cast<double>(X), static_cast<double>(Y) };

  if (!this->FindClosestPointOnContour(X, Y, worldPos, &idx)) return 0;

  // Add a new point at this position
  auto node = new ContourRepresentationNode;
  node->WorldPosition[0] = worldPos[0];
  node->WorldPosition[1] = worldPos[1];
  node->WorldPosition[2] = 0;
  node->Selected = 0;

  this->Internal->Nodes.insert(this->Internal->Nodes.begin() + idx, node);

  this->NeedToRender = 1;

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::DeleteNthNode(int n)
{
  if (n < 0 || static_cast<unsigned int>(n) >= this->Internal->Nodes.size()) return 0;

  this->Internal->Nodes.erase(this->Internal->Nodes.begin() + n);

  this->NeedToRender = 1;

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::DeleteActiveNode()
{
  return this->DeleteNthNode(this->ActiveNode);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::DeleteLastNode()
{
  return this->DeleteNthNode(static_cast<int>(this->Internal->Nodes.size()) - 1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::SetClosedLoop(int val)
{
  if (this->ClosedLoop != val)
  {
    this->ClosedLoop = val;

    this->NeedToRender = 1;
    this->Modified();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::GetNthNodeSlope(int n, double slope[3])
{
  if (n < 0 || static_cast<unsigned int>(n) >= this->Internal->Nodes.size()) return 0;

  int idx1, idx2;

  if (n == 0 && !this->ClosedLoop)
  {
    idx1 = 0;
    idx2 = 1;
  }
  else
  {
    if (n == this->GetNumberOfNodes() - 1 && !this->ClosedLoop)
    {
      idx1 = this->GetNumberOfNodes() - 2;
      idx2 = idx1 + 1;
    }
    else
    {
      idx1 = n - 1;
      idx2 = n + 1;

      if (idx1 < 0)
      {
        idx1 += this->GetNumberOfNodes();
      }

      if (idx2 >= this->GetNumberOfNodes())
      {
        idx2 -= this->GetNumberOfNodes();
      }
    }
  }

  slope[0] = this->Internal->Nodes[idx2]->WorldPosition[0] - this->Internal->Nodes[idx1]->WorldPosition[0];
  slope[1] = this->Internal->Nodes[idx2]->WorldPosition[1] - this->Internal->Nodes[idx1]->WorldPosition[1];
  slope[2] = this->Internal->Nodes[idx2]->WorldPosition[2] - this->Internal->Nodes[idx1]->WorldPosition[2];

  vtkMath::Normalize(slope);

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int ContourRepresentation::ComputeInteractionState(int vtkNotUsed(X), int vtkNotUsed(Y), int vtkNotUsed(modified))
{
  return this->InteractionState;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::GetDisplayFromWorld(double worldPos[3], int displayPos[2])
{
  double dPos[3];
  this->Renderer->SetWorldPoint(worldPos[0], worldPos[1], 0, 0);
  this->Renderer->WorldToDisplay();
  this->Renderer->GetDisplayPoint(dPos);

  displayPos[0] = dPos[0];
  displayPos[1] = dPos[1];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::GetWorldFromDisplay(int displayPos[2], double worldPos[3])
{
  double wPos[4];
  this->Renderer->SetDisplayPoint(displayPos[0], displayPos[1], 0.0);
  this->Renderer->DisplayToWorld();
  this->Renderer->GetWorldPoint(wPos);

  worldPos[0] = wPos[0];
  worldPos[1] = wPos[1];
  worldPos[2] = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::Initialize(vtkPolyData *polydata)
{
  auto points = polydata->GetPoints();
  auto nPoints = points->GetNumberOfPoints();
  if (nPoints <= 0) return; // Yeah right.. build from nothing !

  // Clear all existing nodes.
  for (unsigned int i = 0; i < this->Internal->Nodes.size(); i++)
  {
    delete this->Internal->Nodes[i];
  }
  this->Internal->Nodes.clear();

  //reserver space in memory to speed up vector push_back
  this->Internal->Nodes.reserve(nPoints);

  auto pointIds = polydata->GetCell(0)->GetPointIds();

  // Add nodes without calling rebuild lines
  // to improve performance dramatically(~15x) on large datasets
  for (vtkIdType i = 0; i < nPoints; i++)
  {
    auto pos = points->GetPoint(i);

    // Add a new point at this position
    auto node = new ContourRepresentationNode;
    memcpy(node->WorldPosition, pos, 3*sizeof(double));
    node->Selected = 0;

    this->Internal->Nodes.push_back(node);
  }

  if (pointIds->GetNumberOfIds() > nPoints) this->ClosedLoopOn();

  this->BuildRepresentation();

  // Show the contour.
  this->VisibilityOn();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::SetShowSelectedNodes(int flag)
{
  if (this->ShowSelectedNodes != flag)
  {
    this->ShowSelectedNodes = flag;
    this->Modified();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  //Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Pixel Tolerance: " << this->PixelTolerance << "\n";
  os << indent << "World Tolerance: " << this->WorldTolerance << "\n";

  os << indent << "Closed Loop: " << (this->ClosedLoop ? "On\n" : "Off\n");
  os << indent << "ShowSelectedNodes: " << this->ShowSelectedNodes << endl;

  os << indent << "Current Operation: ";
  if (this->CurrentOperation == ContourRepresentation::Inactive)
    os << "Inactive\n";
  else
    os << "Translate\n";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool ContourRepresentation::CheckNodesForDuplicates(int node1, int node2)
{
  auto numNodes = this->GetNumberOfNodes();

  if ((numNodes < 2) || (node1 > numNodes - 1) || (node2 > numNodes - 1)) return false;

  double node1Position[3];
  double node2Position[3];

  this->GetNthNodeWorldPosition(node1, node1Position);
  this->GetNthNodeWorldPosition(node2, node2Position);

  if ((node1Position[0] == node2Position[0]) && (node1Position[1] == node2Position[1])) return true;

  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool ContourRepresentation::CheckAndCutContourIntersectionInFinalPoint(void)
{
  if (this->GetNumberOfNodes() < 4) return false;

  double intersection[3];
  int node;

  auto nodesNum = this->GetNumberOfNodes();
  int lastNode = (this->CheckNodesForDuplicates(nodesNum - 1, nodesNum - 2) ? nodesNum - 2 : nodesNum - 1);

  // segment (lastNode-1, lastNode)
  if (this->LineIntersection(lastNode - 1, intersection, &node))
  {
    // delete useless nodes
    for (int j = 0; j <= node; j++)
    {
      this->DeleteNthNode(0);
    }

    // repeat the process to detect spiral-like contours (contours with multiple intersections of the (n-2,n-1) segment)
    lastNode = (this->CheckNodesForDuplicates(this->GetNumberOfNodes() - 1, this->GetNumberOfNodes() - 2) ? this->GetNumberOfNodes() - 2 : this->GetNumberOfNodes() - 1);
    while (this->LineIntersection(lastNode - 1, intersection, &node))
    {
      for (int j = 0; j <= node; j++)
      {
        this->DeleteNthNode(0);
      }

      lastNode = (this->CheckNodesForDuplicates(this->GetNumberOfNodes() - 1, this->GetNumberOfNodes() - 2) ? this->GetNumberOfNodes() - 2 : this->GetNumberOfNodes() - 1);
    }

    if (this->CheckNodesForDuplicates(this->GetNumberOfNodes() - 1, this->GetNumberOfNodes() - 2)) this->DeleteLastNode();

    this->DeleteLastNode();

    intersection[2] = 0;

    auto node = new ContourRepresentationNode();
    memcpy(node->WorldPosition, intersection, 3*sizeof(double));
    node->Selected = 0;

    this->Internal->Nodes.insert(this->Internal->Nodes.begin(), node);

    this->NeedToRender = 1;
    return true;
  }

  // segment (lastNode,0)
  if (this->LineIntersection(lastNode, intersection, &node))
  {
    for (int j = this->GetNumberOfNodes() - 1; j > node; j--)
    {
      this->DeleteLastNode();
    }

    // repeat the process to detect spiral-like contours (contours with multiple intersections of the (n-1,0) segment)
    lastNode = (this->CheckNodesForDuplicates(this->GetNumberOfNodes() - 1, this->GetNumberOfNodes() - 2) ? this->GetNumberOfNodes() - 2 : this->GetNumberOfNodes() - 1);

    while (this->LineIntersection(lastNode, intersection, &node))
    {
      for (int j = this->GetNumberOfNodes() - 1; j > node; j--)
      {
        this->DeleteLastNode();
      }

      lastNode = (this->CheckNodesForDuplicates(this->GetNumberOfNodes() - 1, this->GetNumberOfNodes() - 2) ? this->GetNumberOfNodes() - 2 : this->GetNumberOfNodes() - 1);
    }

    intersection[2] = 0;

    auto node = new ContourRepresentationNode();
    memcpy(node->WorldPosition, intersection, 3*sizeof(double));
    node->Selected = 0;

    this->Internal->Nodes.insert(this->Internal->Nodes.end(), node);

    this->NeedToRender = 1;

    // some special cases make two nodes in the same world position, erase one of them
    RemoveDuplicatedNodes();
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool ContourRepresentation::CheckAndCutContourIntersection(void)
{
  if (this->GetNumberOfNodes() < 4) return false;

  double intersection[3];
  int node;

  int lastNode = (this->CheckNodesForDuplicates(this->GetNumberOfNodes() - 1, this->GetNumberOfNodes() - 2) ? this->GetNumberOfNodes() - 2 : this->GetNumberOfNodes() - 1);

  // segment (n-1,0)
  if (this->LineIntersection(lastNode - 1, intersection, &node))
  {
    // delete useless nodes
    for (int j = 0; j <= node; j++)
    {
      this->DeleteNthNode(0);
    }

    // repeat the process to detect spiral-like contours (contours with multiple intersections of the (n-2,n-1) segment)
    lastNode = (this->CheckNodesForDuplicates(this->GetNumberOfNodes() - 1, this->GetNumberOfNodes() - 2) ? this->GetNumberOfNodes() - 2 : this->GetNumberOfNodes() - 1);

    while (LineIntersection(lastNode - 1, intersection, &node))
    {
      for (int j = 0; j <= node; j++)
      {
        this->DeleteNthNode(0);
      }

      lastNode = (this->CheckNodesForDuplicates(this->GetNumberOfNodes() - 1, this->GetNumberOfNodes() - 2) ? this->GetNumberOfNodes() - 2 : this->GetNumberOfNodes() - 1);
    }

    if (this->CheckNodesForDuplicates(this->GetNumberOfNodes() - 1, this->GetNumberOfNodes() - 2)) this->DeleteLastNode();

    this->DeleteLastNode();

    intersection[2] = 0;

    auto node = new ContourRepresentationNode();
    memcpy(node->WorldPosition, intersection, 3*sizeof(double));
    node->Selected = 0;

    this->Internal->Nodes.insert(this->Internal->Nodes.begin(), node);
    // some special cases make two nodes in the same world position, erase one of them
    RemoveDuplicatedNodes();

    this->NeedToRender = 1;
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool ContourRepresentation::CheckContourIntersection(int nodeA)
{
  // must check the intersection of (nodeA-1, nodeA) and (nodeA, nodeA+1)
  auto nodeB = (nodeA == 0) ? (this->GetNumberOfNodes() - 1) : (nodeA - 1);

  for (int i = 0; i < this->GetNumberOfNodes(); i++)
  {
    if ((this->NodesIntersection(nodeA, i)) || (this->NodesIntersection(nodeB, i)))
    {
      return true;
    }
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool ContourRepresentation::LineIntersection(int node, double *intersection, int *intersectionNode)
{
  auto numNodes = this->GetNumberOfNodes() - 1;

  // avoid using the cursor node when (n-1) is the same node as (n)
  if (this->CheckNodesForDuplicates(numNodes, numNodes - 1))
  {
    if (node == numNodes) node--;

    numNodes--;
  }

  if ((node < 0) || (node > numNodes) || (numNodes < 3)) return false;

  double p1[3], p2[3];
  auto previousNode = (node == 0) ? numNodes : node - 1;
  auto nextNode = (node == numNodes) ? 0 : node + 1;

  this->GetNthNodeWorldPosition(nextNode, p1);
  this->GetNthNodeWorldPosition(node, p2);

  // segments (0,1)-(1,2)- ... -(numNodes-1, numNodes);
  for (int i = 0; i < numNodes; i++)
  {
    double u, v;
    double p3[3], p4[3];
    this->GetNthNodeWorldPosition(i, p3);
    this->GetNthNodeWorldPosition(i + 1, p4);

    if ((vtkLine::Intersection(p1, p2, p3, p4, u, v)) && (i != node) && (i != previousNode) && (i != nextNode))
    {
      // sometimes vtkLine::intersection returns true for segments that do not intersect (and u and v are 0.0).
      // ordering the points of the lines and checking u and v solves the problem, except for coincident lines, but
      // we don't need to check those yet
      if (!((0.0 == u) && (0.0 == v)))
      {
        *intersectionNode = i;

        intersection[0] = p1[0] + u * (p2[0] - p1[0]);
        intersection[1] = p1[1] + u * (p2[1] - p1[1]);
        intersection[2] = p1[2] + u * (p2[2] - p1[2]);

        return true;
      }
    }
  }

  // segment (numNodes, 0)
  if ((numNodes != node) && (numNodes != previousNode) && (numNodes != nextNode))
  {
    double u, v;
    double p3[3], p4[3];
    this->GetNthNodeWorldPosition(numNodes, p3);
    this->GetNthNodeWorldPosition(0, p4);

    if (vtkLine::Intersection(p1, p2, p3, p4, u, v))
    {
      // sometimes vtkLine::intersection returns true for segments that do not intersect (and u and v are 0.0).
      // ordering the points of the lines and checking u and v solves the problem, except for coincident lines, but
      // we don't need to check those yet
      if (!((0.0 == u) && (0.0 == v)))
      {
        *intersectionNode = numNodes;

        intersection[0] = p1[0] + u * (p2[0] - p1[0]);
        intersection[1] = p1[1] + u * (p2[1] - p1[1]);
        intersection[2] = p1[2] + u * (p2[2] - p1[2]);

        return true;
      }
    }
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool ContourRepresentation::ShootingAlgorithm(int X, int Y)
{
  if (!this->ClosedLoop) return false;

  double displayPos[3]{ static_cast<double>(X), static_cast<double>(Y), 0.0 };
  double point[3];

  this->Renderer->SetDisplayPoint(displayPos);
  this->Renderer->DisplayToWorld();
  this->Renderer->GetWorldPoint(point);

  double p1[3], p2[3];

  auto right = 0;
  auto left = 0;

  for (int i = 0; i < this->GetNumberOfNodes(); i++)
  {
    this->GetNthNodeWorldPosition(i, p1);
    auto j = (i + 1) % this->GetNumberOfNodes();
    this->GetNthNodeWorldPosition(j, p2);

    // check if the point is a vertex
    if ((point[0] == p1[0]) && (point[1] == p1[1])) return false;

    if (((p1[1] - point[1]) > 0) != ((p2[1] - point[1]) > 0))
    {
      auto x = ((p1[0] - point[0]) * (double) (p2[1] - point[1]) - (p2[0] - point[0]) * (double) (p1[1] - point[1])) / (double) ((p2[1] - point[1]) - (p1[1] - point[1]));

      if (x > 0) right++;
    }

    if (((p1[1] - point[1]) < 0) != ((p2[1] - point[1]) < 0))
    {
      auto x = ((p1[0] - point[0]) * (double) (p2[1] - point[1]) - (p2[0] - point[0]) * (double) (p1[1] - point[1])) / (double) ((p2[1] - point[1]) - (p1[1] - point[1]));

      if (x < 0) left++;
    }
  }

  // check if the point belongs to the frontier
  if ((right % 2) != (left % 2)) return false;

  // if there is an even number of intersections then the point is inside
  if ((right % 2) == 1) return true;

  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::RemoveDuplicatedNodes()
{
  auto i = this->GetNumberOfNodes() - 2;

  while (i > 0)
  {
    double pos1[3], pos2[3];
    this->GetNthNodeWorldPosition(i, pos1);
    this->GetNthNodeWorldPosition(i + 1, pos2);

    if ((pos1[0] == pos2[0]) && (pos1[1] == pos2[1]) && (pos1[2] == pos2[2])) this->DeleteNthNode(i + 1);

    i--;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool ContourRepresentation::NodesIntersection(int nodeA, int nodeC)
{
  double a0[3], a1[3], b0[3], b1[3];

  // don't want to check the obvious case, return false instead of true
  if (nodeA == nodeC) return false;

  auto nodeB = (nodeA + 1) % this->GetNumberOfNodes();
  auto nodeD = (nodeC + 1) % this->GetNumberOfNodes();

  this->GetNthNodeWorldPosition(nodeA, a0);
  this->GetNthNodeWorldPosition(nodeB, a1);
  this->GetNthNodeWorldPosition(nodeC, b0);
  this->GetNthNodeWorldPosition(nodeD, b1);

  // determinant of the matrix whose elements are the coefficients of the parametric equations of the lines A and B
  auto det = ((a1[0] - a0[0]) * (b1[1] - b0[1])) - ((b1[0] - b0[0]) * (a1[1] - a0[1]));

  if (nodeC == nodeB)	                        // check only node A with segment [C,D]
  {
    if (0 != det) return false;

    if ((a0[0] == a1[0]) && (b0[0] == b1[0]))	// special case: [A,B] and [C,D] are vertical line segments.
    {
      if (a0[0] == b0[0])                     // true if [A,B] and [C,D] are in the same vertical line.
      {
        if (b0[1] < b1[1])                    // check if A is in the bounds of [C,D] (y coord).
        {
          return ((b0[1] <= a0[1]) && (a0[1] <= b1[1]));
        }
        else
        {
          return ((b1[1] <= a0[1]) && (a0[1] <= b0[1]));
        }
      }
      else
      {
        return false;
      }
    }

    // for parallel lines to overlap, they need the same y-intercept. integer relations to y-intercepts of [A,B] and [C,D] are as follows.
    auto ab_offset = ((a1[0] - a0[0]) * a0[1] - (a1[1] - a0[1]) * a0[0]) * (b1[0] - b0[0]);
    auto cd_offset = ((b1[0] - b0[0]) * b0[1] - (b1[1] - b0[1]) * b0[0]) * (a1[0] - a0[0]);

    if (cd_offset == ab_offset)             // true only when [A,B] y_intercept == [C,D] y_intercept.
    {
      if (b0[0] < b1[0])                    // check if A is in the bounds of [C,D] (x coord)
      {
        return ((b0[0] <= a0[0]) && (a0[0] <= b1[0]));
      }
      else
      {
        return ((b1[0] <= a0[0]) && (a0[0] <= b0[0]));
      }
    }
    else
    {
      return false;                         // different y_intercepts; no intersection.
    }
  }

  if (nodeA == nodeD)                       // check only node B with segment [C,D]
  {
    if (0 != det) return false;

    if ((a0[0] == a1[0]) && (b0[0] == b1[0])) // special case: [A,B] and [C,D] are vertical line segments.
    {
      if (a0[0] == b0[0])                     // true if [A,B] and [C,D] are in the same vertical line.
      {
        if (b0[1] < b1[1])                    // check if B is in the bounds of [C,D] (y coord).
        {
          return ((b0[1] <= a1[1]) && (a1[1] <= b1[1]));
        }
        else
        {
          return ((b1[1] <= a1[1]) && (a1[1] <= b0[1]));
        }
      }
      else
      {
        return false;
      }
    }

    // for parallel lines to overlap, they need the same y-intercept. integer relations to y-intercepts of A and B are as follows.
    auto ab_offset = ((a1[0] - a0[0]) * a0[1] - (a1[1] - a0[1]) * a0[0]) * (b1[0] - b0[0]);
    auto cd_offset = ((b1[0] - b0[0]) * b0[1] - (b1[1] - b0[1]) * b0[0]) * (a1[0] - a0[0]);

    if (cd_offset == ab_offset)             // true only when [A,B] y_intercept == [C,D] y_intercept.
    {
      if (b0[0] < b1[0])                    // check B is in the bounds of [C,D] (x coord)
      {
        return ((b0[0] <= a1[0]) && (a1[0] <= b1[0]));
      }
      else
      {
        return ((b1[0] <= a1[0]) && (a1[0] <= b0[0]));
      }
    }
    else
    {
      return false;                         // different y_intercepts; no intersection.
    }
  }

                                            // [A,B] and [C,D] are disjoint segments
  if (det == 0)                             // [A,B] and [C,D] are parallel if det == 0
  {
    if ((a0[0] == a1[0]) && (b0[0] == b1[0]))
    {
      if (a0[0] == b0[0])                   // true if [A,B] and [C,D] are in the same vertical line.
      {
        if (b0[1] < b1[1])                  // check if bounds of [A,B] are in the bounds of [C,D]
        {
          return ((b0[1] <= a0[1]) && (a0[1] <= b1[1])) || ((b0[1] <= a1[1]) && (a1[1] <= b1[1]));
        }
        else
        {
          return ((b1[1] <= a0[1]) && (a0[1] <= b0[1])) || ((b1[1] <= a1[1]) && (a1[1] <= b0[1]));
        }
      }
      else
      {
        return false;                       // different vertical lines, no intersection.
      }
    }

    // for parallel lines to overlap, they need the same y-intercept. integer relations to y-intercepts of [A,B] and [C,D] are as follows.
    auto a_offset = ((a1[0] - a0[0]) * a0[1] - (a1[1] - a0[1]) * a0[0]) * (b1[0] - b0[0]);
    auto b_offset = ((b1[0] - b0[0]) * b0[1] - (b1[1] - b0[1]) * b0[0]) * (a1[0] - a0[0]);

    if (b_offset == a_offset)               // true only when [A,B] y_intercept == [C,D] y_intercept.
    {
      if (b0[0] < b1[0])                    // true when some bounds of [A,B] are in the bounds of [C,D].
      {
        return (b0[0] <= a0[0] && a0[0] <= b1[0]) || (b0[0] <= a1[0] && a1[0] <= b1[0]);
      }
      else
      {
        return (b1[0] <= a0[0] && a0[0] <= b0[0]) || (b1[0] <= a1[0] && a1[0] <= b0[0]);
      }
    }
    else
    {
      return false;                         // different y intercepts; no intersection.
    }
  }

  // nMitc[0] = numerator_of_M_inverse_times_c0
  // nMitc[1] = numerator_of_M_inverse_times_c1
  double nMitc[2] = { (b0[0] - a0[0]) * (b1[1] - b0[1]) + (b0[1] - a0[1]) * (b0[0] - b1[0]), (b0[0] - a0[0]) * (a0[1] - a1[1]) + (b0[1] - a0[1]) * (a1[0] - a0[0]) };

  // true if an intersection between two non-parallel lines occurs between the given segment double *s.
  return ((0 <= nMitc[0] && nMitc[0] <= det) && (0 >= nMitc[1] && nMitc[1] >= -det))
      || ((0 >= nMitc[0] && nMitc[0] >= det) && (0 <= nMitc[1] && nMitc[1] <= -det));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ContourRepresentation::TranslatePoints(double *vector)
{
  double worldPos[3];
  for (int i = 0; i < this->GetNumberOfNodes(); i++)
  {
    this->GetNthNodeWorldPosition(i, worldPos);
    worldPos[0] += vector[0];
    worldPos[1] += vector[1];
    worldPos[2] = 0;
    this->SetNthNodeWorldPosition(i, worldPos);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
double ContourRepresentation::FindClosestDistanceToContour(int x, int y)
{
  int displayPos_i[3], displayPos_j[3];
  auto result = VTK_DOUBLE_MAX;
  double tempN, tempD;

  for (int i = 0; i < this->GetNumberOfNodes(); i++)
  {
    auto j = (i + 1) % this->GetNumberOfNodes();

    this->GetNthNodeDisplayPosition(i, displayPos_i);
    this->GetNthNodeDisplayPosition(j, displayPos_j);

    //            (y1-y2)x + (x2-x1)y + (x1y2-x2y1)
    //dist(P,L) = ---------------------------------
    //              sqrt( (x2-x1)^2 + (y2-y1)^2 )

    tempN = ((displayPos_i[1] - displayPos_j[1]) * static_cast<double>(x)) + ((displayPos_j[0] - displayPos_i[0]) * static_cast<double>(y))
          + ((displayPos_i[0] * displayPos_j[1]) - (displayPos_j[0] * displayPos_i[1]));

    tempD = sqrt(pow(displayPos_j[0] - displayPos_i[0], 2) + pow(displayPos_j[1] - displayPos_i[1], 2));

    if (tempD == 0.0) continue;

    auto distance = fabs(tempN) / tempD;

    // if the distance is to a point outside the segment i,i+1, then the real distance to the segment is
    // the distance to one of the nodes
    auto r = ((static_cast<double>(x) - displayPos_i[0]) * (displayPos_j[0] - displayPos_i[0])
           + (static_cast<double>(y) - displayPos_i[1]) * (displayPos_j[1] - displayPos_i[1])) / (tempD * tempD);

    if ((r < 0.0) || (r > 1.0))
    {
      auto dist1 = fabs(sqrt(pow(displayPos_i[0] - static_cast<double>(x), 2) + pow(displayPos_i[1] - static_cast<double>(y), 2)));
      auto dist2 = fabs(sqrt(pow(displayPos_j[0] - static_cast<double>(x), 2) + pow(displayPos_j[1] - static_cast<double>(y), 2)));

      if (dist1 <= dist2)
      {
        distance = dist1;
      }
      else
      {
        distance = dist2;
      }
    }

    if (distance < result)
    {
      result = distance;
    }
  }

  return result;
}
