///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: VoxelVolumeRender.cxx
// Purpose: renders a vtkStructuredPoints volume (raycasting volume or colored meshes)
// Notes: An implementation with vtkMultiBlockDataSet that allow mesh representation with just one
//        actor and mapper is disabled but the code is still present because it's and objective
//        worth pursuing in the future
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
#include <vtkImageCanvasSource2D.h>
#include <vtkTexture.h>
#include <vtkTextureMapToPlane.h>
//#include <vtkCompositePolyDataMapper2.h>
#include <vtkTransformTextureCoords.h>
#include <vtkXGPUInfoList.h>
#include <vtkGPUInfo.h>
//#include <vtkPointData.h>
//#include <vtkCellData.h>
//#include <vtkFloatArray.h>
//#include <vtkCellDataToPointData.h>

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
    this->_CPUmapper = NULL;
    this->_GPUmapper = NULL;
    this->_min = this->_max = Vector3ui(0,0,0);
    this->_renderingIsVolume = true;

// MultiBlockDataSet implementation disabled
//    this->_blockData = vtkSmartPointer<vtkMultiBlockDataSet>::New();
//    this->_blockData->SetUpdateExtentToWholeExtent();
//    this->_blockData->Initialize();

//    // we will use the mesh and ray-cast methods at the same time, but alternating
//    // visibility between the two (MultiBlockDataSet implementation)
//    this->ComputeMeshes();

    // ComputeRayCast();
    ComputeCPURender();
    UpdateFocusExtent();
}

void VoxelVolumeRender::ComputeRayCast(void)
{
	// fallback to CPU render if GPU is not available
    this->_GPUmapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    if (this->_GPUmapper->IsRenderSupported(this->_renderer->GetRenderWindow(), NULL))
    	ComputeGPURender();
    else
    {
    	this->_GPUmapper = NULL;
    	ComputeCPURender();
    }
}

VoxelVolumeRender::~VoxelVolumeRender()
{
    // using smartpointers
    
    // delete actors from renderers before leaving. volume actor always present
	this->_renderer->RemoveActor(_volume);

	std::map<unsigned short, struct ActorInformation* >::iterator it;
	for (it = this->_actorList.begin(); it != this->_actorList.end(); it++)
	{
		this->_renderer->RemoveActor((*it).second->meshActor);
		(*it).second->meshActor = NULL;
		delete (*it).second;
		this->_actorList.erase((*it).first);
	}
}

// MultiblockDataSet implementation disabled

//VoxelVolumeRender::~VoxelVolumeRender()
//{
//    // using smartpointers
//
//    // delete actor from renderer before leaving
//	if (this->_renderingIsVolume)
//		this->_renderer->RemoveActor(this->_volume);
//	else
//		this->_renderer->RemoveActor(this->_mesh);
//}
//
//void VoxelVolumeRender::ComputeMeshes(void)
//{
//	double rgba[4];
//    double weight = 1.0/((3.0*this->_dataManager->GetNumberOfLabels())+1);
//
//	for (unsigned short i = 1; i < this->_dataManager->GetNumberOfLabels(); i++)
//	{
//		struct VoxelVolumeRender::ActorInformation* actorInfo = new struct VoxelVolumeRender::ActorInformation();
//	    actorInfo->actorMin = this->_dataManager->GetBoundingBoxMin(i);
//	    actorInfo->actorMax = this->_dataManager->GetBoundingBoxMax(i);
//	    this->_segmentationInfoList.insert(std::pair<const unsigned short, VoxelVolumeRender::ActorInformation*>(i, actorInfo));
//
//		// first crop the region and then use the vtk-itk pipeline to get a itk::Image of the region
//	    Vector3ui size = this->_dataManager->GetOrientationData()->GetTransformedSize();
//		Vector3ui objectMin = actorInfo->actorMin;
//		Vector3ui objectMax = actorInfo->actorMax;
//
//		// the object bounds collide with the object, we must add one not to clip the mesh at the borders
//		if (objectMin[0] > 0) objectMin[0]--;
//		if (objectMin[1] > 0) objectMin[1]--;
//		if (objectMin[2] > 0) objectMin[2]--;
//		if (objectMax[0] < size[0]) objectMax[0]++;
//		if (objectMax[1] < size[1])	objectMax[1]++;
//		if (objectMax[2] < size[2])	objectMax[2]++;
//
//		// image clipping
//		vtkSmartPointer<vtkImageClip> imageClip = vtkSmartPointer<vtkImageClip>::New();
//		imageClip->SetInput(this->_dataManager->GetStructuredPoints());
//		imageClip->SetOutputWholeExtent(objectMin[0], objectMax[0], objectMin[1], objectMax[1], objectMin[2], objectMax[2]);
//		imageClip->ClipDataOn();
//		this->_progress->Observe(imageClip, "Clip", weight);
//		imageClip->Update();
//		this->_progress->Ignore(imageClip);
//
//	    // generate iso surface
//	    vtkSmartPointer<vtkDiscreteMarchingCubes> marcher = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
//		marcher->SetInput(imageClip->GetOutput());
//		marcher->ReleaseDataFlagOn();
//		marcher->SetNumberOfContours(1);
//		marcher->GenerateValues(1, i, i);
//		marcher->ComputeScalarsOn();
//		marcher->ComputeNormalsOff();
//		marcher->ComputeGradientsOff();
//		this->_progress->Observe(marcher, "March", weight);
//		marcher->Update();
//		this->_progress->Ignore(marcher);
//
//		// pass the cell data (segmentation scalars) to point data to color the segmentation
//		// in the mapper
//		vtkSmartPointer<vtkCellDataToPointData> cell2point = vtkSmartPointer<vtkCellDataToPointData>::New();
//		cell2point->SetInputConnection(marcher->GetOutputPort());
//
//		// decimate surface
//	    vtkSmartPointer<vtkDecimatePro> decimator = vtkSmartPointer<vtkDecimatePro>::New();
//		decimator->SetInputConnection(cell2point->GetOutputPort());
//		decimator->ReleaseDataFlagOn();
//		decimator->SetGlobalWarningDisplay(false);
//		decimator->SetTargetReduction(0.95);
//		decimator->PreserveTopologyOn();
//		decimator->BoundaryVertexDeletionOn();
//		decimator->SplittingOff();
//		this->_progress->Observe(decimator, "Decimate", weight);
//		decimator->Update();
//		this->_progress->Ignore(decimator);
//
//		// surface smoothing
//	    vtkSmartPointer<vtkWindowedSincPolyDataFilter> smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
//		smoother->SetInputConnection(decimator->GetOutputPort());
//		smoother->ReleaseDataFlagOn();
//		smoother->SetGlobalWarningDisplay(false);
//		smoother->BoundarySmoothingOn();
//		smoother->FeatureEdgeSmoothingOn();
//		smoother->SetNumberOfIterations(15);
//		smoother->SetFeatureAngle(120);
//		smoother->SetEdgeAngle(90);
//
//		// compute normals for a better looking volume
//	    vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
//		normals->SetInputConnection(smoother->GetOutputPort());
//		normals->SetInformation(marcher->GetInformation());
//		normals->ReleaseDataFlagOff();
//		normals->SetFeatureAngle(120);
//		normals->GetOutput()->Update();
//
//		this->_blockData->SetBlock(i-1, normals->GetOutput());
//	}
//
//	// copy the lookuptable from datamanager and make all colors transparent
//	vtkSmartPointer<vtkLookupTable> originalLUT = this->_dataManager->GetLookupTable();
//	this->_meshLUT = vtkSmartPointer<vtkLookupTable>::New();
//	this->_meshLUT->Allocate();
//	this->_meshLUT->SetNumberOfTableValues(originalLUT->GetNumberOfTableValues());
//    for (int index = 0; index != originalLUT->GetNumberOfTableValues(); index++)
//    {
//    	originalLUT->GetTableValue(index, rgba);
//        this->_meshLUT->SetTableValue(index, rgba[0], rgba[1], rgba[2], 0.0);
//    }
//    this->_meshLUT->SetTableRange(0,originalLUT->GetNumberOfTableValues()-1);
//
//	// model mapper, will color the segmentations using the scalar field of the points
//    vtkSmartPointer<vtkCompositePolyDataMapper2> isoMapper = vtkSmartPointer<vtkCompositePolyDataMapper2>::New();
//    this->_progress->Observe(isoMapper, "Map", weight);
//	isoMapper->SetInputConnection(this->_blockData->GetProducerPort());
//	isoMapper->ReleaseDataFlagOn();
//	isoMapper->ImmediateModeRenderingOn();
//	isoMapper->SetLookupTable(this->_meshLUT);
//	isoMapper->SetColorModeToMapScalars();
//	isoMapper->SetScalarModeToUsePointData();
//	isoMapper->SetUseLookupTableScalarRange(true);
//	isoMapper->SetResolveCoincidentTopologyToPolygonOffset();
//	isoMapper->SetStatic(false);
//	this->_progress->Ignore(isoMapper);
//
//	// create the actor a assign a color to it, but don't add it to the renderer
//	this->_mesh = vtkSmartPointer<vtkActor>::New();
//	this->_mesh->SetMapper(isoMapper);
//	this->_mesh->GetProperty()->SetOpacity(1);
//	this->_mesh->GetProperty()->SetSpecular(0.2);
//}

// NOTE: some filters make use of SetGlobalWarningDisplay(false) because if we delete
//       one object completely on the editor those filters will print an error during
//       pipeline update that I don't want to see. i already know that there is not
//       data to decimate or smooth if there is no voxel! as a matter of fact, it's
//       not even an error.
//void VoxelVolumeRender::ComputeMesh(const unsigned short label)
//{
//    struct VoxelVolumeRender::ActorInformation* actorInfo;
//
//    // delete previous actor if any exists and the actual object bounding box is bigger than the stored with the actor.
//    if (this->_segmentationInfoList.find(label) != this->_segmentationInfoList.end())
//    {
//    	actorInfo = this->_segmentationInfoList[label];
//
//    	Vector3ui min = this->_dataManager->GetBoundingBoxMin(label);
//    	Vector3ui max = this->_dataManager->GetBoundingBoxMax(label);
//
//    	if ((actorInfo->actorMin[0] <= min[0]) && (actorInfo->actorMin[1] <= min[1]) && (actorInfo->actorMin[2] <= min[2]) &&
//    		(actorInfo->actorMax[0] >= max[0]) && (actorInfo->actorMax[1] >= max[1]) && (actorInfo->actorMax[2] >= max[2]))
//    	{
//    		// actual actor is sufficient and is not being clipped by the bounding box imposed at creation
//    		return;
//    	}
//
//    	delete this->_segmentationInfoList[label];
//    	this->_segmentationInfoList[label] = NULL;
//    	this->_segmentationInfoList.erase(label);
//    }
//
//    actorInfo = new struct VoxelVolumeRender::ActorInformation();
//    actorInfo->actorMin = this->_dataManager->GetBoundingBoxMin(label);
//    actorInfo->actorMax = this->_dataManager->GetBoundingBoxMax(label);
//    this->_segmentationInfoList.insert(std::pair<const unsigned short, VoxelVolumeRender::ActorInformation*>(label, actorInfo));
//
//	// first crop the region
//	Vector3ui objectMin = this->_dataManager->GetBoundingBoxMin(label);
//	Vector3ui objectMax = this->_dataManager->GetBoundingBoxMax(label);
//	Vector3ui size = this->_dataManager->GetOrientationData()->GetTransformedSize();
//	double weight = 1.0/4.0;
//
//	// the object bounds collide with the object, we must add one not to clip the mesh at the borders
//	if (objectMin[0] > 0) objectMin[0]--;
//	if (objectMin[1] > 0) objectMin[1]--;
//	if (objectMin[2] > 0) objectMin[2]--;
//	if (objectMax[0] < size[0]) objectMax[0]++;
//	if (objectMax[1] < size[1])	objectMax[1]++;
//	if (objectMax[2] < size[2])	objectMax[2]++;
//
//	// image clipping
//	vtkSmartPointer<vtkImageClip> imageClip = vtkSmartPointer<vtkImageClip>::New();
//	imageClip->SetInput(this->_dataManager->GetStructuredPoints());
//	imageClip->SetOutputWholeExtent(objectMin[0], objectMax[0], objectMin[1], objectMax[1], objectMin[2], objectMax[2]);
//	imageClip->ClipDataOn();
//	this->_progress->Observe(imageClip, "Clip", weight);
//	imageClip->Update();
//	this->_progress->Ignore(imageClip);
//
//    // generate iso surface
//    vtkSmartPointer<vtkDiscreteMarchingCubes> marcher = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
//	marcher->SetInput(imageClip->GetOutput());
//	marcher->ReleaseDataFlagOn();
//	marcher->SetNumberOfContours(1);
//	marcher->GenerateValues(1, label, label);
//	marcher->ComputeScalarsOn();
//	marcher->ComputeNormalsOff();
//	marcher->ComputeGradientsOff();
//	this->_progress->Observe(marcher, "March", weight);
//	marcher->Update();
//	this->_progress->Ignore(marcher);
//
//	// decimate surface
//    vtkSmartPointer<vtkDecimatePro> decimator = vtkSmartPointer<vtkDecimatePro>::New();
//	decimator->SetInputConnection(marcher->GetOutputPort());
//	decimator->ReleaseDataFlagOn();
//	decimator->SetGlobalWarningDisplay(false);
//	decimator->SetTargetReduction(0.95);
//	decimator->PreserveTopologyOn();
//	decimator->BoundaryVertexDeletionOn();
//	decimator->SplittingOff();
//	this->_progress->Observe(decimator, "Decimate", weight);
//	decimator->Update();
//	this->_progress->Ignore(decimator);
//
//	// surface smoothing
//    vtkSmartPointer<vtkWindowedSincPolyDataFilter> smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
//	smoother->SetInputConnection(decimator->GetOutputPort());
//	smoother->ReleaseDataFlagOn();
//	smoother->SetGlobalWarningDisplay(false);
//	smoother->BoundarySmoothingOn();
//	smoother->FeatureEdgeSmoothingOn();
//	smoother->SetNumberOfIterations(15);
//	smoother->SetFeatureAngle(120);
//	smoother->SetEdgeAngle(90);
//
//	// compute normals for a better looking render
//    vtkSmartPointer<vtkPolyDataNormals> normals = vtkSmartPointer<vtkPolyDataNormals>::New();
//	normals->SetInputConnection(smoother->GetOutputPort());
//	normals->ReleaseDataFlagOn();
//	normals->SetFeatureAngle(120);
//
//	// assign a color to it and update the block in the multi block data
//	vtkPolyData *segmentation = normals->GetOutput();
//	segmentation->Update();
//
//	// we must fill the point array with the segmentation scalar, so we can colour it later
//	vtkSmartPointer<vtkFloatArray> pointData = vtkSmartPointer<vtkFloatArray>::New();
//	pointData->SetNumberOfTuples(segmentation->GetNumberOfPoints());
//
//	for (int j = 0; j < segmentation->GetNumberOfPoints(); j++)
//	    pointData->SetTuple1(j,label);
//
//	segmentation->GetPointData()->SetScalars(pointData);
//	this->_blockData->SetBlock(label-1, segmentation);
//	this->_blockData->Update();
//
//	this->_progress->Reset();
//}

void VoxelVolumeRender::ComputeCPURender()
{
    double rgba[4];

    // model mapper
    this->_CPUmapper = vtkSmartPointer<vtkVolumeRayCastMapper>::New();
    this->_CPUmapper->SetInput(this->_dataManager->GetStructuredPoints());
    this->_CPUmapper->IntermixIntersectingGeometryOn();

    // standard composite function
    vtkSmartPointer<vtkVolumeRayCastCompositeFunction> volumefunction = vtkSmartPointer<vtkVolumeRayCastCompositeFunction>::New();
    this->_CPUmapper->SetVolumeRayCastFunction(volumefunction);

    // assign label colors
    this->_colorfunction = vtkSmartPointer<vtkColorTransferFunction>::New();
    this->_colorfunction->AllowDuplicateScalarsOff();
    for (unsigned short int i = 0; i != this->_dataManager->GetNumberOfColors(); i++)
    {
    	this->_dataManager->GetColorComponents(i, rgba);
    	this->_colorfunction->AddRGBPoint(i,rgba[0], rgba[1], rgba[2]);
    	if ((1.0 == rgba[3]) && (i != 0))
    		this->_highlightedLabels.insert(i);
    }

    // set all labels to an opacity of 0.0 except the highlighted ones
    this->_opacityfunction = vtkSmartPointer<vtkPiecewiseFunction>::New();
    for (unsigned int i = 0; i != this->_dataManager->GetNumberOfColors(); i++)
    	if (this->_highlightedLabels.find(i) != this->_highlightedLabels.end())
    		this->_opacityfunction->AddPoint(i, 1.0);
    	else
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
    this->_volume->SetMapper(this->_CPUmapper);
    this->_volume->SetProperty(volumeproperty);

    this->_renderer->AddVolume(_volume);
}

void VoxelVolumeRender::ComputeGPURender()
{
	double rgba[4];

	vtkSmartPointer<vtkXGPUInfoList> info = vtkSmartPointer<vtkXGPUInfoList>::New();
	info->Probe();

	assert(0 != info->GetNumberOfGPUs());
	vtkSmartPointer<vtkGPUInfo> gpuInfo = info->GetGPUInfo(0);

    vtkSmartPointer<vtkLookupTable> lookupTable = this->_dataManager->GetLookupTable();

    // GPU mapper created before, while instance init
    this->_GPUmapper->SetInput(this->_dataManager->GetStructuredPoints());
    this->_GPUmapper->SetScalarModeToUsePointData();
    this->_GPUmapper->SetAutoAdjustSampleDistances(false);
    this->_GPUmapper->SetMaximumImageSampleDistance(1.0); 		// this allows a much more defined volume when rotating, as it uses 1 ray per pixel

    // at this point if the volume has been reduced (see this->_GPUmapper->GetReductionRatio(double *) ) it's because the volume is bigger
    // than the memory of the card and doesn't fit, there is no workaround for this except get a better graphic card or use the software
    // solution (not really a solution after all, too slow)

    // assign label colors
    this->_colorfunction = vtkSmartPointer<vtkColorTransferFunction>::New();
    this->_colorfunction->AllowDuplicateScalarsOff();
    for (unsigned short int i = 0; i != lookupTable->GetNumberOfTableValues(); i++)
    {
    	lookupTable->GetTableValue(i, rgba);
        this->_colorfunction->AddRGBPoint(i,rgba[0], rgba[1], rgba[2]);
    }

    // we need to set all labels to an opacity of 0.0
    this->_opacityfunction = vtkSmartPointer<vtkPiecewiseFunction>::New();
    this->_opacityfunction->AddPoint(0, 0.0);
    for (int i=1; i != lookupTable->GetNumberOfTableValues(); i++)
        this->_opacityfunction->AddPoint(i, 0.0);

    // volume property
    vtkSmartPointer<vtkVolumeProperty> volumeproperty = vtkSmartPointer<vtkVolumeProperty>::New();
    volumeproperty->SetColor(this->_colorfunction);
    volumeproperty->SetScalarOpacity(this->_opacityfunction);
    volumeproperty->SetSpecular(0.0);
    volumeproperty->ShadeOn();
    volumeproperty->SetInterpolationTypeToNearest();

    // create volume and add to render
    if (NULL != this->_volume)
    	this->_renderer->RemoveActor(this->_volume);

    this->_volume = vtkSmartPointer<vtkVolume>::New();
    this->_volume->SetMapper(this->_GPUmapper);
    this->_volume->SetProperty(volumeproperty);

    this->_renderer->AddVolume(this->_volume);
}

// NOTE: some filters make use of SetGlobalWarningDisplay(false) because if we delete
//       one object completely on the editor those filters will print an error during
//       pipeline update that I don't want to see. i already know that there is not
//       data to decimate or smooth if there is no voxel! as a matter of fact, it's
//       not even an error.
void VoxelVolumeRender::ComputeMesh(const unsigned short label)
{
    double rgba[4];

    struct VoxelVolumeRender::ActorInformation* actorInfo;

    // delete previous actor if any exists and the actual object bounding box is bigger than the stored with the actor.
    if (this->_actorList.find(label) != this->_actorList.end())
    {
    	actorInfo = this->_actorList[label];

    	Vector3ui min = this->_dataManager->GetBoundingBoxMin(label);
    	Vector3ui max = this->_dataManager->GetBoundingBoxMax(label);

    	if ((actorInfo->actorMin[0] <= min[0]) && (actorInfo->actorMin[1] <= min[1]) && (actorInfo->actorMin[2] <= min[2]) &&
    		(actorInfo->actorMax[0] >= max[0]) && (actorInfo->actorMax[1] >= max[1]) && (actorInfo->actorMax[2] >= max[2]))
    	{
    		// actual actor is sufficient and is not being clipped by the bounding box imposed at creation
    		return;
    	}

    	this->_renderer->RemoveActor(this->_actorList[label]->meshActor);
    	delete this->_actorList[label];
    	this->_actorList[label] = NULL;
    	this->_actorList.erase(label);
    }

    actorInfo = new struct VoxelVolumeRender::ActorInformation();
    actorInfo->actorMin = this->_dataManager->GetBoundingBoxMin(label);
    actorInfo->actorMax = this->_dataManager->GetBoundingBoxMax(label);
    actorInfo->meshActor = vtkSmartPointer<vtkActor>::New();
    this->_actorList.insert(std::pair<const unsigned short, VoxelVolumeRender::ActorInformation*>(label, actorInfo));

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
	actorInfo->meshActor->SetMapper(isoMapper);
	this->_dataManager->GetColorComponents(label, rgba);
	actorInfo->meshActor->GetProperty()->SetColor(rgba[0], rgba[1], rgba[2]);
	actorInfo->meshActor->GetProperty()->SetOpacity(1);
	actorInfo->meshActor->GetProperty()->SetSpecular(0.2);

	// add the actor to the renderer
	this->_renderer->AddActor(actorInfo->meshActor);
}

// MultiBlockDataSet implemetantion disabled
//// update object focus extent and center the group of segmentations
//void VoxelVolumeRender::UpdateFocusExtent(void)
//{
//	// no labels case
//	if (this->_highlightedLabels.empty())
//	{
//		if (NULL != this->_GPUmapper)
//		{
//			this->_GPUmapper->SetCroppingRegionPlanes(0,0,0,0,0,0);
//			this->_GPUmapper->CroppingOn();
//			this->_GPUmapper->SetCroppingRegionFlagsToSubVolume();
//			this->_GPUmapper->Update();
//		}
//		else
//		{
//			this->_CPUmapper->SetCroppingRegionPlanes(0,0,0,0,0,0);
//			this->_CPUmapper->CroppingOn();
//			this->_CPUmapper->SetCroppingRegionFlagsToSubVolume();
//			this->_CPUmapper->Update();
//		}
//	    return;
//	}
//
//	double croppingCoords[6];
//	if (NULL != this->_GPUmapper)
//		this->_GPUmapper->GetCroppingRegionPlanes(croppingCoords);
//	else
//		this->_CPUmapper->GetCroppingRegionPlanes(croppingCoords);
//
//	this->_min = this->_max = Vector3ui(0,0,0);
//
//	// calculate combined centroid for the group of segmentations
//	// NOTE: this is not really necesary and it's not the right thing to do, what we should be doing
//	// is (this->_min/2, this->_max/2) coords to center the view, but doing this avoids "jumps" in
//	// the view while operating with multiple labels, as all individual labels are centered in
//	// their centroid.
//	std::set<unsigned short>::iterator it = this->_highlightedLabels.begin();
//
//	// bootstrap min/max values from the first of the selected segmentations
//	this->_min = this->_dataManager->GetBoundingBoxMin((*it));
//	this->_max = this->_dataManager->GetBoundingBoxMax((*it));
//
//	// calculate bounding box
//	for (it = this->_highlightedLabels.begin(); it != this->_highlightedLabels.end(); it++)
//	{
//		Vector3ui minBounds = this->_dataManager->GetBoundingBoxMin((*it));
//		Vector3ui maxBounds = this->_dataManager->GetBoundingBoxMax((*it));
//
//		if ((minBounds != this->_segmentationInfoList[*it]->actorMin) || (maxBounds != this->_segmentationInfoList[*it]->actorMax))
//			this->ComputeMesh(*it);
//
//		if (this->_min[0] > minBounds[0]) this->_min[0] = minBounds[0];
//		if (this->_min[1] > minBounds[1]) this->_min[1] = minBounds[1];
//		if (this->_min[2] > minBounds[2]) this->_min[2] = minBounds[2];
//		if (this->_max[0] < maxBounds[0]) this->_max[0] = maxBounds[0];
//		if (this->_max[1] < maxBounds[1]) this->_max[1] = maxBounds[1];
//		if (this->_max[2] < maxBounds[2]) this->_max[2] = maxBounds[2];
//	}
//
//	// calculate and set focus point
//	Vector3d focuspoint = Vector3d((this->_min[0]+this->_max[0])/2.0,(this->_min[1]+this->_max[1])/2.0,(this->_min[2]+this->_max[2])/2.0);
//	Vector3d spacing = this->_dataManager->GetOrientationData()->GetImageSpacing();
//	this->_renderer->GetActiveCamera()->SetFocalPoint(focuspoint[0]*spacing[0],focuspoint[1]*spacing[1],focuspoint[2]*spacing[2]);
//
//	double bounds[6] = {
//			(this->_min[0]-1.5)*spacing[0], (this->_max[0]+1.5)*spacing[0],
//			(this->_min[1]-1.5)*spacing[1], (this->_max[1]+1.5)*spacing[1],
//			(this->_min[2]-1.5)*spacing[2], (this->_max[2]+1.5)*spacing[2]	};
//
//	// do not update if the region is smaller and contained in the previous one, to avoid recalculation of the mesh actors.
//	if ((bounds[0] >= croppingCoords[0]) && (bounds[1] <= croppingCoords[1]) &&
//		(bounds[2] >= croppingCoords[2]) && (bounds[3] <= croppingCoords[3]) &&
//		(bounds[4] >= croppingCoords[4]) && (bounds[5] <= croppingCoords[5]))
//	{
//		return;
//	}
//
//	if (NULL != this->_GPUmapper)
//	{
//		this->_GPUmapper->SetCroppingRegionPlanes(bounds);
//		this->_GPUmapper->CroppingOn();
//		this->_GPUmapper->SetCroppingRegionFlagsToSubVolume();
//		this->_GPUmapper->Update();
//	}
//	else
//	{
//		this->_CPUmapper->SetCroppingRegionPlanes(bounds);
//		this->_CPUmapper->CroppingOn();
//		this->_CPUmapper->SetCroppingRegionFlagsToSubVolume();
//		this->_CPUmapper->Update();
//	}
//}
//
//void VoxelVolumeRender::ViewAsMesh(void)
//{
//	if (!this->_renderingIsVolume)
//		return;
//
//	this->_renderer->RemoveActor(this->_volume);
//	this->_renderer->AddActor(this->_mesh);
//
//	this->_progress->Reset();
//	this->_renderingIsVolume = false;
//}
//
//void VoxelVolumeRender::ViewAsVolume(void)
//{
//	if (this->_renderingIsVolume)
//		return;
//
//	this->_renderer->RemoveActor(this->_mesh);
//	this->_renderer->AddActor(this->_volume);
//
//	this->_progress->Reset();
//	this->_renderingIsVolume = true;
//}
//
//// NOTE: this->_opacityfunction->Modified() & this->_meshLUT->Modified() not signaled.
//// need to use UpdateColorTable() after calling this one to signal changes to pipeline
//void VoxelVolumeRender::ColorHighlight(const unsigned short label)
//{
//	if (0 == label)
//		return;
//
//	double rgba[4];
//	if (this->_highlightedLabels.find(label) == this->_highlightedLabels.end())
//	{
//		this->_highlightedLabels.insert(label);
//
//		this->_opacityfunction->AddPoint(label, 1.0);
//        this->_meshLUT->GetTableValue(label, rgba);
//        this->_meshLUT->SetTableValue(label, rgba[0], rgba[1], rgba[2], 1.0);
//	}
//}
//
//// NOTE: this->_opacityfunction->Modified() & this->_meshLUT->Modified() not signaled.
//// need to use UpdateColorTable() after calling this one to signal changes to pipeline.
//// NOTE: alpha parameter defaults to 0.0, see .h
//void VoxelVolumeRender::ColorDim(const unsigned short label, double alpha)
//{
//	if (0 == label)
//		return;
//
//	double rgba[4];
//	if (this->_highlightedLabels.find(label) != this->_highlightedLabels.end())
//	{
//		this->_highlightedLabels.erase(label);
//
//		this->_opacityfunction->AddPoint(label, alpha);
//        this->_meshLUT->GetTableValue(label, rgba);
//        this->_meshLUT->SetTableValue(label, rgba[0], rgba[1], rgba[2], alpha);
//	}
//}
//
//void VoxelVolumeRender::ColorHighlightExclusive(unsigned short label)
//{
//	std::set<unsigned short>::iterator it;
//	for (it = this->_highlightedLabels.begin(); it != this->_highlightedLabels.end(); it++)
//		ColorDim(*it);
//
//	if (this->_highlightedLabels.find(label) == this->_highlightedLabels.end())
//		ColorHighlight(label);
//
//	UpdateColorTable();
//}
//
//void VoxelVolumeRender::ColorDimAll(void)
//{
//	std::set<unsigned short>::iterator it;
//	for (it = this->_highlightedLabels.begin(); it != this->_highlightedLabels.end(); it++)
//		ColorDim(*it);
//
//	UpdateColorTable();
//}
//
//// needed to be public as a method so we can signal changes to the opacity function
//void VoxelVolumeRender::UpdateColorTable(void)
//{
//	this->_opacityfunction->Modified();
//	this->_meshLUT->Modified();
//}

// update object focus extent and center the group of segmentations
void VoxelVolumeRender::UpdateFocusExtent(void)
{
	// no labels case
	if (this->_highlightedLabels.empty())
	{
		if (NULL != this->_GPUmapper)
		{
			this->_GPUmapper->SetCroppingRegionPlanes(0,0,0,0,0,0);
			this->_GPUmapper->CroppingOn();
			this->_GPUmapper->SetCroppingRegionFlagsToSubVolume();
			this->_GPUmapper->Update();
		}
		else
		{
			this->_CPUmapper->SetCroppingRegionPlanes(0,0,0,0,0,0);
			this->_CPUmapper->CroppingOn();
			this->_CPUmapper->SetCroppingRegionFlagsToSubVolume();
			this->_CPUmapper->Update();
		}
	    return;
	}

	double croppingCoords[6];
	if (NULL != this->_GPUmapper)
		this->_GPUmapper->GetCroppingRegionPlanes(croppingCoords);
	else
		this->_CPUmapper->GetCroppingRegionPlanes(croppingCoords);

	this->_min = this->_max = Vector3ui(0,0,0);

	// calculate combined centroid for the group of segmentations
	// NOTE: this is not really necesary and it's not the right thing to do, what we should be doing
	// is (this->_min/2, this->_max/2) coords to center the view, but doing this avoids "jumps" in
	// the view while operating with multiple labels, as all individual labels are centered in
	// their centroid.
	std::set<unsigned short>::iterator it = this->_highlightedLabels.begin();

	// bootstrap min/max values from the first of the selected segmentations
	this->_min = this->_dataManager->GetBoundingBoxMin((*it));
	this->_max = this->_dataManager->GetBoundingBoxMax((*it));

	// calculate bounding box
	for (it = this->_highlightedLabels.begin(); it != this->_highlightedLabels.end(); it++)
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

	// calculate and set focus point
	Vector3d focuspoint = Vector3d((this->_min[0]+this->_max[0])/2.0,(this->_min[1]+this->_max[1])/2.0,(this->_min[2]+this->_max[2])/2.0);
	Vector3d spacing = this->_dataManager->GetOrientationData()->GetImageSpacing();
	this->_renderer->GetActiveCamera()->SetFocalPoint(focuspoint[0]*spacing[0],focuspoint[1]*spacing[1],focuspoint[2]*spacing[2]);

	double bounds[6] = {
			(this->_min[0]-1.5)*spacing[0], (this->_max[0]+1.5)*spacing[0],
			(this->_min[1]-1.5)*spacing[1], (this->_max[1]+1.5)*spacing[1],
			(this->_min[2]-1.5)*spacing[2], (this->_max[2]+1.5)*spacing[2]	};

	// do not update if the region is smaller and contained in the previous one, to avoid recalculation of the mesh actors.
	if ((bounds[0] >= croppingCoords[0]) && (bounds[1] <= croppingCoords[1]) &&
		(bounds[2] >= croppingCoords[2]) && (bounds[3] <= croppingCoords[3]) &&
		(bounds[4] >= croppingCoords[4]) && (bounds[5] <= croppingCoords[5]))
	{
		return;
	}

	if (NULL != this->_GPUmapper)
	{
		this->_GPUmapper->SetCroppingRegionPlanes(bounds);
		this->_GPUmapper->CroppingOn();
		this->_GPUmapper->SetCroppingRegionFlagsToSubVolume();
		this->_GPUmapper->Update();
	}
	else
	{
		this->_CPUmapper->SetCroppingRegionPlanes(bounds);
		this->_CPUmapper->CroppingOn();
		this->_CPUmapper->SetCroppingRegionFlagsToSubVolume();
		this->_CPUmapper->Update();
	}

    // if we are rendering as mesh then recompute mesh (if not, mesh will get clipped against
    // boundaries set at the time of creation if the object grows outside old bounding box)
    // Volume render does not need this, updating the extent is just fine
    if (!this->_actorList.empty())
    	this->ViewAsMesh();
}

void VoxelVolumeRender::ViewAsMesh(void)
{
	// hide all highlighted volumes
	std::set<unsigned short>::iterator it;
	for (it = this->_highlightedLabels.begin(); it != this->_highlightedLabels.end(); it++)
	{
		if (this->_renderingIsVolume)
			this->_opacityfunction->AddPoint(*it, 0.0);

		ComputeMesh(*it);
	}
	this->_progress->Reset();

	if (this->_renderingIsVolume)
		this->_opacityfunction->Modified();

	this->_renderingIsVolume = false;
}

void VoxelVolumeRender::ViewAsVolume(void)
{
	if (this->_renderingIsVolume)
		return;

	// delete all actors and while we're at it modify opacity values for volume rendering
	std::map<unsigned short, struct ActorInformation* >::iterator it;
	for (it = this->_actorList.begin(); it != this->_actorList.end(); it++)
	{
		this->_renderer->RemoveActor((*it).second->meshActor);
		(*it).second->meshActor = NULL;
		delete (*it).second;
		this->_opacityfunction->AddPoint((*it).first, 1.0);
		this->_actorList.erase((*it).first);
	}
	this->_actorList.clear();
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
				this->_progress->Reset();
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
void VoxelVolumeRender::ColorDim(const unsigned short label, double alpha)
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
				// actor MUST exist
				this->_renderer->RemoveActor(this->_actorList[label]->meshActor);
				delete this->_actorList[label];
				this->_actorList.erase(label);
				break;
			default: // can't happen
				break;
		}
		this->_highlightedLabels.erase(label);
	}
}

void VoxelVolumeRender::ColorHighlightExclusive(unsigned short label)
{
	std::set<unsigned short>::iterator it;
	for (it = this->_highlightedLabels.begin(); it != this->_highlightedLabels.end(); it++)
		ColorDim(*it);

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
