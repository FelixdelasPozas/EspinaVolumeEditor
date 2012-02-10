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
#include <vtkImageClip.h>

// project includes
#include "DataManager.h"
#include "VoxelVolumeRender.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// VoxelVolumeRender class
//
VoxelVolumeRender::VoxelVolumeRender(DataManager *data, vtkSmartPointer<vtkRenderer> renderer, ProgressAccumulator *progress)
{
    this->_dataManager = data;
    this->_renderer = renderer;
    this->_progress = progress;
    this->_volume = NULL;
    this->_volumemapper = NULL;
    this->_min = this->_max = Vector3ui(0,0,0);
    this->_renderingIsVolume = true;

    // the raycasted volume is always present
    ComputeRayCastVolume();
    UpdateFocusExtent();
}

VoxelVolumeRender::~VoxelVolumeRender()
{
    // using smartpointers
    
    // delete actors from renderers before leaving. volume actor always present
	this->_renderer->RemoveActor(_volume);

	std::map<unsigned short, vtkSmartPointer<vtkActor> >::iterator it;
	for (it = this->_actorList.begin(); it != this->_actorList.end(); it++)
	{
		this->_renderer->RemoveActor((*it).second);
		(*it).second = NULL;
	}
}

// NOTE: some filters make use of SetGlobalWarningDisplay(false) because if we delete
//       one object completely on the editor those filters will print an error during
//       pipeline update that I don't want to see. i already know that there is not
//       data to decimate or smooth if there is no voxel! as a matter of fact, it's
//       not even an error.
void VoxelVolumeRender::ComputeMesh(const unsigned short label)
{
    double rgba[4];

	// first crop the region and then use the vtk-itk pipeline to get a itk::Image of the region
	Vector3ui objectMin = this->_dataManager->GetBoundingBoxMin(label);
	Vector3ui objectMax = this->_dataManager->GetBoundingBoxMax(label);
	Vector3ui size = this->_dataManager->GetOrientationData()->GetTransformedSize();
	double weight = 1.0/4.0;

	// the object bounds collide with the object, we must add one not to clip the mesh at the borders
	if (objectMin[0] > 0) objectMin[0]--;
	if (objectMin[1] > 0) objectMin[1]--;
	if (objectMin[2] > 0) objectMin[2]--;
	if (objectMax[0] < size[0]) objectMax[0]++;
	if (objectMax[1] < size[1])	objectMax[1]++;
	if (objectMax[2] < size[2])	objectMax[2]++;

	// image clipping
	vtkSmartPointer<vtkImageClip> imageClip = vtkSmartPointer<vtkImageClip>::New();
	imageClip->SetInput(this->_dataManager->GetStructuredPoints());
	imageClip->SetOutputWholeExtent(objectMin[0], objectMax[0], objectMin[1], objectMax[1], objectMin[2], objectMax[2]);
	imageClip->ClipDataOn();
	this->_progress->Observe(imageClip, "Clip", weight);
	imageClip->Update();
	this->_progress->Ignore(imageClip);

    // generate iso surface
    vtkSmartPointer<vtkDiscreteMarchingCubes> marcher = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
	marcher->SetInput(imageClip->GetOutput());
	marcher->ReleaseDataFlagOn();
	marcher->SetNumberOfContours(1);
	marcher->GenerateValues(1, label, label);
	marcher->ComputeScalarsOff();
	marcher->ComputeNormalsOff();
	marcher->ComputeGradientsOff();
	this->_progress->Observe(marcher, "March", weight);
	marcher->Update();
	this->_progress->Ignore(marcher);

	// decimate surface
    vtkSmartPointer<vtkDecimatePro> decimator = vtkSmartPointer<vtkDecimatePro>::New();
	decimator->SetInputConnection(marcher->GetOutputPort());
	decimator->ReleaseDataFlagOn();
	decimator->SetGlobalWarningDisplay(false);
	decimator->SetTargetReduction(0.95);
	decimator->PreserveTopologyOn();
	decimator->BoundaryVertexDeletionOn();
	decimator->SplittingOff();
	this->_progress->Observe(decimator, "Decimate", weight);
	decimator->Update();
	this->_progress->Ignore(decimator);

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
    this->_progress->Observe(isoMapper, "Map", weight);
	isoMapper->SetInputConnection(normals->GetOutputPort());
	isoMapper->ReleaseDataFlagOn();
	isoMapper->ImmediateModeRenderingOn();
	isoMapper->ScalarVisibilityOff();
	isoMapper->Update();
	this->_progress->Ignore(isoMapper);

	// create the actor a assign a color to it
	vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(isoMapper);
	this->_dataManager->GetColorComponents(label, rgba);
	actor->GetProperty()->SetColor(rgba[0], rgba[1], rgba[2]);
	actor->GetProperty()->SetOpacity(1);
	actor->GetProperty()->SetSpecular(0.2);

	// add the actor to the renderer
	this->_renderer->AddActor(actor);
	this->_actorList[label] = actor;
}

void VoxelVolumeRender::ComputeRayCastVolume()
{
    double rgba[4];

    // model mapper
    this->_volumemapper = vtkSmartPointer<vtkVolumeRayCastMapper>::New();
    this->_volumemapper->SetInput(this->_dataManager->GetStructuredPoints());
    this->_volumemapper->IntermixIntersectingGeometryOn();

    // standard composite function
    vtkSmartPointer<vtkVolumeRayCastCompositeFunction> volumefunction = vtkSmartPointer<vtkVolumeRayCastCompositeFunction>::New();
    this->_volumemapper->SetVolumeRayCastFunction(volumefunction);

    // assign label colors
    this->_colorfunction = vtkSmartPointer<vtkColorTransferFunction>::New();
    this->_colorfunction->AllowDuplicateScalarsOff();
    for (unsigned short int i = 0; i != this->_dataManager->GetNumberOfColors(); i++)
    {
    	this->_dataManager->GetColorComponents(i, rgba);
    	this->_colorfunction->AddRGBPoint(i,rgba[0], rgba[1], rgba[2]);
    }

    // set all labels to an opacity of 0.0
    this->_opacityfunction = vtkSmartPointer<vtkPiecewiseFunction>::New();
    for (unsigned int i = 0; i != this->_dataManager->GetNumberOfColors(); i++)
    	this->_opacityfunction->AddPoint(i, 0.0);

    this->_opacityfunction->Modified();

    // volume property
    vtkSmartPointer<vtkVolumeProperty> volumeproperty = vtkSmartPointer<vtkVolumeProperty>::New();
    volumeproperty->SetColor(this->_colorfunction);
    volumeproperty->SetScalarOpacity(this->_opacityfunction);
    volumeproperty->DisableGradientOpacityOn();
    volumeproperty->SetSpecular(0);
    volumeproperty->ShadeOn();
    volumeproperty->SetInterpolationTypeToNearest();

    this->_volume = vtkSmartPointer<vtkVolume>::New();
    this->_volume->SetMapper(this->_volumemapper);
    this->_volume->SetProperty(volumeproperty);

    this->_renderer->AddVolume(_volume);
}

// update object focus extent
void VoxelVolumeRender::UpdateFocusExtent(void)
{
	// recalculate focus
	this->_min = this->_max = Vector3ui(0,0,0);
	if (this->_highlightedLabels.empty())
		this->_volumemapper->SetCroppingRegionPlanes(0,0,0,0,0,0);
	else
	{
		std::set<unsigned short>::iterator it = this->_highlightedLabels.begin();
		this->_min = this->_dataManager->GetBoundingBoxMin((*it));
		this->_max = this->_dataManager->GetBoundingBoxMax((*it));
		it++;

		for ( /* empty */ ; it != this->_highlightedLabels.end(); it++)
		{
			Vector3ui minBounds = this->_dataManager->GetBoundingBoxMin((*it));
			Vector3ui maxBounds = this->_dataManager->GetBoundingBoxMax((*it));

			if (this->_min[0] > minBounds[0]) this->_min[0] = minBounds[0];
			if (this->_min[1] > minBounds[1]) this->_min[1] = minBounds[1];
			if (this->_min[2] > minBounds[2]) this->_min[2] = minBounds[2];
			if (this->_max[0] < maxBounds[0]) this->_max[0] = maxBounds[0];
			if (this->_max[1] < maxBounds[1]) this->_max[1] = maxBounds[1];
			if (this->_max[2] < maxBounds[2]) this->_max[2] = maxBounds[2];
		}

		Vector3d spacing = this->_dataManager->GetOrientationData()->GetImageSpacing();
    	this->_volumemapper->SetCroppingRegionPlanes(
    			(this->_min[0]-1.5)*spacing[0], (this->_max[0]+1.5)*spacing[0],
    			(this->_min[1]-1.5)*spacing[1], (this->_max[1]+1.5)*spacing[1],
    			(this->_min[2]-1.5)*spacing[2], (this->_max[2]+1.5)*spacing[2]);
	}

    this->_volumemapper->CroppingOn();
    this->_volumemapper->SetCroppingRegionFlagsToSubVolume();
    this->_volumemapper->Update();

    // if we are rendering as mesh then recompute mesh (if not, mesh will get clipped against
    // boundaries set at the time of creation if the object grows outside old bounding box)
    // Volume render does not need this, updating the extent is just fine
    if (!this->_actorList.empty())
    	this->ViewAsMesh();
}

void VoxelVolumeRender::ViewAsMesh(void)
{
	if (!this->_renderingIsVolume)
		return;

	// hide all highlighted volumes
	std::set<unsigned short>::iterator it;
	for (it = this->_highlightedLabels.begin(); it != this->_highlightedLabels.end(); it++)
	{
		this->_opacityfunction->AddPoint(*it, 0.0);
		ComputeMesh(*it);
	}
	this->_progress->Reset();

	this->_opacityfunction->Modified();
	this->_renderingIsVolume = false;
}

void VoxelVolumeRender::ViewAsVolume(void)
{
	if (this->_renderingIsVolume)
		return;

	// delete all actors
	std::map<unsigned short, vtkSmartPointer<vtkActor> >::iterator it;
	for (it = this->_actorList.begin(); it != this->_actorList.end(); it++)
	{
		this->_renderer->RemoveActor((*it).second);
		(*it).second = NULL;
	}
	this->_actorList.clear();

	std::set<unsigned short>::iterator labelit;
	for (labelit = this->_highlightedLabels.begin(); labelit != this->_highlightedLabels.end(); labelit++)
		this->_opacityfunction->AddPoint(*labelit, 1.0);

	this->_opacityfunction->Modified();
	this->_renderingIsVolume = true;
}

// NOTE: this->_opacityfunction->Modified() not signaled. need to use UpdateColorTable() after
// calling this one to signal changes to pipeline
void VoxelVolumeRender::ColorHighlight(const unsigned short label)
{
	if (0 == label)
		return;

	if (this->_highlightedLabels.find(label) == this->_highlightedLabels.end())
	{
		switch(this->_renderingIsVolume)
		{
			case true:
				this->_opacityfunction->AddPoint(label, 1.0);
				break;
			case false:
				ComputeMesh(label);
				break;
			default:
				break;
		}
		this->_highlightedLabels.insert(label);
	}
}

// NOTE: this->_opacityfunction->Modified() not signaled. need to use UpdateColorTable() after
// calling this one to signal changes to pipeline.
// NOTE: alpha parameter defaults to 0.0, see .h
void VoxelVolumeRender::ColorDim(const unsigned short label, float alpha)
{
	if (0 == label)
		return;

	if (this->_highlightedLabels.find(label) != this->_highlightedLabels.end())
	{
		switch(this->_renderingIsVolume)
		{
			case true:
				this->_opacityfunction->AddPoint(label, alpha);
				break;
			case false:
				this->_renderer->RemoveActor(this->_actorList[label]);
				this->_actorList[label] = NULL;
				this->_actorList.erase(label);
				break;
			default:
				// can't happen
				break;
		}
		this->_highlightedLabels.erase(label);
	}
}

void VoxelVolumeRender::ColorHighlightExclusive(unsigned short label)
{
	std::set<unsigned short>::iterator it;
	for (it = this->_highlightedLabels.begin(); it != this->_highlightedLabels.end(); it++)
	{
		if (label == (*it))
			continue;
		ColorDim(*it);
	}

	if (this->_highlightedLabels.find(label) == this->_highlightedLabels.end())
		ColorHighlight(label);

	this->_opacityfunction->Modified();
}

void VoxelVolumeRender::ColorDimAll(void)
{
	std::set<unsigned short>::iterator it;
	for (it = this->_highlightedLabels.begin(); it != this->_highlightedLabels.end(); it++)
		ColorDim(*it);

	this->_opacityfunction->Modified();
}

// needed to be public as a method so we can signal changes to the opacity function
void VoxelVolumeRender::UpdateColorTable(void)
{
	this->_opacityfunction->Modified();
}

void VoxelVolumeRender::RebuildHighlightedLabels(void)
{
	ColorDimAll();

	for (unsigned int i = 1; i < _dataManager->GetNumberOfColors(); i++)
	{
		double rgba[4];
		_dataManager->GetColorComponents(i, rgba);
		if (1.0 == rgba[3])
		{
			switch(this->_renderingIsVolume)
			{
				case true:
					this->_opacityfunction->AddPoint(i, 1.0);
					break;
				case false:
					ComputeMesh(i);
					break;
				default:
					break;
			}
			this->_highlightedLabels.insert(i);
		}
	}

	this->_opacityfunction->Modified();
}
