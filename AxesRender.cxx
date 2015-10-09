///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: AxesRender.cxx
// Purpose: generate slice planes and crosshair for 3d render view
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// vtk includes
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkAxesActor.h>
#include <vtkRenderWindow.h>

// project includes
#include "AxesRender.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// AxesRender class
//
AxesRender::AxesRender(vtkSmartPointer<vtkRenderer> renderer, Coordinates *OrientationData)
{
    // init
    _spacing = OrientationData->GetImageSpacing();
    Vector3ui size = OrientationData->GetTransformedSize();
    _max = Vector3d((size[0]-1)*_spacing[0], (size[1]-1)*_spacing[1], (size[2]-1)*_spacing[2]);
    _visible = true;

    GenerateSlicePlanes(renderer);
    GenerateVoxelCrosshair(renderer);
    CreateOrientationWidget(renderer);
}

AxesRender::~AxesRender()
{
	// smartpointers should delete everything. we have no need to remove actors from renderer, that
	// is done elsewhere
    
    // just hide the orientationMarkerWidget, trying to delete it segfaults. why? this seems to do
	// the trick
	_axesWidget->SetEnabled(false);
}

void AxesRender::Update(Vector3ui point)
{
    UpdateVoxelCrosshair(point);
    UpdateSlicePlanes(point);
}

void AxesRender::GenerateVoxelCrosshair(vtkSmartPointer<vtkRenderer> renderer)
{
    for (unsigned int idx = 0; idx < 3; idx++)
    {
        vtkSmartPointer<vtkLineSource> line = vtkSmartPointer<vtkLineSource>::New();
        line->SetResolution(1);
        line->Update();
        
        vtkSmartPointer<vtkPolyDataMapper> linemapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        linemapper->SetInputData(line->GetOutput());
        linemapper->SetResolveCoincidentTopologyToPolygonOffset();
            
        vtkSmartPointer<vtkActor> lineactor = vtkSmartPointer<vtkActor>::New();
        lineactor->SetMapper(linemapper);
        lineactor->GetProperty()->SetColor(1,1,1);
        lineactor->GetProperty()->SetLineStipplePattern(0x9999);
        lineactor->GetProperty()->SetLineStippleRepeatFactor(1);
        lineactor->GetProperty()->SetPointSize(1);
        lineactor->GetProperty()->SetLineWidth(2);

        renderer->AddActor(lineactor);
        _crossActors.push_back(lineactor);
        _POILines.push_back(line);
    }
}

void AxesRender::UpdateVoxelCrosshair(Vector3ui point)
{
    std::vector<vtkSmartPointer<vtkLineSource> >::iterator iter = _POILines.begin();
    
    Vector3d point3d(Vector3d(point[0]*_spacing[0], point[1]*_spacing[1], point[2]*_spacing[2]));
    
    (*iter)->SetPoint1(      0, point3d[1], point3d[2]);
    (*iter)->SetPoint2(_max[0], point3d[1], point3d[2]);
    (*iter)->Update();
    iter++;
    
    (*iter)->SetPoint1(point3d[0],       0, point3d[2]);
    (*iter)->SetPoint2(point3d[0], _max[1], point3d[2]);
    (*iter)->Update();
    iter++;
    
    (*iter)->SetPoint1(point3d[0], point3d[1],       0);
    (*iter)->SetPoint2(point3d[0], point3d[1], _max[2]);
    (*iter)->Update();
}

// Voxel slice planes generation/update functions
//
// void AxesRender::GenerateSlicePlanes()
// void AxesRender::UpdateSlicePlanes(Vector3d point)
//
void AxesRender::GenerateSlicePlanes(vtkSmartPointer<vtkRenderer> renderer)
{
    for (unsigned int idx = 0; idx < 3; ++idx)
    {
        vtkSmartPointer<vtkPlaneSource> plane = vtkSmartPointer<vtkPlaneSource>::New();
        plane->Update();
        
        vtkSmartPointer<vtkPolyDataMapper> planemapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        planemapper->SetInputConnection(0, plane->GetOutputPort(0));
        
        vtkSmartPointer<vtkActor> planeactor = vtkSmartPointer<vtkActor>::New();
        planeactor->GetProperty()->SetColor(0.5,0.5,0.5);
        planeactor->GetProperty()->SetSpecular(0);
        planeactor->GetProperty()->SetOpacity(0.3);
        planeactor->GetProperty()->ShadingOff();
        planeactor->GetProperty()->EdgeVisibilityOff();
        planeactor->GetProperty()->LightingOn();
        planeactor->SetMapper(planemapper);

        renderer->AddActor(planeactor);
        _planesActors.push_back(planeactor);
        _planes.push_back(plane);
    }
}

void AxesRender::UpdateSlicePlanes(Vector3ui point)
{
    std::vector<vtkSmartPointer<vtkPlaneSource> >::iterator iter = _planes.begin();
    
    Vector3d point3d(Vector3d(point[0]*_spacing[0], point[1]*_spacing[1], point[2]*_spacing[2]));
    
    (*iter)->SetOrigin(      0,      0,point3d[2]);
    (*iter)->SetPoint1(_max[0],      0,point3d[2]);
    (*iter)->SetPoint2(      0,_max[1],point3d[2]);
    (*iter)->SetNormal(      0,      0,         1);
    (*iter)->Update();
    iter++;
    
    (*iter)->SetOrigin(      0,point3d[1],      0);
    (*iter)->SetPoint1(_max[0],point3d[1],      0);
    (*iter)->SetPoint2(      0,point3d[1],_max[2]);
    (*iter)->SetNormal(      0,         1,      0);
    (*iter)->Update();
    iter++;

    (*iter)->SetOrigin(point3d[0],      0,      0);
    (*iter)->SetPoint1(point3d[0],_max[1],      0);
    (*iter)->SetPoint2(point3d[0],      0,_max[2]);
    (*iter)->SetNormal(         1,      0,      0);
    (*iter)->Update();
}

void AxesRender::CreateOrientationWidget(vtkSmartPointer<vtkRenderer> renderer)
{
    // initialize orientation widget from render viewport, see headerfile for a warning.
    vtkSmartPointer < vtkAxesActor > axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->DragableOff();
    axes->PickableOff();
    _axesWidget = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    _axesWidget->SetOrientationMarker(axes);
    _axesWidget->SetInteractor(renderer->GetRenderWindow()->GetInteractor());
    _axesWidget->SetViewport(0.0, 0.0, 0.3, 0.3);
    _axesWidget->SetEnabled(true);
    _axesWidget->InteractiveOff();
}

bool AxesRender::isVisible()
{
    return _visible;
}

void AxesRender::setVisible(bool value)
{
    if (_visible == value)
        return;
    
    std::vector< vtkSmartPointer<vtkActor> >::iterator it;
    
    if(value)
    {
            _visible = true;
            for (it = _planesActors.begin(); it != _planesActors.end(); it++)
                (*it)->SetVisibility(true);
            for (it = _crossActors.begin(); it != _crossActors.end(); it++)
                (*it)->SetVisibility(true);
    }
    else
    {
            _visible = false;
            for (it = _planesActors.begin(); it != _planesActors.end(); it++)
                (*it)->SetVisibility(false);
            for (it = _crossActors.begin(); it != _crossActors.end(); it++)
                (*it)->SetVisibility(false);
    }
}
