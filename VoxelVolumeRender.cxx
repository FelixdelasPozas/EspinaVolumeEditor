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
    this->_meshActor = NULL;
    this->_volume = NULL;
    this->_volumemapper = NULL;
    this->_objectLabel = 0;

    // the raycasted volume is always present
    ComputeRayCastVolume();
    FocusSegmentation(0);
}

VoxelVolumeRender::~VoxelVolumeRender()
{
    // using smartpointers
    
    // delete actors from renderers before leaving. volume actor always present
	this->_renderer->RemoveActor(_volume);

	if (NULL != this->_meshActor)
		this->_renderer->RemoveActor(this->_meshActor);
}

// NOTE: some filters make use of SetGlobalWarningDisplay(false) because if we delete
//       one object completely on the editor those filters will print an error during
//       pipeline update that I don't want to see. i already know that there is not
//       data to decimate or smooth if there is no voxel! as a matter of fact, it's
//       not even an error.
void VoxelVolumeRender::ComputeMesh(const unsigned short label)
{
    double rgba[4];

    // recompute actor when object bounds change just by calling ComputeMesh() again
    if (this->_meshActor != NULL)
    {
		this->_renderer->RemoveActor(this->_meshActor);
		this->_meshActor = NULL;
    }

	// first crop the region and then use the vtk-itk pipeline to get a itk::Image of the region
	Vector3ui objectMin = _dataManager->GetBoundingBoxMin(label);
	Vector3ui objectMax = _dataManager->GetBoundingBoxMax(label);
	Vector3ui size = _dataManager->GetOrientationData()->GetTransformedSize();

	// the object bounds collide with the object, we must add one not to clip the mesh at the borders
	if (objectMin[0] > 0)
		objectMin[0]--;

	if (objectMin[1] > 0)
		objectMin[1]--;

	if (objectMin[2] > 0)
		objectMin[2]--;

	if (objectMax[0] < size[0])
		objectMax[0]++;

	if (objectMax[1] < size[1])
		objectMax[1]++;

	if (objectMax[2] < size[2])
		objectMax[2]++;

	// image clipping
	vtkSmartPointer<vtkImageClip> imageClip = vtkSmartPointer<vtkImageClip>::New();
	imageClip->SetInput(this->_dataManager->GetStructuredPoints());
	imageClip->SetOutputWholeExtent(objectMin[0], objectMax[0], objectMin[1], objectMax[1], objectMin[2], objectMax[2]);
	imageClip->ClipDataOn();
	this->_progress->Observe(imageClip, "Clip", 1.0/4.0);
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
	this->_progress->Observe(marcher, "March", 1.0/4.0);
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
	this->_progress->Observe(decimator, "Decimate", 1.0/4.0);
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
    this->_progress->Observe(isoMapper, "Map", 1.0/4.0);
	isoMapper->SetInputConnection(normals->GetOutputPort());
	isoMapper->ReleaseDataFlagOn();
	isoMapper->ImmediateModeRenderingOn();
	isoMapper->ScalarVisibilityOff();
	isoMapper->Update();
	this->_progress->Ignore(isoMapper);

	// create the actor a assign a color to it
	this->_meshActor = vtkSmartPointer<vtkActor>::New();
	this->_meshActor->SetMapper(isoMapper);
	this->_dataManager->GetColorComponents(label, rgba);
	this->_meshActor->GetProperty()->SetColor(rgba[0], rgba[1], rgba[2]);
	this->_meshActor->GetProperty()->SetOpacity(1);
	this->_meshActor->GetProperty()->SetSpecular(0.2);

	// add the actor to the renderer
	this->_renderer->AddActor(this->_meshActor);

	this->_progress->Reset();
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

    // create volume and add to render
    if (NULL != this->_volume)
    {
    	this->_renderer->RemoveActor(this->_volume);
    	this->_volume = NULL;
    }

    this->_volume = vtkSmartPointer<vtkVolume>::New();
    this->_volume->SetMapper(this->_volumemapper);
    this->_volume->SetProperty(volumeproperty);

    this->_renderer->AddVolume(_volume);
}

// update object focus extent
void VoxelVolumeRender::UpdateFocusExtent(void)
{
    // if the selected label has no voxels it has no centroid, don't show anything
    if (0LL == this->_dataManager->GetNumberOfVoxelsForLabel(this->_objectLabel) || (0 == this->_objectLabel))
    	this->_volumemapper->SetCroppingRegionPlanes(0,0,0,0,0,0);
    else
    {
    	Vector3d spacing = this->_dataManager->GetOrientationData()->GetImageSpacing();
    	Vector3ui min = this->_dataManager->GetBoundingBoxMin(this->_objectLabel);
    	Vector3ui max = this->_dataManager->GetBoundingBoxMax(this->_objectLabel);

    	// it should really be (origin-0.5) and (origin+object_size+0.5) for rendering the
    	// correct bounding box for the object, but if we do it that way very thin
    	// objects aren't rendered correctly, so 1.5 corrects this.
    	this->_volumemapper->SetCroppingRegionPlanes(
    			(min[0]-1.5)*spacing[0], (max[0]+1.5)*spacing[0],
    			(min[1]-1.5)*spacing[1], (max[1]+1.5)*spacing[1],
    			(min[2]-1.5)*spacing[2], (max[2]+1.5)*spacing[2]);
    }

    this->_volumemapper->CroppingOn();
    this->_volumemapper->SetCroppingRegionFlagsToSubVolume();
    this->_volumemapper->Update();

    // if we are rendering as mesh then recompute mesh (if not, mesh will get clipped against
    // boundaries set at the time of creation if the object grows outside old bounding box)
    if (NULL != this->_meshActor)
    	this->ViewAsMesh();
}

void VoxelVolumeRender::FocusSegmentation(unsigned short label)
{
	// dim previous label
	ColorDim(this->_objectLabel);

	this->_objectLabel = label;

	ViewAsVolume();
	UpdateFocusExtent();
	this->_opacityfunction->Modified();
}

void VoxelVolumeRender::CenterSegmentation(unsigned short label)
{
    // if the selected label has no voxels it has no centroid, also don't want to center
	// the background label
    if ((0LL == this->_dataManager->GetNumberOfVoxelsForLabel(label)) || (0 == label))
    	return;

	Vector3d spacing = this->_dataManager->GetOrientationData()->GetImageSpacing();

    // change voxel renderer to move around the centroid of the object
    Vector3d centroid = this->_dataManager->GetCentroidForObject(label);
    this->_renderer->GetActiveCamera()->SetFocalPoint(centroid[0]*spacing[0],centroid[1]*spacing[1],centroid[2]*spacing[2]);
}

void VoxelVolumeRender::ViewAsMesh(void)
{
	if (0 != this->_objectLabel)
	{
		// hide color as we only want to show the mesh
		ColorDim(this->_objectLabel);
		this->_opacityfunction->Modified();
		ComputeMesh(this->_objectLabel);
	}
}

void VoxelVolumeRender::ViewAsVolume(void)
{
	if (NULL != this->_meshActor)
	{
		this->_renderer->RemoveActor(this->_meshActor);
		this->_meshActor = NULL;
	}

	if (0 != this->_objectLabel)
		ColorHighlightExclusive(this->_objectLabel);
}

// NOTE: this->_opacityfunction->Modified() not signaled. need to use UpdateColorTable() after
// calling this one to signal changes to pipeline
void VoxelVolumeRender::ColorHighlight(const unsigned short label)
{
	if (0 == label)
		return;

	if (this->_highlightedLabels.find(label) == this->_highlightedLabels.end())
	{
		this->_opacityfunction->AddPoint(label, 1.0);
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
		this->_opacityfunction->AddPoint(label, alpha);
		this->_highlightedLabels.erase(label);
	}
}

void VoxelVolumeRender::ColorHighlightExclusive(unsigned short label)
{
	std::set<unsigned short>::iterator it;
	for (it = this->_highlightedLabels.begin(); it != this->_highlightedLabels.end(); it++)
	{
		if ((*it) == label)
			continue;

		ColorDim(*it);
	}

	// do not highlight color if we are rendering as a mesh
	if ((this->_highlightedLabels.find(label) == this->_highlightedLabels.end()) && (NULL == this->_meshActor))
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

void VoxelVolumeRender::ResetHighlightedLabels(void)
{
	this->_highlightedLabels.clear();

	for (unsigned int i = 1; i < _dataManager->GetNumberOfColors(); i++)
		this->_opacityfunction->AddPoint(i, 0.0);

	this->_opacityfunction->Modified();
}

void VoxelVolumeRender::FocusSelection(Vector3ui min, Vector3ui max)
{
   	Vector3d spacing = this->_dataManager->GetOrientationData()->GetImageSpacing();

   	// it should really be (origin-0.5) and (origin+object_size+0.5) for rendering the
   	// correct bounding box for the object, but if we do it that way very thin
   	// objects aren't rendered correctly, so 1.5 corrects this.
   	this->_volumemapper->SetCroppingRegionPlanes(
    			(min[0]-1.5)*spacing[0], (max[0]+1.5)*spacing[0],
    			(min[1]-1.5)*spacing[1], (max[1]+1.5)*spacing[1],
    			(min[2]-1.5)*spacing[2], (max[2]+1.5)*spacing[2]);

    this->_volumemapper->CroppingOn();
    this->_volumemapper->SetCroppingRegionFlagsToSubVolume();
    this->_volumemapper->Update();
}
