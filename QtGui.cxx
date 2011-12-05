///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtGui.h
// Purpose: Volume Editor Qt GUI class
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes 
#include <QtGui>        // including <QtGui> saves us to include every class user, <QString>, <QFileDialog>,... 
#include "QtGui.h"

// itk includes
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkLabelMap.h>
#include <itkLabelObject.h>
#include <itkLabelImageToLabelMapFilter.h>
#include <itkLabelMapToLabelImageFilter.h>
#include <itkSmartPointer.h>
#include <itkVTKImageExport.h>
#include <itkMetaImageIO.h>
#include <itkCastImageFilter.h>
#include <itkChangeInformationImageFilter.h>
#include <itkLabelGeometryImageFilter.h>
        
// vtk includes 
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkInteractorStyleImage.h>
#include <vtkCoordinate.h>
#include <vtkCamera.h>
#include <vtkMetaImageReader.h>
#include <vtkImageToStructuredPoints.h>
#include <vtkImageFlip.h>
#include <vtkImageCast.h>
#include <vtkImageToStructuredPoints.h>
#include <vtkImageImport.h>
#include <vtkInformation.h>
#include <vtkImageChangeInformation.h>

// project includes 
#include "DataManager.h"
#include "VoxelVolumeRender.h"
#include "Coordinates.h"
#include "VectorSpaceAlgebra.h"
#include "EditorOperations.h"
#include "QtAbout.h"
#include "QtPreferences.h"
#include "itkvtkpipeline.h"

EspinaVolumeEditor::EspinaVolumeEditor(QApplication *app, QWidget *p) : QMainWindow(p)
{
	setupUi(this); // this sets up GUI
	
	showMaximized();
	
	// sanitize pointers in case we exit the program without loading anything.
	this->_orientationData = NULL;
	this->_sagittalSliceVisualization = NULL;
	this->_coronalSliceVisualization = NULL;
	this->_axialSliceVisualization = NULL;
	this->_axesRender = NULL;
	this->_volumeRender = NULL;
	this->_editorOperations = NULL;
	this->_dataManager = NULL;
	
	// connect menu commands with signals (menu definitions are in the ui file)
	connect(a_fileOpen, SIGNAL(triggered()), this, SLOT(EditorOpen()));
	connect(a_fileReferenceOpen, SIGNAL(triggered()), this, SLOT(EditorReferenceOpen()));
    connect(a_fileSave, SIGNAL(triggered()), this, SLOT(EditorSave()));
    connect(a_fileExit, SIGNAL(triggered()), this, SLOT(EditorExit()));

    connect(a_undo, SIGNAL(triggered()), this, SLOT(OperationUndo()));
    connect(a_redo, SIGNAL(triggered()), this, SLOT(OperationRedo()));
    
    connect(a_fulltoggle, SIGNAL(triggered()), this, SLOT(FullscreenToggle()));
    connect(a_preferences, SIGNAL(triggered()), this, SLOT(Preferences()));

    connect(a_about, SIGNAL(triggered()), this, SLOT(About()));
    
    // configure widgets
    connect(axialslider, SIGNAL(valueChanged(int)), this, SLOT(MoveAxialSlider(int)));
    connect(axialslider, SIGNAL(sliderReleased()), this, SLOT(SliceSliderReleased()));
    connect(axialslider, SIGNAL(sliderPressed()), this, SLOT(SliceSliderPressed()));

    connect(coronalslider, SIGNAL(valueChanged(int)), this, SLOT(MoveCoronalSlider(int)));
    connect(coronalslider, SIGNAL(sliderReleased()), this, SLOT(SliceSliderReleased()));
    connect(coronalslider, SIGNAL(sliderPressed()), this, SLOT(SliceSliderPressed()));

    connect(sagittalslider, SIGNAL(valueChanged(int)), this, SLOT(MoveSagittalSlider(int)));
    connect(sagittalslider, SIGNAL(sliderReleased()), this, SLOT(SliceSliderReleased()));
    connect(sagittalslider, SIGNAL(sliderPressed()), this, SLOT(SliceSliderPressed()));
    
    connect(labelselector, SIGNAL(currentRowChanged(int)), this, SLOT(LabelSelectionChanged(int)));
    
    connect(XspinBox, SIGNAL(valueChanged(int)), this, SLOT(ChangeXspinBox(int)));
    connect(YspinBox, SIGNAL(valueChanged(int)), this, SLOT(ChangeYspinBox(int)));
    connect(ZspinBox, SIGNAL(valueChanged(int)), this, SLOT(ChangeZspinBox(int)));
    
    connect(erodeoperation, SIGNAL(clicked(bool)), this, SLOT(ErodeVolume()));
    connect(dilateoperation, SIGNAL(clicked(bool)), this, SLOT(DilateVolume()));
    connect(openoperation, SIGNAL(clicked(bool)), this, SLOT(OpenVolume()));
    connect(closeoperation, SIGNAL(clicked(bool)), this, SLOT(CloseVolume()));
    connect(watershedoperation, SIGNAL(clicked(bool)), this, SLOT(WatershedVolume()));
    
    connect(rendertypebutton, SIGNAL(clicked(bool)), this, SLOT(SwitchVoxelRender()));
    connect(axestypebutton, SIGNAL(clicked(bool)), this, SLOT(SwitchAxesView()));
    
    connect(viewbutton, SIGNAL(toggled(bool)), this, SLOT(EditorSelectionEnd(bool)));
    connect(paintbutton, SIGNAL(toggled(bool)), this, SLOT(EditorSelectionEnd(bool)));
    connect(cutbutton, SIGNAL(clicked(bool)), this, SLOT(EditorCut()));
    connect(relabelbutton, SIGNAL(clicked(bool)), this, SLOT(EditorRelabel()));
    connect(pickerbutton, SIGNAL(clicked(bool)), this, SLOT(EditorSelectionEnd(bool)));

    connect(axialresetbutton, SIGNAL(clicked(bool)), this, SLOT(ViewReset()));
    connect(coronalresetbutton, SIGNAL(clicked(bool)), this, SLOT(ViewReset()));
    connect(sagittalresetbutton, SIGNAL(clicked(bool)), this, SLOT(ViewReset()));
    connect(voxelresetbutton, SIGNAL(clicked(bool)), this, SLOT(ViewReset()));

    connect(axialsizebutton, SIGNAL(clicked(bool)), this, SLOT(ViewZoom()));
    connect(sagittalsizebutton, SIGNAL(clicked(bool)), this, SLOT(ViewZoom()));
    connect(coronalsizebutton, SIGNAL(clicked(bool)), this, SLOT(ViewZoom()));
    connect(rendersizebutton, SIGNAL(clicked(bool)), this, SLOT(ViewZoom()));
    
    connect(renderdisablebutton, SIGNAL(clicked(bool)), this, SLOT(DisableRenderView()));

    XspinBox->setReadOnly(false);
    XspinBox->setWrapping(false);
    XspinBox->setAccelerated(true);
    YspinBox->setReadOnly(false);
    YspinBox->setWrapping(false);
    YspinBox->setAccelerated(true);
    ZspinBox->setReadOnly(false);
    ZspinBox->setWrapping(false);
    ZspinBox->setAccelerated(true);
    
    // disable unused widgets
    progressLabel->hide();
    progressBar->hide();
    
    // boolean values
    this->updatevoxelrenderer = false;
    this->updateslicerenderers = false;
    this->updatepointlabel = false;
    
     // initialize renderers
    vtkSmartPointer<vtkInteractorStyleImage> axialinteractorstyle = vtkSmartPointer<vtkInteractorStyleImage>::New();
    axialinteractorstyle->AutoAdjustCameraClippingRangeOn();
    this->_axialViewRenderer = vtkSmartPointer<vtkRenderer>::New();
    this->_axialViewRenderer->SetBackground(0, 0, 0);
    axialview->GetRenderWindow()->AddRenderer(_axialViewRenderer);
    axialview->GetRenderWindow()->GetInteractor()->SetInteractorStyle(axialinteractorstyle);

    vtkSmartPointer<vtkInteractorStyleImage> coronalinteractorstyle = vtkSmartPointer<vtkInteractorStyleImage>::New();
    coronalinteractorstyle->AutoAdjustCameraClippingRangeOn();
    this->_coronalViewRenderer = vtkSmartPointer<vtkRenderer>::New();
    this->_coronalViewRenderer->SetBackground(0, 0, 0);
    coronalview->GetRenderWindow()->AddRenderer(_coronalViewRenderer);
    coronalview->GetRenderWindow()->GetInteractor()->SetInteractorStyle(coronalinteractorstyle);

    vtkSmartPointer<vtkInteractorStyleImage> sagittalinteractorstyle = vtkSmartPointer<vtkInteractorStyleImage>::New();
    sagittalinteractorstyle->AutoAdjustCameraClippingRangeOn();
    this->_sagittalViewRenderer = vtkSmartPointer<vtkRenderer>::New();
    this->_sagittalViewRenderer->SetBackground(0, 0, 0);
    sagittalview->GetRenderWindow()->AddRenderer(_sagittalViewRenderer);
    sagittalview->GetRenderWindow()->GetInteractor()->SetInteractorStyle(sagittalinteractorstyle);

    vtkSmartPointer<vtkInteractorStyleTrackballCamera> voxelinteractorstyle = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    voxelinteractorstyle->AutoAdjustCameraClippingRangeOn();
    this->_voxelViewRenderer = vtkSmartPointer<vtkRenderer>::New();
    this->_voxelViewRenderer->SetBackground(0, 0, 0);
    renderview->GetRenderWindow()->AddRenderer(_voxelViewRenderer);
    renderview->GetRenderWindow()->GetInteractor()->SetInteractorStyle(voxelinteractorstyle);
    
    _connections = vtkSmartPointer<vtkEventQtSlotConnect>::New();
    
    // we need to go deeper than window interactor to get mouse release events, so instead connecting the
    // interactor we must connect the interactor-style because once the interactor gets a left click it
    // delegates the rest to the style so we don't get the mouse release event.  
    _connections->Connect(axialview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(),
                          vtkCommand::LeftButtonPressEvent, this,
                          SLOT(AxialInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(axialview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::LeftButtonReleaseEvent, this, 
                          SLOT(AxialInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(axialview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(),
                          vtkCommand::RightButtonPressEvent, this,
                          SLOT(AxialInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(axialview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::RightButtonReleaseEvent, this, 
                          SLOT(AxialInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(axialview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(),
                          vtkCommand::MiddleButtonPressEvent, this,
                          SLOT(AxialInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(axialview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MiddleButtonReleaseEvent, this, 
                          SLOT(AxialInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(axialview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MouseMoveEvent, this, 
                          SLOT(AxialInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(axialview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MouseWheelForwardEvent, this, 
                          SLOT(AxialInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(axialview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MouseWheelBackwardEvent, this, 
                          SLOT(AxialInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(coronalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(),
                          vtkCommand::LeftButtonPressEvent, this,
                          SLOT(CoronalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(coronalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::LeftButtonReleaseEvent, this, 
                          SLOT(CoronalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(coronalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(),
                          vtkCommand::RightButtonPressEvent, this,
                          SLOT(CoronalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(coronalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::RightButtonReleaseEvent, this, 
                          SLOT(CoronalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(coronalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(),
                          vtkCommand::MiddleButtonPressEvent, this,
                          SLOT(CoronalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(coronalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MiddleButtonReleaseEvent, this, 
                          SLOT(CoronalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(coronalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MouseMoveEvent, this, 
                          SLOT(CoronalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(coronalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MouseWheelForwardEvent, this, 
                          SLOT(CoronalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(coronalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MouseWheelBackwardEvent, this, 
                          SLOT(CoronalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(sagittalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(),
                          vtkCommand::LeftButtonPressEvent, this,
                          SLOT(SagittalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(sagittalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::LeftButtonReleaseEvent, this, 
                          SLOT(SagittalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(sagittalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(),
                          vtkCommand::RightButtonPressEvent, this,
                          SLOT(SagittalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(sagittalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::RightButtonReleaseEvent, this, 
                          SLOT(SagittalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(sagittalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(),
                          vtkCommand::MiddleButtonPressEvent, this,
                          SLOT(SagittalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(sagittalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MiddleButtonReleaseEvent, this, 
                          SLOT(SagittalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(sagittalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MouseMoveEvent, this, 
                          SLOT(SagittalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    _connections->Connect(sagittalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MouseWheelForwardEvent, this, 
                          SLOT(SagittalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

    _connections->Connect(sagittalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle(), 
                          vtkCommand::MouseWheelBackwardEvent, this, 
                          SLOT(SagittalInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));
    
    // init some global variables
    this->_hasReferenceImage = false;
    this->_selectedLabel = 0;
    this->_pointScalar = 0;
    this->_dataManager = new DataManager();
    this->_editorOperations = new EditorOperations(_dataManager);
    this->_sagittalSliceVisualization = new SliceVisualization(SliceVisualization::Sagittal);
    this->_coronalSliceVisualization = new SliceVisualization(SliceVisualization::Coronal);
    this->_axialSliceVisualization = new SliceVisualization(SliceVisualization::Axial);

    
    // initialize editor progress bar
    this->_progress = new ProgressAccumulator(app);
    this->_progress->SetProgressBar(progressBar, progressLabel);
    this->_progress->Reset();
}

EspinaVolumeEditor::~EspinaVolumeEditor()
{
    // clean everything not using smartpointers
    if (this->_orientationData)
        delete _orientationData;
    
    if (this->_sagittalSliceVisualization)
        delete _sagittalSliceVisualization;
    
    if (this->_coronalSliceVisualization)
        delete _coronalSliceVisualization;
    
    if (this->_axialSliceVisualization)
        delete _axialSliceVisualization;
    
    if (this->_axesRender)
        delete _axesRender;

    if (this->_volumeRender)
        delete _volumeRender;
    
    if (this->_editorOperations)
        delete _editorOperations;
    
    if (this->_dataManager)
        delete _dataManager;
    
    if (this->_progress)
        delete _progress;
}

void EspinaVolumeEditor::EditorOpen(void)
{
	renderview->setEnabled(true);
	axialview->setEnabled(true);
	sagittalview->setEnabled(true);
	coronalview->setEnabled(true);

    QMessageBox msgBox;

    QString filename = QFileDialog::getOpenFileName(this, tr("Open Espina Segmentation Image"), QDir::currentPath(), QObject::tr("Espina segmentation files (*.segmha)"));

    if(!filename.isNull())
        filename.toAscii();
    else
        return;
    
    // MetaImageIO needed to read an image without a estandar extension (segmha in this case)
    typedef itk::Image<unsigned short, 3> ImageType;
    typedef itk::ImageFileReader<ImageType> ReaderType;
    itk::SmartPointer<itk::MetaImageIO> io = itk::MetaImageIO::New();
    io->SetFileName(filename.toStdString().c_str());
    itk::SmartPointer<ReaderType> reader = ReaderType::New();
    reader->SetImageIO(io);
    reader->SetFileName(filename.toStdString().c_str());
    reader->ReleaseDataFlagOn();

	try
	{
		reader->Update();
	} 
	catch (itk::ExceptionObject & excp)
	{
		_progress->ManualReset();
		msgBox.setIcon(QMessageBox::Critical);

		std::string text = std::string("An error occurred loading the segmentation file.\nThe operation has been aborted.");
		msgBox.setText(text.c_str());
		msgBox.setDetailedText(excp.what());
		msgBox.exec();
		return;
	}
	
	// file reading succesfull, parse additional espina metadata
	_fileMetadata = new Metadata;
	if (!_fileMetadata->Read(filename))
	{
		_progress->ManualReset();
		msgBox.setIcon(QMessageBox::Critical);

		std::string text = std::string("An error occurred parsing the espina segmentation data from file \"");
		text += filename.toStdString();
		text += std::string("\".\nThe operation has been aborted.");
		msgBox.setText(text.c_str());
		msgBox.exec();
		return;
	}

    // clean all viewports
    _voxelViewRenderer->RemoveAllViewProps();
    _axialViewRenderer->RemoveAllViewProps();
    _sagittalViewRenderer->RemoveAllViewProps();
    _coronalViewRenderer->RemoveAllViewProps();
    
    // do not update the viewports while loading
    this->updatevoxelrenderer = false;
    this->updateslicerenderers = false;
    this->updatepointlabel = false;
    
    // have to check first if we have a session, and preserve preferences (have to change this some day
    // to save preferences as global, not per class instance)
    if (this->_orientationData)
        delete _orientationData;

    if (this->_sagittalSliceVisualization)
    {
    	unsigned int opacity = _sagittalSliceVisualization->GetSegmentationOpacity();
        delete _sagittalSliceVisualization;
        _sagittalSliceVisualization = new SliceVisualization(SliceVisualization::Sagittal);
        _sagittalSliceVisualization->SetSegmentationOpacity(opacity);
    }

    if (this->_coronalSliceVisualization)
    {
        unsigned int opacity = _coronalSliceVisualization->GetSegmentationOpacity();
        delete _coronalSliceVisualization;
        _coronalSliceVisualization = new SliceVisualization(SliceVisualization::Coronal);
        _coronalSliceVisualization->SetSegmentationOpacity(opacity);
    }

    if (this->_axialSliceVisualization)
    {
        unsigned int opacity = _axialSliceVisualization->GetSegmentationOpacity();
        delete _axialSliceVisualization;
        _axialSliceVisualization = new SliceVisualization(SliceVisualization::Axial);
        _axialSliceVisualization->SetSegmentationOpacity(opacity);
    }

    if (this->_axesRender)
        delete _axesRender;

    if (this->_volumeRender)
        delete _volumeRender;
    
    if (this->_dataManager)
    {
		unsigned long int size = _dataManager->GetUndoRedoBufferSize(); 
        delete _dataManager;
        _dataManager = new DataManager();
        _dataManager->SetUndoRedoBufferSize(size);
    }
    
    if (this->_editorOperations)
    {
		unsigned int radius = _editorOperations->GetFiltersRadius(); 
		double level = _editorOperations->GetWatershedLevel();
        delete _editorOperations;
        _editorOperations = new EditorOperations(_dataManager);
        _editorOperations->SetFiltersRadius(radius);
        _editorOperations->SetWatershedLevel(level);
    }
    
    // here we go after file read:
    // itkimage(unsigned short,3) -> itklabelmap -> itkimage-> vtkimage -> vtkstructuredpoints
	_progress->ManualSet("Load");

    // get image orientation data
    _orientationData = new Coordinates(reader->GetOutput());

    typedef itk::ChangeInformationImageFilter<ImageType> ChangeInfoType;
    itk::SmartPointer<ChangeInfoType> infoChanger = ChangeInfoType::New();
    infoChanger->SetInput(reader->GetOutput());
    infoChanger->ReleaseDataFlagOn();
    infoChanger->ChangeOriginOn();
    infoChanger->ReleaseDataFlagOn();
    ImageType::PointType newOrigin;
    newOrigin[0] = 0.0;
    newOrigin[1] = 0.0;
    newOrigin[2] = 0.0;
    infoChanger->SetOutputOrigin(newOrigin);
    _progress->Observe(infoChanger,"Fix Image", 0.14);
    infoChanger->Update();
    _progress->Ignore(infoChanger);
	
    // itkimage->itklabelmap
    typedef itk::LabelImageToLabelMapFilter<ImageType, LabelMapType> ConverterType;
    itk::SmartPointer<ConverterType> converter = ConverterType::New();

    converter->SetInput(infoChanger->GetOutput());
    converter->ReleaseDataFlagOn();
    _progress->Observe(converter,"Label Map", 0.14);
    converter->Update();
    _progress->Ignore(converter);
    converter->GetOutput()->Optimize();
    assert(0 != converter->GetOutput()->GetNumberOfLabelObjects());

	// flatten labelmap, modify origin and store scalar label values
	_dataManager->Initialize(converter->GetOutput(), _orientationData, _fileMetadata);

	// check if there are unused objects
	_fileMetadata->CompactObjects();

	std::vector<unsigned int> unusedLabels = _fileMetadata->GetUnusedObjectsLabels();
	if (0 != unusedLabels.size())
	{
		std::stringstream out;
		msgBox.setIcon(QMessageBox::Warning);

		qApp->restoreOverrideCursor();
		std::string text = std::string("The segmentation contains unused objects (with no voxels assigned).\nThose objects will be discarded.\n");
		msgBox.setText(text.c_str());
		std::string details = std::string("Unused objects: label ");
		for (unsigned int i = 0; i < unusedLabels.size(); i++)
		{
			out << unusedLabels.at(i);
			details += out.str();
			if ((i+1) < unusedLabels.size())
				details += std::string(", label ");
		}
		msgBox.setDetailedText(details.c_str());
		msgBox.exec();
		qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
	}

	// itklabelmap->itkimage
	typedef itk::LabelMapToLabelImageFilter<LabelMapType, ImageType> LabelMapToImageFilterType;
	itk::SmartPointer<LabelMapToImageFilterType> labelconverter = LabelMapToImageFilterType::New();

	labelconverter->SetInput(_dataManager->GetLabelMap());
	labelconverter->SetNumberOfThreads(1);						// if number of threads > 1 filter crashes (¿¿??)
	labelconverter->ReleaseDataFlagOn();

	_progress->Observe(labelconverter,"Convert Image", 0.14);
	labelconverter->Update();
	_progress->Ignore(labelconverter);

	// itkimage->vtkimage
	typedef itk::VTKImageExport<ImageType> ITKExport;
	itk::SmartPointer<ITKExport> itkExporter = ITKExport::New();
	vtkSmartPointer<vtkImageImport> vtkImporter = vtkSmartPointer<vtkImageImport>::New();
	itkExporter->SetInput(labelconverter->GetOutput());
	ConnectPipelines(itkExporter, vtkImporter);
	_progress->Observe(vtkImporter,"Import", 0.14);
	_progress->Observe(itkExporter,"Export", 0.14);
	vtkImporter->Update();
	_progress->Ignore(itkExporter);
	_progress->Ignore(vtkImporter);
	
	// get the vtkStructuredPoints out of the vtkImage
	vtkSmartPointer<vtkImageToStructuredPoints> convert = vtkSmartPointer<vtkImageToStructuredPoints>::New();
	convert->SetInput(vtkImporter->GetOutput());
	convert->ReleaseDataFlagOn();
	_progress->Observe(convert,"Convert Points",0.14);
	convert->Update();
	_progress->Ignore(convert);
	
	// now we have our structuredpoints
	_dataManager->SetStructuredPoints(convert->GetStructuredPointsOutput());

    // add volume actors to 3D renderer
	_volumeRender = new VoxelVolumeRender(_dataManager, _voxelViewRenderer, _progress);

    // set the default POI (point of interest)
    Vector3ui size = _orientationData->GetTransformedSize();
    _POI[0] = (size[0]-1)/2;
    _POI[1] = (size[1]-1)/2;
    _POI[2] = (size[2]-1)/2;
    
    // visualize slices in all planes
    _sagittalSliceVisualization->Initialize(_dataManager->GetStructuredPoints(), _dataManager->GetLookupTable(), _sagittalViewRenderer, _orientationData);
    _coronalSliceVisualization->Initialize(_dataManager->GetStructuredPoints(), _dataManager->GetLookupTable(),  _coronalViewRenderer, _orientationData);
    _axialSliceVisualization->Initialize(_dataManager->GetStructuredPoints(), _dataManager->GetLookupTable(), _axialViewRenderer, _orientationData);
    _axialSliceVisualization->Update(_POI);
    _coronalSliceVisualization->Update(_POI);
    _sagittalSliceVisualization->Update(_POI);

    // we don't initialize slider position because thats what the spinBoxes will do on init with POI+1 because sliders go 1-max and POI is 0-(max-1)
    axialslider->setEnabled(false);
    axialslider->setMinimum(1);
    axialslider->setMaximum(size[2]);
    axialslider->setEnabled(true);
    coronalslider->setEnabled(false);
    coronalslider->setMinimum(1);
    coronalslider->setMaximum(size[1]);
    coronalslider->setEnabled(true);
    sagittalslider->setEnabled(false);
    sagittalslider->setMinimum(1);
    sagittalslider->setMaximum(size[0]);
    sagittalslider->setEnabled(true);

    // initialize spinbox positions with POI+1 because sliders & spinBoxes go 1-max and POI is 0-(max-1)
    // it also initializes sliders and renders the viewports
    XspinBox->setRange(1, size[0]);
    XspinBox->setEnabled(true);
    XspinBox->setValue(_POI[0]+1);
    YspinBox->setRange(1, size[1]);
    YspinBox->setEnabled(true);
    YspinBox->setValue(_POI[1]+1);
    ZspinBox->setRange(1, size[2]);
    ZspinBox->setEnabled(true);
    ZspinBox->setValue(_POI[2]+1);

    // fill selection label combobox and draw label combobox
    FillColorLabels();
    this->updatepointlabel = true;
    SetPointLabel();
    
    // initalize EditorOperations instance
    _editorOperations->Initialize(_voxelViewRenderer, _orientationData, _progress);

    // enable disabled widgets
    viewbutton->setEnabled(true);
    paintbutton->setEnabled(true);
    pickerbutton->setEnabled(true);
    selectbutton->setEnabled(true);
    axialresetbutton->setEnabled(true);
    coronalresetbutton->setEnabled(true);
    sagittalresetbutton->setEnabled(true);
    voxelresetbutton->setEnabled(true);
    rendertypebutton->setEnabled(false);
    axestypebutton->setEnabled(true);

    erodeoperation->setEnabled(false);
    dilateoperation->setEnabled(false);
    openoperation->setEnabled(false);
    closeoperation->setEnabled(false);
    watershedoperation->setEnabled(false);

    a_fileSave->setEnabled(true);
    a_fileReferenceOpen->setEnabled(true);
    axialsizebutton->setEnabled(true);
    coronalsizebutton->setEnabled(true);
    sagittalsizebutton->setEnabled(true);
    rendersizebutton->setEnabled(true);
    renderdisablebutton->setEnabled(true);
    
    // needed to maximize/mininize views, not really necessary but looks better
    viewgrid->setColumnMinimumWidth(0,0);
    viewgrid->setColumnMinimumWidth(1,0);
    viewgrid->setRowMinimumHeight(0,0);
    viewgrid->setRowMinimumHeight(1,0);
    
    // set axes initial state
    _axesRender = new AxesRender(_voxelViewRenderer, _orientationData);
    _axesRender->Update(_POI);

    // update all renderers
    _axialViewRenderer->ResetCamera();
    _axialSliceVisualization->ZoomEvent();
    _coronalViewRenderer->ResetCamera();
    _coronalSliceVisualization->ZoomEvent();
    _sagittalViewRenderer->ResetCamera();
    _sagittalSliceVisualization->ZoomEvent();
    _voxelViewRenderer->ResetCamera();
    
    // we can now begin updating the viewports
    this->updatevoxelrenderer = true;
    this->updateslicerenderers = true;
    this->renderisavolume = true;
    UpdateViewports(All);
    
    // reset parts of the GUI, needed when loading another image (another session) to reset buttons
    // and items to their initial states. Same goes for selected label.
    axestypebutton->setIcon(QIcon(":newPrefix/icons/noaxes.png"));
    labelselector->setCurrentRow(0);
    _selectedLabel = 0;
    viewbutton->setChecked(true);

    // initially without a reference image
    _hasReferenceImage = false;
    
    _progress->ManualReset();
}

void EspinaVolumeEditor::EditorReferenceOpen(void)
{
	QMessageBox msgBox;

	QString filename = QFileDialog::getOpenFileName(this, tr("Open Reference Image"), QDir::currentPath(), QObject::tr("image files (*.mhd *.mha);;All files (*.*)"));

	if (!filename.isNull())
		filename.toAscii();
	else
		return;

	vtkSmartPointer<vtkMetaImageReader> reader = vtkSmartPointer<vtkMetaImageReader>::New();
	reader->SetFileName(filename.toStdString().c_str());
	
	try
	{
		reader->Update();
	} 
	catch (...)
	{
		msgBox.setIcon(QMessageBox::Critical);

		std::string text = std::string("An error occurred loading the segmentation reference file.\nThe operation has been aborted.");
		msgBox.setText(text.c_str());
		msgBox.exec();
		return;
	}

    _progress->ManualSet("Load");
    
    // don't ask me why but espina segmentation and reference image have different
    // orientation, to match both images we'll have to flip the volume in the Y and Z
    // axis, but preserving the image extent
    vtkSmartPointer<vtkImageFlip> imageflipY = vtkSmartPointer<vtkImageFlip>::New();
    imageflipY->SetInput(reader->GetOutput());
    imageflipY->SetFilteredAxis(1);
    imageflipY->PreserveImageExtentOn();
    _progress->Observe(imageflipY,"Flip Y Axis",(1.0/4.0));
    imageflipY->Update();
    _progress->Ignore(imageflipY);

    vtkSmartPointer<vtkImageFlip> imageflipZ = vtkSmartPointer<vtkImageFlip>::New();
    imageflipZ->SetInput(imageflipY->GetOutput());
    imageflipZ->SetFilteredAxis(2);
    imageflipZ->PreserveImageExtentOn();
    _progress->Observe(imageflipZ,"Flip Z Axis",(1.0/4.0));
    imageflipZ->Update();
    _progress->Ignore(imageflipZ);
    
    vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
    image = imageflipZ->GetOutput();

    // need to check that segmentation image and reference image have the same origin, size, spacing and direction
    int size[3];
    image->GetDimensions(size);
    Vector3ui segmentationsize = _orientationData->GetImageSize();
    if (segmentationsize != Vector3ui(size[0], size[1], size[2]))
    {
    	_progress->ManualReset();
    	std::stringstream out;
		msgBox.setIcon(QMessageBox::Critical);

		std::string text = std::string("Reference and segmentation images have different dimensions.\nReference size is [");
		out << size[0] << std::string(",") << size[1] << std::string(",") << size[2] << std::string("].\nSegmentation size is [");
		out << segmentationsize[0] << std::string(",") << segmentationsize[1] << std::string(",") << segmentationsize[2];
		out << std::string("].\nThe operation has been aborted.");
		text += out.str();
		msgBox.setText(text.c_str());
		msgBox.exec();
		return;
    }

    double origin[3];
    image->GetOrigin(origin);
    Vector3d segmentationorigin = _orientationData->GetImageOrigin();
    if (segmentationorigin != Vector3d(origin[0], origin[1], origin[2]))
    {
    	qApp->restoreOverrideCursor();
    	std::stringstream out;
		msgBox.setIcon(QMessageBox::Warning);

		std::string text = std::string("Reference and segmentation images have different origin of coordinates.\nReference origin is [");
		out << origin[0] << "," << origin[1] << "," << origin[2] << "].\nSegmentation origin is [" << segmentationorigin[0] << ",";
		out << segmentationorigin[1] << "," << segmentationorigin[2] << "].\nEditor will use segmentation origin.";
		text += out.str();
		msgBox.setText(text.c_str());
		msgBox.exec();
		qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
    }

    double spacing[3];
    image->GetSpacing(spacing);
    Vector3d segmentationspacing = _orientationData->GetImageSpacing();
    if (segmentationspacing != Vector3d(spacing[0], spacing[1], spacing[2]))
    {
    	qApp->restoreOverrideCursor();
    	std::stringstream out;
		msgBox.setIcon(QMessageBox::Warning);

		std::string text = std::string("Reference and segmentation images have different point spacing.\nReference spacing is [");
		out << spacing[0] << "," << spacing[1] << "," << spacing[2] << "].\nSegmentation spacing is [" << segmentationspacing[0] << ",";
		out << segmentationspacing[1] << "," << segmentationspacing[2] << "].\nEditor will use segmentation spacing for both.";
		text += out.str();
		msgBox.setText(text.c_str());
		msgBox.exec();
		qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
	}

	vtkSmartPointer<vtkImageChangeInformation> changer = vtkSmartPointer<vtkImageChangeInformation>::New();
	changer->SetInput(image);

    if (segmentationspacing != Vector3d(spacing[0], spacing[1], spacing[2]))
    	changer->SetOutputSpacing(segmentationspacing[0], segmentationspacing[1], segmentationspacing[2]);

    changer->SetOutputOrigin(0.0,0.0,0.0);
    changer->ReleaseDataFlagOn();

  	_progress->Observe(changer, "Fix Image", (1.0/4.0));
   	changer->Update();
   	_progress->Ignore(changer);

    vtkSmartPointer<vtkImageToStructuredPoints> convert = vtkSmartPointer<vtkImageToStructuredPoints>::New();
    convert->SetInput(changer->GetOutput());
    _progress->Observe(convert,"Convert",(1.0/4.0));
    convert->Update();
    _progress->Ignore(convert);
    
    vtkSmartPointer<vtkStructuredPoints> structuredPoints = vtkSmartPointer<vtkStructuredPoints>::New();
    structuredPoints = convert->GetStructuredPointsOutput();
    structuredPoints->Update();

    // now that we have a reference image make the background of the segmentation completely transparent
    double rgba[4] = { 0.0, 0.0, 0.0, 0.0 };
    _dataManager->GetLookupTable()->SetTableValue(0,rgba);
    _dataManager->GetLookupTable()->Modified();
    
    // pass reference image to slice visualization
    _axialSliceVisualization->SetReferenceImage(structuredPoints);
    _coronalSliceVisualization->SetReferenceImage(structuredPoints);
    _sagittalSliceVisualization->SetReferenceImage(structuredPoints);
    UpdateViewports(Slices);
    
    // NOTE: structuredPoints pointer is not stored so when this method ends just the slices have a reference
    // to the data, if a new reference image is loaded THEN it's memory gets freed as there are not more
    // references to it in memory.
	_hasReferenceImage = true;
	
	// reset editing state
    viewbutton->setChecked(true);
    
    _progress->ManualReset();
}

void EspinaVolumeEditor::EditorSave()
{
	QMessageBox msgBox;
	string filenameStd;

    QString filename = QFileDialog::getSaveFileName(this, tr("Save Segmentation Image"), QDir::currentPath(), QObject::tr("label image files (*.segmha)"));

    if(!filename.isNull())
        filename.toAscii();
    else
        return;

    filenameStd = filename.toStdString();
    std::size_t found;

    // check if user entered file extension "segmha" or not, if not just add it
    if (std::string::npos == (found = filenameStd.rfind(".segmha")))
    	filenameStd += std::string(".segmha");

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    _editorOperations->SaveImage(filenameStd);

	if (!_fileMetadata->Write(QString(filenameStd.c_str()), _dataManager))
	{
		msgBox.setIcon(QMessageBox::Critical);

		std::string text = std::string("An error occurred saving the segmentation metadata to file \"");
		text += filenameStd;
		text += std::string("\".\nThe segmentation data has been saved, but the metadata has not.\nThe file could be unusable.");
		msgBox.setText(text.c_str());
		msgBox.exec();
	}
}

void EspinaVolumeEditor::EditorExit()
{
    _acceptedChanges = false;       // no need to do this, we just make it explicit for future coders
    qApp->exit();
}

void EspinaVolumeEditor::FullscreenToggle()
{
    QAction *action = qobject_cast<QAction *>(sender());
    
    if ((windowState() & Qt::WindowFullScreen))
    {
        action->setStatusTip(tr("Set application fullscreen on"));
        action->setChecked(false);
    }
    else
    {
        action->setStatusTip(tr("Set application fullscreen off"));
        action->setChecked(true);
    }
    setWindowState(windowState() ^ Qt::WindowFullScreen);
}

void EspinaVolumeEditor::MoveAxialSlider(int value)
{
    if (!axialslider->isEnabled())
        return;

    ZspinBox->setValue(value);

    // slider values are in the range [1-size] but coordinates are [0-(slices-1)]
    value--;
    _POI[2] = value;
    if (updatepointlabel)
    	SetPointLabel();
    
    _sagittalSliceVisualization->UpdateCrosshair(_POI);
    _coronalSliceVisualization->UpdateCrosshair(_POI);
    _axialSliceVisualization->UpdateSlice(_POI);
    
    if (updateslicerenderers)
        UpdateViewports(Slices);
    
    if (updatevoxelrenderer)
    {
        _axesRender->Update(_POI);
        if (_axesRender->isVisible())
            UpdateViewports(Voxel);
    }
}

void EspinaVolumeEditor::MoveCoronalSlider(int value)
{
    if (!coronalslider->isEnabled())  
        return;
    
    YspinBox->setValue(value);

    // slider values are in the range [1-size] but coordinates are [0-(slices-1)]
    value--;

    _POI[1] = value;
    if (updatepointlabel)
    	SetPointLabel();

    _sagittalSliceVisualization->UpdateCrosshair(_POI);
    _coronalSliceVisualization->UpdateSlice(_POI);
    _axialSliceVisualization->UpdateCrosshair(_POI);

    if (updateslicerenderers)
        UpdateViewports(Slices);

    if (updatevoxelrenderer)
    {
        _axesRender->Update(_POI);
        if (_axesRender->isVisible())
            UpdateViewports(Voxel);
    }
}

void EspinaVolumeEditor::MoveSagittalSlider(int value)
{
    if (!sagittalslider->isEnabled())
        return;

    XspinBox->setValue(value);
    
    // slider values are in the range [1-size] but coordinates are [0-(slices-1)]
    value--;
    
    _POI[0] = value;
    if (updatepointlabel)
    	SetPointLabel();

    _sagittalSliceVisualization->UpdateSlice(_POI);
    _coronalSliceVisualization->UpdateCrosshair(_POI);
    _axialSliceVisualization->UpdateCrosshair(_POI);
    
    if (updateslicerenderers)
        UpdateViewports(Slices);

    if (updatevoxelrenderer)
    {
        _axesRender->Update(_POI);
        if (_axesRender->isVisible())
            UpdateViewports(Voxel);
    }

}

void EspinaVolumeEditor::SliceSliderPressed(void)
{
    // continous rendering of the render view would hog the system, so we disable it
    // while the user moves the slider. Once the slider is released, we will render
    // the view's final state
    updatevoxelrenderer = false;
}

void EspinaVolumeEditor::SliceSliderReleased(void)
{
    updatevoxelrenderer = true;
    _axesRender->Update(_POI);
    UpdateViewports(Voxel);
}

void EspinaVolumeEditor::ChangeXspinBox(int value)
{
    // sagittal slider is in charge of updating things
    sagittalslider->setSliderPosition(value);
}

void EspinaVolumeEditor::ChangeYspinBox(int value)
{
    // sagittal slider is in charge of updating things
    coronalslider->setSliderPosition(value);
}

void EspinaVolumeEditor::ChangeZspinBox(int value)
{
    // sagittal slider is in charge of updating things
    axialslider->setSliderPosition(value);
}

void EspinaVolumeEditor::SetPointLabel()
{
	// get pixel value
    _pointScalar = _dataManager->GetVoxelScalar(_POI[0],_POI[1],_POI[2]);

    // the labelselector change event deselects pickerbutton and selects view button
    if (pickerbutton->isChecked())
    	labelselector->setCurrentRow(_pointScalar);

    if (0 == _pointScalar)
    {
        pointlabelnumber->setText(" Background");
        pointlabelcolor->setText(" None");
        return;
    }

    // point with label selected
    double rgba[4];
    _dataManager->GetLookupTable()->GetTableValue(_pointScalar, rgba);
    
    // we need to use double vales to build icon as some colours are too close
    // and could get identical if we use the values converted to int.
    QPixmap icon(32,16);
    QColor color;
    color.setRgbF(rgba[0], rgba[1], rgba[2], 1);
    icon.fill(color);

    unsigned short labelindex = _dataManager->GetScalarForLabel(_pointScalar);
    std::stringstream out1;
    out1 << labelindex;
    pointlabelnumber->setText(out1.str().c_str());
    pointlabelcolor->setPixmap(icon);
}

void EspinaVolumeEditor::FillColorLabels()
{
    // we need to disable it to avoid sending signals while updating
    labelselector->setEnabled(false);
    
    if (labelselector->count() != 0)
    	labelselector->clear();

    unsigned int num_colors = _dataManager->GetLookupTable()->GetNumberOfTableValues();
    labelselector->insertItem(0, "Background");
    
    // color 0 is black, so we will start from 1
    for (unsigned int i = 1; i < num_colors; i++)
    {
        double rgba[4];
        QPixmap icon(16,16);
        QColor color;

    	_dataManager->GetLookupTable()->GetTableValue(i, rgba);
        color.setRgbF(rgba[0], rgba[1], rgba[2], 1);
        icon.fill(color);
        std::stringstream out;
        out << _fileMetadata->GetObjectSegmentName(i) << " " << _dataManager->GetScalarForLabel(i);
        QListWidgetItem *item = new QListWidgetItem(QIcon(icon), QString(out.str().c_str()));
        labelselector->addItem(item);
    }

    labelselector->setCurrentRow(_selectedLabel);
    labelselector->setEnabled(true);
}

void EspinaVolumeEditor::LabelSelectionChanged(int value)
{
	static unsigned short previousValue = 0;
    double rgba[4];

    if (!labelselector->isEnabled() || (value == -1) || (previousValue == value))
        return;
    
    // first dim the previously selected label, if any
    // BEWARE: we could have changed the lookuptable and previous value could refer a label that no longer exists
    if ((previousValue > 0) && (previousValue < _dataManager->GetLookupTable()->GetNumberOfTableValues()))
    {
    	_dataManager->GetLookupTable()->GetTableValue(previousValue, rgba);
    	_dataManager->GetLookupTable()->SetTableValue(previousValue, rgba[0], rgba[1], rgba[2], 0.4);
    }
    
    previousValue = value;
    _selectedLabel = value;

    // if actual value != 0 (background) highlight it with an alpha of 1
    if(value != 0)
    {
    	_dataManager->GetLookupTable()->GetTableValue(value, rgba);
    	_dataManager->GetLookupTable()->SetTableValue(value, rgba[0], rgba[1], rgba[2], 1);
    }
    _dataManager->GetLookupTable()->Modified();

    // focus volume renderer and reset switch render button
    _volumeRender->UpdateFocus(value);
    renderisavolume = true;
    _volumeRender->ViewAsVolume();
    rendertypebutton->setIcon(QIcon(":/newPrefix/icons/mesh.png"));
    rendertypebutton->setToolTip(tr("Switch to mesh renderer"));

    // don't want filters with the background label
    switch (_selectedLabel)
    {
    	case 0:
    		erodeoperation->setEnabled(false);
    		dilateoperation->setEnabled(false);
    		openoperation->setEnabled(false);
    		closeoperation->setEnabled(false);
    		watershedoperation->setEnabled(false);
    	    if (pickerbutton->isChecked())
    	    	viewbutton->setChecked(true);
    	    rendertypebutton->setEnabled(false);
    	    UpdateViewports(All);
    		return;
    		break;
    	default:
    		erodeoperation->setEnabled(true);
    		dilateoperation->setEnabled(true);
    		openoperation->setEnabled(true);
    		closeoperation->setEnabled(true);
    		watershedoperation->setEnabled(true);
    		if(_voxelViewRenderer->GetDraw() != false)
    			rendertypebutton->setEnabled(true);
    		break;
    }

    // if the user selected the label by hand I don't want to change the POI, just highlight the label
    // in the picked point
    if (pickerbutton->isChecked())
    {
    	viewbutton->setChecked(true);
    	UpdateViewports(All);
    	return;
    }

    // if the selected label has no voxels it has no centroid
    if (0LL == _dataManager->GetNumberOfVoxelsForLabel(value))
    {
    	UpdateViewports(All);
    	return;
    }

    Vector3d newPOI = _dataManager->GetCentroidForObject(value);

    updateslicerenderers = false;
    updatevoxelrenderer = false;
    updatepointlabel = false;

    // POI values start in 0, spinboxes start in 1
    _POI[0] = static_cast<unsigned int>(newPOI[0]);
    _POI[1] = static_cast<unsigned int>(newPOI[1]);
    _POI[2] = static_cast<unsigned int>(newPOI[2]);
    ZspinBox->setValue(_POI[2]+1);
    YspinBox->setValue(_POI[1]+1);
    XspinBox->setValue(_POI[0]+1);

    _sagittalSliceVisualization->Update(_POI);
    _coronalSliceVisualization->Update(_POI);
    _axialSliceVisualization->Update(_POI);
    _axesRender->Update(_POI);

    SetPointLabel();

    // change slice view to the new label
    Vector3d spacing = _orientationData->GetImageSpacing();
    double coords[3];
    _axialViewRenderer->GetActiveCamera()->GetPosition(coords);
    _axialViewRenderer->GetActiveCamera()->SetPosition(_POI[0]*spacing[0],_POI[1]*spacing[1],coords[2]);
    _axialViewRenderer->GetActiveCamera()->SetFocalPoint(_POI[0]*spacing[0],_POI[1]*spacing[1],0.0);
    _axialSliceVisualization->ZoomEvent();
    _coronalViewRenderer->GetActiveCamera()->GetPosition(coords);
    _coronalViewRenderer->GetActiveCamera()->SetPosition(_POI[0]*spacing[0],_POI[2]*spacing[2],coords[2]);
    _coronalViewRenderer->GetActiveCamera()->SetFocalPoint(_POI[0]*spacing[0],_POI[2]*spacing[2],0.0);
    _coronalSliceVisualization->ZoomEvent();
    _sagittalViewRenderer->GetActiveCamera()->GetPosition(coords);
    _sagittalViewRenderer->GetActiveCamera()->SetPosition(_POI[1]*spacing[1],_POI[2]*spacing[2],coords[2]);
    _sagittalViewRenderer->GetActiveCamera()->SetFocalPoint(_POI[1]*spacing[1],_POI[2]*spacing[2],0.0);
    _sagittalSliceVisualization->ZoomEvent();

    updatepointlabel = true;
    updateslicerenderers = true;
    updatevoxelrenderer = true;

    UpdateViewports(All);
}

void EspinaVolumeEditor::Preferences()
{
    QtPreferences configdialog(this);
    
    configdialog.SetInitialOptions(
    		_dataManager->GetUndoRedoBufferSize(), 
    		_dataManager->GetUndoRedoBufferCapacity(), 
    		_editorOperations->GetFiltersRadius(), 
    		_editorOperations->GetWatershedLevel(), 
    		_axialSliceVisualization->GetSegmentationOpacity());
    
    if (true == _hasReferenceImage)
    	configdialog.EnableVisualizationBox();
    
    configdialog.exec();
    
    if (!configdialog.ModifiedData())
        return;

    _editorOperations->SetFiltersRadius(configdialog.GetRadius());
    _editorOperations->SetWatershedLevel(configdialog.GetLevel());
    _dataManager->SetUndoRedoBufferSize(configdialog.GetSize());

    // the undo/redo system size could have been modified and then some actions could have been deleted
    UpdateUndoRedoMenu();
    
    if (true == _hasReferenceImage)
    {
		_axialSliceVisualization->SetSegmentationOpacity(configdialog.GetSegmentationOpacity());
		_sagittalSliceVisualization->SetSegmentationOpacity(configdialog.GetSegmentationOpacity());
		_coronalSliceVisualization->SetSegmentationOpacity(configdialog.GetSegmentationOpacity());
	    // the visualization options could have been modified so we must update the slices
	    UpdateViewports(Slices);
    }
}

void EspinaVolumeEditor::ViewReset()
{
    QToolButton *button = qobject_cast<QToolButton *>(sender());
    
    if (button == axialresetbutton)
    {
        _axialViewRenderer->ResetCamera();
        _axialSliceVisualization->ZoomEvent();
        UpdateViewports(Axial);
    }
    
    if (button == coronalresetbutton)
    {
        _coronalViewRenderer->ResetCamera();
        _coronalSliceVisualization->ZoomEvent();
        UpdateViewports(Coronal);
    }
    
    if (button == sagittalresetbutton)
    {
        _sagittalViewRenderer->ResetCamera();
        _sagittalSliceVisualization->ZoomEvent();
        UpdateViewports(Sagittal);
    }
    
    if (button == voxelresetbutton)
    {
        _voxelViewRenderer->ResetCamera();
        UpdateViewports(Voxel);
    }
}

void EspinaVolumeEditor::SwitchVoxelRender()
{
    switch(renderisavolume)
    {
    	case false:
    		renderisavolume = true;
    		_volumeRender->ViewAsVolume();
            rendertypebutton->setIcon(QIcon(":/newPrefix/icons/mesh.png"));
            rendertypebutton->setToolTip(tr("Switch to mesh renderer"));
            break;
        case true:
        	renderisavolume = false;
        	_volumeRender->ViewAsMesh();
        	rendertypebutton->setIcon(QIcon(":/newPrefix/icons/voxel.png"));
            rendertypebutton->setToolTip(tr("Switch to volume renderer"));
            break;
        default:
            break;
    }
    
    UpdateViewports(Voxel);
}

void EspinaVolumeEditor::SwitchAxesView()
{
    if (_axesRender->isVisible())
    {
        _axesRender->setVisible(false);
        axestypebutton->setIcon(QIcon(":newPrefix/icons/axes.png"));
        axestypebutton->setToolTip(tr("Turn on axes planes rendering"));
    }
    else
    {
        _axesRender->setVisible(true);
        axestypebutton->setIcon(QIcon(":newPrefix/icons/noaxes.png"));
        axestypebutton->setToolTip(tr("Turn off axes planes rendering"));
    }
    UpdateViewports(Voxel);
}

void EspinaVolumeEditor::EditorSelectionEnd(bool value)
{
    if (!value)
        return;
    
    _editorOperations->ClearSelection();
    _axialSliceVisualization->ClearSelection();
    _coronalSliceVisualization->ClearSelection();
    _sagittalSliceVisualization->ClearSelection();
    
    relabelbutton->setEnabled(false);
    cutbutton->setEnabled(false);
    UpdateViewports(All);
}

void EspinaVolumeEditor::EditorCut()
{
    _editorOperations->CutSelection(_selectedLabel);
    SetPointLabel();
    UpdateUndoRedoMenu();
    UpdateViewports(All);
}

void EspinaVolumeEditor::EditorRelabel()
{
    if (_editorOperations->RelabelSelection(this, _selectedLabel, _dataManager->GetLookupTable(), _fileMetadata))
    {
        FillColorLabels();

        // not faster but easier
    	delete _volumeRender;
    	_volumeRender = new VoxelVolumeRender(_dataManager, _voxelViewRenderer, _progress);
    	_volumeRender->UpdateFocus(_selectedLabel);
    }

    SetPointLabel();
    UpdateUndoRedoMenu();
    UpdateViewports(All);
}

void EspinaVolumeEditor::UpdateViewports(VIEWPORTSENUM view)
{
    // updating does not happen in hidden views to avoid wasting CPU,
    // only when the users minimize a view updating is enabled again
    switch (view)
    {
        case Voxel:
            if (renderview->isVisible())
                _voxelViewRenderer->GetRenderWindow()->Render();
            break;
        case Slices:
            if (axialview->isVisible())
                _axialViewRenderer->GetRenderWindow()->Render();
            if (coronalview->isVisible())
                _coronalViewRenderer->GetRenderWindow()->Render();
            if (sagittalview->isVisible())
                _sagittalViewRenderer->GetRenderWindow()->Render();
            break;
        case All:
            if (axialview->isVisible())
                _axialViewRenderer->GetRenderWindow()->Render();
            if (coronalview->isVisible())
                _coronalViewRenderer->GetRenderWindow()->Render();
            if (sagittalview->isVisible())
                _sagittalViewRenderer->GetRenderWindow()->Render();
            if (renderview->isVisible())
                _voxelViewRenderer->GetRenderWindow()->Render();
            break;
        case Axial:
            if (axialview->isVisible())
                _axialViewRenderer->GetRenderWindow()->Render();
            break;
        case Coronal:
            if (coronalview->isVisible())
                _coronalViewRenderer->GetRenderWindow()->Render();
            break;
        case Sagittal:
            if (sagittalview->isVisible())
                _sagittalViewRenderer->GetRenderWindow()->Render();
            break;
        default:
            break;
    }
}

void EspinaVolumeEditor::About()
{
    QtAbout aboutdialog(this);
    aboutdialog.exec();
}

void EspinaVolumeEditor::ErodeVolume()
{
    _editorOperations->ErodeSelection(_selectedLabel);

    SetPointLabel();
    UpdateUndoRedoMenu();
    UpdateViewports(All);
}

void EspinaVolumeEditor::DilateVolume()
{
    _editorOperations->DilateSelection(_selectedLabel);

    SetPointLabel();
    UpdateUndoRedoMenu();
    _volumeRender->UpdateFocusExtent();
    UpdateViewports(All);
}

void EspinaVolumeEditor::OpenVolume()
{
    _editorOperations->OpenSelection(_selectedLabel);

    SetPointLabel();
    UpdateUndoRedoMenu();
    _volumeRender->UpdateFocusExtent();
    UpdateViewports(All);
}

void EspinaVolumeEditor::CloseVolume()
{
    _editorOperations->CloseSelection(_selectedLabel);

    SetPointLabel();
    UpdateUndoRedoMenu();
    _volumeRender->UpdateFocusExtent();
    UpdateViewports(All);
}

void EspinaVolumeEditor::WatershedVolume()
{
    _editorOperations->WatershedSelection(_selectedLabel);

    FillColorLabels();

    // not faster but easier
	delete _volumeRender;
	_volumeRender = new VoxelVolumeRender(_dataManager, _voxelViewRenderer, _progress);
	_volumeRender->UpdateFocus(_selectedLabel);

    SetPointLabel();
    UpdateUndoRedoMenu();
    UpdateViewports(All);
}

void EspinaVolumeEditor::UpdateUndoRedoMenu()
{
	std::string text;

    if (_dataManager->IsUndoBufferEmpty())
        text = std::string("Undo");
    else
    	text = std::string("Undo ") + _dataManager->GetUndoActionString();

    a_undo->setText(text.c_str());
    a_undo->setEnabled(!(_dataManager->IsUndoBufferEmpty()));

    if (_dataManager->IsRedoBufferEmpty())
    	text = std::string("Redo");
    else
    	text = std::string("Redo ") + _dataManager->GetRedoActionString();

    a_redo->setText(text.c_str());
    a_redo->setEnabled(!(_dataManager->IsRedoBufferEmpty()));
}

void EspinaVolumeEditor::OperationUndo()
{
    std::string text = std::string("Undo ") + _dataManager->GetUndoActionString();
    _progress->ManualSet(text);

    _dataManager->DoUndoOperation();
    
    // update UI
    if (_selectedLabel > _dataManager->GetLookupTable()->GetNumberOfTableValues()-1)
        _selectedLabel = 0;
        
    LabelSelectionChanged(_selectedLabel);
    SetPointLabel();
    FillColorLabels();
    UpdateUndoRedoMenu();
    UpdateViewports(All);
    _progress->ManualReset();
}

void EspinaVolumeEditor::OperationRedo()
{
	std::string text = std::string("Redo ") + _dataManager->GetRedoActionString();
    _progress->ManualSet(text);
    
    _dataManager->DoRedoOperation();
    
    // update UI
    if (_selectedLabel > _dataManager->GetLookupTable()->GetNumberOfTableValues()-1)
        _selectedLabel = 0;
        
    LabelSelectionChanged(_selectedLabel);
    SetPointLabel();
    FillColorLabels();
    UpdateUndoRedoMenu();
    UpdateViewports(All);
    _progress->ManualReset();
}

void EspinaVolumeEditor::AxialInteraction(vtkObject* object, unsigned long event, void*, void*, vtkCommand*)
{
    static bool leftButtonStillDown = false;
    static bool rightButtonStillDown = false;
    static bool middleButtonStillDown = false;
    
    // to pass events forward
    static vtkSmartPointer<vtkInteractorStyle> style = vtkInteractorStyle::SafeDownCast(axialview->GetRenderWindow()->GetInteractor()->GetInteractorStyle());

    switch(event)
    {
    	// sliders go [1-size], spinboxes go [0-(size-1)], that's the reason of the values added to POI.
        case vtkCommand::MouseWheelForwardEvent:
        	axialslider->setSliderPosition(_POI[2]+2);
            break;
        case vtkCommand::MouseWheelBackwardEvent:
        	axialslider->setSliderPosition(_POI[2]);
            break;
        case vtkCommand::RightButtonPressEvent:
            rightButtonStillDown = true;
            style->OnRightButtonDown();
            break;
        case vtkCommand::RightButtonReleaseEvent:
            rightButtonStillDown = false;
            style->OnRightButtonUp();
            break;
        case vtkCommand::LeftButtonPressEvent:
            leftButtonStillDown = true;
            AxialXYPick(event);
            break;
        case vtkCommand::LeftButtonReleaseEvent:
            leftButtonStillDown = false;
            AxialXYPick(event);
            break;
        case vtkCommand::MiddleButtonPressEvent:
            middleButtonStillDown = true;
            style->OnMiddleButtonDown();
            break;
        case vtkCommand::MiddleButtonReleaseEvent:
            middleButtonStillDown = false;
            style->OnMiddleButtonUp();
            break;
        case vtkCommand::MouseMoveEvent:
            if (!leftButtonStillDown && !rightButtonStillDown && !middleButtonStillDown)
            {
                style->OnMouseMove();
                return;
            }
            
            if (leftButtonStillDown)
            {
                style->OnMouseMove();
                AxialXYPick(event);
                return;
            }
            
            if (rightButtonStillDown || middleButtonStillDown)
            {
                style->OnMouseMove();
                _axialSliceVisualization->ZoomEvent();
                return;
            }
            break;
        default:
            // ignore event, can't happen as we have registered this function with only these commands up there
            break;
    }
}

void EspinaVolumeEditor::CoronalInteraction(vtkObject* object, unsigned long event, void*, void*, vtkCommand*)
{
    static bool leftButtonStillDown = false;
    static bool rightButtonStillDown = false;
    static bool middleButtonStillDown = false;
    
    // to pass events forward
    static vtkSmartPointer<vtkInteractorStyle> style = vtkInteractorStyle::SafeDownCast(coronalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle());

    switch(event)
    {
    	// sliders go [1-size], spinboxes go [0-(size-1)], that's the reason of the values added to POI.
        case vtkCommand::MouseWheelForwardEvent:
        	coronalslider->setSliderPosition(_POI[1]+2);
        	break;
        case vtkCommand::MouseWheelBackwardEvent:
        	coronalslider->setSliderPosition(_POI[1]);
        	break;
        case vtkCommand::RightButtonPressEvent:
            rightButtonStillDown = true;
            style->OnRightButtonDown();
            break;
        case vtkCommand::RightButtonReleaseEvent:
            rightButtonStillDown = false;
            style->OnRightButtonUp();
            break;
        case vtkCommand::LeftButtonPressEvent:
            leftButtonStillDown = true;
            CoronalXYPick(event);
            break;
        case vtkCommand::LeftButtonReleaseEvent:
            leftButtonStillDown = false;
            CoronalXYPick(event);
            break;
        case vtkCommand::MiddleButtonPressEvent:
            middleButtonStillDown = true;
            style->OnMiddleButtonDown();
            break;
        case vtkCommand::MiddleButtonReleaseEvent:
            middleButtonStillDown = false;
            style->OnMiddleButtonUp();
            break;
        case vtkCommand::MouseMoveEvent:
            if (!leftButtonStillDown && !rightButtonStillDown && !middleButtonStillDown)
            {
                style->OnMouseMove();
                return;
            }
            
            if (leftButtonStillDown)
            {
                style->OnMouseMove();
                CoronalXYPick(event);
                return;
            }
            
            if (rightButtonStillDown || middleButtonStillDown)
            {
                style->OnMouseMove();
                _coronalSliceVisualization->ZoomEvent();
                return;
            }
            break;
        default:
            // ignore event, can't happen as we have registered this function with only these commands up there
            break;
    }
}

void EspinaVolumeEditor::SagittalInteraction(vtkObject* object, unsigned long event, void*, void*, vtkCommand*)
{
    static bool leftButtonStillDown = false;
    static bool rightButtonStillDown = false;
    static bool middleButtonStillDown = false;
    
    // to pass events forward
    static vtkSmartPointer<vtkInteractorStyle> style = vtkInteractorStyle::SafeDownCast(sagittalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle());

    switch(event)
    {
    // sliders go [1-size], spinboxes go [0-(size-1)], that's the reason of the values added to POI.
        case vtkCommand::MouseWheelForwardEvent:
        	sagittalslider->setSliderPosition(_POI[0]+2);
            break;
        case vtkCommand::MouseWheelBackwardEvent:
        	sagittalslider->setSliderPosition(_POI[0]);
            break;
        case vtkCommand::RightButtonPressEvent:
            rightButtonStillDown = true;
            style->OnRightButtonDown();
            break;
        case vtkCommand::RightButtonReleaseEvent:
            rightButtonStillDown = false;
            style->OnRightButtonUp();
            break;
        case vtkCommand::LeftButtonPressEvent:
            leftButtonStillDown = true;
            SagittalXYPick(event);
            break;
        case vtkCommand::LeftButtonReleaseEvent:
            leftButtonStillDown = false;
            SagittalXYPick(event);
            break;
        case vtkCommand::MiddleButtonPressEvent:
            middleButtonStillDown = true;
            style->OnMiddleButtonDown();
            break;
        case vtkCommand::MiddleButtonReleaseEvent:
            middleButtonStillDown = false;
            style->OnMiddleButtonUp();
            break;
        case vtkCommand::MouseMoveEvent:
            if (!leftButtonStillDown && !rightButtonStillDown && !middleButtonStillDown)
            {
                style->OnMouseMove();
                return;
            }
            
            if (leftButtonStillDown)
            {
                style->OnMouseMove();
                SagittalXYPick(event);
                return;
            }
            
            if (rightButtonStillDown || middleButtonStillDown)
            {
                style->OnMouseMove();
                _sagittalSliceVisualization->ZoomEvent();
                return;
            }
            break;
        default:
            // ignore event, can't happen as we have registered this function with only these commands up there
            break;
    }
}

void EspinaVolumeEditor::AxialXYPick(unsigned long event)
{
    // static var stores data between calls
    static SliceVisualization::PickingType pickedProp = SliceVisualization::None;
    SliceVisualization::PickingType actualPick = SliceVisualization::None;
    
    // get coords
    int X = axialview->GetRenderWindow()->GetInteractor()->GetEventPosition()[0];
    int Y = axialview->GetRenderWindow()->GetInteractor()->GetEventPosition()[1];

    // we need to know first if we are picking something from the view
    actualPick = _axialSliceVisualization->GetPickPosition(&X, &Y);
    if (SliceVisualization::None == actualPick)
    {
      	// fix a glitch in rendering
      	if (vtkCommand::LeftButtonReleaseEvent == event)
      	{
      		updatevoxelrenderer = true;
            updateslicerenderers = true;
            UpdateViewports(All);
            pickedProp = SliceVisualization::None;
     	}
      	return;
    }
    
    // we need to know if we have started picking or we've picked something before
    if (SliceVisualization::None == pickedProp)
        pickedProp = actualPick;
    
    // if we were picking or painting but released the button, draw voxel view's final state
    if (vtkCommand::LeftButtonReleaseEvent == event)
    {
        updatevoxelrenderer = true;
        updateslicerenderers = true;

        if ((paintbutton->isChecked()) && (pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice))
        {
            _dataManager->OperationEnd();
            _volumeRender->UpdateFocusExtent();
            UpdateUndoRedoMenu();
        }
        _axesRender->Update(_POI);
        UpdateViewports(All);

        pickedProp = SliceVisualization::None;
        return;
    }

    if (vtkCommand::LeftButtonPressEvent == event)
    {
        if ((paintbutton->isChecked()) && (pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice))
            _dataManager->OperationStart("Paint");
    }

    updatevoxelrenderer = false;
    updateslicerenderers = false;
    updatepointlabel = false;
    
    // updating slider positions updates POI 
    if (actualPick == pickedProp)
    {
        sagittalslider->setSliderPosition(X+1);
        coronalslider->setSliderPosition(Y+1);
        
        // move camera if we are picking the thumbnail & update thumbnail to reflect new position        
        if (actualPick == SliceVisualization::Thumbnail)
        {
            double coords[3];
            Vector3d spacing = _orientationData->GetImageSpacing();
            _axialViewRenderer->GetActiveCamera()->GetPosition(coords);
            _axialViewRenderer->GetActiveCamera()->SetPosition(X*spacing[0],Y*spacing[1],coords[2]);
            _axialViewRenderer->GetActiveCamera()->SetFocalPoint(X*spacing[0],Y*spacing[1],0.0);
            _axialSliceVisualization->ZoomEvent();
        }
    }
    
    // get pixel value or pick a label if color picker is activated (managed in SetPointLabel())
    SetPointLabel();
    updatepointlabel = true;

    if ((pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice) && (paintbutton->isChecked()))
        _dataManager->SetVoxelScalar(_POI[0], _POI[1], _POI[2], _selectedLabel);
    
    if ((pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice) && (selectbutton->isChecked()))
    {
        cutbutton->setEnabled(true);
        relabelbutton->setEnabled(true);
        _editorOperations->AddSelectionPoint(Vector3ui(_POI[0], _POI[1], _POI[2]));
        _axialSliceVisualization->SetSelection(_editorOperations->GetSelection());
        _coronalSliceVisualization->SetSelection(_editorOperations->GetSelection());
        _sagittalSliceVisualization->SetSelection(_editorOperations->GetSelection());
    }
        
    // we will render the viewports as rendersliceviews == false now
    UpdateViewports(Slices);    
}

void EspinaVolumeEditor::CoronalXYPick(unsigned long event)
{
    // static var stores data between calls
    static SliceVisualization::PickingType pickedProp = SliceVisualization::None;
    SliceVisualization::PickingType actualPick = SliceVisualization::None;
    
    // get coords
    int X = coronalview->GetRenderWindow()->GetInteractor()->GetEventPosition()[0];
    int Y = coronalview->GetRenderWindow()->GetInteractor()->GetEventPosition()[1];

    // we need to know first if we are picking something from the view
    actualPick = _coronalSliceVisualization->GetPickPosition(&X, &Y);
    if (SliceVisualization::None == actualPick)
    {
      	// fix a glitch in rendering
      	if (vtkCommand::LeftButtonReleaseEvent == event)
      	{
      		updatevoxelrenderer = true;
            updateslicerenderers = true;
            UpdateViewports(All);
            pickedProp = SliceVisualization::None;
     	}
      	return;
    }
    
    // we need to know if we have started picking or we've picked something before
    if (SliceVisualization::None == pickedProp)
        pickedProp = actualPick;
    
    // if we were picking or painting but released the button, draw voxel view's final state
    if (vtkCommand::LeftButtonReleaseEvent == event)
    {
        updatevoxelrenderer = true;
        updateslicerenderers = true;

        if ((paintbutton->isChecked()) && (pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice))
        {
            _dataManager->OperationEnd();
            _volumeRender->UpdateFocusExtent();
            UpdateUndoRedoMenu();
        }
        _axesRender->Update(_POI);
        UpdateViewports(Voxel);
        
        pickedProp = SliceVisualization::None;
        return;
    }

    if (vtkCommand::LeftButtonPressEvent == event)
    {
        if ((paintbutton->isChecked()) && (pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice))
            _dataManager->OperationStart("Paint");
    }

    updatevoxelrenderer = false;
    updateslicerenderers = false;
    updatepointlabel = false;
    
    // updating slider positions updates POI
    if (actualPick == pickedProp)
    {
        sagittalslider->setSliderPosition(X+1);
        axialslider->setSliderPosition(Y+1);
        
        // move camera if we are picking the thumbnail & update thumbnail to reflect new position        
        if (actualPick == SliceVisualization::Thumbnail)
        {
            double coords[3];
            Vector3d spacing = _orientationData->GetImageSpacing();
            _coronalViewRenderer->GetActiveCamera()->GetPosition(coords);
            _coronalViewRenderer->GetActiveCamera()->SetPosition(X*spacing[0],Y*spacing[2],coords[2]);
            _coronalViewRenderer->GetActiveCamera()->SetFocalPoint(X*spacing[0],Y*spacing[2],0.0);
            _coronalSliceVisualization->ZoomEvent();
        }
    }

    // get pixel value
    SetPointLabel();
    updatepointlabel = true;

    if ((paintbutton->isChecked()) && (pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice))
        _dataManager->SetVoxelScalar(_POI[0], _POI[1], _POI[2],_selectedLabel);
    
    if ((selectbutton->isChecked()) && (pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice))
    {
        cutbutton->setEnabled(true);
        relabelbutton->setEnabled(true);
        _editorOperations->AddSelectionPoint(Vector3ui(_POI[0], _POI[1], _POI[2]));
        _axialSliceVisualization->SetSelection(_editorOperations->GetSelection());
        _coronalSliceVisualization->SetSelection(_editorOperations->GetSelection());
        _sagittalSliceVisualization->SetSelection(_editorOperations->GetSelection());
    }
        
    // we will render the viewports as rendersliceviews == false now
    UpdateViewports(Slices);    
}

void EspinaVolumeEditor::SagittalXYPick(unsigned long event)
{
    // static var stores data between calls
    static SliceVisualization::PickingType pickedProp = SliceVisualization::None;
    SliceVisualization::PickingType actualPick = SliceVisualization::None;
    
    // get coords
    int X = sagittalview->GetRenderWindow()->GetInteractor()->GetEventPosition()[0];
    int Y = sagittalview->GetRenderWindow()->GetInteractor()->GetEventPosition()[1];

    // we need to know first if we are picking something from the view
    actualPick = _sagittalSliceVisualization->GetPickPosition(&X, &Y);
    if (SliceVisualization::None == actualPick)
    {
      	// fix a glitch in rendering
      	if (vtkCommand::LeftButtonReleaseEvent == event)
      	{
      		updatevoxelrenderer = true;
            updateslicerenderers = true;
            UpdateViewports(All);
            pickedProp = SliceVisualization::None;
     	}
      	return;
    }
    
    // we need to know if we have started picking or we've picked something before
    if (SliceVisualization::None == pickedProp)
        pickedProp = actualPick;
    
    // if we were picking or painting but released the button, draw voxel view's final state
    if (vtkCommand::LeftButtonReleaseEvent == event)
    {
        updatevoxelrenderer = true;
        updateslicerenderers = true;

        if ((paintbutton->isChecked()) && (pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice))
        {
            _dataManager->OperationEnd();
            _volumeRender->UpdateFocusExtent();
            UpdateUndoRedoMenu();
        }
        _axesRender->Update(_POI);
        UpdateViewports(Voxel);
        
        pickedProp = SliceVisualization::None;
        return;
    }

    if (vtkCommand::LeftButtonPressEvent == event)
    {
        if ((paintbutton->isChecked()) && (pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice))
            _dataManager->OperationStart("Paint");
    }

    updatevoxelrenderer = false;
    updateslicerenderers = false;
    updatepointlabel = false;
    
    // updating slider positions updates POI 
    if (actualPick == pickedProp)
    {
        coronalslider->setSliderPosition(X+1);
        axialslider->setSliderPosition(Y+1);
        
        // move camera if we are picking the thumbnail & update thumbnail to reflect new position        
        if (actualPick == SliceVisualization::Thumbnail)
        {
            double coords[3];
            Vector3d spacing = _orientationData->GetImageSpacing();
            _sagittalViewRenderer->GetActiveCamera()->GetPosition(coords);
            _sagittalViewRenderer->GetActiveCamera()->SetPosition(X*spacing[1],Y*spacing[2],coords[2]);
            _sagittalViewRenderer->GetActiveCamera()->SetFocalPoint(X*spacing[1],Y*spacing[2],0.0);
            _sagittalSliceVisualization->ZoomEvent();
        }
    }

    // get pixel value
    SetPointLabel();
    updatepointlabel = true;

    if ((paintbutton->isChecked()) && (pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice))
        _dataManager->SetVoxelScalar(_POI[0], _POI[1], _POI[2],_selectedLabel);
    
    if ((selectbutton->isChecked()) && (pickedProp == SliceVisualization::Slice) && (actualPick == SliceVisualization::Slice))
    {
        cutbutton->setEnabled(true);
        relabelbutton->setEnabled(true);
        _editorOperations->AddSelectionPoint(Vector3ui(_POI[0], _POI[1], _POI[2]));
        _axialSliceVisualization->SetSelection(_editorOperations->GetSelection());
        _coronalSliceVisualization->SetSelection(_editorOperations->GetSelection());
        _sagittalSliceVisualization->SetSelection(_editorOperations->GetSelection());
    }
        
    // we will render the viewports as rendersliceviews == false now
    UpdateViewports(Slices);    
}

bool EspinaVolumeEditor::VolumeModified()
{
    return _acceptedChanges;
}

itk::SmartPointer<LabelMapType> EspinaVolumeEditor::GetOutput()
{
    if (_acceptedChanges == false)
        return NULL;

    return _editorOperations->GetImageLabelMap();
}

void EspinaVolumeEditor::SetInitialFreeValue(unsigned short value)
{
    _dataManager->SetFirstFreeValue(value);
}

unsigned short EspinaVolumeEditor::GetLastUsedScalarValue()
{
    if ((_acceptedChanges == false) || (!GetUserCreatedNewLabels()))
        return 0;

    return _dataManager->GetLastUsedValue();
}

double* EspinaVolumeEditor::GetRGBAColorFromValue(unsigned short value)
{
    if (_acceptedChanges == false)
        return NULL;

    return _dataManager->GetRGBAColorForScalar(value);
}

bool EspinaVolumeEditor::GetUserCreatedNewLabels()
{
    if ((_acceptedChanges == false) || (_dataManager->GetFirstFreeValue() == 1))
        return false;
    
    return (_dataManager->GetLastUsedValue() >= _dataManager->GetFirstFreeValue());
}

void EspinaVolumeEditor::ViewZoom(void)
{
    // static variable stores status between calls
    static bool zoomstatus = false;
    
    QToolButton *button = qobject_cast<QToolButton *>(sender());
    
    switch (zoomstatus)
    {
        case true:
            viewgrid->setColumnStretch(0,1);
            viewgrid->setColumnStretch(1,1);
            viewgrid->setRowStretch(0,1);
            viewgrid->setRowStretch(1,1);
            
            if (button == axialsizebutton)
            {
                button->setStatusTip(tr("Maximize Axial view"));
                button->setToolTip(tr("Maximize Axial view"));
            }
            else
            {
                axialview->show();
                axialresetbutton->show();
                axialsizebutton->show();
                axialslider->show();
            }

            if (button == sagittalsizebutton)
            {
                button->setStatusTip(tr("Maximize Sagittal view"));
                button->setToolTip(tr("Maximize Sagittal view"));
            }
            else
            {
                sagittalview->show();
                sagittalresetbutton->show();
                sagittalsizebutton->show();
                sagittalslider->show();
            }
            
            if (button == coronalsizebutton)
            {
                button->setStatusTip(tr("Maximize Coronal view"));
                button->setToolTip(tr("Maximize Coronal view"));
            }
            else
            {
                coronalview->show();
                coronalresetbutton->show();
                coronalsizebutton->show();
                coronalslider->show();
            }
            
            if (button == rendersizebutton)
            {
                button->setStatusTip(tr("Maximize render view"));
                button->setToolTip(tr("Maximize render view"));
            }
            else
            {
                renderview->show();
                renderbar->insertSpacerItem(2,renderspacer);    // spacers can't hide() or show(), it must be removed and inserted
                voxelresetbutton->show();
                rendersizebutton->show();
                axestypebutton->show();
                rendertypebutton->show();
                renderdisablebutton->show();
            }

            button->setIcon(QIcon(":/newPrefix/icons/tomax.png"));

            // we weren't updating the other view when zoomed, so we must update all views now.
            UpdateViewports(All);
            break;
        case false:
            if (button == axialsizebutton)
            {
                viewgrid->setColumnStretch(0,1);
                viewgrid->setColumnStretch(1,0);
                viewgrid->setRowStretch(0,0);
                viewgrid->setRowStretch(1,1);

                button->setStatusTip(tr("Minimize Axial view"));
                button->setToolTip(tr("Minimize Axial view"));
            }
            else
            {
                axialview->hide();
                axialresetbutton->hide();
                axialsizebutton->hide();
                axialslider->hide();
            }

            if (button == sagittalsizebutton)
            {
                viewgrid->setColumnStretch(0,0);
                viewgrid->setColumnStretch(1,1);
                viewgrid->setRowStretch(0,0);
                viewgrid->setRowStretch(1,1);

                button->setStatusTip(tr("Minimize Sagittal view"));
                button->setToolTip(tr("Minimize Sagittal view"));
            }
            else
            {
                sagittalview->hide();
                sagittalresetbutton->hide();
                sagittalsizebutton->hide();
                sagittalslider->hide();
            }
            
            if (button == coronalsizebutton)
            {
                viewgrid->setColumnStretch(0,0);
                viewgrid->setColumnStretch(1,1);
                viewgrid->setRowStretch(0,1);
                viewgrid->setRowStretch(1,0);

                button->setStatusTip(tr("Minimize Coronal view"));
                button->setToolTip(tr("Minimize Coronal view"));
            }
            else
            {
                coronalview->hide();
                coronalresetbutton->hide();
                coronalsizebutton->hide();
                coronalslider->hide();
            }
            
            if (button == rendersizebutton)
            {
                viewgrid->setColumnStretch(0,1);
                viewgrid->setColumnStretch(1,0);
                viewgrid->setRowStretch(0,1);
                viewgrid->setRowStretch(1,0);

                button->setStatusTip(tr("Minimize render view"));
                button->setToolTip(tr("Minimize render view"));
            }
            else
            {
                renderview->hide();
                renderbar->removeItem(renderspacer);    // spacers can't hide() or show(), it must be removed and added
                voxelresetbutton->hide();
                rendersizebutton->hide();
                axestypebutton->hide();
                rendertypebutton->hide();
                renderdisablebutton->hide();
            }

            button->setIcon(QIcon(":/newPrefix/icons/tomin.png"));
            break;
        default:
            // never happens
            break;
    }

    // slice thumbnail could have now different view limits as the view size has changed, update it  
    if (button == axialsizebutton)
        _axialSliceVisualization->ZoomEvent();
    
    if (button == coronalsizebutton)
        _coronalSliceVisualization->ZoomEvent();

    if (button == sagittalsizebutton)
        _sagittalSliceVisualization->ZoomEvent();
    
    repaint();
    zoomstatus = !zoomstatus;
}

void EspinaVolumeEditor::DisableRenderView(void)
{
	static bool disabled = false;
	disabled = !disabled;

	switch(disabled)
	{
		case true:
			renderview->setEnabled(false);
			_voxelViewRenderer->DrawOff();
            voxelresetbutton->setEnabled(false);
            rendersizebutton->setEnabled(false);
            axestypebutton->setEnabled(false);
            rendertypebutton->setEnabled(false);
			renderdisablebutton->setIcon(QIcon(":/newPrefix/icons/cog_add.png"));
            renderdisablebutton->setStatusTip(tr("Enable render view"));
            renderdisablebutton->setToolTip(tr("Enables the rendering view of the volume"));
			break;
		case false:
			renderview->setEnabled(true);
			_voxelViewRenderer->DrawOn();
            voxelresetbutton->setEnabled(true);
            rendersizebutton->setEnabled(true);
            axestypebutton->setEnabled(true);
            if (_selectedLabel != 0)
            	rendertypebutton->setEnabled(true);
			renderdisablebutton->setIcon(QIcon(":/newPrefix/icons/cog_delete.png"));
            renderdisablebutton->setStatusTip(tr("Disable render view"));
            renderdisablebutton->setToolTip(tr("Disables the rendering view of the volume"));
            UpdateViewports(Voxel);
			break;
		default:
			break;

	}
}
