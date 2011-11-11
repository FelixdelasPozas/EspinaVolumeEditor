///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: VoxelVolumeRender.cxx
// Purpose: renders a vtkStructuredPoints volume (raycasting volume or colored meshes)
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// vtk includes
#include <vtkDiscreteMarchingCubes.h>
#include <vtkDecimatePro.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkActor.h>
#include <vtkLookupTable.h>
#include <vtkProperty.h>
#include <vtkStructuredPoints.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkVolumeRayCastMapper.h>
#include <vtkColorTransferFunction.h>
#include <vtkVolumeRayCastCompositeFunction.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

// project includes
#include "VoxelVolumeRender.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// VoxelVolumeRender class
//
VoxelVolumeRender::VoxelVolumeRender(
        vtkSmartPointer<vtkStructuredPoints> points, 
        vtkSmartPointer<vtkLookupTable> table, 
        vtkSmartPointer<vtkRenderer> renderer, 
        RenderType rendertype,
        ProgressAccumulator *progress)
{
    _structuredPoints = points;
    _lookupTable = table;
    _renderer = renderer;
    _renderType = rendertype;
    _progress = progress;
}

VoxelVolumeRender::~VoxelVolumeRender()
{
    // using smartpointers
    
    // delete actors from renderers before leaving;
    DeleteActors();
}

// note: some filters make use of SetGlobalWarningDisplay(false) because if we delete
//       one object completely on the editor those filters will print an error during
//       pipeline update that I don't want to see. i already know that there is not
//       data to decimate or smooth if there is no voxel! it's not an error in fact.
void VoxelVolumeRender::ComputeMeshes()
{
    double rgba[4];
    
    double weight = 1/((_lookupTable->GetNumberOfTableValues()-1)*3.0);
    
    for(int label=1, i = 0; i != _lookupTable->GetNumberOfTableValues()-1; i++, label++)
    {
        // generate iso surface
        vtkSmartPointer<vtkDiscreteMarchingCubes> marcher = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
        _progress->Observe(marcher, "March", weight);
        marcher->SetInput(_structuredPoints);
        marcher->SetNumberOfContours(1);
        marcher->GenerateValues(1,label,label);
        marcher->ComputeScalarsOff();
        marcher->ComputeNormalsOff();
        marcher->ComputeGradientsOff();
        marcher->Update();
        _progress->Ignore(marcher);
        
        // decimate surface
        vtkSmartPointer<vtkDecimatePro> decimator = vtkSmartPointer<vtkDecimatePro>::New();
        _progress->Observe(decimator, "Decimate", weight);
        decimator->SetInputConnection(marcher->GetOutputPort());
        decimator->SetGlobalWarningDisplay(false);
        decimator->SetTargetReduction(0.95);
        decimator->PreserveTopologyOn();
        decimator->BoundaryVertexDeletionOn();
        decimator->SplittingOff();
        decimator->Update();
        _progress->Ignore(decimator);
        
        // surface smoothing
        vtkSmartPointer<vtkWindowedSincPolyDataFilter> smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
        smoother->SetInputConnection(decimator->GetOutputPort());
        smoother->SetGlobalWarningDisplay(false);
        smoother->BoundarySmoothingOn();
        smoother->FeatureEdgeSmoothingOn();
        smoother->SetNumberOfIterations(15);
        smoother->SetFeatureAngle(120);
        smoother->SetEdgeAngle(90);
        
        // compute normals for a better looking render
        vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
        normals->SetInputConnection(smoother->GetOutputPort());
        normals->SetFeatureAngle(120);

        // model mapper
        vtkSmartPointer<vtkPolyDataMapper> isoMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        _progress->Observe(isoMapper, "Map", weight);
        isoMapper->SetInputConnection(normals->GetOutputPort());
        isoMapper->ImmediateModeRenderingOff();
        isoMapper->ScalarVisibilityOff();
        isoMapper->Update();
        _progress->Ignore(isoMapper);

        // create the actor a assign a color to it
        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(isoMapper);
        _lookupTable->GetTableValue(label, rgba);
        actor->GetProperty()->SetColor(rgba[0], rgba[1], rgba[2]);
        
        // opacity is 1/4 of color table values if value != 1 to indicate that
        // the user has one label selected that is not the background label 
        // (needed for volume<->mesh switch)
        if (rgba[3] != 1)
            actor->GetProperty()->SetOpacity(rgba[3]/4);
        else
            actor->GetProperty()->SetOpacity(rgba[3]);
        actor->GetProperty()->SetSpecular(0.2);
 
        // add the actor to the renderer
        _renderer->AddActor(actor);
        
        // map actor
        _actormap.insert(std::pair<unsigned int, vtkSmartPointer<vtkActor> >(label,actor));
    }
    _progress->Reset();
}

void VoxelVolumeRender::ComputeRayCastVolume()
{
    double rgba[4];
    
    // model mapper
    vtkSmartPointer<vtkVolumeRayCastMapper> volumemapper = vtkSmartPointer<vtkVolumeRayCastMapper>::New();
    volumemapper->SetInput(_structuredPoints);
    volumemapper->IntermixIntersectingGeometryOn();
    
    // standard composite function
    vtkSmartPointer<vtkVolumeRayCastCompositeFunction> volumefunction = vtkSmartPointer<vtkVolumeRayCastCompositeFunction>::New();
    volumemapper->SetVolumeRayCastFunction(volumefunction);

    // assign label colors
    vtkSmartPointer<vtkColorTransferFunction> colorfunction = vtkSmartPointer<vtkColorTransferFunction>::New();
    for (unsigned short int i = 0; i != _lookupTable->GetNumberOfTableValues(); i++)
    {
        _lookupTable->GetTableValue(i, rgba);
        colorfunction->AddRGBPoint(i,rgba[0], rgba[1], rgba[2]);
    }

    // we need to set the label 0 to transparent in the volume opacity property
    _opacityfunction = vtkSmartPointer<vtkPiecewiseFunction>::New();
    _opacityfunction->AddPoint(0, 0.0);
    for (int i=0; i != _lookupTable->GetNumberOfTableValues()-1; i++)
    {
        _lookupTable->GetTableValue(i+1, rgba);
        
        // opacity is 1/4 of color table values if value != 1 to indicate that
        // the user has one label selected that is not the background label 
        // (needed for volume<->mesh switch)
        if (rgba[3] != 1)
            _opacityfunction->AddPoint(i+1, rgba[3]/4);
        else
            _opacityfunction->AddPoint(i+1, rgba[3]);
    }
    
    // volume property
    vtkSmartPointer<vtkVolumeProperty> volumeproperty = vtkSmartPointer<vtkVolumeProperty>::New();
    volumeproperty->SetColor(colorfunction);
    volumeproperty->SetScalarOpacity(_opacityfunction);
    volumeproperty->SetSpecular(0.2);
    volumeproperty->ShadeOn();
    volumeproperty->SetInterpolationTypeToNearest();
    
    // create volume and add to render
    _volume = vtkSmartPointer<vtkVolume>::New();
    _volume->SetMapper(volumemapper);
    _volume->SetProperty(volumeproperty);
    
    _renderer->AddVolume(_volume);
}

void VoxelVolumeRender::Compute()
{
    switch(_renderType)
    {
        case RayCast:
            ComputeRayCastVolume();
            break;
        case Meshes:
            ComputeMeshes();
            break;
        default:
            break;
    }
}

void VoxelVolumeRender::UpdateColorTable(int value, double alpha)
{
    switch(_renderType)
    {
        case RayCast:
            _opacityfunction->AddPoint(value, alpha);
            _opacityfunction->Modified();
            break;
        case Meshes:
            _actormap[value]->GetProperty()->SetOpacity(alpha);
            _actormap[value]->Modified();
            break;
        default:
            break;
    }
}

VoxelVolumeRender::RenderType VoxelVolumeRender::GetRenderType()
{
    return _renderType;
}

void VoxelVolumeRender::DeleteActors()
{
    std::map< int, vtkSmartPointer<vtkActor> >::iterator it;
    
    switch(_renderType)
    {
        case RayCast:
            _renderer->RemoveActor(_volume);
            break;
        case Meshes:
            for (it = _actormap.begin(); it != _actormap.end(); it++)
                _renderer->RemoveActor((*it).second);
            break;
        default:
            break;
    }
}
