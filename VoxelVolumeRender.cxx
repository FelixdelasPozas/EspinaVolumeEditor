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
#include <vtkColorTransferFunction.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>
#include <vtkVolumeRayCastCompositeFunction.h>
#include <vtkCamera.h>

// project includes
#include "DataManager.h"
#include "VoxelVolumeRender.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// VoxelVolumeRender class
//
VoxelVolumeRender::VoxelVolumeRender(DataManager *data, vtkSmartPointer<vtkRenderer> renderer, ProgressAccumulator *progress)
{
    _dataManager = data;
    _renderer = renderer;
    _progress = progress;
    _meshActor = NULL;
    _volume = NULL;
    _volumemapper = NULL;
//    _GPUmapper = NULL;
    _objectLabel = 0;

    // the raycasted volume is always present
    ComputeRayCastVolume();
//    ComputeGPURender();
}

VoxelVolumeRender::~VoxelVolumeRender()
{
    // using smartpointers
    
    // delete actors from renderers before leaving;
	_renderer->RemoveActor(_volume);
	_renderer->RemoveActor(_meshActor);
}

// note: some filters make use of SetGlobalWarningDisplay(false) because if we delete
//       one object completely on the editor those filters will print an error during
//       pipeline update that I don't want to see. i already know that there is not
//       data to decimate or smooth if there is no voxel! as a matter of fact, it's
//       not even an error.
void VoxelVolumeRender::ComputeMesh(unsigned short label)
{
    double rgba[4];

    // generate iso surface
    vtkSmartPointer<vtkDiscreteMarchingCubes> marcher = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
	marcher->SetInput(_dataManager->GetStructuredPoints());
	marcher->ReleaseDataFlagOn();
	marcher->SetNumberOfContours(1);
	marcher->GenerateValues(1, label, label);
	marcher->ComputeScalarsOff();
	marcher->ComputeNormalsOff();
	marcher->ComputeGradientsOff();
	_progress->Observe(marcher, "March", 1.0/3.0);
	marcher->Update();
	_progress->Ignore(marcher);

	// decimate surface
    vtkSmartPointer<vtkDecimatePro> decimator = vtkSmartPointer<vtkDecimatePro>::New();
	decimator->SetInputConnection(marcher->GetOutputPort());
	decimator->ReleaseDataFlagOn();
	decimator->SetGlobalWarningDisplay(false);
	decimator->SetTargetReduction(0.95);
	decimator->PreserveTopologyOn();
	decimator->BoundaryVertexDeletionOn();
	decimator->SplittingOff();
	_progress->Observe(decimator, "Decimate", 1.0/3.0);
	decimator->Update();
	_progress->Ignore(decimator);

	// surface smoothing
    vtkSmartPointer<vtkWindowedSincPolyDataFilter> smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
	smoother->SetInputConnection(decimator->GetOutputPort());
	smoother->ReleaseDataFlagOn();
	smoother->SetGlobalWarningDisplay(false);
	smoother->BoundarySmoothingOn();
	smoother->FeatureEdgeSmoothingOn();
	smoother->SetNumberOfIterations(15);
	smoother->SetFeatureAngle(120);
	smoother->SetEdgeAngle(90);

	// compute normals for a better looking render
    vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
	normals->SetInputConnection(smoother->GetOutputPort());
	normals->ReleaseDataFlagOn();
	normals->SetFeatureAngle(120);

	// model mapper
    vtkSmartPointer<vtkPolyDataMapper> isoMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	_progress->Observe(isoMapper, "Map", 1.0/3.0);
	isoMapper->SetInputConnection(normals->GetOutputPort());
	isoMapper->ReleaseDataFlagOn();
	isoMapper->ImmediateModeRenderingOn();
	isoMapper->ScalarVisibilityOff();
	isoMapper->Update();
	_progress->Ignore(isoMapper);

	// create the actor a assign a color to it
	if (NULL != _meshActor)
		_renderer->RemoveActor(_meshActor);

    _meshActor = vtkSmartPointer<vtkActor>::New();
    _meshActor->SetMapper(isoMapper);
	_dataManager->GetColorComponents(label, rgba);
	_meshActor->GetProperty()->SetColor(rgba[0], rgba[1], rgba[2]);
	_meshActor->GetProperty()->SetOpacity(1);
	_meshActor->GetProperty()->SetSpecular(0.2);

	// add the actor to the renderer
	_renderer->AddActor(_meshActor);

    _progress->Reset();
}

void VoxelVolumeRender::ComputeRayCastVolume()
{
    double rgba[4];

    // model mapper
    _volumemapper = vtkSmartPointer<vtkVolumeRayCastMapper>::New();
    _volumemapper->SetInput(_dataManager->GetStructuredPoints());
    _volumemapper->IntermixIntersectingGeometryOn();

    // standard composite function
    vtkSmartPointer<vtkVolumeRayCastCompositeFunction> volumefunction = vtkSmartPointer<vtkVolumeRayCastCompositeFunction>::New();
    _volumemapper->SetVolumeRayCastFunction(volumefunction);

    // assign label colors
    _colorfunction = vtkSmartPointer<vtkColorTransferFunction>::New();
    _colorfunction->AllowDuplicateScalarsOff();
    for (unsigned short int i = 0; i != _dataManager->GetNumberOfColors(); i++)
    {
        _dataManager->GetColorComponents(i, rgba);
        _colorfunction->AddRGBPoint(i,rgba[0], rgba[1], rgba[2]);
    }

    // we need to set all labels to an opacity of 0.1
    _opacityfunction = vtkSmartPointer<vtkPiecewiseFunction>::New();
    _opacityfunction->AddPoint(0, 0.0);
    for (unsigned int i = 1; i != _dataManager->GetNumberOfColors(); i++)
        _opacityfunction->AddPoint(i, 0.1);

    // volume property
    vtkSmartPointer<vtkVolumeProperty> volumeproperty = vtkSmartPointer<vtkVolumeProperty>::New();
    volumeproperty->SetColor(_colorfunction);
    volumeproperty->SetScalarOpacity(_opacityfunction);
    volumeproperty->DisableGradientOpacityOn();
    volumeproperty->SetSpecular(0);
    volumeproperty->ShadeOn();
    volumeproperty->SetInterpolationTypeToNearest();

    // create volume and add to render
    if (NULL != _volume)
    	_renderer->RemoveActor(_volume);

    _volume = vtkSmartPointer<vtkVolume>::New();
    _volume->SetMapper(_volumemapper);
    _volume->SetProperty(volumeproperty);

    _renderer->AddVolume(_volume);
}

// TODO: check vtkSmartVolumeMapper on the next library release
//
//	// this renderer is unused by default, as it renders fast but still has errors updating
//	// volume colors and it hasn't the precision of the software renderer
//	void VoxelVolumeRender::ComputeGPURender()
//	{
//		double rgba[4];
//		vtkSmartPointer<vtkLookupTable> table = _dataManager->GetLookupTable();
//
//		// GPU mapper
//		_GPUmapper = vtkSmartPointer<vtkSmartVolumeMapper>::New();
//		_GPUmapper->SetInput(_dataManager->GetStructuredPoints());
//		_GPUmapper->SetRequestedRenderMode(vtkSmartVolumeMapper::GPURenderMode);
//		_GPUmapper->SetInterpolationModeToNearestNeighbor();
//
//		// assign label colors
//		_colorfunction = vtkSmartPointer<vtkColorTransferFunction>::New();
//		_colorfunction->AllowDuplicateScalarsOff();
//		for (unsigned short int i = 0; i != table->GetNumberOfTableValues(); i++)
//		{
//			table->GetTableValue(i, rgba);
//			_colorfunction->AddRGBPoint(i,rgba[0], rgba[1], rgba[2]);
//		}
//
//		// we need to set all labels to an opacity of 0.1
//		_opacityfunction = vtkSmartPointer<vtkPiecewiseFunction>::New();
//		_opacityfunction->AddPoint(0, 0.0);
//		for (int i=1; i != table->GetNumberOfTableValues(); i++)
//			_opacityfunction->AddPoint(i, 0.1);
//
//		// volume property
//		vtkSmartPointer<vtkVolumeProperty> volumeproperty = vtkSmartPointer<vtkVolumeProperty>::New();
//		volumeproperty->SetColor(_colorfunction);
//		volumeproperty->SetScalarOpacity(_opacityfunction);
//		volumeproperty->SetSpecular(0.2);
//		volumeproperty->ShadeOn();
//		volumeproperty->SetInterpolationTypeToNearest();
//
//		// create volume and add to render
//		if (NULL != _volume)
//			_renderer->RemoveActor(_volume);
//
//		_volume = vtkSmartPointer<vtkVolume>::New();
//		_volume->SetMapper(_GPUmapper);
//		_volume->SetProperty(volumeproperty);
//
//		_renderer->AddVolume(_volume);
//	}

void VoxelVolumeRender::UpdateColorTable(int value, double alpha)
{
	_opacityfunction->AddPoint(value, alpha);
    _opacityfunction->Modified();
}

// update focus
void VoxelVolumeRender::UpdateFocusExtent(void)
{
	Vector3d spacing = _dataManager->GetOrientationData()->GetImageSpacing();
	Vector3ui min = _dataManager->GetBoundingBoxMin(_objectLabel);
	Vector3ui max = _dataManager->GetBoundingBoxMax(_objectLabel);

	// it should really be (origin-0.5) and (origin+size+0.5) for rendering the
	// correct bounding box for the object, but if we do it that way thin
	// objects aren't rendered correctly, so 1.5 corrects this.
	_volumemapper->SetCroppingRegionPlanes(
			(min[0]-1.5)*spacing[0], (max[0]+0.5)*spacing[0],
			(min[1]-1.5)*spacing[1], (max[1]+0.5)*spacing[1],
			(min[2]-1.5)*spacing[2], (max[2]+0.5)*spacing[2]);
	_volumemapper->CroppingOn();
	_volumemapper->SetCroppingRegionFlagsToSubVolume();
	_volumemapper->Update();
}

void VoxelVolumeRender::FocusSegmentation(unsigned short label)
{
	// dim previous label
	if (0 != _objectLabel)
		UpdateColorTable(_objectLabel, 0.1);

	_objectLabel = label;

	ViewAsVolume();
	UpdateFocusExtent();
}

void VoxelVolumeRender::CenterSegmentation(unsigned short label)
{
    // if the selected label has no voxels it has no centroid
    if ((0LL == _dataManager->GetNumberOfVoxelsForLabel(label)) || (0 == label))
    	return;

	Vector3d spacing = _dataManager->GetOrientationData()->GetImageSpacing();

    // change voxel renderer to move around new POI
    Vector3d centroid = _dataManager->GetCentroidForObject(label);
    _renderer->GetActiveCamera()->SetFocalPoint(centroid[0]*spacing[0],centroid[1]*spacing[1],centroid[2]*spacing[2]);
}

void VoxelVolumeRender::ViewAsMesh(void)
{
	if (0 != _objectLabel)
	{
		UpdateColorTable(_objectLabel, 0.0);
		ComputeMesh(_objectLabel);
	}
}

void VoxelVolumeRender::ViewAsVolume(void)
{
	if (NULL != _meshActor)
		_renderer->RemoveActor(_meshActor);

	if (0 != _objectLabel)
		UpdateColorTable(_objectLabel, 1.0);
}
