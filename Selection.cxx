///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Selection.cxx
// Purpose: Manages selection areas
// Notes: Selected points have a value of 255 in the selection volume
///////////////////////////////////////////////////////////////////////////////////////////////////

// vtk includes
#include <vtkImageImport.h>
#include <vtkDiscreteMarchingCubes.h>
#include <vtkTextureMapToPlane.h>
#include <vtkTransformTextureCoords.h>
#include <vtkPolyDataMapper.h>
#include <vtkImageCanvasSource2D.h>
#include <vtkProperty.h>
#include <vtkImageClip.h>
#include <vtkImageConstantPad.h>
#include <vtkImageToStructuredPoints.h>

#include <vtkVolumeTextureMapper.h>
#include <vtkVolumeProperty.h>

// itk includes
#include <itkConnectedThresholdImageFilter.h>
#include <itkSmartPointer.h>
#include <itkVTKImageExport.h>
#include <itkImageRegionConstIteratorWithIndex.h>
#include <itkImageDuplicator.h>

// project includes
#include "Selection.h"
#include "itkvtkpipeline.h"
#include "SliceVisualization.h"

// qt includes
#include <QtGui>

///////////////////////////////////////////////////////////////////////////////////////////////////
// Selection class
//
Selection::Selection()
{
	// pointer inicialization to NULL
	this->_volume = NULL;
	this->_actor = NULL;
	this->_renderer = NULL;
	this->_dataManager = NULL;
	this->_selectionType = EMPTY;
}

Selection::~Selection()
{
    if (this->_actor != NULL)
    	this->_renderer->RemoveActor(_actor);

    // everything else handled by smartpointers, standard destructors or other classes destructors
}

void Selection::Initialize(Coordinates *orientation, vtkSmartPointer<vtkRenderer> renderer, DataManager *data)
{
	this->_size = orientation->GetTransformedSize() - Vector3ui(1,1,1);
	this->_min = Vector3ui(0,0,0);
	this->_max = this->_size;
	this->_renderer = renderer;
	this->_dataManager = data;

	// create selection volume
	Vector3d origin = orientation->GetImageOrigin();
	Vector3d spacing = orientation->GetImageSpacing();
	this->_volume = vtkSmartPointer<vtkStructuredPoints>::New();
	this->_volume->Initialize();
	this->_volume->SetNumberOfScalarComponents(1);
	this->_volume->SetScalarTypeToUnsignedChar();
	this->_volume->SetOrigin(origin[0], origin[1], origin[2]);
	this->_volume->SetSpacing(spacing[0], spacing[1], spacing[2]);
	this->_volume->SetExtent(0, this->_size[0], 0, this->_size[1], 0, this->_size[2]);
	this->_volume->AllocateScalars();
	this->_volume->Update();

	// create volume selection texture
	vtkSmartPointer<vtkImageCanvasSource2D> textureIcon = vtkSmartPointer<vtkImageCanvasSource2D>::New();
	textureIcon->SetScalarTypeToUnsignedChar();
	textureIcon->SetExtent(0, 15, 0, 15, 0, 0);
	textureIcon->SetNumberOfScalarComponents(4);
	textureIcon->SetDrawColor(0,0,0,0);             // transparent color
	textureIcon->FillBox(0,15,0,15);
	textureIcon->SetDrawColor(255,255,255,150);     // "somewhat transparent" white
	textureIcon->DrawSegment(0, 0, 15, 15);
	textureIcon->DrawSegment(1, 0, 15, 14);
	textureIcon->DrawSegment(0, 1, 14, 15);
	textureIcon->DrawSegment(15, 0, 15, 0);
	textureIcon->DrawSegment(0, 15, 0, 15);

	this->_texture = vtkSmartPointer<vtkTexture>::New();
	this->_texture->SetInputConnection(textureIcon->GetOutputPort());
	this->_texture->RepeatOn();
	this->_texture->InterpolateOn();
	this->_texture->ReleaseDataFlagOn();
}

void Selection::AddSelectionPoint(const Vector3ui point)
{
    // how many points do we have?
    switch (this->_selectedPoints.size())
    {
        case 0:
        	this->_selectionType = CUBE;
        	this->_selectedPoints.push_back(point);
            break;
        case 2:
        	this->_selectedPoints.pop_back();
        	this->_selectedPoints.push_back(point);
            break;
        default:
            this->_selectedPoints.push_back(point);
            break;
    }

    ComputeSelectionCube();
    ComputeActor();
}

void Selection::FillSelectionCube(unsigned char value)
{
	for (unsigned int x = this->_min[0]; x <= this->_max[0]; x++)
		for (unsigned int y = this->_min[1]; y <= this->_max[1]; y++)
			for (unsigned int z = this->_min[2]; z <= this->_max[2]; z++)
			{
				unsigned char *voxel = static_cast<unsigned char*>(this->_volume->GetScalarPointer(x,y,z));
				*voxel = value;
			}
}

void Selection::ComputeSelectionCube()
{
    std::vector<Vector3ui>::iterator it;

    // clear previously selected data before creating a new selection cube, if there is any
    FillSelectionCube(VOXEL_UNSELECTED);

    // initialize with first point
    this->_max = this->_min = this->_selectedPoints[0];

    // get selection bounds
    for (it = this->_selectedPoints.begin(); it != this->_selectedPoints.end(); it++)
    {
        if ((*it)[0] < this->_min[0])
        	this->_min[0] = (*it)[0];

        if ((*it)[1] < this->_min[1])
        	this->_min[1] = (*it)[1];

        if ((*it)[2] < this->_min[2])
        	this->_min[2] = (*it)[2];

        if ((*it)[0] > this->_max[0])
        	this->_max[0] = (*it)[0];

        if ((*it)[1] > this->_max[1])
        	this->_max[1] = (*it)[1];

        if ((*it)[2] > this->_max[2])
        	this->_max[2] = (*it)[2];
    }

    // fill selected values in volume and update volume
    FillSelectionCube(VOXEL_SELECTED);
    this->_volume->Modified();

    this->_axialView->ClearSelections();
    this->_coronalView->ClearSelections();
    this->_sagittalView->ClearSelections();

	vtkSmartPointer<vtkImageClip> imageClip = vtkSmartPointer<vtkImageClip>::New();
	imageClip->SetInput(this->_volume);
	imageClip->SetOutputWholeExtent(this->_min[0], this->_max[0], this->_min[1], this->_max[1], this->_min[2], this->_max[2]);
	imageClip->ClipDataOn();
	imageClip->Update();

	this->_axialView->SetSelectionVolume(imageClip->GetOutput());
	this->_coronalView->SetSelectionVolume(imageClip->GetOutput());
	this->_sagittalView->SetSelectionVolume(imageClip->GetOutput());
}


void Selection::ClearSelection(void)
{
	// don't do anything if there's no selection
	if (this->_selectionType == EMPTY)
		return;

    // remove the 3d render view actor
    if (NULL != this->_actor)
    {
    	this->_renderer->RemoveActor(this->_actor);
    	this->_actor = NULL;
    }

	FillSelectionCube(VOXEL_UNSELECTED);

	// clear selection points and bounds
    this->_selectedPoints.clear();
    this->_min = Vector3ui(0,0,0);
    this->_max = _size;
    this->_selectionType = EMPTY;

    this->_axialView->ClearSelections();
    this->_coronalView->ClearSelections();
    this->_sagittalView->ClearSelections();
}

const Selection::SelectionType Selection::GetSelectionType()
{
    return this->_selectionType;
}

void Selection::AddArea(Vector3ui point, const unsigned short label)
{
	// if the user picked in an already selected area just return
	unsigned char *voxel = static_cast<unsigned char*>(this->_volume->GetScalarPointer(point[0], point[1], point[2]));
	if (VOXEL_SELECTED == *voxel)
		return;

	itk::Index<3> seed;
	seed[0] = point[0];
	seed[1] = point[1];
	seed[2] = point[2];

    itk::SmartPointer<ImageType> image = ImageType::New();
    image = GetSegmentationItkImage(label);

    typedef itk::ConnectedThresholdImageFilter<ImageType, ImageTypeUC> ConnectedThresholdFilterType;
    itk::SmartPointer<ConnectedThresholdFilterType> connectThreshold = ConnectedThresholdFilterType::New();
    connectThreshold->SetInput(image);
    connectThreshold->AddSeed(seed);
    connectThreshold->ReleaseDataFlagOn();
    connectThreshold->SetLower(label);
    connectThreshold->SetUpper(label);
    connectThreshold->SetReplaceValue(VOXEL_SELECTED);
    connectThreshold->SetConnectivity(ConnectedThresholdFilterType::FullConnectivity);

	try
	{
		connectThreshold->UpdateLargestPossibleRegion();
	}
	catch (itk::ExceptionObject &excp)
	{
	    QMessageBox msgBox;
	    msgBox.setIcon(QMessageBox::Critical);
		msgBox.setText("An error occurred in connected thresholding.\nThe operation has been aborted.");
		msgBox.setDetailedText(excp.what());
		msgBox.exec();
		return;
	}

	// extract the interesting subvolume of our selection buffer
    Vector3ui min = this->_dataManager->GetBoundingBoxMin(label);
    Vector3ui max = this->_dataManager->GetBoundingBoxMax(label);

    if (this->_selectionType == EMPTY)
    {
    	this->_min = min;
    	this->_max = max;
    }
    else
    {
		// not our first, update selection bounds
		if (this->_min[0] > min[0]) this->_min[0] = min[0];
		if (this->_min[1] > min[1]) this->_min[1] = min[1];
		if (this->_min[2] > min[2]) this->_min[2] = min[2];
		if (this->_max[0] < max[0]) this->_max[0] = max[0];
		if (this->_max[1] < max[1]) this->_max[1] = max[1];
		if (this->_max[2] < max[2]) this->_max[2] = max[2];
    }

    // clip selection volume and send the sub-volume to slices for reslice+actor
	vtkSmartPointer<vtkImageClip> imageClip = vtkSmartPointer<vtkImageClip>::New();
	imageClip->SetInput(this->_volume);
	imageClip->SetOutputWholeExtent(min[0], max[0], min[1], max[1], min[2], max[2]);
	imageClip->ClipDataOn();
	imageClip->Update();
	imageClip->AbortExecuteOn();

	// create a copy to desconect the subvolume from the pipeline (bc i don't want to update the actors
	// i will create later when i update de selection volume)
	vtkSmartPointer<vtkImageData> subvolume = vtkSmartPointer<vtkImageData>::New();
	subvolume->DeepCopy(imageClip->GetOutput());
	subvolume->Update();

	// iterate through image and copy selected points to selection volume
	typedef itk::ImageRegionConstIteratorWithIndex<ImageTypeUC> IteratorType;
	IteratorType it(connectThreshold->GetOutput(), connectThreshold->GetOutput()->GetLargestPossibleRegion());

	ImageType::IndexType index;
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    	index = it.GetIndex();

    	if (VOXEL_SELECTED == it.Get())
    	{
    		unsigned char *voxel = static_cast<unsigned char*>(this->_volume->GetScalarPointer(index[0], index[1], index[2]));
    		*voxel = VOXEL_SELECTED;

    		unsigned char *voxel2 = static_cast<unsigned char*>(subvolume->GetScalarPointer(index[0], index[1], index[2]));
    		*voxel2 = VOXEL_SELECTED;
    	}
    	else
    	{
    		unsigned char *voxel = static_cast<unsigned char*>(subvolume->GetScalarPointer(index[0], index[1], index[2]));
    		*voxel = VOXEL_UNSELECTED;
    	}
    }
    this->_volume->Modified();
    this->_volume->Update();
    subvolume->Update();

	this->_axialView->SetSelectionVolume(subvolume);
    this->_coronalView->SetSelectionVolume(subvolume);
	this->_sagittalView->SetSelectionVolume(subvolume);

	ComputeActor();
	this->_selectionType = VOLUME;
}

itk::SmartPointer<ImageType> Selection::GetSegmentationItkImage(const unsigned short label)
{
	Vector3ui objectMin = _dataManager->GetBoundingBoxMin(label);
	Vector3ui objectMax = _dataManager->GetBoundingBoxMax(label);

	// first crop the region and then use the vtk-itk pipeline to get a
	// itk::Image of the region
	vtkSmartPointer<vtkImageClip> imageClip = vtkSmartPointer<vtkImageClip>::New();
	imageClip->SetInput(this->_dataManager->GetStructuredPoints());
	imageClip->SetOutputWholeExtent(objectMin[0], objectMax[0], objectMin[1], objectMax[1], objectMin[2], objectMax[2]);
	imageClip->ClipDataOn();
	imageClip->Update();

    typedef itk::VTKImageImport<ImageType> ITKImport;
    itk::SmartPointer<ITKImport> itkImport = ITKImport::New();
    vtkSmartPointer<vtkImageExport> vtkExport = vtkSmartPointer<vtkImageExport>::New();
    vtkExport->SetInput(imageClip->GetOutput());
    ConnectPipelines(vtkExport, itkImport);
    itkImport->Update();

    // need to duplicate the output, or it will be destroyed when the pipeline goes
    // out of scope, and register it to increase the reference count to return the image.
    typedef itk::ImageDuplicator< ImageType > DuplicatorType;
    itk::SmartPointer<DuplicatorType> duplicator = DuplicatorType::New();
    duplicator->SetInputImage(itkImport->GetOutput());
    duplicator->Update();

    itk::SmartPointer<ImageType> image = ImageType::New();
    image = duplicator->GetOutput();
    image->Register();

    return image;
}

// label must be specified always, but it's only used when there's nothing selected
itk::SmartPointer<ImageType> Selection::GetSelectionItkImage(const unsigned short label, const unsigned int boundsGrow)
{
	Vector3ui objectMin, objectMax;

	// first crop the region and then use the vtk-itk pipeline to get a
	// itk::Image of the region
	vtkSmartPointer<vtkImageClip> imageClip = vtkSmartPointer<vtkImageClip>::New();
	imageClip->SetInput(this->_dataManager->GetStructuredPoints());

	if (EMPTY == this->_selectionType)
	{
		objectMin = _dataManager->GetBoundingBoxMin(label);
		objectMax = _dataManager->GetBoundingBoxMax(label);
	}
	else
	{
		objectMin = this->_min;
		objectMax = this->_max;
	}

	// take care of the limits of the image when growing
	if (objectMin[0] < boundsGrow)
		objectMin[0] = 0;
	else
		objectMin[0] -= boundsGrow;

	if (objectMin[1] < boundsGrow)
		objectMin[1] = 0;
	else
		objectMin[1] -= boundsGrow;

	if (objectMin[2] < boundsGrow)
		objectMin[2] = 0;
	else
		objectMin[2] -= boundsGrow;

	if ((objectMax[0] + boundsGrow) > this->_size[0])
		objectMax[0] = _size[0];
	else
		objectMax[0] += boundsGrow;

	if ((objectMax[1] + boundsGrow) > this->_size[1])
		objectMax[1] = _size[1];
	else
		objectMax[1] += boundsGrow;

	if ((objectMax[2] + boundsGrow) > this->_size[2])
		objectMax[2] = _size[2];
	else
		objectMax[2] += boundsGrow;

	imageClip->SetOutputWholeExtent(objectMin[0], objectMax[0], objectMin[1], objectMax[1], objectMin[2], objectMax[2]);
	imageClip->ClipDataOn();
	imageClip->Update();

    typedef itk::VTKImageImport<ImageType> ITKImport;
    itk::SmartPointer<ITKImport> itkImport = ITKImport::New();
    vtkSmartPointer<vtkImageExport> vtkExport = vtkSmartPointer<vtkImageExport>::New();
    vtkExport->SetInput(imageClip->GetOutput());
    ConnectPipelines(vtkExport, itkImport);
    itkImport->Update();

    // need to duplicate the output, or it will be destroyed when the pipeline goes
    // out of scope, and register it to increase the reference count to return the image.
    typedef itk::ImageDuplicator< ImageType > DuplicatorType;
    itk::SmartPointer<DuplicatorType> duplicator = DuplicatorType::New();
    duplicator->SetInputImage(itkImport->GetOutput());
    duplicator->Update();

    itk::SmartPointer<ImageType> image = ImageType::New();
    image = duplicator->GetOutput();
    image->Register();

    return image;
}

itk::SmartPointer<ImageType> Selection::GetItkImage(void)
{
    typedef itk::VTKImageImport<ImageType> ITKImport;
    itk::SmartPointer<ITKImport> itkImport = ITKImport::New();
    vtkSmartPointer<vtkImageExport> vtkExport = vtkSmartPointer<vtkImageExport>::New();
    vtkExport->SetInput(_dataManager->GetStructuredPoints());
    ConnectPipelines(vtkExport, itkImport);
    itkImport->Update();

    // need to duplicate the output, or it will be destroyed when the pipeline goes
    // out of scope, and register it to increase the reference count to return the image.
    typedef itk::ImageDuplicator< ImageType > DuplicatorType;
    itk::SmartPointer<DuplicatorType> duplicator = DuplicatorType::New();
    duplicator->SetInputImage(itkImport->GetOutput());
    duplicator->Update();

    itk::SmartPointer<ImageType> image = ImageType::New();
    image = duplicator->GetOutput();
    image->Register();

    return image;
}

const Vector3ui Selection::GetSelectedMinimumBouds(void)
{
	return this->_min;
}

const Vector3ui Selection::GetSelectedMaximumBouds(void)
{
	return this->_max;
}

bool Selection::VoxelIsInsideSelection(unsigned int x, unsigned int y, unsigned int z)
{
	unsigned char *voxel = static_cast<unsigned char*>(this->_volume->GetScalarPointer(x,y,z));

	return (VOXEL_SELECTED == *voxel);
}

void Selection::ClearSelectionBuffer(void)
{
	unsigned char *pointer = static_cast<unsigned char *>(_volume->GetScalarPointer());
	Vector3ui size = this->_size + Vector3ui(1,1,1);
	memset(pointer, VOXEL_UNSELECTED, size[0]*size[1]*size[2]);
}

void Selection::ComputeActor(void)
{
	if (this->_actor != NULL)
		return;

	// create and setup actor for selection area... some parts of the pipeline have SetGlobalWarningDisplay(false)
	// because i don't want to generate a warning when used with empty input data (no user selection)

    // generate surface
    vtkSmartPointer<vtkDiscreteMarchingCubes> marcher = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
	marcher->SetInput(this->_volume);
	marcher->ReleaseDataFlagOn();
	marcher->SetGlobalWarningDisplay(false);
	marcher->SetNumberOfContours(1);
	marcher->GenerateValues(1, VOXEL_SELECTED, VOXEL_SELECTED);
	marcher->ComputeScalarsOff();
	marcher->ComputeNormalsOff();
	marcher->ComputeGradientsOff();

	// NOTE: not using normals to render the selection because we need to represent as many voxels as possible, also
	// we don't decimate our mesh for the same reason. Because the segmentations used are usually very small there
	// shouldn't be any rendering/performance problems.

    vtkSmartPointer<vtkTextureMapToPlane> textureMapper = vtkSmartPointer<vtkTextureMapToPlane>::New();
    textureMapper->SetInputConnection(marcher->GetOutputPort());
	textureMapper->SetGlobalWarningDisplay(false);
    textureMapper->AutomaticPlaneGenerationOn();

    vtkSmartPointer<vtkTransformTextureCoords> textureTrans = vtkSmartPointer<vtkTransformTextureCoords>::New();
    textureTrans->SetInputConnection(textureMapper->GetOutputPort());
    textureTrans->SetGlobalWarningDisplay(false);
    textureTrans->SetScale(_size[0], _size[1], _size[2]);

	// model mapper
    vtkSmartPointer<vtkPolyDataMapper> polydataMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	polydataMapper->SetInputConnection(textureTrans->GetOutputPort());
	polydataMapper->SetResolveCoincidentTopologyToOff();

    this->_actor = vtkSmartPointer<vtkActor>::New();
    this->_actor->SetMapper(polydataMapper);
    this->_actor->SetTexture(this->_texture);
    this->_actor->GetProperty()->SetOpacity(1);
    this->_actor->SetVisibility(true);

    this->_renderer->AddActor(this->_actor);
}

void Selection::SetSliceViews(SliceVisualization* axialSliceView, SliceVisualization* coronalSliceView, SliceVisualization* sagittalSliceView)
{
	this->_axialView = axialSliceView;
	this->_coronalView = coronalSliceView;
	this->_sagittalView = sagittalSliceView;
}

