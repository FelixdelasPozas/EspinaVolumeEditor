///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Selection.cxx
// Purpose: Manages selection areas
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// vtk includes
#include <vtkDiscreteMarchingCubes.h>
#include <vtkTextureMapToPlane.h>
#include <vtkTransformTextureCoords.h>
#include <vtkPolyDataMapper.h>
#include <vtkImageCanvasSource2D.h>
#include <vtkProperty.h>
#include <vtkImageClip.h>
#include <vtkWidgetRepresentation.h>
#include <vtkBoxRepresentation.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCommand.h>
#include <vtkLinearContourLineInterpolator.h>
#include <vtkImageActorPointPlacer.h>
#include <vtkFastNumericConversion.h>

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
#include "FocalPlanePointPlacer.h"
#include "ContourRepresentation.h"
#include "ContourRepresentationGlyph.h"

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
	this->_selectionType = Selection::EMPTY;
	this->_size = this->_max = this->_min = Vector3ui(0,0,0);
	this->_spacing = Vector3d(0.0,0.0,0.0);

    this->_axialBoxWidget = NULL;
    this->_coronalBoxWidget = NULL;
    this->_sagittalBoxWidget = NULL;
    this->_contourWidget = NULL;
    this->_boxRender = NULL;
    this->_selectionIsValid = true;

    this->_rotatedImage = NULL;
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
	vtkSmartPointer<BoxSelectionRepresentation2D> rep;
	BoxSelectionRepresentation2D *repPointer = NULL;
	double bounds[6];

    // how many points do we have?
    switch (this->_selectedPoints.size())
    {
        case 0:
        	this->_selectionType = Selection::CUBE;
        	this->_selectedPoints.push_back(point);
        	this->ComputeSelectionBounds();

        	// 3D representation of the box
        	this->_boxRender = new BoxSelectionRepresentation3D();
        	this->_boxRender->SetRenderer(this->_renderer);

        	bounds[0] = (static_cast<double>(this->_min[0])-0.5)*this->_spacing[0];
        	bounds[1] = (static_cast<double>(this->_max[0])+0.5)*this->_spacing[0];
        	bounds[2] = (static_cast<double>(this->_min[1])-0.5)*this->_spacing[1];
        	bounds[3] = (static_cast<double>(this->_max[1])+0.5)*this->_spacing[1];
        	bounds[4] = (static_cast<double>(this->_min[2])-0.5)*this->_spacing[2];
        	bounds[5] = (static_cast<double>(this->_max[2])+0.5)*this->_spacing[2];
        	this->_boxRender->PlaceBox(bounds);

        	// initialize 2D selection widgets
        	rep = vtkSmartPointer<BoxSelectionRepresentation2D>::New();
        	rep->SetMinimumSelectionSize(-(this->_spacing[0]/2.0), -(this->_spacing[1]/2.0));
        	rep->SetMaximumSelectionSize((static_cast<double>(this->_size[0])+0.5)*this->_spacing[0], (static_cast<double>(this->_size[1])+0.5)*this->_spacing[1]);
        	rep->SetMinimumSize(this->_spacing[0], this->_spacing[1]);
        	rep->SetSpacing(this->_spacing[0], this->_spacing[1]);
        	rep->SetBoxCoordinates(point[0], point[1], point[0], point[1]);
        	this->_axialBoxWidget = vtkSmartPointer<BoxSelectionWidget>::New();
        	this->_axialBoxWidget->SetInteractor(this->_axialView->GetViewRenderer()->GetRenderWindow()->GetInteractor());
        	this->_axialBoxWidget->SetRepresentation(rep);
        	this->_axialBoxWidget->SetEnabled(true);

        	rep = vtkSmartPointer<BoxSelectionRepresentation2D>::New();
        	rep->SetMinimumSelectionSize(-(this->_spacing[0]/2.0), -(this->_spacing[2]/2.0));
        	rep->SetMaximumSelectionSize((static_cast<double>(this->_size[0])+0.5)*this->_spacing[0], (static_cast<double>(this->_size[2])+0.5)*this->_spacing[2]);
        	rep->SetMinimumSize(this->_spacing[0], this->_spacing[2]);
        	rep->SetSpacing(this->_spacing[0], this->_spacing[2]);
        	rep->SetBoxCoordinates(point[0], point[2], point[0], point[2]);
        	this->_coronalBoxWidget = vtkSmartPointer<BoxSelectionWidget>::New();
        	this->_coronalBoxWidget->SetInteractor(this->_coronalView->GetViewRenderer()->GetRenderWindow()->GetInteractor());
        	this->_coronalBoxWidget->SetRepresentation(rep);
        	this->_coronalBoxWidget->SetEnabled(true);

        	rep = vtkSmartPointer<BoxSelectionRepresentation2D>::New();
        	rep->SetMinimumSelectionSize(-(this->_spacing[1]/2.0), -(this->_spacing[2]/2.0));
        	rep->SetMaximumSelectionSize((static_cast<double>(this->_size[1])+0.5)*this->_spacing[1], (static_cast<double>(this->_size[2])+0.5)*this->_spacing[2]);
        	rep->SetMinimumSize(this->_spacing[1], this->_spacing[2]);
        	rep->SetSpacing(this->_spacing[1], this->_spacing[2]);
        	rep->SetBoxCoordinates(point[1], point[2], point[1], point[2]);
        	this->_sagittalBoxWidget = vtkSmartPointer<BoxSelectionWidget>::New();
        	this->_sagittalBoxWidget->SetInteractor(this->_sagittalView->GetViewRenderer()->GetRenderWindow()->GetInteractor());
        	this->_sagittalBoxWidget->SetRepresentation(rep);
        	this->_sagittalBoxWidget->SetEnabled(true);

        	// set callbacks for widget interaction
        	this->_widgetsCallbackCommand = vtkSmartPointer<vtkCallbackCommand>::New();
        	this->_widgetsCallbackCommand->SetCallback(this->BoxSelectionWidgetCallback);
        	this->_widgetsCallbackCommand->SetClientData(this);

        	this->_axialBoxWidget->AddObserver(vtkCommand::StartInteractionEvent, this->_widgetsCallbackCommand);
        	this->_axialBoxWidget->AddObserver(vtkCommand::EndInteractionEvent, this->_widgetsCallbackCommand);
        	this->_axialBoxWidget->AddObserver(vtkCommand::InteractionEvent, this->_widgetsCallbackCommand);

        	this->_coronalBoxWidget->AddObserver(vtkCommand::StartInteractionEvent, this->_widgetsCallbackCommand);
        	this->_coronalBoxWidget->AddObserver(vtkCommand::EndInteractionEvent, this->_widgetsCallbackCommand);
        	this->_coronalBoxWidget->AddObserver(vtkCommand::InteractionEvent, this->_widgetsCallbackCommand);

        	this->_sagittalBoxWidget->AddObserver(vtkCommand::StartInteractionEvent, this->_widgetsCallbackCommand);
        	this->_sagittalBoxWidget->AddObserver(vtkCommand::EndInteractionEvent, this->_widgetsCallbackCommand);
        	this->_sagittalBoxWidget->AddObserver(vtkCommand::InteractionEvent, this->_widgetsCallbackCommand);

        	// make the slices aware of a selection box, so they could hide and show it accordingly when the slice changes
        	this->_axialView->SetSliceWidget(this->_axialBoxWidget);
        	this->_coronalView->SetSliceWidget(this->_coronalBoxWidget);
        	this->_sagittalView->SetSliceWidget(this->_sagittalBoxWidget);
            break;
        default:
        	// cases for sizes 1 and 2, "nearly" identical except a pop_back() in case 2
        	if (this->_selectedPoints.size() == 2)
        		this->_selectedPoints.pop_back();

            this->_selectedPoints.push_back(point);
            this->ComputeSelectionBounds();

        	bounds[0] = (static_cast<double>(this->_min[0])-0.5)*this->_spacing[0];
        	bounds[1] = (static_cast<double>(this->_max[0])+0.5)*this->_spacing[0];
        	bounds[2] = (static_cast<double>(this->_min[1])-0.5)*this->_spacing[1];
        	bounds[3] = (static_cast<double>(this->_max[1])+0.5)*this->_spacing[1];
        	bounds[4] = (static_cast<double>(this->_min[2])-0.5)*this->_spacing[2];
        	bounds[5] = (static_cast<double>(this->_max[2])+0.5)*this->_spacing[2];
        	this->_boxRender->PlaceBox(bounds);

        	// update widget representations
            this->_axialBoxWidget->SetEnabled(false);
            this->_coronalBoxWidget->SetEnabled(false);
            this->_sagittalBoxWidget->SetEnabled(false);

            repPointer = this->_axialBoxWidget->GetBorderRepresentation();
            repPointer->SetBoxCoordinates(this->_min[0], this->_min[1], this->_max[0], this->_max[1]);
            repPointer = this->_coronalBoxWidget->GetBorderRepresentation();
            repPointer->SetBoxCoordinates(this->_min[0], this->_min[2], this->_max[0], this->_max[2]);
            repPointer = this->_sagittalBoxWidget->GetBorderRepresentation();
            repPointer->SetBoxCoordinates(this->_min[1], this->_min[2], this->_max[1], this->_max[2]);

        	this->_axialBoxWidget->SetEnabled(true);
        	this->_coronalBoxWidget->SetEnabled(true);
        	this->_sagittalBoxWidget->SetEnabled(true);
            break;
    }

    ComputeSelectionCube();
}

void Selection::ComputeSelectionBounds()
{
	if (this->_selectedPoints.empty())
		return;

	std::vector<Vector3ui>::const_iterator it;

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
}

void Selection::ComputeSelectionCube()
{
    // clear previously selected data before creating a new selection cube, if there is any
    DeleteSelectionActors();
    DeleteSelectionVolumes();
    this->_axialView->ClearSelections();
    this->_coronalView->ClearSelections();
    this->_sagittalView->ClearSelections();

    // bounds can be negative so we need to avoid unsigned types
    Vector3i minBounds = Vector3i(static_cast<int>(this->_min[0]), static_cast<int>(this->_min[1]), static_cast<int>(this->_min[2]));
    Vector3i maxBounds = Vector3i(static_cast<int>(this->_max[0]), static_cast<int>(this->_max[1]), static_cast<int>(this->_max[2]));

    //modify bounds so theres a point of separation between bounds and actor
    if (this->_min[0] >= 0) minBounds[0]--;
    if (this->_min[1] >= 0) minBounds[1]--;
    if (this->_min[2] >= 0) minBounds[2]--;
    if (this->_max[0] <= this->_size[0]) maxBounds[0]++;
    if (this->_max[1] <= this->_size[1]) maxBounds[1]++;
    if (this->_max[2] <= this->_size[2]) maxBounds[2]++;

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
	DeleteSelectionActors();
	DeleteSelectionVolumes();
    this->_axialView->ClearSelections();
    this->_coronalView->ClearSelections();
    this->_sagittalView->ClearSelections();

    if (this->_selectionType == Selection::CUBE)
    {
    	this->_axialView->SetSliceWidget(NULL);
    	this->_coronalView->SetSliceWidget(NULL);
    	this->_sagittalView->SetSliceWidget(NULL);
		this->_axialBoxWidget = NULL;
		this->_coronalBoxWidget = NULL;
		this->_sagittalBoxWidget = NULL;
		this->_widgetsCallbackCommand = NULL;

		delete this->_boxRender;
    }

    if (this->_selectionType == Selection::CONTOUR)
    {
    	this->_axialView->SetSliceWidget(NULL);
    	this->_coronalView->SetSliceWidget(NULL);
    	this->_sagittalView->SetSliceWidget(NULL);
    	this->_contourWidget = NULL;
    	this->_rotatedImage = NULL;
    	this->_widgetsCallbackCommand = NULL;
    	this->_selectionIsValid = true;
    }
	// clear selection points and bounds
    this->_selectedPoints.clear();
    this->_min = Vector3ui(0,0,0);
    this->_max = this->_size;
    this->_selectionType = Selection::EMPTY;
}

const Selection::SelectionType Selection::GetSelectionType()
{
    return this->_selectionType;
}

void Selection::AddArea(const Vector3ui point)
{
	// if the user picked in an already selected area just return
	if (VoxelIsInsideSelection(point[0], point[1], point[2]))
		return;

	itk::Index<3> seed;
	seed[0] = point[0];
	seed[1] = point[1];
	seed[2] = point[2];

	unsigned short label = this->_dataManager->GetVoxelScalar(point[0], point[1], point[2]);
	assert(label != 0);
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
	    msgBox.setCaption("Error while selecting");
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
	    msgBox.setCaption("Error while selecting");
	    msgBox.setIcon(QMessageBox::Critical);
		msgBox.setText("An error occurred converting an itk image to a vtk image.\nThe operation has been aborted.");
		msgBox.setDetailedText(excp.what());
		msgBox.exec();
		return;
	}

	// extract the interesting subvolume of our selection buffer
    Vector3ui min = this->_dataManager->GetBoundingBoxMin(label);
    Vector3ui max = this->_dataManager->GetBoundingBoxMax(label);

    if (this->_selectionType == Selection::EMPTY)
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

	this->_selectionType = Selection::VOLUME;
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

	switch(this->_selectionType)
	{
		case Selection::EMPTY:
		case Selection::DISC:
			objectMin = _dataManager->GetBoundingBoxMin(label);
			objectMax = _dataManager->GetBoundingBoxMax(label);
			break;
		default: // VOLUME, CUBE, CONTOUR
			objectMin = this->_min;
			objectMax = this->_max;
			break;
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
	{
		if (true == VoxelIsInsideSelectionSubvolume(*it, x,y,z))
			return true;
	}

	return false;
}

bool Selection::VoxelIsInsideSelectionSubvolume(vtkSmartPointer<vtkImageData> subvolume, unsigned int x, unsigned int y, unsigned int z)
{
	int extent[6];
	subvolume->GetExtent(extent);

	switch(this->_selectionType)
	{
		case Selection::CONTOUR:
		case Selection::DISC:
		{
			double origin[3];
			subvolume->GetOrigin(origin);
			int point[3] = { static_cast<int>(x) - static_cast<int>(origin[0]/this->_spacing[0]),
							 static_cast<int>(y) - static_cast<int>(origin[1]/this->_spacing[1]),
							 static_cast<int>(z) - static_cast<int>(origin[2]/this->_spacing[2]) };

			if ((extent[0] <= point[0]) && (extent[1] >= point[0]) && (extent[2] <= point[1]) && (extent[3] >= point[1]) && (extent[4] <= point[2]) && (extent[5] >= point[2]))
			{
				unsigned char *voxel = static_cast<unsigned char*>(subvolume->GetScalarPointer(point[0], point[1], point[2]));
				return (VOXEL_SELECTED == *voxel);
			}
			break;
		}
		default:
			if ((extent[0] <= static_cast<int>(x)) && (extent[1] >= static_cast<int>(x)) && (extent[2] <= static_cast<int>(y)) &&
				(extent[3] >= static_cast<int>(y)) && (extent[4] <= static_cast<int>(z)) && (extent[5] >= static_cast<int>(z)))
			{
				unsigned char *voxel = static_cast<unsigned char*>(subvolume->GetScalarPointer(x,y,z));
				return (VOXEL_SELECTED == *voxel);
			}
			break;
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
    textureTrans->SetScale(this->_size[0], this->_size[1], this->_size[2]);

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

void Selection::SetSelectionDisc(int x, int y, int z, int radius, SliceVisualization* sliceView)
{
	// static vars are used when the selection changes radius or orientation;
	static int selectionRadius = 0;
	static SliceVisualization::OrientationType selectionOrientation = SliceVisualization::NoOrientation;

	// create volume if it doesn't exists
	if (this->_selectionVolumesList.empty())
	{
		vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
		image->SetSpacing(_spacing[0], _spacing[1], _spacing[2]);
		image->SetScalarTypeToInt();
		image->SetOrigin(0.0,0.0,0.0);

		int extent[6] = { 0,0,0,0,0,0 };

		switch(sliceView->GetOrientationType())
		{
			case SliceVisualization::Axial:
				extent[1] =  extent[3] = (radius * 2) - 2;
				break;
			case SliceVisualization::Coronal:
				extent[1] =  extent[5] = (radius * 2) - 2;
				break;
			case SliceVisualization::Sagittal:
				extent[3] =  extent[5] = (radius * 2) - 2;
				break;
			default:
				break;
		}
		image->SetExtent(extent);
		image->AllocateScalars();

		// after creating the image, fill it according to radius value
		int *pointer;

		switch(sliceView->GetOrientationType())
		{
			case SliceVisualization::Axial:
				for (int a = 0; a <= extent[1]; a++)
					for (int b = 0; b <= extent[3]; b++)
					{
						pointer = static_cast<int*>(image->GetScalarPointer(a,b,0));
						if (((pow(abs(radius-1 - a), 2) + pow(abs(radius-1 - b), 2))) <= pow(radius-1,2))
							*pointer = static_cast<int>(Selection::VOXEL_SELECTED);
						else
							*pointer = static_cast<int>(Selection::VOXEL_UNSELECTED);
					}
				break;
			case SliceVisualization::Coronal:
				for (int a = 0; a <= extent[1]; a++)
					for (int b = 0; b <= extent[5]; b++)
					{
						pointer = static_cast<int*>(image->GetScalarPointer(a,0,b));
						if (((pow(abs(radius-1 - a), 2) + pow(abs(radius-1 - b), 2))) <= pow(radius-1,2))
							*pointer = static_cast<int>(Selection::VOXEL_SELECTED);
						else
							*pointer = static_cast<int>(Selection::VOXEL_UNSELECTED);
					}
				break;
			case SliceVisualization::Sagittal:
				for (int a = 0; a <= extent[3]; a++)
					for (int b = 0; b <= extent[5]; b++)
					{
						pointer = static_cast<int*>(image->GetScalarPointer(0,a,b));
						if (((pow(abs(radius-1 - a), 2) + pow(abs(radius-1 - b), 2))) <= pow(radius-1,2))
							*pointer = static_cast<int>(Selection::VOXEL_SELECTED);
						else
							*pointer = static_cast<int>(Selection::VOXEL_UNSELECTED);
					}
				break;
			default:
				break;
		}
		image->Update();

		// create clipper and changer to set the pipeline, but update them later
		int clipperExtent[6] = { 0,0,0,0,0,0 };
		this->_clipper = vtkSmartPointer<vtkImageClip>::New();
		this->_clipper->SetInput(image);
		this->_clipper->ClipDataOn();
		this->_clipper->SetOutputWholeExtent(clipperExtent);

		this->_changer = vtkSmartPointer<vtkImageChangeInformation>::New();
		this->_changer->SetInput(this->_clipper->GetOutput());

		vtkSmartPointer<vtkImageData> translatedVolume = this->_changer->GetOutput();
		this->_selectionVolumesList.push_back(translatedVolume);
		this->_axialView->SetSelectionVolume(translatedVolume, false);
		this->_coronalView->SetSelectionVolume(translatedVolume, false);
		this->_sagittalView->SetSelectionVolume(translatedVolume, false);
		selectionRadius = radius;
		selectionOrientation = sliceView->GetOrientationType();
		this->_selectionType = Selection::DISC;
	}
	else
		if ((selectionOrientation != sliceView->GetOrientationType()) || (selectionRadius != radius))
		{
			// recompute volume when the user changes parameters or goes from one view to another
			this->_axialView->ClearSelections();
			this->_coronalView->ClearSelections();
			this->_sagittalView->ClearSelections();
			this->_selectionVolumesList.pop_back();
			this->SetSelectionDisc(x,y,z,radius,sliceView);
		}

	// update the disc representation according to parameters
	int clipperExtent[6] = { 0, 0, 0, 0, 0, 0 };
	switch (sliceView->GetOrientationType())
	{
		case SliceVisualization::Axial:
			clipperExtent[0] = ((x - radius) < 0) ? abs(x - radius) : 0;
			clipperExtent[1] = ((x + radius) > this->_size[0]) ? (radius - x + this->_size[0]) : (radius * 2) - 2;
			clipperExtent[2] = ((y - radius) < 0) ? abs(y - radius) : 0;
			clipperExtent[3] = ((y + radius) > this->_size[1]) ? (radius - y + this->_size[1]) : (radius * 2) - 2;
			break;
		case SliceVisualization::Coronal:
			clipperExtent[0] = ((x - radius) < 0) ? abs(x - radius) : 0;
			clipperExtent[1] = ((x + radius) > this->_size[0]) ? (radius - x + this->_size[0]) : (radius * 2) - 2;
			clipperExtent[4] = ((z - radius) < 0) ? abs(z - radius) : 0;
			clipperExtent[5] = ((z + radius) > this->_size[2]) ? (radius - z + this->_size[2]) : (radius * 2) - 2;
			break;
		case SliceVisualization::Sagittal:
			clipperExtent[2] = ((y - radius) < 0) ? abs(y - radius) : 0;
			clipperExtent[3] = ((y + radius) > this->_size[1]) ? (radius - y + this->_size[1]) : (radius * 2) - 2;
			clipperExtent[4] = ((z - radius) < 0) ? abs(z - radius) : 0;
			clipperExtent[5] = ((z + radius) > this->_size[2]) ? (radius - z + this->_size[2]) : (radius * 2) - 2;
			break;
		default:
			break;
	}
	this->_clipper->SetOutputWholeExtent(clipperExtent);
	this->_clipper->Update();

	switch (sliceView->GetOrientationType())
	{
		case SliceVisualization::Axial:
			this->_changer->SetOutputOrigin((x - radius) * _spacing[0], (y - radius) * _spacing[1], z * _spacing[2]);
			this->_min = Vector3ui(((x - radius) > 0 ? (x - radius) : 0), ((y - radius) > 0 ? (y - radius) : 0), z);
			this->_max = Vector3ui(((x + radius) < this->_size[0] ? (x + radius) : this->_size[0]),
					((y + radius) < this->_size[1] ? (y + radius) : this->_size[1]), static_cast<unsigned int>(z));
			break;
		case SliceVisualization::Coronal:
			this->_changer->SetOutputOrigin((x - radius) * _spacing[0], y * _spacing[1], (z - radius) * _spacing[2]);
			this->_min = Vector3ui(((x - radius) > 0 ? (x - radius) : 0), y, ((z - radius) > 0 ? (z - radius) : 0));
			this->_max = Vector3ui(((x + radius) < this->_size[0] ? (x + radius) : this->_size[0]), static_cast<unsigned int>(y),
					((z + radius) < this->_size[2] ? (z + radius) : this->_size[2]));
			break;
		case SliceVisualization::Sagittal:
			this->_changer->SetOutputOrigin(x * _spacing[0], (y - radius) * _spacing[1], (z - radius) * _spacing[2]);
			this->_min = Vector3ui(x, ((y - radius) > 0 ? (y - radius) : 0), ((z - radius) > 0 ? (z - radius) : 0));
			this->_max = Vector3ui(static_cast<unsigned int>(x), ((y + radius) < this->_size[1] ? (y + radius) : this->_size[1]),
					((z + radius) < this->_size[2] ? (z + radius) : this->_size[2]));
			break;
		default:
			break;
	}
	this->_changer->Update();
}

void Selection::BoxSelectionWidgetCallback (vtkObject *caller, unsigned long event, void *clientdata, void *calldata)
{
	Selection *self = static_cast<Selection *> (clientdata);
	Vector3ui min = self->GetSelectedMinimumBouds();
	Vector3ui max = self->GetSelectedMaximumBouds();

	BoxSelectionWidget *callerWidget = static_cast<BoxSelectionWidget*>(caller);
	BoxSelectionRepresentation2D *callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(callerWidget->GetRepresentation());
	double *spacing = callerRepresentation->GetSpacing();
	double *pos1 = callerRepresentation->GetPosition();
	double *pos2 = callerRepresentation->GetPosition2();

	unsigned int pos1i[2] = { floor(pos1[0]/spacing[0]) + 1, floor(pos1[1]/spacing[1]) + 1 };
	unsigned int pos2i[2] = { floor(pos2[0]/spacing[0]), floor(pos2[1]/spacing[1]) };

	if (self->_axialBoxWidget == callerWidget)
	{
		// axial coordinates refer to first and second 3D coordinates
		if ((pos1i[0] != min[0]) || (pos1i[1] != min[1]) || (pos2i[0] != max[0]) || (pos2i[1] != max[1]))
		{
			callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->_coronalBoxWidget->GetRepresentation());
			callerRepresentation->SetBoxCoordinates(pos1i[0], min[2], pos2i[0], max[2]);

			callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->_sagittalBoxWidget->GetRepresentation());
			callerRepresentation->SetBoxCoordinates(pos1i[1], min[2], pos2i[1], max[2]);

			self->_min = Vector3ui(pos1i[0], pos1i[1], min[2]);
			self->_max = Vector3ui(pos2i[0], pos2i[1], max[2]);
			self->_selectedPoints.clear();
			self->_selectedPoints.push_back(Vector3ui(pos1i[0], pos1i[1], min[2]));
			self->_selectedPoints.push_back(Vector3ui(pos2i[0], pos2i[1], max[2]));
		}
	}

	if (self->_coronalBoxWidget == callerWidget)
	{
		// coronal coordinates refer to first and third 3D coordinates
		if ((pos1i[0] != min[0]) || (pos1i[1] != min[2]) || (pos2i[0] != max[0]) || (pos2i[1] != max[2]))
		{
			callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->_axialBoxWidget->GetRepresentation());
			callerRepresentation->SetBoxCoordinates(pos1i[0], min[1], pos2i[0], max[1]);

			callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->_sagittalBoxWidget->GetRepresentation());
			callerRepresentation->SetBoxCoordinates(min[1], pos1i[1], max[1], pos2i[1]);

			self->_min = Vector3ui(pos1i[0], min[1], pos1i[1]);
			self->_max = Vector3ui(pos2i[0], max[1], pos2i[1]);
			self->_selectedPoints.clear();
			self->_selectedPoints.push_back(Vector3ui(pos1i[0], min[1], pos1i[1]));
			self->_selectedPoints.push_back(Vector3ui(pos2i[0], max[1], pos2i[1]));
		}
	}

	if (self->_sagittalBoxWidget == callerWidget)
	{
		// sagittal coordinates refer to second and third 3D coordinates
		if ((pos1i[0] != min[1]) || (pos1i[1] != min[2]) || (pos2i[0] != max[1]) || (pos2i[1] != max[2]))
		{
			callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->_axialBoxWidget->GetRepresentation());
			callerRepresentation->SetBoxCoordinates(min[0], pos1i[0], max[0], pos2i[0]);

			callerRepresentation = static_cast<BoxSelectionRepresentation2D*>(self->_coronalBoxWidget->GetRepresentation());
			callerRepresentation->SetBoxCoordinates(min[0], pos1i[1], max[0], pos2i[1]);

			self->_min = Vector3ui(min[0], pos1i[0], pos1i[1]);
			self->_max = Vector3ui(max[0], pos2i[0], pos2i[1]);
			self->_selectedPoints.clear();
			self->_selectedPoints.push_back(Vector3ui(min[0], pos1i[0], pos1i[1]));
			self->_selectedPoints.push_back(Vector3ui(max[0], pos2i[0], pos2i[1]));
		}
	}

	self->ComputeSelectionCube();

	double bounds[6] = {
			(static_cast<double>(self->_min[0])-0.5)*self->_spacing[0], (static_cast<double>(self->_max[0])+0.5)*self->_spacing[0], (static_cast<double>(self->_min[1])-0.5)*self->_spacing[1],
			(static_cast<double>(self->_max[1])+0.5)*self->_spacing[1], (static_cast<double>(self->_min[2])-0.5)*self->_spacing[2], (static_cast<double>(self->_max[2])+0.5)*self->_spacing[2]  };
	self->_boxRender->PlaceBox(bounds);

	// update renderers, but only update the 3D render when the widget interaction has finished to avoid performance problems
	self->_axialBoxWidget->GetInteractor()->GetRenderWindow()->Render();
	self->_coronalBoxWidget->GetInteractor()->GetRenderWindow()->Render();
	self->_sagittalBoxWidget->GetInteractor()->GetRenderWindow()->Render();

	if (event == vtkCommand::EndInteractionEvent)
		self->_renderer->GetRenderWindow()->Render();
}

void Selection::ComputeLassoBounds(unsigned int *iBounds)
{
	ContourRepresentation* representation = static_cast<ContourRepresentation*>(this->_contourWidget->GetRepresentation());
	double *bounds = representation->GetBounds();

	// this copy is needed because we want to change the values and we mustn't do it in the original pointer or we will mess
	// with the representation
	double dBounds[6] = { bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5] };

	switch(this->_contourWidget->GetOrientation())
	{
		case 0: // axial
			// first check if the contour is inside the slice
			if ((dBounds[1] < 0) || (dBounds[3] < 0) ||	(dBounds[0] > (this->_size[0]*this->_spacing[0])) || (dBounds[2] > (this->_size[1]*this->_spacing[1])))
				this->_selectionIsValid = false;
			else
				this->_selectionIsValid = true;

			dBounds[4] = dBounds[5] = this->_min[2]*this->_spacing[2];
			break;
		case 1: // coronal
			if ((dBounds[1] < 0) || (dBounds[3] < 0) || (dBounds[0] > (this->_size[0]*this->_spacing[0])) || (dBounds[2] > (this->_size[2]*this->_spacing[2])))
				this->_selectionIsValid = false;
			else
				this->_selectionIsValid = true;

			dBounds[4] = dBounds[2];
			dBounds[5] = dBounds[3];
			dBounds[2] = dBounds[3] = this->_min[1]*this->_spacing[1];
			break;
		case 2: // sagittal
			if ((dBounds[1] < 0) || (dBounds[3] < 0) ||	(dBounds[0] > (this->_size[1]*this->_spacing[1])) || (dBounds[2] > (this->_size[2]*this->_spacing[2])))
				this->_selectionIsValid = false;
			else
				this->_selectionIsValid = true;

			dBounds[4] = dBounds[2];
			dBounds[5] = dBounds[3];
			dBounds[2] = dBounds[0];
			dBounds[3] = dBounds[1];
			dBounds[0] = dBounds[1] = this->_min[0]*this->_spacing[0];
			break;
	}

	for(unsigned int i = 0; i < 3; i++)
	{
		unsigned int j = 2*i;
		unsigned int k = j+1;

		if (dBounds[j] < 0)
			iBounds[j] = 0;
		else
			if (dBounds[j] > this->_size[i]*this->_spacing[i])
				iBounds[j] = this->_size[i];
			else
			{
				iBounds[j] = floor(dBounds[j]/this->_spacing[i]);

				if (fmod(dBounds[j],this->_spacing[i]) > (0.5*this->_spacing[i]))
					iBounds[j]++;
			}

		if (dBounds[k] < 0)
			iBounds[k] = 0;
		else
			if (dBounds[k] > this->_size[i]*this->_spacing[i])
				iBounds[k] = this->_size[i];
			else
			{
				iBounds[k] = floor(dBounds[k]/this->_spacing[i]);

				if (fmod(dBounds[k],this->_spacing[i]) > (0.5*this->_spacing[i]))
					iBounds[k]++;
			}
	}
}

void Selection::AddContourInitialPoint(const Vector3ui point, SliceVisualization *callerSlice)
{
	// if there is an existing contour just return
	if (this->_contourWidget != NULL)
		return;

	this->_selectionType = CONTOUR;
	this->_min = point;
	this->_max = point;
	double worldPos[3] = { 0.0, 0.0, 0.0 };

	// interpolate points with lines
	vtkSmartPointer<vtkLinearContourLineInterpolator> interpolator = vtkSmartPointer<vtkLinearContourLineInterpolator>::New();
	FocalPlanePointPlacer *pointPlacer = FocalPlanePointPlacer::New();
	ContourRepresentationGlyph *representation = ContourRepresentationGlyph::New();

	// create widget and representation
	this->_contourWidget = vtkSmartPointer<ContourWidget>::New();
	this->_contourWidget->SetInteractor(callerSlice->GetViewRenderer()->GetRenderWindow()->GetInteractor());
	this->_contourWidget->SetContinuousDraw(true);
	this->_contourWidget->SetFollowCursor(true);

	switch(callerSlice->GetOrientationType())
	{
		case SliceVisualization::Axial:
			this->_contourWidget->SetOrientation(0);
			pointPlacer->SetSpacing(this->_spacing[0], this->_spacing[1]);
			representation->SetSpacing(this->_spacing[0], this->_spacing[1]);
			worldPos[0] = point[0] * this->_spacing[0];
			worldPos[1] = point[1] * this->_spacing[1];
			break;
		case SliceVisualization::Coronal:
			this->_contourWidget->SetOrientation(1);
			pointPlacer->SetSpacing(this->_spacing[0], this->_spacing[2]);
			representation->SetSpacing(this->_spacing[0], this->_spacing[2]);
			worldPos[0] = point[0] * this->_spacing[0];
			worldPos[1] = point[2] * this->_spacing[2];
			break;
		case SliceVisualization::Sagittal:
			this->_contourWidget->SetOrientation(2);
			pointPlacer->SetSpacing(this->_spacing[1], this->_spacing[2]);
			representation->SetSpacing(this->_spacing[1], this->_spacing[2]);
			worldPos[0] = point[1] * this->_spacing[1];
			worldPos[1] = point[2] * this->_spacing[2];
			break;
		default:
			break;
	}

	pointPlacer->UpdateInternalState();

	representation->SetPointPlacer(pointPlacer);
	representation->SetLineInterpolator(interpolator);

	this->_contourWidget->SetRepresentation(representation);
	this->_contourWidget->SetEnabled(true);
	this->_contourWidget->On();

	representation->AddNodeAtWorldPosition(worldPos);
	representation->AddNodeAtWorldPosition(worldPos);
	representation->SetVisibility(true);
	representation->Modified();

	// set callbacks for widget interaction
	this->_widgetsCallbackCommand = vtkSmartPointer<vtkCallbackCommand>::New();
	this->_widgetsCallbackCommand->SetCallback(this->ContourSelectionWidgetCallback);
	this->_widgetsCallbackCommand->SetClientData(this);

	this->_contourWidget->AddObserver(vtkCommand::StartInteractionEvent, this->_widgetsCallbackCommand);
	this->_contourWidget->AddObserver(vtkCommand::EndInteractionEvent, this->_widgetsCallbackCommand);
	this->_contourWidget->AddObserver(vtkCommand::InteractionEvent, this->_widgetsCallbackCommand);
	this->_contourWidget->AddObserver(vtkCommand::KeyPressEvent, this->_widgetsCallbackCommand);

	// make the slice aware of a contour selection
	callerSlice->SetSliceWidget(this->_contourWidget);

	// create the volume that will represent user selected points. the spacing is 1 because we will not
	// use it's output directly, we will create our own volume instead with ComputeContourSelectionVolume()
	this->_polyDataToStencil = vtkSmartPointer<vtkPolyDataToImageStencil>::New();
	this->_polyDataToStencil->SetInput(representation->GetContourRepresentationAsPolyData());
	this->_polyDataToStencil->SetOutputOrigin(0,0,0);

	// change stencil properties according to slice orientation
	switch(callerSlice->GetOrientationType())
	{
		case SliceVisualization::Axial:
			this->_polyDataToStencil->SetOutputSpacing(this->_spacing[0], this->_spacing[1], this->_spacing[2]);
			this->_polyDataToStencil->SetOutputWholeExtent(0, this->_size[0], 0, this->_size[1], 0, 0);
			break;
		case SliceVisualization::Coronal:
			this->_polyDataToStencil->SetOutputSpacing(this->_spacing[0], this->_spacing[2], this->_spacing[1]);
			this->_polyDataToStencil->SetOutputWholeExtent(0, this->_size[0], 0, this->_size[2], 0, 0);
			break;
		case SliceVisualization::Sagittal:
			this->_polyDataToStencil->SetOutputSpacing(this->_spacing[1], this->_spacing[2], this->_spacing[0]);
			this->_polyDataToStencil->SetOutputWholeExtent(0, this->_size[1], 0, this->_size[2], 0, 0);
			break;
		default:
			break;
	}
	this->_polyDataToStencil->SetTolerance(0);

	this->_stencilToImage = vtkSmartPointer<vtkImageStencilToImage>::New();
	this->_stencilToImage->SetInputConnection(this->_polyDataToStencil->GetOutputPort());
	this->_stencilToImage->SetOutputScalarTypeToInt();
	this->_stencilToImage->SetInsideValue(VOXEL_SELECTED);
	this->_stencilToImage->SetOutsideValue(VOXEL_UNSELECTED);

	// bootstrap operations
	this->_contourWidget->SelectAction(this->_contourWidget);
}

void Selection::ContourSelectionWidgetCallback (vtkObject *caller, unsigned long event, void *clientdata, void *calldata)
{
	Selection *self = static_cast<Selection *> (clientdata);
	ContourWidget *widget = static_cast<ContourWidget*>(caller);
	ContourRepresentation *representation = static_cast<ContourRepresentation *>(widget->GetRepresentation());

	// if bounds are not defined means that the representation is not valid
	double *dBounds = representation->GetBounds();
	if (dBounds == NULL)
		return;

	// calculate correct bounds for min/max vectors and for correctly cutting the selection volume
	unsigned int iBounds[6];
	self->ComputeLassoBounds(iBounds);

	self->_min = Vector3ui(iBounds[0], iBounds[2], iBounds[4]);
	self->_max = Vector3ui(iBounds[1], iBounds[3], iBounds[5]);

	if (vtkCommand::EndInteractionEvent == event)
		representation->PlaceFinalPoints();

	// update the representation volume (update the pipeline)
	self->_polyDataToStencil->SetInput(representation->GetContourRepresentationAsPolyData());
	self->_polyDataToStencil->Modified();

	// adquire new rotated and clipped image
	self->ComputeContourSelectionVolume(iBounds);

	if (self->_rotatedImage != NULL)
	{
	    self->DeleteSelectionActors();
	    self->DeleteSelectionVolumes();
	    self->_axialView->ClearSelections();
	    self->_coronalView->ClearSelections();
	    self->_sagittalView->ClearSelections();

	    self->_selectionVolumesList.push_back(self->_rotatedImage);
	    self->_axialView->SetSelectionVolume(self->_rotatedImage, true);
	    self->_coronalView->SetSelectionVolume(self->_rotatedImage, true);
	    self->_sagittalView->SetSelectionVolume(self->_rotatedImage, true);
	}
	else
		return;

	// update renderers
	self->_axialView->GetViewRenderer()->GetRenderWindow()->Render();
	self->_coronalView->GetViewRenderer()->GetRenderWindow()->Render();
	self->_sagittalView->GetViewRenderer()->GetRenderWindow()->Render();
}

void Selection::ComputeContourSelectionVolume(const unsigned int *bounds)
{
	vtkImageData *image = this->_stencilToImage->GetOutput();
	image->Update();

	// need to create a volume with the extent changed (1 voxel thicker on every axis) for the slices
	// to correctly hide/show the volume and the widgets
	int extent[6];
	image->GetExtent(extent);

	// do not operate on empty/uninitialized images
	if ((extent[1] < extent[0]) || (extent[3] < extent[2]) || extent[5] < extent[4])
		return;

	if (this->_rotatedImage != NULL)
	{
		this->_rotatedImage->Delete();
		this->_rotatedImage = NULL;
	}

	// create the volume and properties according to the slice orientation and use contour representation bounds to make it smaller.
	this->_rotatedImage = vtkImageData::New();
	this->_rotatedImage->SetScalarTypeToInt();
	this->_rotatedImage->SetSpacing(this->_spacing[0], this->_spacing[1], this->_spacing[2]);
	this->_rotatedImage->SetOrigin((static_cast<int>(bounds[0])-1)*this->_spacing[0], (static_cast<int>(bounds[2])-1)*this->_spacing[1], (static_cast<int>(bounds[4])-1)*this->_spacing[2]);
	this->_rotatedImage->SetDimensions(bounds[1]-bounds[0]+3, bounds[3]-bounds[2]+3, bounds[5]-bounds[4]+3);
	this->_rotatedImage->AllocateScalars();
	memset(this->_rotatedImage->GetScalarPointer(), 0, this->_rotatedImage->GetScalarSize()*(bounds[1]-bounds[0]+3)*(bounds[3]-bounds[2]+3)*(bounds[5]-bounds[4]+3));

	if (this->_selectionIsValid)
		switch(this->_contourWidget->GetOrientation())
		{
			case 0: // axial
				for (unsigned int i = bounds[0]; i <= bounds[1]; i++)
					for (unsigned int j = bounds[2]; j <= bounds[3]; j++)
					{
						int *pixel = static_cast<int*>(image->GetScalarPointer(i,j,0));
						int *rotatedPixel = static_cast<int*>(this->_rotatedImage->GetScalarPointer(i-bounds[0]+1,j-bounds[2]+1,1));
						*rotatedPixel = *pixel;
					}
				break;
			case 1: // coronal
				for (unsigned int i = bounds[0]; i <= bounds[1]; i++)
					for (unsigned int j = bounds[4]; j <= bounds[5]; j++)
					{
						int *pixel = static_cast<int*>(image->GetScalarPointer(i,j,0));
						int* rotatedPixel = static_cast<int*>(this->_rotatedImage->GetScalarPointer(i-bounds[0]+1,1,j-bounds[4]+1));
						*rotatedPixel = *pixel;
					}
				break;
			case 2: // sagittal
				for (unsigned int i = bounds[2]; i <= bounds[3]; i++)
					for (unsigned int j = bounds[4]; j <= bounds[5]; j++)
					{
						int *pixel = static_cast<int*>(image->GetScalarPointer(i,j,0));
						int* rotatedPixel = static_cast<int*>(this->_rotatedImage->GetScalarPointer(1,i-bounds[2]+1,j-bounds[4]+1));
						*rotatedPixel = *pixel;
					}
				break;
			default: // can't happen
				break;
		}
}

void Selection::UpdateContourSlice(Vector3ui point)
{
	if (this->_selectionType != CONTOUR)
		return;

	double origin[3];
	this->_rotatedImage->GetOrigin(origin);

	switch(this->_contourWidget->GetOrientation())
	{
		case 0: // axial
			if (point[2] == this->_min[2])
				return;

			this->_min[2] = this->_max[2] = point[2];
			origin[2] = (static_cast<int>(point[2])-1)*this->_spacing[2];
			break;
		case 1: // coronal
			if (point[1] == this->_min[1])
				return;

			this->_min[1] = this->_max[1] = point[1];
			origin[1] = (static_cast<int>(point[1])-1)*this->_spacing[1];
			break;
		case 2: // sagittal
			if (point[0] == this->_min[0])
				return;

			this->_min[0] = this->_max[0] = point[0];
			origin[0] = (static_cast<int>(point[0])-1)*this->_spacing[0];
			break;
		default:
			break;
	}

	this->_changer = vtkSmartPointer<vtkImageChangeInformation>::New();
	this->_changer->SetInput(this->_rotatedImage);
	this->_changer->SetOutputOrigin(origin);
	this->_changer->Update();

    this->DeleteSelectionActors();
    this->DeleteSelectionVolumes();
    this->_axialView->ClearSelections();
    this->_coronalView->ClearSelections();
    this->_sagittalView->ClearSelections();

    this->_selectionVolumesList.push_back(this->_changer->GetOutput());
    this->_axialView->SetSelectionVolume(this->_changer->GetOutput(), true);
    this->_coronalView->SetSelectionVolume(this->_changer->GetOutput(), true);
    this->_sagittalView->SetSelectionVolume(this->_changer->GetOutput(), true);
}
