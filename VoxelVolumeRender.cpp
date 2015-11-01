///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: VoxelVolumeRender.cpp
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
#include <vtkTransformTextureCoords.h>

// project includes
#include "DataManager.h"
#include "VoxelVolumeRender.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// VoxelVolumeRender class
//
VoxelVolumeRender::VoxelVolumeRender(std::shared_ptr<DataManager> dataManager, vtkSmartPointer<vtkRenderer> renderer, std::shared_ptr<ProgressAccumulator> pa)
: m_renderer{renderer}
, m_progress{pa}
, m_dataManager{dataManager}
, m_opacityfunction{nullptr}
, m_colorfunction{nullptr}
, m_volume{nullptr}
, m_mesh{nullptr}
, m_GPUmapper{nullptr}
, m_min{Vector3ui{0,0,0}}
, m_max{Vector3ui{0,0,0}}
, m_renderingIsVolume{true}
{
  computeGPURender();
  updateFocusExtent();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
VoxelVolumeRender::~VoxelVolumeRender()
{
  // delete actors from renderers before leaving. volume actor always present
  m_renderer->RemoveActor(m_volume);

  for (auto it: m_actors)
  {
    m_renderer->RemoveActor(it.second->mesh);
  }

  m_actors.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void VoxelVolumeRender::computeGPURender()
{
  double rgba[4];
  auto lookupTable = m_dataManager->GetLookupTable();

  m_GPUmapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
  m_GPUmapper->SetInputData(m_dataManager->GetStructuredPoints());
  m_GPUmapper->SetScalarModeToUsePointData();
  m_GPUmapper->SetAutoAdjustSampleDistances(false);
  m_GPUmapper->SetMaximumImageSampleDistance(1.0); 		// this allows a much more defined volume when rotating, as it uses 1 ray per pixel

  // at this point if the volume has been reduced (see m_GPUmapper->GetReductionRatio(double *) ) it's because the volume is bigger
  // than the memory of the card and doesn't fit, there is no workaround for this except get a better graphic card or use the software
  // solution (not really a solution after all, too slow)

  // assign label colors
  m_colorfunction = vtkSmartPointer<vtkColorTransferFunction>::New();
  m_colorfunction->AllowDuplicateScalarsOff();
  for (unsigned short int i = 0; i != lookupTable->GetNumberOfTableValues(); i++)
  {
    lookupTable->GetTableValue(i, rgba);
    m_colorfunction->AddRGBPoint(i, rgba[0], rgba[1], rgba[2]);
  }

  // we need to set all labels to an opacity of 0.0
  m_opacityfunction = vtkSmartPointer<vtkPiecewiseFunction>::New();
  m_opacityfunction->AddPoint(0, 0.0);

  for (int i = 1; i != lookupTable->GetNumberOfTableValues(); i++)
  {
    m_opacityfunction->AddPoint(i, 0.0);
  }

  // volume property
  vtkSmartPointer<vtkVolumeProperty> volumeproperty = vtkSmartPointer<vtkVolumeProperty>::New();
  volumeproperty->SetColor(m_colorfunction);
  volumeproperty->SetScalarOpacity(m_opacityfunction);
  volumeproperty->SetSpecular(0.0);
  volumeproperty->ShadeOn();
  volumeproperty->SetInterpolationTypeToNearest();

  m_volume = vtkSmartPointer<vtkVolume>::New();
  m_volume->SetMapper(m_GPUmapper);
  m_volume->SetProperty(volumeproperty);

  m_renderer->AddVolume(m_volume);

  for(auto label: m_dataManager->GetSelectedLabelsSet())
  {
    colorHighlight(label);
  }
}

// NOTE: some filters make use of SetGlobalWarningDisplay(false) because if we delete
//       one object completely on the editor those filters will print an error during
//       pipeline update that I don't want to see. i already know that there is not
//       data to decimate or smooth if there is no voxel! as a matter of fact, it's
//       not even an error.
///////////////////////////////////////////////////////////////////////////////////////////////////
void VoxelVolumeRender::computeMesh(const unsigned short label)
{
  std::shared_ptr<Pipeline> actorInfo;

  // delete previous actor if any exists and the actual object bounding box is bigger than the stored with the actor.
  if (m_actors.find(label) != m_actors.end())
  {
    actorInfo = m_actors[label];

    auto min = m_dataManager->GetBoundingBoxMin(label);
    auto max = m_dataManager->GetBoundingBoxMax(label);

    if ((actorInfo->min[0] <= min[0]) && (actorInfo->min[1] <= min[1]) && (actorInfo->min[2] <= min[2]) &&
        (actorInfo->max[0] >= max[0]) && (actorInfo->max[1] >= max[1]) && (actorInfo->max[2] >= max[2]))
    {
      // actual actor is sufficient and is not being clipped by the bounding box imposed at creation
      return;
    }

    m_renderer->RemoveActor(m_actors[label]->mesh);
    m_actors[label]->mesh = nullptr;
    m_actors[label] = nullptr;
    m_actors.erase(label);
  }

  actorInfo = std::make_shared<Pipeline>();
  actorInfo->min = m_dataManager->GetBoundingBoxMin(label);
  actorInfo->max = m_dataManager->GetBoundingBoxMax(label);
  actorInfo->mesh = vtkSmartPointer<vtkActor>::New();
  m_actors.insert(std::pair<const unsigned short, std::shared_ptr<Pipeline>>(label, actorInfo));

  // first crop the region and then use the vtk-itk pipeline to get a itk::Image of the region
  auto objectMin = m_dataManager->GetBoundingBoxMin(label);
  auto objectMax = m_dataManager->GetBoundingBoxMax(label);
  auto size = m_dataManager->GetOrientationData()->GetTransformedSize();
  auto weight = 1.0 / 4.0;

  // the object bounds collide with the object, we must add one not to clip the mesh at the borders
  if (objectMin[0] > 0) objectMin[0]--;
  if (objectMin[1] > 0) objectMin[1]--;
  if (objectMin[2] > 0) objectMin[2]--;
  if (objectMax[0] < size[0]) objectMax[0]++;
  if (objectMax[1] < size[1]) objectMax[1]++;
  if (objectMax[2] < size[2]) objectMax[2]++;

  // image clipping
  auto imageClip = vtkSmartPointer<vtkImageClip>::New();
  imageClip->SetInputData(m_dataManager->GetStructuredPoints());
  imageClip->SetOutputWholeExtent(objectMin[0], objectMax[0], objectMin[1], objectMax[1], objectMin[2], objectMax[2]);
  imageClip->ClipDataOn();
  m_progress->Observe(imageClip, "Clip", weight);
  imageClip->Update();
  m_progress->Ignore(imageClip);

  // generate iso surface
  auto marcher = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
  marcher->SetInputData(imageClip->GetOutput());
  marcher->ReleaseDataFlagOn();
  marcher->SetNumberOfContours(1);
  marcher->GenerateValues(1, label, label);
  marcher->ComputeScalarsOff();
  marcher->ComputeNormalsOff();
  marcher->ComputeGradientsOff();
  m_progress->Observe(marcher, "March", weight);
  marcher->Update();
  m_progress->Ignore(marcher);

  // decimate surface
  auto decimator = vtkSmartPointer<vtkDecimatePro>::New();
  decimator->SetInputConnection(marcher->GetOutputPort());
  decimator->ReleaseDataFlagOn();
  decimator->SetGlobalWarningDisplay(false);
  decimator->SetTargetReduction(0.95);
  decimator->PreserveTopologyOn();
  decimator->BoundaryVertexDeletionOn();
  decimator->SplittingOff();
  m_progress->Observe(decimator, "Decimate", weight);
  decimator->Update();
  m_progress->Ignore(decimator);

  // surface smoothing
  auto smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
  smoother->SetInputConnection(decimator->GetOutputPort());
  smoother->ReleaseDataFlagOn();
  smoother->SetGlobalWarningDisplay(false);
  smoother->BoundarySmoothingOn();
  smoother->FeatureEdgeSmoothingOn();
  smoother->SetNumberOfIterations(15);
  smoother->SetFeatureAngle(120);
  smoother->SetEdgeAngle(90);

  // compute normals for a better looking render
  auto normals = vtkSmartPointer<vtkPolyDataNormals>::New();
  normals->SetInputConnection(smoother->GetOutputPort());
  normals->ReleaseDataFlagOn();
  normals->SetFeatureAngle(120);

  // model mapper
  auto isoMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  m_progress->Observe(isoMapper, "Map", weight);
  isoMapper->SetInputConnection(normals->GetOutputPort());
  isoMapper->ReleaseDataFlagOn();
  isoMapper->ImmediateModeRenderingOn();
  isoMapper->ScalarVisibilityOff();
  isoMapper->Update();
  m_progress->Ignore(isoMapper);

  // create the actor and assign a color to it
  actorInfo->mesh->SetMapper(isoMapper);
  auto color = m_dataManager->GetColorComponents(label);
  actorInfo->mesh->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
  actorInfo->mesh->GetProperty()->SetOpacity(1);
  actorInfo->mesh->GetProperty()->SetSpecular(0.2);

  // add the actor to the renderer
  m_renderer->AddActor(actorInfo->mesh);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void VoxelVolumeRender::updateFocusExtent(void)
{
  // no labels case
  if (m_highlightedLabels.empty())
  {
    m_GPUmapper->SetCroppingRegionPlanes(0, 0, 0, 0, 0, 0);
    m_GPUmapper->CroppingOn();
    m_GPUmapper->SetCroppingRegionFlagsToSubVolume();
    m_GPUmapper->Update();
    return;
  }

  // calculate combined centroid for the group of segmentations
  // NOTE: this is not really necesary and it's not the right thing to do, what we should be doing
  // is (_min/2, _max/2) coords to center the view, but doing this avoids "jumps" in
  // the view while operating with multiple labels, as all individual labels are centered in
  // their centroid.
  m_min = m_max = Vector3ui(0, 0, 0);

  // bootstrap min/max values from the first of the selected segmentations
  m_min = m_dataManager->GetBoundingBoxMin(*m_highlightedLabels.cbegin());
  m_max = m_dataManager->GetBoundingBoxMax(*m_highlightedLabels.cbegin());

  // calculate bounding box
  for (auto it: m_highlightedLabels)
  {
    auto minBounds = m_dataManager->GetBoundingBoxMin(it);
    auto maxBounds = m_dataManager->GetBoundingBoxMax(it);

    if (m_min[0] > minBounds[0]) m_min[0] = minBounds[0];
    if (m_min[1] > minBounds[1]) m_min[1] = minBounds[1];
    if (m_min[2] > minBounds[2]) m_min[2] = minBounds[2];
    if (m_max[0] < maxBounds[0]) m_max[0] = maxBounds[0];
    if (m_max[1] < maxBounds[1]) m_max[1] = maxBounds[1];
    if (m_max[2] < maxBounds[2]) m_max[2] = maxBounds[2];
  }

  // calculate and set focus point
  auto focuspoint = Vector3d((m_min[0] + m_max[0]) / 2.0, (m_min[1] + m_max[1]) / 2.0, (m_min[2] + m_max[2]) / 2.0);
  auto spacing = m_dataManager->GetOrientationData()->GetImageSpacing();
  m_renderer->GetActiveCamera()->SetFocalPoint(focuspoint[0] * spacing[0], focuspoint[1] * spacing[1], focuspoint[2] * spacing[2]);

  double bounds[6] = { (m_min[0] - 1.5) * spacing[0],
                       (m_max[0] + 1.5) * spacing[0],
                       (m_min[1] - 1.5) * spacing[1],
                       (m_max[1] + 1.5) * spacing[1],
                       (m_min[2] - 1.5) * spacing[2],
                       (m_max[2] + 1.5) * spacing[2] };

  m_GPUmapper->SetCroppingRegionPlanes(bounds);
  m_GPUmapper->CroppingOn();
  m_GPUmapper->SetCroppingRegionFlagsToSubVolume();
  m_GPUmapper->Update();

  // if we are rendering as mesh then recompute mesh (if not, mesh will get clipped against
  // boundaries set at the time of creation if the object grows outside old bounding box)
  // Volume render does not need this, updating the extent is just fine
  if (!m_actors.empty())
  {
    viewAsMesh();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void VoxelVolumeRender::viewAsMesh()
{
  if(!m_renderingIsVolume) return;

  // hide all highlighted volumes
  for (auto label: m_highlightedLabels)
  {
    m_opacityfunction->AddPoint(label, 0.0);
    computeMesh(label);
  }
  m_progress->Reset();

  m_opacityfunction->Modified();
  m_renderingIsVolume = false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void VoxelVolumeRender::viewAsVolume(void)
{
  if (m_renderingIsVolume) return;

  // delete all actors and while we're at it modify opacity values for volume rendering
  std::set<unsigned short> toDelete;
  for (auto actor: m_actors)
  {
    m_renderer->RemoveActor(actor.second->mesh);
    actor.second->mesh = nullptr;
    m_actors[actor.first] = nullptr;
    m_opacityfunction->AddPoint(actor.first, 1.0);
    toDelete.insert(actor.first);
  }

  for(auto it: toDelete)
  {
    m_actors.erase(it);
  }

  m_actors.clear();
  m_opacityfunction->Modified();
  m_renderingIsVolume = true;
}

// NOTE: m_opacityfunction->Modified() not signaled. need to use UpdateColorTable() after
// calling this one to signal changes to pipeline
///////////////////////////////////////////////////////////////////////////////////////////////////
void VoxelVolumeRender::colorHighlight(const unsigned short label)
{
  if (0 == label) return;

  if (m_highlightedLabels.find(label) == m_highlightedLabels.end())
  {
    if (m_renderingIsVolume)
    {
      m_opacityfunction->AddPoint(label, 1.0);
    }
    else
    {
      computeMesh(label);
      m_progress->Reset();
    }
    m_highlightedLabels.insert(label);
    m_volume->Update();
  }
}

// NOTE: m_opacityfunction->Modified() not signaled. need to use UpdateColorTable() after
// calling this one to signal changes to pipeline.
///////////////////////////////////////////////////////////////////////////////////////////////////
void VoxelVolumeRender::colorDim(const unsigned short label, double alpha)
{
  if (0 == label) return;

  if (m_highlightedLabels.find(label) != m_highlightedLabels.end())
  {
    if (m_renderingIsVolume)
    {
      m_opacityfunction->AddPoint(label, alpha);
    }
    else
    {
      // actor MUST exist
      m_renderer->RemoveActor(m_actors[label]->mesh);
      m_actors[label]->mesh = nullptr;
      m_actors.erase(label);
    }
    m_highlightedLabels.erase(label);
    m_volume->Update();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void VoxelVolumeRender::colorHighlightExclusive(unsigned short label)
{
  auto highlighted = m_highlightedLabels;
  for (auto it: highlighted)
  {
    if(it == label) continue;

    colorDim(it);
  }

  if (m_highlightedLabels.find(label) == m_highlightedLabels.end())
  {
    colorHighlight(label);
  }

  m_opacityfunction->Modified();
  m_volume->Update();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void VoxelVolumeRender::colorDimAll(void)
{
  auto highlighted = m_highlightedLabels;
  for (auto it: highlighted)
  {
    colorDim(it);
  }

  m_opacityfunction->Modified();
  m_volume->Update();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void VoxelVolumeRender::updateColorTable(void)
{
  m_opacityfunction->Modified();
  m_volume->Update();
}
