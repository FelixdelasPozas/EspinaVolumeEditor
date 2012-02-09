///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Selection.cxx
// Purpose: Manages selection areas
// Notes: Selected points have a value of 255 in the selection volume
///////////////////////////////////////////////////////////////////////////////////////////////////

// vtk includes
#include <vtkDiscreteMarchingCubes.h>
#include <vtkTextureMapToPlane.h>
#include <vtkTransformTextureCoords.h>
#include <vtkPolyDataMapper.h>
#include <vtkImageCanvasSource2D.h>
#include <vtkProperty.h>
#include <vtkImageClip.h>

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
	this->_axialView = NULL;
	this->_coronalView = NULL;
	this->_sagittalView = NULL;
	this->_renderer = NULL;
	this->_dataManager = NULL;
	this->_selectionType = EMPTY;
	this->_size = this->_max = this->_min = Vector3ui(0,0,0);
	this->_spacing = Vector3d(0.0,0.0,0.0);
}

Selection::~Selection()
{
    DeleteSelectionActors();
    DeleteSelectionVolumes();

    // everything else handled by smartpointers, standard destructors or other classes destructors
}

void Selection::Initialize(Coordinates *orientation, vtkSmartPointer<vtkRenderer> renderer, DataManager *data)
{
	this->_size = orientation->GetTransformedSize() - Vector3ui(1,1,1);
	this->_spacing = orientation->GetImageSpacing();
	this->_max = this->_size;
	this->_renderer = renderer;
	this->_dataManager = data;

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
}


void Selection::ComputeSelectionCube()
{
    std::vector<Vector3ui>::iterator it;

    // clear previously selected data before creating a new selection cube, if there is any
    DeleteSelectionActors();
    DeleteSelectionVolumes();
    this->_axialView->ClearSelections();
    this->_coronalView->ClearSelections();
    this->_sagittalView->ClearSelections();

    // initialize with first point
    this->_max = this->_min = this->_selectedPoints[0];

    // get actual selection bounds
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

    Vector3ui minBounds = this->_min;
    Vector3ui maxBounds = this->_max;

    //modify bounds so theres a point of separation between bounds and actor
    if (this->_min[0] > 0) minBounds[0]--;
    if (this->_min[1] > 0) minBounds[1]--;
    if (this->_min[2] > 0) minBounds[2]--;
    if (this->_max[0] < this->_size[0]) maxBounds[0]++;
    if (this->_max[1] < this->_size[1]) maxBounds[1]++;
    if (this->_max[2] < this->_size[2]) maxBounds[2]++;

    // create selection volume (plus borders for correct actor generation)
    vtkSmartPointer<vtkImageData> subvolume = vtkSmartPointer<vtkImageData>::New();
	subvolume->SetNumberOfScalarComponents(1);
	subvolume->SetScalarTypeToUnsignedChar();
    subvolume->SetSpacing(this->_spacing[0], this->_spacing[1], this->_spacing[2]);
    subvolume->SetExtent(minBounds[0], maxBounds[0], minBounds[1], maxBounds[1], minBounds[2], maxBounds[2]);
    subvolume->AllocateScalars();
    subvolume->Update();

    // fill volume
    unsigned char *voxel = static_cast<unsigned char*>(subvolume->GetScalarPointer());
    memset(voxel, 0, (maxBounds[0]-minBounds[0]+1)*(maxBounds[1]-minBounds[1]+1)*(maxBounds[2]-minBounds[2]+1));
	for (unsigned int x = this->_min[0]; x <= this->_max[0]; x++)
		for (unsigned int y = this->_min[1]; y <= this->_max[1]; y++)
			for (unsigned int z = this->_min[2]; z <= this->_max[2]; z++)
			{
				voxel = static_cast<unsigned char*>(subvolume->GetScalarPointer(x,y,z));
				*voxel = VOXEL_SELECTED;
			}

    subvolume->Modified();

    // create textured actors for the slice views
	this->_axialView->SetSelectionVolume(subvolume);
	this->_coronalView->SetSelectionVolume(subvolume);
	this->_sagittalView->SetSelectionVolume(subvolume);

	// create render actor and add it to the list of actors (but there can only be one as this is a cube selection)
	ComputeActor(subvolume);
}


void Selection::ClearSelection(void)
{
	// don't do anything if there's no selection
	if (this->_selectionType == EMPTY)
		return;

	DeleteSelectionActors();
	DeleteSelectionVolumes();
    this->_axialView->ClearSelections();
    this->_coronalView->ClearSelections();
    this->_sagittalView->ClearSelections();

	// clear selection points and bounds
    this->_selectedPoints.clear();
    this->_min = Vector3ui(0,0,0);
    this->_max = _size;
    this->_selectionType = EMPTY;
}

const Selection::SelectionType Selection::GetSelectionType()
{
    return this->_selectionType;
}

void Selection::AddArea(Vector3ui point, const unsigned short label)
{
	// if the user picked in an already selected area just return
	if (VoxelIsInsideSelection(point[0], point[1], point[2]))
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

	typedef itk::VTKImageExport<ImageTypeUC> ITKExport;
	itk::SmartPointer<ITKExport> itkExporter = ITKExport::New();
	vtkSmartPointer<vtkImageImport> vtkImporter = vtkSmartPointer<vtkImageImport>::New();
	itkExporter->SetInput(connectThreshold->GetOutput());
	ConnectPipelines(itkExporter, vtkImporter);

	try
	{
		vtkImporter->Update();
	}
	catch (itk::ExceptionObject &excp)
	{
	    QMessageBox msgBox;
	    msgBox.setIcon(QMessageBox::Critical);
		msgBox.setText("An error occurred converting an itk image to a vtk image.\nThe operation has been aborted.");
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

	// create a copy of the subvolume
	vtkSmartPointer<vtkImageData> subvolume = vtkSmartPointer<vtkImageData>::New();
	subvolume->DeepCopy(vtkImporter->GetOutput());
	subvolume->Update();

	// add volume to list
	this->_selectionVolumesList.push_back(subvolume);

	// generate textured slice actors
	this->_axialView->SetSelectionVolume(subvolume);
    this->_coronalView->SetSelectionVolume(subvolume);
	this->_sagittalView->SetSelectionVolume(subvolume);

	// generate render actor
	ComputeActor(subvolume);

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
	std::vector<vtkSmartPointer<vtkImageData> >::iterator it;
	for (it = this->_selectionVolumesList.begin(); it != this->_selectionVolumesList.end(); it++)
		if (VoxelIsInsideSelectionSubvolume(*it, x,y,z))
			return true;

	return false;
}

bool Selection::VoxelIsInsideSelectionSubvolume(vtkSmartPointer<vtkImageData> subvolume, unsigned int x, unsigned int y, unsigned int z)
{
	int extent[6];
	subvolume->GetExtent(extent);

	if ((extent[0] <= static_cast<int>(x)) && (extent[1] >= static_cast<int>(x)) && (extent[2] <= static_cast<int>(y)) &&
		(extent[3] >= static_cast<int>(y)) && (extent[4] <= static_cast<int>(z)) && (extent[5] >= static_cast<int>(z)))
	{
		unsigned char *voxel = static_cast<unsigned char*>(subvolume->GetScalarPointer(x,y,z));
		return (VOXEL_SELECTED == *voxel);
	}

	return false;
}

void Selection::ComputeActor(vtkSmartPointer<vtkImageData> volume)
{
	// create and setup actor for selection area... some parts of the pipeline have SetGlobalWarningDisplay(false)
	// because i don't want to generate a warning when used with empty input data (no user selection)

    // generate surface
    vtkSmartPointer<vtkDiscreteMarchingCubes> marcher = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
	marcher->SetInput(volume);
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

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(polydataMapper);
    actor->SetTexture(this->_texture);
    actor->GetProperty()->SetOpacity(1);
    actor->SetVisibility(true);

    this->_renderer->AddActor(actor);

    // add actor to the actor list
    this->_selectionActorsList.push_back(actor);
}

void Selection::SetSliceViews(SliceVisualization* axialSliceView, SliceVisualization* coronalSliceView, SliceVisualization* sagittalSliceView)
{
	this->_axialView = axialSliceView;
	this->_coronalView = coronalSliceView;
	this->_sagittalView = sagittalSliceView;
}

void Selection::DeleteSelectionVolumes(void)
{
	std::vector<vtkSmartPointer<vtkImageData> >::iterator it;
	for (it = this->_selectionVolumesList.begin(); it != this->_selectionVolumesList.end(); it++)
		(*it) = NULL;

	this->_selectionVolumesList.clear();
}

void Selection::DeleteSelectionActors(void)
{
	std::vector<vtkSmartPointer<vtkActor> >::iterator it;
	for (it = this->_selectionActorsList.begin(); it != this->_selectionActorsList.end(); it++)
	{
		this->_renderer->RemoveActor((*it));
		(*it) = NULL;
	}

	this->_selectionActorsList.clear();
}
