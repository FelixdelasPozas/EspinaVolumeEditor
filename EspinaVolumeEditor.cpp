///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: EspinaVolumeEditor.cpp
// Purpose: Volume Editor Qt GUI class
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes 

// Qt
#include <QtGui>
#include <QMetaType>
#include <QFile>

// itk
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
#include <vtkImageFlip.h>
#include <vtkImageCast.h>
#include <vtkImageToStructuredPoints.h>
#include <vtkImageImport.h>
#include <vtkInformation.h>
#include <vtkImageChangeInformation.h>

// project includes
#include <EspinaVolumeEditor.h>
#include "DataManager.h"
#include "VoxelVolumeRender.h"
#include "Coordinates.h"
#include "VectorSpaceAlgebra.h"
#include "EditorOperations.h"
#include "QtAbout.h"
#include "QtPreferences.h"
#include "QtSessionInfo.h"
#include "QtKeyboardHelp.h"
#include "itkvtkpipeline.h"
#include "Selection.h"

using ImageType                 = itk::Image<unsigned short, 3>;
using ReaderType                = itk::ImageFileReader<ImageType>;
using ChangeInfoType            = itk::ChangeInformationImageFilter<ImageType>;
using ConverterType             = itk::LabelImageToLabelMapFilter<ImageType, LabelMapType>;
using LabelMapToImageFilterType = itk::LabelMapToLabelImageFilter<LabelMapType, ImageType>;
using ITKExport                 = itk::VTKImageExport<ImageType>;

///////////////////////////////////////////////////////////////////////////////////////////////////
EspinaVolumeEditor::EspinaVolumeEditor(QApplication *app, QWidget *parent)
: QMainWindow{parent}
, m_updateVoxelRenderer {false}
, m_updateSliceRenderers{false}
, m_updatePointLabel    {false}
, m_renderIsAVolume     {true}
, m_axialRenderer       {vtkSmartPointer<vtkRenderer>::New()}
, m_coronalRenderer     {vtkSmartPointer<vtkRenderer>::New()}
, m_sagittalRenderer    {vtkSmartPointer<vtkRenderer>::New()}
, m_volumeRenderer      {vtkSmartPointer<vtkRenderer>::New()}
, m_axialView           {std::make_shared<SliceVisualization>(SliceVisualization::Orientation::Axial)}
, m_coronalView         {std::make_shared<SliceVisualization>(SliceVisualization::Orientation::Coronal)}
, m_sagittalView        {std::make_shared<SliceVisualization>(SliceVisualization::Orientation::Sagittal)}
, m_volumeView          {nullptr}
, m_axesRender          {nullptr}
, m_orientationData     {nullptr}
, m_fileMetadata        {nullptr}
, m_saveSessionThread   {nullptr}
, m_dataManager         {std::make_shared<DataManager>()}
, m_editorOperations    {std::make_shared<EditorOperations>(m_dataManager)}
, m_progress            {std::make_shared<ProgressAccumulator>()}
, m_POI                 {Vector3ui{0,0,0}}
, m_pointScalar         {0}
, m_connections         {vtkSmartPointer<vtkEventQtSlotConnect>::New()}
, m_hasReferenceImage   {false}
, m_segmentationsVisible{true}
, m_saveSessionTime     {20 * 60 * 1000}
, m_saveSessionEnabled  {false}
, m_segmentationFileName{QString()}
, m_referenceFileName   {QString()}
, m_brushRadius         {1}
{
  setupUi(this); // this sets up GUI

  showMaximized();

  connectSignals();

  XspinBox->setReadOnly(false);
  XspinBox->setWrapping(false);
  XspinBox->setAccelerated(true);
  YspinBox->setReadOnly(false);
  YspinBox->setWrapping(false);
  YspinBox->setAccelerated(true);
  ZspinBox->setReadOnly(false);
  ZspinBox->setWrapping(false);
  ZspinBox->setAccelerated(true);

  // set selection mode
  labelselector->setSelectionMode(QAbstractItemView::ExtendedSelection);

  // disable unused widgets
  progressBar->hide();

  // initialize views
  auto axialinteractorstyle = vtkSmartPointer<vtkInteractorStyleImage>::New();
  axialinteractorstyle->AutoAdjustCameraClippingRangeOn();
  axialinteractorstyle->KeyPressActivationOff();
  m_axialRenderer->SetBackground(0, 0, 0);
  m_axialRenderer->GetActiveCamera()->SetParallelProjection(true);
  axialview->GetRenderWindow()->AddRenderer(m_axialRenderer);
  axialview->GetRenderWindow()->GetInteractor()->SetInteractorStyle(axialinteractorstyle);
  axialinteractorstyle->RemoveAllObservers();

  auto coronalinteractorstyle = vtkSmartPointer<vtkInteractorStyleImage>::New();
  coronalinteractorstyle->AutoAdjustCameraClippingRangeOn();
  coronalinteractorstyle->KeyPressActivationOff();
  m_coronalRenderer->SetBackground(0, 0, 0);
  m_coronalRenderer->GetActiveCamera()->SetParallelProjection(true);
  coronalview->GetRenderWindow()->AddRenderer(m_coronalRenderer);
  coronalview->GetRenderWindow()->GetInteractor()->SetInteractorStyle(coronalinteractorstyle);

  auto sagittalinteractorstyle = vtkSmartPointer<vtkInteractorStyleImage>::New();
  sagittalinteractorstyle->AutoAdjustCameraClippingRangeOn();
  sagittalinteractorstyle->KeyPressActivationOff();
  m_sagittalRenderer->SetBackground(0, 0, 0);
  m_sagittalRenderer->GetActiveCamera()->SetParallelProjection(true);
  sagittalview->GetRenderWindow()->AddRenderer(m_sagittalRenderer);
  sagittalview->GetRenderWindow()->GetInteractor()->SetInteractorStyle(sagittalinteractorstyle);

  auto voxelinteractorstyle = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
  voxelinteractorstyle->AutoAdjustCameraClippingRangeOn();
  voxelinteractorstyle->KeyPressActivationOff();
  m_volumeRenderer->SetBackground(0, 0, 0);
  renderview->GetRenderWindow()->AddRenderer(m_volumeRenderer);
  renderview->GetRenderWindow()->GetInteractor()->SetInteractorStyle(voxelinteractorstyle);

  // we need to go deeper than window interactor to get mouse release events, so instead connecting the
  // interactor we must connect the interactor-style because once the interactor gets a left click it
  // delegates the rest to the style so we don't get the mouse release event.
  m_connections->Connect(axialinteractorstyle, vtkCommand::LeftButtonPressEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(axialinteractorstyle, vtkCommand::LeftButtonReleaseEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(axialinteractorstyle, vtkCommand::RightButtonPressEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(axialinteractorstyle, vtkCommand::RightButtonReleaseEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(axialinteractorstyle, vtkCommand::MiddleButtonPressEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(axialinteractorstyle, vtkCommand::MiddleButtonReleaseEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(axialinteractorstyle, vtkCommand::MouseMoveEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(axialinteractorstyle, vtkCommand::MouseWheelForwardEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(axialinteractorstyle, vtkCommand::MouseWheelBackwardEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(coronalinteractorstyle, vtkCommand::LeftButtonPressEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(coronalinteractorstyle, vtkCommand::LeftButtonReleaseEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(coronalinteractorstyle, vtkCommand::RightButtonPressEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(coronalinteractorstyle, vtkCommand::RightButtonReleaseEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(coronalinteractorstyle, vtkCommand::MiddleButtonPressEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(coronalinteractorstyle, vtkCommand::MiddleButtonReleaseEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(coronalinteractorstyle, vtkCommand::MouseMoveEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(coronalinteractorstyle, vtkCommand::MouseWheelForwardEvent,
                          this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(coronalinteractorstyle, vtkCommand::MouseWheelBackwardEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(sagittalinteractorstyle, vtkCommand::LeftButtonPressEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(sagittalinteractorstyle, vtkCommand::LeftButtonReleaseEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(sagittalinteractorstyle, vtkCommand::RightButtonPressEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(sagittalinteractorstyle, vtkCommand::RightButtonReleaseEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(sagittalinteractorstyle, vtkCommand::MiddleButtonPressEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(sagittalinteractorstyle, vtkCommand::MiddleButtonReleaseEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(sagittalinteractorstyle, vtkCommand::MouseMoveEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(sagittalinteractorstyle, vtkCommand::MouseWheelForwardEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  m_connections->Connect(sagittalinteractorstyle, vtkCommand::MouseWheelBackwardEvent,
                         this, SLOT(sliceInteraction(vtkObject*, unsigned long, void*, void*, vtkCommand*)));

  axialview   ->installEventFilter(this);
  coronalview ->installEventFilter(this);
  sagittalview->installEventFilter(this);

  loadSettings();

  // initialize editor progress bar
  m_progress->SetProgressBar(progressBar);
  m_progress->Reset();

  // let's see if a previous session crashed
	auto homedir = QDir::tempPath();
	auto baseFilename = homedir + QString("/espinaeditor");
	auto temporalFilename = baseFilename + QString(".session");
	auto temporalFilenameMHA = baseFilename + QString(".mha");

	QFile file(temporalFilename);
	QFile fileMHA(temporalFilenameMHA);

	if (file.exists() && fileMHA.exists())
	{
		char *buffer;
		unsigned short int size;
		std::ifstream infile;
		auto detailedText = QString("Session segmentation file is:\n");

		// get the information about the file for the user
		infile.open(temporalFilename.toStdString().c_str(), std::ofstream::in|std::ofstream::binary);
		infile.read(reinterpret_cast<char*>(&size), sizeof(unsigned short int));
		buffer = (char*) malloc(size+1);
		infile.read(buffer, size);
		buffer[size] = '\0';
		auto segmentationFilename = QString(buffer);
		free(buffer);
		detailedText += segmentationFilename;
		infile.close();

		QMessageBox msgBox(this);
		msgBox.setWindowIcon(QIcon(":/newPrefix/icons/brain.png"));
		msgBox.setIcon(QMessageBox::Information);
		msgBox.setCaption("Previous session data detected");
		msgBox.setText("Data from a previous Editor session exists (maybe the editor crashed or didn't exit cleanly).");
		msgBox.setInformativeText("Do you want to restore that session?");
		msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.setDetailedText(detailedText);

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

		int returnValue = msgBox.exec();

	    switch (returnValue)
	    {
	    	case QMessageBox::Yes:
	    		restoreSavedSession();
	    		break;
	    	case QMessageBox::No:
	    		removeSessionFiles();
	    		break;
	    	default:
	    		break;
	    }
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
EspinaVolumeEditor::~EspinaVolumeEditor()
{
  removeSessionFiles();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::open()
{
  QFileDialog dialog(this, tr("Open Espina Segmentation Image"), QDir::currentPath(), QObject::tr("EspINA segmentation files (*.segmha)"));
  dialog.setOption(QFileDialog::DontUseNativeDialog, false);

  auto dialogSize = dialog.sizeHint();
  auto rect = this->rect();
  dialog.move(QPoint( rect.width()/2 - dialogSize.width()/2, rect.height()/2 - dialogSize.height()/2 ) );

  QString filename;

  if(!dialog.exec()) return;

  filename = dialog.selectedFile();

  if (filename.isNull()) return;

  filename.toAscii();

  renderview  ->setEnabled(true);
  axialview   ->setEnabled(true);
  sagittalview->setEnabled(true);
  coronalview ->setEnabled(true);

  QMutexLocker locker(&m_mutex);

  // store segmentation filename
  m_segmentationFileName = filename;
  m_referenceFileName = QString();

  // MetaImageIO needed to read an image without a estandar extension (segmha in this case)
  auto io = itk::MetaImageIO::New();
  io->SetFileName(filename.toStdString().c_str());
  auto reader = ReaderType::New();
  reader->SetImageIO(io);
  reader->SetFileName(filename.toStdString().c_str());
  reader->ReleaseDataFlagOn();

  try
  {
    reader->Update();
  }
  catch (itk::ExceptionObject & excp)
  {
    m_progress->ManualReset();

    QMessageBox msgBox(this);
    msgBox.setWindowIcon(QIcon(":/newPrefix/icons/brain.png"));
    msgBox.setCaption("Error loading segmentation file");
    msgBox.setIcon(QMessageBox::Critical);

    std::string text = std::string("An error occurred loading the segmentation file.\nThe operation has been aborted.");
    msgBox.setText(text.c_str());
    msgBox.setDetailedText(excp.what());

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

    msgBox.exec();
    return;
  }

  m_fileMetadata = std::make_shared<Metadata>();
  if (!m_fileMetadata->read(filename))
  {
    m_progress->ManualReset();

    QMessageBox msgBox(this);
    msgBox.setWindowIcon(QIcon(":/newPrefix/icons/brain.png"));
    msgBox.setCaption("Error loading segmentation file");
    msgBox.setIcon(QMessageBox::Critical);

    std::string text = std::string("An error occurred parsing the espina segmentation data from file \"");
    text += filename.toStdString();
    text += std::string("\".\nThe operation has been aborted.");
    msgBox.setText(text.c_str());

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

    msgBox.exec();
    return;
  }

  // clean all viewports
  m_volumeRenderer  ->RemoveAllViewProps();
  m_axialRenderer   ->RemoveAllViewProps();
  m_sagittalRenderer->RemoveAllViewProps();
  m_coronalRenderer ->RemoveAllViewProps();

  // do not update the viewports while loading
  m_updateVoxelRenderer  = false;
  m_updateSliceRenderers = false;
  m_updatePointLabel     = false;

  // have to check first if we have an ongoing session, and preserve preferences (have to change this some day
  // to save preferences as global, not per class instance)
  m_orientationData = nullptr;

  if (m_sagittalView)
  {
    auto opacity = m_sagittalView->segmentationOpacity();
    m_sagittalView = std::make_shared<SliceVisualization>(SliceVisualization::Orientation::Sagittal);
    m_sagittalView->setSegmentationOpacity(opacity);
  }

  if (m_coronalView)
  {
    auto opacity = m_coronalView->segmentationOpacity();
    m_coronalView = std::make_shared<SliceVisualization>(SliceVisualization::Orientation::Coronal);
    m_coronalView->setSegmentationOpacity(opacity);
  }

  if (m_axialView)
  {
    auto opacity = m_axialView->segmentationOpacity();
    m_axialView = std::make_shared<SliceVisualization>(SliceVisualization::Orientation::Axial);
    m_axialView->setSegmentationOpacity(opacity);
  }

  m_axesRender = nullptr;
  m_volumeView = nullptr;

  if (m_dataManager)
  {
    auto size = m_dataManager->GetUndoRedoBufferSize();
    m_dataManager = std::make_shared<DataManager>();
    m_dataManager->SetUndoRedoBufferSize(size);
  }

  if (m_editorOperations)
  {
    auto radius = m_editorOperations->GetFiltersRadius();
    auto level = m_editorOperations->GetWatershedLevel();
    m_editorOperations = std::make_shared<EditorOperations>(m_dataManager);
    m_editorOperations->SetFiltersRadius(radius);
    m_editorOperations->SetWatershedLevel(level);
  }

  // here we go after file read:
  // itkimage(unsigned short,3) -> itklabelmap -> itkimage-> vtkimage -> vtkstructuredpoints
  m_progress->ManualSet("Load");

  // get image orientation data
  m_orientationData = std::make_shared<Coordinates>(reader->GetOutput());

  auto infoChanger = ChangeInfoType::New();
  infoChanger->SetInput(reader->GetOutput());
  infoChanger->ReleaseDataFlagOn();
  infoChanger->ChangeOriginOn();
  infoChanger->ReleaseDataFlagOn();
  ImageType::PointType newOrigin;
  newOrigin[0] = 0.0;
  newOrigin[1] = 0.0;
  newOrigin[2] = 0.0;
  infoChanger->SetOutputOrigin(newOrigin);
  m_progress->Observe(infoChanger, "Fix Image", 0.14);
  infoChanger->Update();
  m_progress->Ignore(infoChanger);

  // itkimage->itklabelmap
  auto converter = ConverterType::New();
  converter->SetInput(infoChanger->GetOutput());
  converter->ReleaseDataFlagOn();
  m_progress->Observe(converter, "Label Map", 0.14);
  converter->Update();
  m_progress->Ignore(converter);
  converter->GetOutput()->Optimize();
  assert(0 != converter->GetOutput()->GetNumberOfLabelObjects());

  // flatten labelmap, modify origin and store scalar label values
  m_dataManager->Initialize(converter->GetOutput(), m_orientationData, m_fileMetadata);

  // check if there are unused objects
  m_fileMetadata->compact();

  auto unusedLabels = m_fileMetadata->unusedLabels();
  if (0 != unusedLabels.size())
  {
    QMessageBox msgBox(this);
    msgBox.setWindowIcon(QIcon(":/newPrefix/icons/brain.png"));
    msgBox.setCaption("Unused objects detected");
    msgBox.setIcon(QMessageBox::Warning);

    QApplication::restoreOverrideCursor();

    auto text = QString("The segmentation contains unused objects (with no voxels assigned).\nThose objects will be discarded.\n");
    msgBox.setText(text);
    auto details = QString("Unused objects:\n");
    for (auto label: unusedLabels)
    {
      details += QString("label %1").arg(label) + QString("\n");
    }
    msgBox.setDetailedText(details);

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

    msgBox.exec();

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  }

  // itklabelmap->itkimage
  auto labelconverter = LabelMapToImageFilterType::New();
  labelconverter->SetInput(m_dataManager->GetLabelMap());
  labelconverter->SetNumberOfThreads(1); // if number of threads > 1 filter crashes (¿¿??)
  labelconverter->ReleaseDataFlagOn();

  m_progress->Observe(labelconverter, "Convert Image", 0.14);
  labelconverter->Update();
  m_progress->Ignore(labelconverter);

  // itkimage->vtkimage
  auto itkExporter = ITKExport::New();
  auto vtkImporter = vtkSmartPointer<vtkImageImport>::New();
  itkExporter->SetInput(labelconverter->GetOutput());
  ConnectPipelines(itkExporter, vtkImporter);
  m_progress->Observe(vtkImporter, "Import", 0.14);
  m_progress->Observe(itkExporter, "Export", 0.14);
  vtkImporter->Update();
  m_progress->Ignore(itkExporter);
  m_progress->Ignore(vtkImporter);

  // get the vtkStructuredPoints out of the vtkImage
  auto convert = vtkSmartPointer<vtkImageToStructuredPoints>::New();
  convert->SetInputData(vtkImporter->GetOutput());
  convert->ReleaseDataFlagOn();
  m_progress->Observe(convert, "Convert Points", 0.14);
  convert->Update();
  m_progress->Ignore(convert);

  // now we have our structuredpoints
  m_dataManager->SetStructuredPoints(convert->GetStructuredPointsOutput());

  // gui setup
  initializeGUI();

  // initially without a reference image
  m_hasReferenceImage = false;

  // start session timer
  if (m_saveSessionEnabled)
  {
    m_sessionTimer.start(m_saveSessionTime, true);
  }

  // put the name of the opened file in the window title
  auto caption = QString("Espina Volume Editor - ") + filename;
  setCaption(QString(caption));

  // get the working set of labels for this file, if exists.
  // change the disallowed chars first in the filename, hope it doesn't collide with another file
  filename.replace(QChar('/'), QChar('\\'));
  QSettings editorSettings("UPM", "Espina Volume Editor");
  editorSettings.beginGroup("UserData");

  // get working set of labels for this file and apply it, if it exists
  // NOTE: Qlist<qvariant> to std::set<label scalars> to std::set<label indexes> and set the last one as selected the selected labels set
  if ((editorSettings.value(filename).isValid()) && (true == editorSettings.contains(filename)))
  {
    auto labelList = editorSettings.value(filename).toList();

    std::set<unsigned short> labelScalars;
    for (auto label: labelList)
    {
      labelScalars.insert(static_cast<unsigned short>(label.toUInt()));
    }

    std::set<unsigned short> labelIndexes;
    for (auto index: labelIndexes)
    {
      labelIndexes.insert(m_dataManager->GetLabelForScalar(index));
    }

    // make sure is a valid group of labels (deleting invalid labels)
    for (auto label: labelScalars)
    {
      if (label > m_dataManager->GetNumberOfLabels())
      {
        labelScalars.erase(label);
      }
    }
    selectLabels(labelIndexes);
  }

  m_progress->ManualReset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::referenceOpen()
{
  QFileDialog dialog(this, tr("Open Reference Image"), QDir::currentPath(), QObject::tr("image files (*.mhd *.mha);;All files (*.*)"));
  dialog.setOption(QFileDialog::DontUseNativeDialog, false);

  auto dialogSize = dialog.sizeHint();
  auto rect = this->rect();
  dialog.move(QPoint( rect.width()/2 - dialogSize.width()/2, rect.height()/2 - dialogSize.height()/2 ) );

  QString filename;

  if(!dialog.exec()) return;

  filename = dialog.selectedFile();

  if (filename.isNull()) return;

  filename.toAscii();
  loadReferenceFile(filename);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::loadReferenceFile(const QString &filename)
{
  // store reference filename
  m_referenceFileName = filename;

  auto reader = vtkSmartPointer<vtkMetaImageReader>::New();
  reader->SetFileName(filename.toStdString().c_str());

  try
  {
    reader->Update();
  }
  catch (...)
  {
    QMessageBox msgBox(this);
    msgBox.setWindowIcon(QIcon(":/newPrefix/icons/brain.png"));
    msgBox.setCaption("Error loading reference file");
    msgBox.setIcon(QMessageBox::Critical);

    auto text = QString("An error occurred loading the segmentation reference file.\nThe operation has been aborted.");
    msgBox.setText(text);

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

    msgBox.exec();
    return;
  }

  m_progress->ManualSet("Load");

  // don't ask me why but espina segmentation and reference image have different
  // orientation, to match both images we'll have to flip the volume in the Y and Z
  // axis, but preserving the image extent
  auto imageflipY = vtkSmartPointer<vtkImageFlip>::New();
  imageflipY->SetInputData(reader->GetOutput());
  imageflipY->SetFilteredAxis(1);
  imageflipY->PreserveImageExtentOn();
  m_progress->Observe(imageflipY, "Flip Y Axis", (1.0 / 4.0));
  imageflipY->Update();
  m_progress->Ignore(imageflipY);

  auto imageflipZ = vtkSmartPointer<vtkImageFlip>::New();
  imageflipZ->SetInputData(imageflipY->GetOutput());
  imageflipZ->SetFilteredAxis(2);
  imageflipZ->PreserveImageExtentOn();
  m_progress->Observe(imageflipZ, "Flip Z Axis", (1.0 / 4.0));
  imageflipZ->Update();
  m_progress->Ignore(imageflipZ);

  auto image = vtkSmartPointer<vtkImageData>::New();
  image = imageflipZ->GetOutput();

  // need to check that segmentation image and reference image have the same origin, size, spacing and direction
  int size[3];
  image->GetDimensions(size);
  auto segmentationsize = m_orientationData->GetImageSize();
  if (segmentationsize != Vector3ui(size[0], size[1], size[2]))
  {
    m_progress->ManualReset();
    QMessageBox msgBox(this);
    msgBox.setWindowIcon(QIcon(":/newPrefix/icons/brain.png"));
    msgBox.setCaption("Segmentation size mismatch");
    msgBox.setIcon(QMessageBox::Critical);

    auto text = QString("Reference and segmentation images have different dimensions.\n");
    text += QString("Reference size is [%1, %2, %3]\n").arg(size[0]).arg(size[1]).arg(size[2]);
    text += QString("Segmentation size is [%1, %2, %3]\n").arg(segmentationsize[0]).arg(segmentationsize[1]).arg(segmentationsize[2]);
    text += QString("The operation has been aborted.");
    msgBox.setText(text);

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

    msgBox.exec();
    return;
  }

  double origin[3];
  image->GetOrigin(origin);
  auto segmentationorigin = m_orientationData->GetImageOrigin();
  if (segmentationorigin != Vector3d(origin[0], origin[1], origin[2]))
  {
    QApplication::restoreOverrideCursor();

    QMessageBox msgBox(this);
    msgBox.setWindowIcon(QIcon(":/newPrefix/icons/brain.png"));
    msgBox.setCaption("Segmentation origin mismatch");
    msgBox.setIcon(QMessageBox::Warning);

    auto text = QString("Reference and segmentation images have different origin of coordinates.\n");
    text += QString("Reference origin is [%1, %2, %3]\n").arg(origin[0]).arg(origin[1]).arg(origin[2]);
    text += QString("Segmentation origin is [%1, %2, %3]\n").arg(segmentationorigin[0]).arg(segmentationorigin[1]).arg(segmentationorigin[2]);
    text += QString("Editor will use segmentation origin.");
    msgBox.setText(text);

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

    msgBox.exec();

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  }

  double spacing[3];
  image->GetSpacing(spacing);
  auto segmentationspacing = m_orientationData->GetImageSpacing();
  if (segmentationspacing != Vector3d(spacing[0], spacing[1], spacing[2]))
  {
    QApplication::restoreOverrideCursor();

    QMessageBox msgBox(this);
    msgBox.setWindowIcon(QIcon(":/newPrefix/icons/brain.png"));
    msgBox.setCaption("Segmentation spacing mismatch");
    msgBox.setIcon(QMessageBox::Warning);

    auto text = QString("Reference and segmentation images have different point spacing.\n");
    text += QString("Reference spacing is [%1, %2, %3]\n").arg(spacing[0]).arg(spacing[1]).arg(spacing[2]);
    text += QString("Segmentation spacing is [%1, %2, %3]\n").arg(segmentationspacing[0]).arg(segmentationspacing[1]).arg(segmentationspacing[2]);
    text += QString("Editor will use segmentation spacing for both.");
    msgBox.setText(text);

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

    msgBox.exec();

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  }

  auto changer = vtkSmartPointer<vtkImageChangeInformation>::New();
  changer->SetInputData(image);

  if (segmentationspacing != Vector3d(spacing[0], spacing[1], spacing[2]))
  {
    changer->SetOutputSpacing(segmentationspacing[0], segmentationspacing[1], segmentationspacing[2]);
  }

  changer->SetOutputOrigin(0.0, 0.0, 0.0);
  changer->ReleaseDataFlagOn();

  m_progress->Observe(changer, "Fix Image", (1.0 / 4.0));
  changer->Update();
  m_progress->Ignore(changer);

  auto convert = vtkSmartPointer<vtkImageToStructuredPoints>::New();
  convert->SetInputData(changer->GetOutput());
  m_progress->Observe(convert, "Convert", (1.0 / 4.0));
  convert->Update();
  m_progress->Ignore(convert);

  auto structuredPoints = vtkSmartPointer<vtkStructuredPoints>::New();
  structuredPoints = convert->GetStructuredPointsOutput();
  structuredPoints->Modified();

  // now that we have a reference image make the background of the segmentation completely transparent
  auto color = QColor::fromRgbF(0,0,0,0);
  m_dataManager->SetColorComponents(0, color);

  // pass reference image to slice visualization
  m_axialView->setReferenceImage(structuredPoints);
  m_coronalView->setReferenceImage(structuredPoints);
  m_sagittalView->setReferenceImage(structuredPoints);
  updateViewports(ViewPorts::Slices);

  // NOTE: structuredPoints pointer is not stored so when this method ends just the slices have a reference
  // to the data, if a new reference image is loaded THEN it's memory gets freed as there are not more
  // references to it in memory. It will also be freed if the three slices are destroyed when loading a new
  // segmentation file.
  m_hasReferenceImage = true;

  // reset editing state
  viewbutton->setChecked(true);

  // enable segmentation switch views
  m_segmentationsVisible = true;
  eyebutton->setIcon(QPixmap(":/newPrefix/icons/eyeoff.svg"));
  eyebutton->setToolTip(tr("Hide all segmentations"));
  eyebutton->setStatusTip(tr("Hide all segmentations"));
  eyelabel->setText(tr("Hide objects"));
  eyelabel->setToolTip(tr("Hide all segmentations"));
  eyelabel->setStatusTip(tr("Hide all segmentations"));
  eyebutton->setEnabled(true);
  eyelabel->setEnabled(true);
  a_hide_segmentations->setEnabled(true);
  a_hide_segmentations->setText(tr("Hide Segmentations"));
  a_hide_segmentations->setIcon(QPixmap(":/newPrefix/icons/eyeoff.svg"));

  m_progress->ManualReset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::save()
{
  QMutexLocker locker(&m_mutex);

  QFileDialog dialog(this, tr("Save Segmentation Image"), QDir::currentPath(), QObject::tr("label image files (*.segmha)"));
  dialog.setOption(QFileDialog::DontUseNativeDialog, false);

  auto dialogSize = dialog.sizeHint();
  auto rect = this->rect();
  dialog.move(QPoint( rect.width()/2 - dialogSize.width()/2, rect.height()/2 - dialogSize.height()/2 ) );

  QString filename;

  if(!dialog.exec()) return;

  filename = dialog.selectedFile();

  if (filename.isNull()) return;

  filename.toAscii();
  auto filenameStd = filename.toStdString();
  std::size_t found;

  // check if user entered file extension "segmha" or not, if not just add it
  if (std::string::npos == (found = filenameStd.rfind(".segmha")))
  {
    filenameStd += std::string(".segmha");
  }

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  m_editorOperations->SaveImage(filenameStd);

  if (!m_fileMetadata->write(QString(filenameStd.c_str()), m_dataManager))
  {
    QMessageBox msgBox(this);
    msgBox.setWindowIcon(QIcon(":/newPrefix/icons/brain.png"));
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setCaption("Error saving segmentation file");

    auto text = QString("An error occurred saving the segmentation metadata to file \"");
    text += QString(filenameStd.c_str());
    text += QString("\".\nThe segmentation data has been saved, but the metadata has not.\nThe file could be unusable.");
    msgBox.setText(text);

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

    msgBox.exec();
  }

  // save the set of labels as settings, not the indexes but the scalars
  QSettings editorSettings("UPM", "Espina Volume Editor");
  QString filenameQt(filenameStd.c_str());
  filenameQt.replace(QChar('/'), QChar('\\'));
  editorSettings.beginGroup("UserData");

  auto labelIndexes = m_dataManager->GetSelectedLabelsSet();
  std::set<unsigned short> labelScalars;

  for (auto index: labelIndexes)
  {
    labelScalars.insert(m_dataManager->GetScalarForLabel(index));
  }

  QList<QVariant> labelList;
  for (auto scalar: labelScalars)
  {
    labelList.append(static_cast<int>(scalar));
  }

  QVariant variant;
  variant.setValue(labelList);
  editorSettings.setValue(filenameQt, variant);
  editorSettings.sync();

  m_segmentationFileName = filename;

  // put the name of the opened file in the window title
  auto caption = QString("Espina Volume Editor - ") + m_segmentationFileName;
  setCaption(QString(caption));

  // after saving the segmentation file we don't need the (actually) stored session files, remove them
  // and reset the timer
  if (m_saveSessionEnabled)
  {
    removeSessionFiles();
    m_sessionTimer.stop();
    m_sessionTimer.start(m_saveSessionTime, true);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::exit()
{
  QApplication::exit();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::fullscreenToggle()
{
  auto action = qobject_cast<QAction *>(sender());

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

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::onAxialSliderModified(int value)
{
  if (!axialslider->isEnabled()) return;

  ZspinBox->setValue(value);

  // slider values are in the range [1-size] but coordinates are [0-(slices-1)]
  value--;
  m_POI[2] = value;
  if (m_updatePointLabel) updatePointLabel();

  m_sagittalView->updateCrosshair(m_POI);
  m_coronalView->updateCrosshair(m_POI);
  m_axialView->updateSlice(m_POI);
  m_editorOperations->UpdateContourSlice(m_POI);

  if (m_updateSliceRenderers)
  {
    updateViewports(ViewPorts::Slices);
  }

  if (m_updateVoxelRenderer)
  {
    m_axesRender->Update(m_POI);
    if (m_axesRender->isVisible())
    {
      updateViewports(ViewPorts::Render);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::onCoronalSliderModified(int value)
{
  if (!coronalslider->isEnabled()) return;

  YspinBox->setValue(value);

  // slider values are in the range [1-size] but coordinates are [0-(slices-1)]
  value--;
  m_POI[1] = value;
  if (m_updatePointLabel) updatePointLabel();

  m_sagittalView->updateCrosshair(m_POI);
  m_coronalView->updateSlice(m_POI);
  m_axialView->updateCrosshair(m_POI);
  m_editorOperations->UpdateContourSlice(m_POI);

  if (m_updateSliceRenderers)
  {
    updateViewports(ViewPorts::Slices);
  }

  if (m_updateVoxelRenderer)
  {
    m_axesRender->Update(m_POI);
    if (m_axesRender->isVisible())
    {
      updateViewports(ViewPorts::Render);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::onSagittalSliderModified(int value)
{
  if (!sagittalslider->isEnabled()) return;

  XspinBox->setValue(value);

  // slider values are in the range [1-size] but coordinates are [0-(slices-1)]
  value--;
  m_POI[0] = value;
  if (m_updatePointLabel) updatePointLabel();

  m_sagittalView->updateSlice(m_POI);
  m_coronalView->updateCrosshair(m_POI);
  m_axialView->updateCrosshair(m_POI);
  m_editorOperations->UpdateContourSlice(m_POI);

  if (m_updateSliceRenderers)
  {
    updateViewports(ViewPorts::Slices);
  }

  if (m_updateVoxelRenderer)
  {
    m_axesRender->Update(m_POI);
    if (m_axesRender->isVisible())
    {
      updateViewports(ViewPorts::Render);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::onSliderPressed(void)
{
  // continous rendering of the render view would hog the system, so we disable it
  // while the user moves the slider. Once the slider is released, we will render
  // the view's final state
  m_updateVoxelRenderer = false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::onSliderReleased(void)
{
  m_updateVoxelRenderer = true;
  m_axesRender->Update(m_POI);
  updateViewports(ViewPorts::Render);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::onSpinBoxXModified(int value)
{
  // sagittal slider is in charge of updating things
  sagittalslider->setSliderPosition(value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::onSpinBoxYModified(int value)
{
  // sagittal slider is in charge of updating things
  coronalslider->setSliderPosition(value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::onSpinBoxZModified(int value)
{
  // sagittal slider is in charge of updating things
  axialslider->setSliderPosition(value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::updatePointLabel()
{
  // get pixel value
  m_pointScalar = m_dataManager->GetVoxelScalar(m_POI);

  if (0 == m_pointScalar)
  {
    pointlabelnumber->setText(" Background");
    pointlabelcolor->setText(" None");
    pointlabelname->setText(" None");
    return;
  }

  auto color = m_dataManager->GetColorComponents(m_pointScalar);

  // we need to use double vales to build icon as some colours are too close
  // and could get identical if we use the values converted to int.
  QPixmap icon(32, 16);
  icon.fill(color);

  auto labelindex = m_dataManager->GetScalarForLabel(m_pointScalar);
  pointlabelnumber->setText(QString().number(labelindex));
  pointlabelcolor->setPixmap(icon);

  pointlabelname->setText(m_fileMetadata->objectSegmentName(m_pointScalar).c_str());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::fillColorLabels()
{
  // we need to disable it to avoid sending signals while updating
  labelselector->blockSignals(true);
  labelselector->clear();

  // first insert background label
  QListWidgetItem *newItem = new QListWidgetItem;
  newItem->setText("Background");
  labelselector->insertItem(0, newItem);

  // iterate over the colors to fill the table
  for (unsigned int i = 1; i < m_dataManager->GetNumberOfColors(); ++i)
  {
    QPixmap icon(16, 16);
    auto color = m_dataManager->GetColorComponents(i);
    color.setAlphaF(1);
    icon.fill(color);

    std::stringstream out;
    out << m_fileMetadata->objectSegmentName(i) << " " << m_dataManager->GetScalarForLabel(i);
    newItem = new QListWidgetItem(QIcon(icon), QString(out.str().c_str()));
    labelselector->insertItem(i, newItem);

    if (0LL == m_dataManager->GetNumberOfVoxelsForLabel(i))
    {
      labelselector->item(i)->setHidden(true);
      labelselector->item(i)->setSelected(false);
    }
  }

  // select the selected labels in the qlistwidget or, if the set is empty, select the background label
  auto labelSet = m_dataManager->GetSelectedLabelsSet();
  for (auto label: labelSet)
  {
    labelselector->item(label)->setSelected(true);
  }

  // if the set of selected labels are empty then the background label is the one selected
  if (labelSet.empty())
  {
    labelselector->item(0)->setSelected(true);
  }

  labelselector->blockSignals(false);
  labelselector->setEnabled(true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::onLabelSelectionInteraction(QListWidgetItem *unused1, QListWidgetItem *unused2)
{
  if (wandButton->isChecked()) viewbutton->setChecked(true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::onSelectionChanged()
{
  if (!labelselector->isEnabled()) return;

  labelselector->blockSignals(true);

  // get the selected items group in the labelselector widget and get their indexes
  std::set<unsigned short> labelsList;
  auto selectedItems = labelselector->selectedItems();

  for (auto item: selectedItems)
  {
    // we need to deselect the background label or, if it's left as selected, will interfere
    // with the rest of the function
    if (0 == labelselector->row(item))
    {
      labelselector->item(0)->setSelected(false);
      continue;
    }

    labelsList.insert(static_cast<unsigned short>(labelselector->row(item)));
  }

  // modify views according to selected group of labels
  switch (labelsList.size())
  {
    case 0: // background label selected or no label selected
      labelselector->clearSelection();
      labelselector->item(0)->setSelected(true);
      m_dataManager->ColorDimAll();
      m_volumeView->colorDimAll();
      break;
    case 1: // single or extended selection, but only one label. brackets needed because of declaration of the iterator
    {
      auto it = labelsList.begin();
      m_dataManager->ColorHighlightExclusive(*it);
      m_volumeView->colorHighlightExclusive(*it);
      labelselector->setCurrentItem(labelselector->item(*it), QItemSelectionModel::ClearAndSelect);
      break;
    }
    default: // multiple labels selection
    {
      // clear the labels not present in the actual selection and highlight labels present in the actual avoiding
      // highlighting/dimmng the ones already present in both groups
      auto selectedLabels = m_dataManager->GetSelectedLabelsSet();
      for (auto label: selectedLabels)
      {
        if (labelsList.find(label) == labelsList.end())
        {
          m_dataManager->ColorDim(label);
          m_volumeView->colorDim(label);
        }
      }

      for (auto label: labelsList)
      {
        if (selectedLabels.find(label) == selectedLabels.end())
        {
          m_dataManager->ColorHighlight(label);
          m_volumeView->colorHighlight(label);
        }
      }
      break;
    }
  }
  labelselector->blockSignals(false);

  // adjust interface according to the selected group of labels
  switch (labelsList.size())
  {
    case 0:
    {
      bool selectionCanRelabel = (selectbutton->isChecked() && (Selection::Type::CUBE == m_editorOperations->GetSelectionType()))
          || (lassoButton->isChecked() && (Selection::Type::CONTOUR == m_editorOperations->GetSelectionType()));

      cutbutton->setEnabled(false);
      if (renderview->isEnabled()) rendertypebutton->setEnabled(false);
      relabelbutton->setEnabled(selectionCanRelabel);
      enableOperations(false);
      break;
    }
    case 1:
      cutbutton->setEnabled(true);
      rendertypebutton->setEnabled(renderview->isEnabled());
      relabelbutton->setEnabled(true);
      enableOperations(!wandButton->isChecked() && !lassoButton->isChecked());
      break;
    default:
      cutbutton->setEnabled(true);
      rendertypebutton->setEnabled(renderview->isEnabled());
      relabelbutton->setEnabled(true);
      enableOperations(false);
      break;
  }

  m_volumeView->updateColorTable();
  m_volumeView->updateFocusExtent();

  // if we have only one segmentation selected then center the slice views over the centroid of the segmentation,
  // but only if the user is not picking colours, selecting a box, erasing or painting.
  if ((m_dataManager->GetSelectedLabelSetSize() == 1) && viewbutton->isChecked())
  {
    std::set<unsigned short>::iterator it = m_dataManager->GetSelectedLabelsSet().begin();
    if (0LL != m_dataManager->GetNumberOfVoxelsForLabel(*it))
    {
      // center slice views in the centroid of the object
      auto newPOI = m_dataManager->GetCentroidForObject(*it);

      // to prevent unwanted updates to the view it's not enough to blocksignals() of the
      // involved qt elements
      m_updateSliceRenderers = false;
      m_updateVoxelRenderer  = false;
      m_updatePointLabel     = false;

      // POI values start in 0, spinboxes start in 1
      m_POI[0] = static_cast<unsigned int>(newPOI[0]);
      m_POI[1] = static_cast<unsigned int>(newPOI[1]);
      m_POI[2] = static_cast<unsigned int>(newPOI[2]);
      ZspinBox->setValue(m_POI[2] + 1);
      YspinBox->setValue(m_POI[1] + 1);
      XspinBox->setValue(m_POI[0] + 1);

      m_sagittalView->update(m_POI);
      m_coronalView->update(m_POI);
      m_axialView->update(m_POI);
      m_axesRender->Update(m_POI);
      updatePointLabel();

      // change slice view to the new label
      auto spacing = m_orientationData->GetImageSpacing();
      double coords[3];
      m_axialRenderer->GetActiveCamera()->GetPosition(coords);
      m_axialRenderer->GetActiveCamera()->SetPosition(m_POI[0] * spacing[0], m_POI[1] * spacing[1], coords[2]);
      m_axialRenderer->GetActiveCamera()->SetFocalPoint(m_POI[0] * spacing[0], m_POI[1] * spacing[1], 0.0);
      m_axialView->zoomEvent();
      m_coronalRenderer->GetActiveCamera()->GetPosition(coords);
      m_coronalRenderer->GetActiveCamera()->SetPosition(m_POI[0] * spacing[0], m_POI[2] * spacing[2], coords[2]);
      m_coronalRenderer->GetActiveCamera()->SetFocalPoint(m_POI[0] * spacing[0], m_POI[2] * spacing[2], 0.0);
      m_coronalView->zoomEvent();
      m_sagittalRenderer->GetActiveCamera()->GetPosition(coords);
      m_sagittalRenderer->GetActiveCamera()->SetPosition(m_POI[1] * spacing[1], m_POI[2] * spacing[2], coords[2]);
      m_sagittalRenderer->GetActiveCamera()->SetFocalPoint(m_POI[1] * spacing[1], m_POI[2] * spacing[2], 0.0);
      m_sagittalView->zoomEvent();

      m_updatePointLabel = true;
      m_updateSliceRenderers = true;
      m_updateVoxelRenderer = true;
    }
  }

  m_axialView->updateSlice(m_POI);
  m_coronalView->updateSlice(m_POI);
  m_sagittalView->updateSlice(m_POI);
  updateViewports(ViewPorts::All);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::preferences()
{
  QtPreferences configdialog(this);

  configdialog.SetInitialOptions(m_dataManager->GetUndoRedoBufferSize(),
                                 m_dataManager->GetUndoRedoBufferCapacity(),
                                 m_editorOperations->GetFiltersRadius(),
                                 m_editorOperations->GetWatershedLevel(),
                                 m_axialView->segmentationOpacity()*100,
                                 m_saveSessionTime,
                                 m_saveSessionEnabled,
                                 m_brushRadius);

  if (m_hasReferenceImage)
  {
    configdialog.enableVisualizationBox();
  }

  configdialog.exec();

  if (!configdialog.isModified()) return;

  // save settings
  QSettings editorSettings("UPM", "Espina Volume Editor");
  editorSettings.beginGroup("Editor");
  editorSettings.setValue("UndoRedo System Buffer Size", static_cast<unsigned long long>(configdialog.size()));
  editorSettings.setValue("Filters Radius", configdialog.radius());
  editorSettings.setValue("Watershed Flood Level", configdialog.level());
  editorSettings.setValue("Segmentation Opacity", configdialog.opacity());
  editorSettings.setValue("Paint-Erase Radius", configdialog.brushRadius());
  editorSettings.setValue("Autosave Session Data", configdialog.isAutoSaveEnabled());
  editorSettings.setValue("Autosave Session Time", configdialog.autoSaveInterval());
  editorSettings.sync();

  // configure editor
  m_editorOperations->SetFiltersRadius(configdialog.radius());
  m_editorOperations->SetWatershedLevel(configdialog.level());
  m_dataManager->SetUndoRedoBufferSize(configdialog.size());
  m_brushRadius = configdialog.brushRadius();

  if (m_saveSessionTime != (configdialog.autoSaveInterval() * 60 * 1000))
  {
    // time for saving session data changed, just update the timer
    m_saveSessionTime = configdialog.autoSaveInterval() * 60 * 1000;
    m_sessionTimer.changeInterval(m_saveSessionTime);
  }

  if (!configdialog.isAutoSaveEnabled())
  {
    m_saveSessionEnabled = false;
    m_sessionTimer.stop();
  }
  else
  {
    m_saveSessionEnabled = true;
    if (!m_sessionTimer.isActive() && (!m_segmentationFileName.isEmpty()))
    {
      m_sessionTimer.start(m_saveSessionTime, true);
    }
  }

  // the undo/redo system size could have been modified and then some actions could have been deleted
  updateUndoRedoMenu();

  if (true == m_hasReferenceImage)
  {
    auto opacity = configdialog.opacity()/100.0;
    m_axialView->setSegmentationOpacity(opacity);
    m_sagittalView->setSegmentationOpacity(opacity);
    m_coronalView->setSegmentationOpacity(opacity);

    m_axialView->updateActors();
    m_coronalView->updateActors();
    m_sagittalView->updateActors();

    // the visualization options could have been modified so we must update the slices
    updateViewports(ViewPorts::Slices);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::resetViews()
{
  auto button = qobject_cast<QToolButton *>(sender());

  if (button == axialresetbutton)
  {
    m_axialRenderer->ResetCamera();
    m_axialView->zoomEvent();
    updateViewports(ViewPorts::Axial);
  }

  if (button == coronalresetbutton)
  {
    m_coronalRenderer->ResetCamera();
    m_coronalView->zoomEvent();
    updateViewports(ViewPorts::Coronal);
  }

  if (button == sagittalresetbutton)
  {
    m_sagittalRenderer->ResetCamera();
    m_sagittalView->zoomEvent();
    updateViewports(ViewPorts::Sagittal);
  }

  if (button == voxelresetbutton)
  {
    m_volumeRenderer->ResetCamera();
    updateViewports(ViewPorts::Render);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::renderTypeSwitch()
{
  if (!renderview->isEnabled()) return;

  if (m_renderIsAVolume)
  {
    m_volumeView->viewAsMesh();
    rendertypebutton->setIcon(QIcon(":/newPrefix/icons/voxel.png"));
    rendertypebutton->setToolTip(tr("Switch to volume renderer"));
  }
  else
  {
    m_volumeView->viewAsVolume();
    rendertypebutton->setIcon(QIcon(":/newPrefix/icons/mesh.png"));
    rendertypebutton->setToolTip(tr("Switch to mesh renderer"));
  }

  m_renderIsAVolume = !m_renderIsAVolume;
  updateViewports(ViewPorts::Render);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::axesViewToggle()
{
  if (m_axesRender->isVisible())
  {
    m_axesRender->setVisible(false);
    axestypebutton->setIcon(QIcon(":newPrefix/icons/axes.png"));
    axestypebutton->setToolTip(tr("Turn on axes planes rendering"));
  }
  else
  {
    m_axesRender->Update(m_POI);
    m_axesRender->setVisible(true);
    axestypebutton->setIcon(QIcon(":newPrefix/icons/noaxes.png"));
    axestypebutton->setToolTip(tr("Turn off axes planes rendering"));
  }
  updateViewports(ViewPorts::Render);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::cut()
{
  QMutexLocker locker(&m_mutex);

  m_editorOperations->Cut(m_dataManager->GetSelectedLabelsSet());
  auto labels = m_dataManager->GetSelectedLabelsSet();

  // hide completely "deleted" labels
  labelselector->blockSignals(true);
  for (auto label: labels)
  {
    if (0LL == m_dataManager->GetNumberOfVoxelsForLabel(label))
    {
      labelselector->item(label)->setHidden(true);
      labelselector->item(label)->setSelected(false);
      labels.erase(label);
    }
  }

  // check if we must select background label becase other labels have been deleted
  if (labels.empty())
  {
    labelselector->item(0)->setSelected(true);
  }

  labelselector->blockSignals(false);
  onSelectionChanged();

  updatePointLabel();
  updateUndoRedoMenu();
  updateViewports(ViewPorts::All);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::relabel()
{
  QMutexLocker locker(&m_mutex);

  std::set<unsigned short> labels = m_dataManager->GetSelectedLabelsSet();
  bool isANewColor{false};

  if (m_editorOperations->Relabel(this, m_fileMetadata, &labels, &isANewColor))
  {
    if (isANewColor)
    {
      restartVoxelRender();
      fillColorLabels();
    }

    // hide "deleted" labels it they have no voxels
    auto oldLabels = m_dataManager->GetSelectedLabelsSet();

    labelselector->blockSignals(true);
    for (auto label: oldLabels)
    {
      if (0LL == m_dataManager->GetNumberOfVoxelsForLabel(label))
      {
        labelselector->item(label)->setHidden(true);
        labelselector->item(label)->setSelected(false);
      }
    }
    labelselector->blockSignals(false);

    selectLabels(labels);
    updatePointLabel();
    updateUndoRedoMenu();
    updateViewports(ViewPorts::All);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::updateViewports(EspinaVolumeEditor::ViewPorts view)
{
  // updating does not happen in hidden views to avoid wasting CPU,
  // only when the users minimize a view updating is enabled again
  switch (view)
  {
    case ViewPorts::Render:
      if (renderview->isVisible()) m_volumeRenderer->GetRenderWindow()->Render();
      break;
    case ViewPorts::Slices:
      if (axialview->isVisible()) m_axialRenderer->GetRenderWindow()->Render();
      if (coronalview->isVisible()) m_coronalRenderer->GetRenderWindow()->Render();
      if (sagittalview->isVisible()) m_sagittalRenderer->GetRenderWindow()->Render();
      break;
    case ViewPorts::All:
      if (axialview->isVisible()) m_axialRenderer->GetRenderWindow()->Render();
      if (coronalview->isVisible()) m_coronalRenderer->GetRenderWindow()->Render();
      if (sagittalview->isVisible()) m_sagittalRenderer->GetRenderWindow()->Render();
      if (renderview->isVisible()) m_volumeRenderer->GetRenderWindow()->Render();
      break;
    case ViewPorts::Axial:
      if (axialview->isVisible()) m_axialRenderer->GetRenderWindow()->Render();
      break;
    case ViewPorts::Coronal:
      if (coronalview->isVisible()) m_coronalRenderer->GetRenderWindow()->Render();
      break;
    case ViewPorts::Sagittal:
      if (sagittalview->isVisible()) m_sagittalRenderer->GetRenderWindow()->Render();
      break;
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::about()
{
  QtAbout aboutdialog(this);
  aboutdialog.exec();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::keyboardHelp()
{
  QtKeyboardHelp keyboardHelpDialog(this);
  keyboardHelpDialog.exec();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::erodeVolumes()
{
  QMutexLocker locker(&m_mutex);
  auto label = m_dataManager->GetSelectedLabelsSet().begin().operator *();

  m_editorOperations->Erode(label);

  // the label could be empty right now
  if (0LL == m_dataManager->GetNumberOfVoxelsForLabel(label))
  {
    labelselector->blockSignals(true);
    labelselector->item(label)->setHidden(true);
    labelselector->item(label)->setSelected(false);
    labelselector->item(0)->setSelected(true);
    labelselector->blockSignals(false);
    onSelectionChanged();
  }

  updatePointLabel();
  updateUndoRedoMenu();
  updateViewports(ViewPorts::All);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::dilateVolumes()
{
  QMutexLocker locker(&m_mutex);
  auto label = m_dataManager->GetSelectedLabelsSet().begin().operator *();

  m_editorOperations->Dilate(label);

  updatePointLabel();
  updateUndoRedoMenu();
  m_volumeView->updateFocusExtent();
  updateViewports(ViewPorts::All);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::openVolumes()
{
  QMutexLocker locker(&m_mutex);
  auto label = m_dataManager->GetSelectedLabelsSet().begin().operator *();

  m_editorOperations->Open(label);

  updatePointLabel();
  updateUndoRedoMenu();
  m_volumeView->updateFocusExtent();
  updateViewports(ViewPorts::All);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::closeVolumes()
{
  QMutexLocker locker(&m_mutex);
  auto label = m_dataManager->GetSelectedLabelsSet().begin().operator *();

  m_editorOperations->Close(label);

  updatePointLabel();
  updateUndoRedoMenu();
  m_volumeView->updateFocusExtent();
  updateViewports(ViewPorts::All);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::watershedVolumes()
{
  QMutexLocker locker(&m_mutex);
  auto label = m_dataManager->GetSelectedLabelsSet().begin().operator *();
  auto generatedLabels = m_editorOperations->Watershed(label);

  restartVoxelRender();
  fillColorLabels();
  updatePointLabel();
  selectLabels(generatedLabels);
  updateUndoRedoMenu();
  updateViewports(ViewPorts::All);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::updateUndoRedoMenu()
{
  std::string text;

  if (m_dataManager->IsUndoBufferEmpty())
    text = std::string("Undo");
  else
    text = std::string("Undo ") + m_dataManager->GetUndoActionString();

  a_undo->setText(text.c_str());
  a_undo->setEnabled(!(m_dataManager->IsUndoBufferEmpty()));

  if (m_dataManager->IsRedoBufferEmpty())
    text = std::string("Redo");
  else
    text = std::string("Redo ") + m_dataManager->GetRedoActionString();

  a_redo->setText(text.c_str());
  a_redo->setEnabled(!(m_dataManager->IsRedoBufferEmpty()));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::undo()
{
  QMutexLocker locker(&m_mutex);

  auto text = std::string("Undo ") + m_dataManager->GetUndoActionString();
  m_progress->ManualSet(text);

  m_dataManager->DoUndoOperation();

  restartVoxelRender();
  updatePointLabel();
  fillColorLabels();
  onSelectionChanged();

  // scroll to last selected label
  if (!m_dataManager->GetSelectedLabelsSet().empty())
  {
    std::set<unsigned short>::reverse_iterator rit = m_dataManager->GetSelectedLabelsSet().rbegin();
    labelselector->scrollToItem(labelselector->item(*rit), QAbstractItemView::PositionAtBottom);
  }

  // the operation is now on the redo buffer, get its type and put the interface in
  // that state, any other operation won't change the editor state
  if (std::string("Paint").compare(m_dataManager->GetRedoActionString()) == 0)
  {
    paintbutton->setChecked(true);
  }
  else
  {
    if (std::string("Erase").compare(m_dataManager->GetRedoActionString()) == 0)
    {
      erasebutton->setChecked(true);
    }
  }

  updateUndoRedoMenu();
  updateViewports(ViewPorts::All);
  m_progress->ManualReset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::redo()
{
  QMutexLocker locker(&m_mutex);

  auto text = std::string("Redo ") + m_dataManager->GetRedoActionString();
  m_progress->ManualSet(text);

  m_dataManager->DoRedoOperation();

  restartVoxelRender();
  updatePointLabel();
  fillColorLabels();
  onSelectionChanged();

  // scroll to last selected label
  if (!m_dataManager->GetSelectedLabelsSet().empty())
  {
    std::set<unsigned short>::reverse_iterator rit = m_dataManager->GetSelectedLabelsSet().rbegin();
    labelselector->scrollToItem(labelselector->item(*rit), QAbstractItemView::PositionAtBottom);
  }

  // the operation is now on the undo buffer, get its type and put the interface in
  // that state, any other operation won't change the editor state
  if (std::string("Paint").compare(m_dataManager->GetUndoActionString()) == 0)
  {
    paintbutton->setChecked(true);
  }
  else
  {
    if (std::string("Erase").compare(m_dataManager->GetUndoActionString()) == 0)
    {
      erasebutton->setChecked(true);
    }
  }

  updateUndoRedoMenu();
  updateViewports(ViewPorts::All);
  m_progress->ManualReset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::sliceInteraction(vtkObject* object, unsigned long event, void *unused1, void *unused2, vtkCommand *unused3)
{
  auto axialInteractorStyle = axialview->GetRenderWindow()->GetInteractor()->GetInteractorStyle();
  auto coronalInteractorStyle = coronalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle();
  auto sagittalInteractorStyle = sagittalview->GetRenderWindow()->GetInteractor()->GetInteractorStyle();

  static bool leftButtonStillDown = false;
  static bool rightButtonStillDown = false;
  static bool middleButtonStillDown = false;
  std::shared_ptr<SliceVisualization> sliceView = nullptr;

  // identify view to pass events forward
  auto style = vtkInteractorStyle::SafeDownCast(object);

  if (style == axialInteractorStyle) sliceView = m_axialView;
  if (style == coronalInteractorStyle) sliceView = m_coronalView;
  if (style == sagittalInteractorStyle) sliceView = m_sagittalView;

  assert(nullptr != sliceView);

  switch (event)
  {
    // sliders go [1-size], spinboxes go [0-(size-1)], that's the reason of the values added to POI.
    case vtkCommand::MouseWheelForwardEvent:
      // SliceXYPick will call setSliderPosition once the selection actors have been moved to their final positions
      // idem for MouseWheelBackwardEvent;
      sliceXYPick(event, sliceView);
      break;
    case vtkCommand::MouseWheelBackwardEvent:
      sliceXYPick(event, sliceView);
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
      sliceXYPick(event, sliceView);
      break;
    case vtkCommand::LeftButtonReleaseEvent:
      leftButtonStillDown = false;
      sliceXYPick(event, sliceView);
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
        if (paintbutton->isChecked() || erasebutton->isChecked())
        {
          sliceXYPick(event, sliceView);
        }

        style->OnMouseMove();
        return;
      }

      if (leftButtonStillDown)
      {
        sliceXYPick(event, sliceView);
        style->OnMouseMove();
        return;
      }

      if (rightButtonStillDown || middleButtonStillDown)
      {
        style->OnMouseMove();
        switch (sliceView->orientationType())
        {
          case SliceVisualization::Orientation::Axial:
            m_axialView->zoomEvent();
            break;
          case SliceVisualization::Orientation::Coronal:
            m_coronalView->zoomEvent();
            break;
          case SliceVisualization::Orientation::Sagittal:
            m_sagittalView->zoomEvent();
            break;
          default:
            break;
        }
        return;
      }
      break;
    default:
      // ignore event, can't happen as we have registered this function with only these commands up there
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::sliceXYPick(const unsigned long event, std::shared_ptr<SliceVisualization> view)
{
  // if we are modifing the volume get the lock first
  if (paintbutton->isChecked() || erasebutton->isChecked())
  {
    QMutexLocker locker(&m_mutex);
  }

  // static vars stores data between calls
  static SliceVisualization::PickType previousPick = SliceVisualization::PickType::None;
  static bool leftButtonStillDown = false;

  auto actualPick = SliceVisualization::PickType::None;

  int X, Y;
  switch (view->orientationType())
  {
    case SliceVisualization::Orientation::Axial:
      X = axialview->GetRenderWindow()->GetInteractor()->GetEventPosition()[0];
      Y = axialview->GetRenderWindow()->GetInteractor()->GetEventPosition()[1];
      actualPick = m_axialView->pickData(&X, &Y);
      break;
    case SliceVisualization::Orientation::Coronal:
      X = coronalview->GetRenderWindow()->GetInteractor()->GetEventPosition()[0];
      Y = coronalview->GetRenderWindow()->GetInteractor()->GetEventPosition()[1];
      actualPick = m_coronalView->pickData(&X, &Y);
      break;
    case SliceVisualization::Orientation::Sagittal:
      X = sagittalview->GetRenderWindow()->GetInteractor()->GetEventPosition()[0];
      Y = sagittalview->GetRenderWindow()->GetInteractor()->GetEventPosition()[1];
      actualPick = m_sagittalView->pickData(&X, &Y);
      break;
    default:
      break;
  }

  // picked out of area
  if (SliceVisualization::PickType::None == actualPick)
  {
    if ((vtkCommand::LeftButtonReleaseEvent == event) || (vtkCommand::LeftButtonPressEvent == event))
    {
      leftButtonStillDown = false;
      previousPick = SliceVisualization::PickType::None;

      // handle a special case when the user starts an operation in the slice but moves out of it and releases the button,
      // we have to finish the operation and update the undo/redo menu.
      if (m_dataManager->GetActualActionString() != std::string(""))
      {
        m_dataManager->OperationEnd();
        updateUndoRedoMenu();
        m_volumeView->updateFocusExtent();
      }

      m_updateVoxelRenderer = true;
      m_updateSliceRenderers = true;
      updateViewports(ViewPorts::All);
    }

    if ((vtkCommand::MouseWheelBackwardEvent == event) || (vtkCommand::MouseWheelForwardEvent == event))
    {
      updateViewports(ViewPorts::Slices);
    }

    return;
  }

  // we need to know if we have started picking or we've picked something before
  if (SliceVisualization::PickType::None == previousPick)
  {
    previousPick = actualPick;
  }
  else
  {
    if (previousPick != actualPick)
    {
      if ((vtkCommand::LeftButtonReleaseEvent == event) || (vtkCommand::LeftButtonPressEvent == event))
      {
        leftButtonStillDown = false;
        previousPick = SliceVisualization::PickType::None;

        // handle a special case when the user starts an operation in the slice but moves out of it and releases the button,
        // we have to finish the operation and update the undo/redo menu.
        if (m_dataManager->GetActualActionString() != std::string(""))
        {
          m_dataManager->OperationEnd();
          updateUndoRedoMenu();
          m_volumeView->updateFocusExtent();
        }

        m_updateVoxelRenderer = true;
        m_updateSliceRenderers = true;
        updateViewports(ViewPorts::All);
      }

      // handle when the users crosses one prop while working with the other
      if ((vtkCommand::MouseMoveEvent == event) && !leftButtonStillDown)
      {
        previousPick = actualPick;
      }

      if ((vtkCommand::MouseWheelBackwardEvent == event) || (vtkCommand::MouseWheelForwardEvent == event))
      {
        updateViewports(ViewPorts::Slices);
      }

      return;
    }
  }

  // NOTE: from now on previousPick = actualPick

  // handle mouse movements when the user is painting or erasing
  if (((vtkCommand::MouseWheelBackwardEvent == event) || (vtkCommand::MouseWheelForwardEvent == event) || (vtkCommand::MouseMoveEvent == event)))
  {
    if ((paintbutton->isChecked() || erasebutton->isChecked()) && (SliceVisualization::PickType::Slice == actualPick))
    {
      int x, y, z;
      switch (view->orientationType())
      {
        case SliceVisualization::Orientation::Axial:
          x = X + 1;
          y = Y + 1;
          switch (event)
          {
            case vtkCommand::MouseWheelForwardEvent:
              z = axialslider->value();
              break;
            case vtkCommand::MouseWheelBackwardEvent:
              z = axialslider->value() - 2;
              break;
            default:
              z = axialslider->value() - 1;
              break;
          }
          break;
        case SliceVisualization::Orientation::Coronal:
          x = X + 1;
          z = Y + 1;
          switch (event)
          {
            case vtkCommand::MouseWheelForwardEvent:
              y = coronalslider->value();
              break;
            case vtkCommand::MouseWheelBackwardEvent:
              y = coronalslider->value() - 2;
              break;
            default:
              y = coronalslider->value() - 1;
              break;
          }
          break;
        case SliceVisualization::Orientation::Sagittal:
          y = X + 1;
          z = Y + 1;
          switch (event)
          {
            case vtkCommand::MouseWheelForwardEvent:
              x = sagittalslider->value();
              break;
            case vtkCommand::MouseWheelBackwardEvent:
              x = sagittalslider->value() - 2;
              break;
            default:
              x = sagittalslider->value() - 1;
              break;
          }
          break;
        default:
          x = y = z = 0;
          break;
      }

      m_editorOperations->UpdatePaintEraseActors(Vector3i{x, y, z}, m_brushRadius, view);
    }

    // once the actors have been moved, then move the slice and update the view
    if (vtkCommand::MouseWheelForwardEvent == event)
    {
      switch (view->orientationType())
      {
        case SliceVisualization::Orientation::Axial:
          axialslider->setSliderPosition(m_POI[2] + 2);
          break;
        case SliceVisualization::Orientation::Coronal:
          coronalslider->setSliderPosition(m_POI[1] + 2);
          break;
        case SliceVisualization::Orientation::Sagittal:
          sagittalslider->setSliderPosition(m_POI[0] + 2);
          break;
        default:
          break;
      }
    }

    if (vtkCommand::MouseWheelBackwardEvent == event)
    {
      switch (view->orientationType())
      {
        case SliceVisualization::Orientation::Axial:
          axialslider->setSliderPosition(m_POI[2]);
          break;
        case SliceVisualization::Orientation::Coronal:
          coronalslider->setSliderPosition(m_POI[1]);
          break;
        case SliceVisualization::Orientation::Sagittal:
          sagittalslider->setSliderPosition(m_POI[0]);
          break;
        default:
          break;
      }
    }

    // return if we are not doing an operation
    if (!leftButtonStillDown)
    {
      updateViewports(ViewPorts::Slices);
      return;
    }
  }

  // if we were picking or painting but released the button, draw voxel view's final state
  if (vtkCommand::LeftButtonReleaseEvent == event)
  {
    leftButtonStillDown = false;
    m_updateVoxelRenderer = true;
    m_updateSliceRenderers = true;

    // this is to handle cases where the user clicks out of the slice and enters in the slice and releases the button,
    // it's a useless case (as the user doesn't do any operation) but crashed the editor as there is no operation
    // on course
    if ((erasebutton->isChecked() || paintbutton->isChecked()) &&
        (actualPick == SliceVisualization::PickType::Slice)    &&
        !(m_dataManager->GetActualActionString() == std::string("")))
    {
      m_dataManager->OperationEnd();

      // some labels could be empty after a paint or a erase operation
      labelselector->blockSignals(true);
      for (unsigned short i = 1; i < m_dataManager->GetNumberOfLabels(); i++)
        if (0LL == m_dataManager->GetNumberOfVoxelsForLabel(i))
        {
          labelselector->item(i)->setHidden(true);
          labelselector->item(i)->setSelected(false);
        }
      labelselector->blockSignals(false);
      onSelectionChanged();

      m_volumeView->updateFocusExtent();
      updateUndoRedoMenu();
    }

    m_axesRender->Update(m_POI);
    updateViewports(ViewPorts::All);

    previousPick = SliceVisualization::PickType::None;
    return;
  }

  if (vtkCommand::LeftButtonPressEvent == event)
  {
    leftButtonStillDown = true;

    if (paintbutton->isChecked() && actualPick == SliceVisualization::PickType::Slice) m_dataManager->OperationStart("Paint");
    if (erasebutton->isChecked() && actualPick == SliceVisualization::PickType::Slice) m_dataManager->OperationStart("Erase");
  }

  m_updateVoxelRenderer = false;
  m_updateSliceRenderers = false;

  // get pixel value or pick a label if color picker is activated (managed in GetPointLabel())
  updatePointLabel();

  // updating slider positions updates POI
  if (leftButtonStillDown)
  {
    double coords[3];
    auto spacing = m_orientationData->GetImageSpacing();

    switch (view->orientationType())
    {
      case SliceVisualization::Orientation::Axial:
        sagittalslider->setSliderPosition(X + 1);
        coronalslider->setSliderPosition(Y + 1);

        // move camera if we are picking the thumbnail & update thumbnail to reflect new position
        if (actualPick == SliceVisualization::PickType::Thumbnail)
        {
          m_axialRenderer->GetActiveCamera()->GetPosition(coords);
          m_axialRenderer->GetActiveCamera()->SetPosition(X * spacing[0], Y * spacing[1], coords[2]);
          m_axialRenderer->GetActiveCamera()->SetFocalPoint(X * spacing[0], Y * spacing[1], 0.0);
          m_axialView->zoomEvent();
        }
        else
        {
          applyUserAction(view);
          m_volumeView->updateFocusExtent();
        }
        break;
      case SliceVisualization::Orientation::Coronal:
        sagittalslider->setSliderPosition(X + 1);
        axialslider->setSliderPosition(Y + 1);

        // move camera if we are picking the thumbnail & update thumbnail to reflect new position
        if (actualPick == SliceVisualization::PickType::Thumbnail)
        {
          m_coronalRenderer->GetActiveCamera()->GetPosition(coords);
          m_coronalRenderer->GetActiveCamera()->SetPosition(X * spacing[0], Y * spacing[2], coords[2]);
          m_coronalRenderer->GetActiveCamera()->SetFocalPoint(X * spacing[0], Y * spacing[2], 0.0);
          m_coronalView->zoomEvent();
        }
        else
        {
          applyUserAction(view);
          m_volumeView->updateFocusExtent();
        }
        break;
      case SliceVisualization::Orientation::Sagittal:
        coronalslider->setSliderPosition(X + 1);
        axialslider->setSliderPosition(Y + 1);

        // move camera if we are picking the thumbnail & update thumbnail to reflect new position
        if (actualPick == SliceVisualization::PickType::Thumbnail)
        {
          m_sagittalRenderer->GetActiveCamera()->GetPosition(coords);
          m_sagittalRenderer->GetActiveCamera()->SetPosition(X * spacing[1], Y * spacing[2], coords[2]);
          m_sagittalRenderer->GetActiveCamera()->SetFocalPoint(X * spacing[1], Y * spacing[2], 0.0);
          m_sagittalView->zoomEvent();
        }
        else
        {
          applyUserAction(view);
          m_volumeView->updateFocusExtent();
        }
        break;
      default:
        break;
    }
  }

  updateViewports(ViewPorts::Slices);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::onViewZoom(void)
{
  // static variable stores status between calls
  static bool zoomstatus = false;

  auto button = qobject_cast<QToolButton *>(sender());

  if (zoomstatus)
  {
    viewgrid->setColumnStretch(0, 1);
    viewgrid->setColumnStretch(1, 1);
    viewgrid->setRowStretch(0, 1);
    viewgrid->setRowStretch(1, 1);

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
      renderbar->insertSpacerItem(2, renderspacer);    // spacers can't hide() or show(), it must be removed and inserted
      voxelresetbutton->show();
      rendersizebutton->show();
      axestypebutton->show();
      rendertypebutton->show();
      renderdisablebutton->show();
    }

    button->setIcon(QIcon(":/newPrefix/icons/tomax.png"));

    // we weren't updating the other view when zoomed, so we must update all views now.
    updateViewports(ViewPorts::All);
  }
  else
  {
    if (button == axialsizebutton)
    {
      viewgrid->setColumnStretch(0, 1);
      viewgrid->setColumnStretch(1, 0);
      viewgrid->setRowStretch(0, 0);
      viewgrid->setRowStretch(1, 1);

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
      viewgrid->setColumnStretch(0, 0);
      viewgrid->setColumnStretch(1, 1);
      viewgrid->setRowStretch(0, 0);
      viewgrid->setRowStretch(1, 1);

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
      viewgrid->setColumnStretch(0, 0);
      viewgrid->setColumnStretch(1, 1);
      viewgrid->setRowStretch(0, 1);
      viewgrid->setRowStretch(1, 0);

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
      viewgrid->setColumnStretch(0, 1);
      viewgrid->setColumnStretch(1, 0);
      viewgrid->setRowStretch(0, 1);
      viewgrid->setRowStretch(1, 0);

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
  }

  // slice thumbnail could have now different view limits as the view size has changed, update it
  if (button == axialsizebutton) m_axialView->zoomEvent();
  if (button == coronalsizebutton) m_coronalView->zoomEvent();
  if (button == sagittalsizebutton) m_sagittalView->zoomEvent();

  repaint();
  zoomstatus = !zoomstatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::renderViewToggle(void)
{
  static bool disabled = false;
  disabled = !disabled;

  if (disabled)
  {
    renderview->setEnabled(false);
    m_volumeRenderer->DrawOff();
    voxelresetbutton->setEnabled(false);
    rendersizebutton->setEnabled(false);
    axestypebutton->setEnabled(false);
    rendertypebutton->setEnabled(false);
    renderdisablebutton->setIcon(QIcon(":/newPrefix/icons/cog_add.png"));
    renderdisablebutton->setStatusTip(tr("Enable render view"));
    renderdisablebutton->setToolTip(tr("Enables the rendering view of the volume"));
  }
  else
  {
    renderview->setEnabled(true);
    m_volumeRenderer->DrawOn();
    voxelresetbutton->setEnabled(true);
    rendersizebutton->setEnabled(true);
    axestypebutton->setEnabled(true);
    if (!m_dataManager->GetSelectedLabelsSet().empty()) rendertypebutton->setEnabled(true);
    renderdisablebutton->setIcon(QIcon(":/newPrefix/icons/cog_delete.png"));
    renderdisablebutton->setStatusTip(tr("Disable render view"));
    renderdisablebutton->setToolTip(tr("Disables the rendering view of the volume"));
    updateViewports(ViewPorts::Render);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::saveSession(void)
{
  m_saveSessionThread = std::make_shared<SaveSessionThread>(this);
  m_saveSessionThread->start();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::saveSessionStart(void)
{
  m_progress->ManualSet("Save Session", 0, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::saveSessionProgress(int value)
{
  m_progress->ManualUpdate(value, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::saveSessionEnd(void)
{
  m_progress->ManualReset(true);

  // we use singleshot timers so until the save session operation has ended we don't restart it
  m_sessionTimer.start(m_saveSessionTime, true);

  m_saveSessionThread = nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::segmentationViewToggle(void)
{
  // don't care about the key if the image doesn't have a reference image
  if (!m_hasReferenceImage) return;

  if (!m_segmentationsVisible)
  {
    eyebutton->setIcon(QPixmap(":/newPrefix/icons/eyeoff.svg"));
    eyebutton->setToolTip(tr("Hide all segmentations"));
    eyebutton->setStatusTip(tr("Hide all segmentations"));
    eyelabel->setText(tr("Hide"));
    eyelabel->setToolTip(tr("Hide all segmentations"));
    eyelabel->setStatusTip(tr("Hide all segmentations"));
    a_hide_segmentations->setText(tr("Hide Segmentations"));
    a_hide_segmentations->setIcon(QPixmap(":/newPrefix/icons/eyeoff.svg"));
  }
  else
  {
    eyebutton->setIcon(QPixmap(":/newPrefix/icons/eyeon.svg"));
    eyebutton->setToolTip(tr("Show all segmentations"));
    eyebutton->setStatusTip(tr("Show all segmentations"));
    eyelabel->setText(tr("Show"));
    eyelabel->setToolTip(tr("Show all segmentations"));
    eyelabel->setStatusTip(tr("Show all segmentations"));
    a_hide_segmentations->setText(tr("Show Segmentations"));
    a_hide_segmentations->setIcon(QPixmap(":/newPrefix/icons/eyeon.svg"));
  }

  m_segmentationsVisible = !m_segmentationsVisible;
  m_axialView->toggleSegmentationView();
  m_coronalView->toggleSegmentationView();
  m_sagittalView->toggleSegmentationView();

  updateViewports(ViewPorts::Slices);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::restoreSavedSession(void)
{
  // this only happens while starting the editor so we can assume that all classes are empty
  // from session generated data.
  m_progress->ManualSet("Restore Session", 0);

  auto homedir = QDir::tempPath();
  auto baseFilename = homedir + QString("/espinaeditor");
  auto temporalFilename = baseFilename + QString(".session");
  auto temporalFilenameMHA = baseFilename + QString(".mha");

  char *buffer;
  unsigned short int size;
  std::ifstream infile;
  infile.open(temporalFilename.toStdString().c_str(), std::ofstream::in | std::ofstream::binary);

  // read original segmentation file name
  infile.read(reinterpret_cast<char*>(&size), sizeof(unsigned short int));
  buffer = (char*) malloc(size + 1);
  infile.read(buffer, size);
  buffer[size] = '\0';
  m_segmentationFileName = QString(buffer);
  free(buffer);

  // read _hasReferenceImage and _referenceFileName if it has one
  infile.read(reinterpret_cast<char*>(&m_hasReferenceImage), sizeof(bool));
  if (m_hasReferenceImage)
  {
    infile.read(reinterpret_cast<char*>(&size), sizeof(unsigned short int));
    buffer = (char*) malloc(size + 1);
    infile.read(buffer, size);
    buffer[size] = '\0';
    m_referenceFileName = QString(buffer);
    free(buffer);
  }

  // read _POI
  infile.read(reinterpret_cast<char*>(&m_POI[0]), sizeof(unsigned int));
  infile.read(reinterpret_cast<char*>(&m_POI[1]), sizeof(unsigned int));
  infile.read(reinterpret_cast<char*>(&m_POI[2]), sizeof(unsigned int));

  renderview->setEnabled(true);
  axialview->setEnabled(true);
  sagittalview->setEnabled(true);
  coronalview->setEnabled(true);

  QMutexLocker locker(&m_mutex);

  auto io = itk::MetaImageIO::New();
  io->SetFileName(temporalFilenameMHA.toStdString().c_str());
  auto reader = ReaderType::New();
  reader->SetImageIO(io);
  reader->SetFileName(temporalFilenameMHA.toStdString().c_str());
  reader->ReleaseDataFlagOn();

  try
  {
    reader->Update();
  }
  catch (itk::ExceptionObject & excp)
  {
    m_progress->ManualReset();
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setCaption("Error loading segmentation file");
    msgBox.setText("An error occurred loading the segmentation file.\nThe operation has been aborted.");
    msgBox.setDetailedText(excp.what());

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

    msgBox.exec();
    return;
  }

  // do not update the viewports while loading
  m_updateVoxelRenderer  = false;
  m_updateSliceRenderers = false;
  m_updatePointLabel     = false;

  m_fileMetadata = std::make_shared<Metadata>();

  // read _Metadata objects
  infile.read(reinterpret_cast<char*>(&size), sizeof(unsigned short int));
  for (unsigned int i = 0; i < size; i++)
  {
    unsigned int scalar;
    unsigned int segment;
    unsigned int selected;
    infile.read(reinterpret_cast<char*>(&scalar), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&segment), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&selected), sizeof(unsigned int));
    m_fileMetadata->addObject(scalar, segment, selected);
  }

  infile.read(reinterpret_cast<char*>(&size), sizeof(unsigned short int));
  for (unsigned int i = 0; i < size; i++)
  {
    Vector3ui inclusive;
    Vector3ui exclusive;
    infile.read(reinterpret_cast<char*>(&inclusive[0]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&inclusive[1]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&inclusive[2]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&exclusive[0]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&exclusive[1]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&exclusive[2]), sizeof(unsigned int));
    m_fileMetadata->addBrick(inclusive, exclusive);
  }

  infile.read(reinterpret_cast<char*>(&size), sizeof(unsigned short int));
  for (unsigned int i = 0; i < size; i++)
  {
    char *buffer;
    int nameSize;
    Vector3ui color;
    unsigned int value;
    std::string name;
    infile.read(reinterpret_cast<char*>(&color[0]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&color[1]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&color[2]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&value), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&nameSize), sizeof(unsigned short int));
    buffer = (char*) malloc(nameSize + 1);
    infile.read(buffer, nameSize);
    buffer[nameSize] = '\0';
    name = std::string(buffer);
    m_fileMetadata->addSegment(name, value, QColor::fromRgb(color[0], color[1], color[2]));
  }

  infile.read(reinterpret_cast<char*>(&m_fileMetadata->hasUnassignedTag), sizeof(bool));
  infile.read(reinterpret_cast<char*>(&m_fileMetadata->unassignedTagPosition), sizeof(int));

  m_orientationData = std::make_shared<Coordinates>(reader->GetOutput());
  auto imageSize = m_orientationData->GetTransformedSize();

  // itkimage->itklabelmap
  auto converter = ConverterType::New();
  converter->SetInput(reader->GetOutput());
  converter->ReleaseDataFlagOn();
  converter->Update();
  converter->GetOutput()->Optimize();
  assert(0 != converter->GetOutput()->GetNumberOfLabelObjects());

  // flatten labelmap, modify origin and store scalar label values
  m_dataManager->Initialize(converter->GetOutput(), m_orientationData, m_fileMetadata);

  // overwrite _dataManager object vector
  infile.read(reinterpret_cast<char*>(&size), sizeof(unsigned short int));
  for (unsigned int i = 0; i < size; i++)
  {
    unsigned short int position;
    infile.read(reinterpret_cast<char*>(&position), sizeof(unsigned short int));
    auto object = m_dataManager->ObjectVector[position];
    infile.read(reinterpret_cast<char*>(&object->scalar), sizeof(unsigned short));
    infile.read(reinterpret_cast<char*>(&object->size), sizeof(unsigned long long int));
    infile.read(reinterpret_cast<char*>(&object->centroid[0]), sizeof(double));
    infile.read(reinterpret_cast<char*>(&object->centroid[1]), sizeof(double));
    infile.read(reinterpret_cast<char*>(&object->centroid[2]), sizeof(double));
    infile.read(reinterpret_cast<char*>(&object->min[0]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&object->min[1]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&object->min[2]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&object->max[0]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&object->max[1]), sizeof(unsigned int));
    infile.read(reinterpret_cast<char*>(&object->max[2]), sizeof(unsigned int));
  }
  infile.close();

  // itklabelmap->itkimage
  auto labelconverter = LabelMapToImageFilterType::New();
  labelconverter->SetInput(m_dataManager->GetLabelMap());
  labelconverter->SetNumberOfThreads(1);						// if number of threads > 1 filter crashes (¿¿??)
  labelconverter->ReleaseDataFlagOn();
  labelconverter->Update();

  // itkimage->vtkimage
  auto itkExporter = ITKExport::New();
  auto vtkImporter = vtkSmartPointer<vtkImageImport>::New();
  itkExporter->SetInput(reader->GetOutput());
  ConnectPipelines(itkExporter, vtkImporter);
  vtkImporter->Update();

  // get the vtkStructuredPoints out of the vtkImage
  auto convert = vtkSmartPointer<vtkImageToStructuredPoints>::New();
  convert->SetInputData(vtkImporter->GetOutput());
  convert->ReleaseDataFlagOn();
  convert->Update();

  // now we have our structuredpoints
  m_dataManager->SetStructuredPoints(convert->GetStructuredPointsOutput());

  // initialize the GUI
  initializeGUI();

  // load reference file if it has one
  if (m_hasReferenceImage)
  {
    loadReferenceFile(QString(m_referenceFileName));
  }

  // get the working set of labels for the temporal file
  QString filename(temporalFilename);
  filename.replace(QChar('/'), QChar('\\'));

  QSettings editorSettings("UPM", "Espina Volume Editor");
  editorSettings.beginGroup("Editor");

  // get working set of labels for this file and apply it, if it exists
  if ((editorSettings.value(filename).isValid()) && (true == editorSettings.contains(filename)))
  {
    QList<QVariant> labelList = editorSettings.value(filename).toList();

    std::set<unsigned short> labelScalars;
    for (auto label: labelList)
    {
      labelScalars.insert(static_cast<unsigned short>(label.toUInt()));
    }

    std::set<unsigned short> labelIndexes;
    for (auto index: labelIndexes)
    {
      labelIndexes.insert(m_dataManager->GetLabelForScalar(index));
    }

    // make sure is a valid group of labels (deleting invalid labels)
    for (auto scalar: labelScalars)
    {
      if (scalar > m_dataManager->GetNumberOfLabels())
      {
        labelScalars.erase(scalar);
      }
    }

    selectLabels(labelIndexes);
  }

  // put the name of the opened file in the window title
  auto caption = QString("Espina Volume Editor - ") + m_segmentationFileName;
  setCaption(caption);

  // start session timer
  if (m_saveSessionEnabled)
  {
    m_sessionTimer.start(m_saveSessionTime, true);
  }

  m_progress->ManualReset();

  // NOTE: we don't delete the session files in the case the editor crashes again
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::removeSessionFiles(void)
{
  // delete the temporal session files, if they exists
  auto homedir = QDir::tempPath();
  auto baseFilename = homedir + QString("/espinaeditor");
  auto temporalFilename = baseFilename + QString(".session");
  auto temporalFilenameMHA = baseFilename + QString(".mha");

  QFile file(temporalFilename);
  if (file.exists() && !file.remove())
  {
    QMessageBox msgBox(this);
    msgBox.setCaption("Error trying to remove file");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("An error occurred exiting the editor.\n.Editor session file couldn't be removed.");

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

    msgBox.exec();
  }

  QFile fileMHA(temporalFilenameMHA);
  if (fileMHA.exists() && !fileMHA.remove())
  {
    QMessageBox msgBox(this);
    msgBox.setCaption("Error trying to remove file");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("An error occurred exiting the editor.\n.Editor MHA session file couldn't be removed.");

    auto msgSize = msgBox.sizeHint();
    auto rect = this->rect();
    msgBox.move(QPoint( rect.width()/2 - msgSize.width()/2, rect.height()/2 - msgSize.height()/2 ) );

    msgBox.exec();
  }

  // remove stored metadata
  QSettings editorSettings("UPM", "Espina Volume Editor");
  editorSettings.beginGroup("Editor");

  QString filename(temporalFilename);
  filename.replace(QChar('/'), QChar('\\'));
  editorSettings.remove(filename);
  editorSettings.sync();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::initializeGUI(void)
{
  // set POI (point of interest)
  auto imageSize = m_orientationData->GetTransformedSize();
  m_POI[0] = (imageSize[0] - 1) / 2;
  m_POI[1] = (imageSize[1] - 1) / 2;
  m_POI[2] = (imageSize[2] - 1) / 2;

  // add volume actors to 3D renderer
  m_volumeView = std::make_shared<VoxelVolumeRender>(m_dataManager, m_volumeRenderer, m_progress);

  // visualize slices in all planes
  m_sagittalView->initialize(m_dataManager->GetStructuredPoints(), m_dataManager->GetLookupTable(), m_sagittalRenderer, m_orientationData);
  m_coronalView->initialize(m_dataManager->GetStructuredPoints(), m_dataManager->GetLookupTable(), m_coronalRenderer, m_orientationData);
  m_axialView->initialize(m_dataManager->GetStructuredPoints(), m_dataManager->GetLookupTable(), m_axialRenderer, m_orientationData);
  m_axialView->update(m_POI);
  m_coronalView->update(m_POI);
  m_sagittalView->update(m_POI);

  // we don't initialize slider position because thats what the spinBoxes will do on init with POI+1 because sliders go 1-max and POI is 0-(max-1)
  axialslider->setEnabled(false);
  axialslider->setMinimum(1);
  axialslider->setMaximum(imageSize[2]);
  axialslider->setEnabled(true);
  coronalslider->setEnabled(false);
  coronalslider->setMinimum(1);
  coronalslider->setMaximum(imageSize[1]);
  coronalslider->setEnabled(true);
  sagittalslider->setEnabled(false);
  sagittalslider->setMinimum(1);
  sagittalslider->setMaximum(imageSize[0]);
  sagittalslider->setEnabled(true);

  // initialize spinbox positions with POI+1 because sliders & spinBoxes go 1-max and POI is 0-(max-1)
  // it also initializes sliders and renders the viewports
  XspinBox->setRange(1, imageSize[0]);
  XspinBox->setEnabled(true);
  XspinBox->setValue(m_POI[0] + 1);
  YspinBox->setRange(1, imageSize[1]);
  YspinBox->setEnabled(true);
  YspinBox->setValue(m_POI[1] + 1);
  ZspinBox->setRange(1, imageSize[2]);
  ZspinBox->setEnabled(true);
  ZspinBox->setValue(m_POI[2] + 1);

  // fill selection label combobox and draw label combobox
  fillColorLabels();
  m_updatePointLabel = true;
  updatePointLabel();

  // initalize EditorOperations instance
  m_editorOperations->Initialize(m_volumeRenderer, m_orientationData, m_progress);
  m_editorOperations->SetSliceViews(m_axialView, m_coronalView, m_sagittalView);

  // enable disabled widgets
  viewbutton->setEnabled(true);
  paintbutton->setEnabled(true);
  erasebutton->setEnabled(true);
  pickerbutton->setEnabled(true);
  wandButton->setEnabled(true);
  selectbutton->setEnabled(true);
  lassoButton->setEnabled(true);
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
  a_fileInfo->setEnabled(true);
  axialsizebutton->setEnabled(true);
  coronalsizebutton->setEnabled(true);
  sagittalsizebutton->setEnabled(true);
  rendersizebutton->setEnabled(true);
  renderdisablebutton->setEnabled(true);

  eyebutton->setEnabled(false);
  eyelabel->setEnabled(false);
  a_hide_segmentations->setEnabled(false);

  // needed to maximize/mininize views, not really necessary but looks better
  viewgrid->setColumnMinimumWidth(0, 0);
  viewgrid->setColumnMinimumWidth(1, 0);
  viewgrid->setRowMinimumHeight(0, 0);
  viewgrid->setRowMinimumHeight(1, 0);

  // set axes initial state
  m_axesRender = std::make_shared<AxesRender>(m_volumeRenderer, m_orientationData);
  m_axesRender->Update(m_POI);

  // update all renderers
  m_axialRenderer->ResetCamera();
  m_axialView->zoomEvent();
  m_coronalRenderer->ResetCamera();
  m_coronalView->zoomEvent();
  m_sagittalRenderer->ResetCamera();
  m_sagittalView->zoomEvent();
  m_volumeRenderer->ResetCamera();

  // reset parts of the GUI, needed when loading another image (another session) to reset buttons
  // and items to their initial states. Same goes for selected label.
  axestypebutton->setIcon(QIcon(":newPrefix/icons/noaxes.png"));
  labelselector->setCurrentRow(0);
  viewbutton->setChecked(true);

  // we can now begin updating the viewports
  m_updateVoxelRenderer = true;
  m_updateSliceRenderers = true;
  m_renderIsAVolume = true;
  updateViewports(ViewPorts::All);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::ToggleButtonDefault(bool value)
{
  if (value)
  {
    m_editorOperations->ClearSelection();
    labelselector->update();

    // need to update the gui according to selected label set
    onSelectionChanged();

    updateViewports(ViewPorts::All);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::eraseOrPaintButtonToggle(bool value)
{
  if (value)
  {
    m_editorOperations->ClearSelection();
    labelselector->update();
    // only one label allowed for paint so we will choose the last one, if it exists
    if ((m_dataManager->GetSelectedLabelSetSize() > 1) && paintbutton->isChecked())
    {
      auto labels = m_dataManager->GetSelectedLabelsSet();
      std::set<unsigned short>::reverse_iterator rit = labels.rbegin();
      if (rit != labels.rend())
      {
        labelselector->blockSignals(true);
        labelselector->clearSelection();
        labelselector->blockSignals(false);
        labelselector->item(*rit)->setSelected(true);
        labelselector->scrollToItem(labelselector->item(*rit));
      }
      else
      {
        labelselector->clearSelection();
      }
    }

    // only one label while we are painting, multiple if erasing
    if (paintbutton->isChecked())
    {
      labelselector->setSelectionMode(QAbstractItemView::SingleSelection);
    }
    else
    {
      labelselector->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

    // get the mouse position and put the actor there if it's over a slice widget, this only
    // can happen when the user uses the keyboard shortcuts to activate the paint/erase operation
    // as the mouse could be still over a slice widget (if the user clicks on the button of the
    // UI then it's out of the widget obviously)
    auto spacing = m_dataManager->GetOrientationData()->GetImageSpacing();
    double dPoint[3] = { 0, 0, 0 };
    int iPoint[3];

    if (axialview->underMouse())
    {
      auto widgetPos = axialview->mapFromGlobal(QCursor::pos());
      auto widgetRect = axialview->rect();

      m_axialRenderer->SetDisplayPoint(widgetPos.x() - widgetRect.left(), widgetRect.bottom() - widgetPos.y(), 0);
      m_axialRenderer->DisplayToWorld();
      m_axialRenderer->GetWorldPoint(dPoint);

      iPoint[0] = floor(dPoint[0] / spacing[0]) + ((fmod(dPoint[0], spacing[0]) > (0.5 * spacing[0])) ? 1 : 0) + 1;
      iPoint[1] = floor(dPoint[1] / spacing[1]) + ((fmod(dPoint[1], spacing[1]) > (0.5 * spacing[1])) ? 1 : 0) + 1;
      iPoint[2] = axialslider->value() - 1;

      m_editorOperations->UpdatePaintEraseActors(Vector3i{iPoint[0], iPoint[1], iPoint[2]}, m_brushRadius, m_axialView);

    }
    else
    {
      if (coronalview->underMouse())
      {
        auto widgetPos = coronalview->mapFromGlobal(QCursor::pos());
        auto widgetRect = coronalview->rect();

        m_coronalRenderer->SetDisplayPoint(widgetPos.x() - widgetRect.left(), widgetRect.bottom() - widgetPos.y(), 0);
        m_coronalRenderer->DisplayToWorld();
        m_coronalRenderer->GetWorldPoint(dPoint);

        iPoint[0] = floor(dPoint[0] / spacing[0]) + ((fmod(dPoint[0], spacing[0]) > (0.5 * spacing[0])) ? 1 : 0) + 1;
        iPoint[1] = coronalslider->value() - 1;
        iPoint[2] = floor(dPoint[1] / spacing[2]) + ((fmod(dPoint[1], spacing[2]) > (0.5 * spacing[2])) ? 1 : 0) + 1;

        m_editorOperations->UpdatePaintEraseActors(Vector3i{iPoint[0], iPoint[1], iPoint[2]}, m_brushRadius, m_coronalView);
      }
      else
      {
        if (sagittalview->underMouse())
        {
          QPoint widgetPos = sagittalview->mapFromGlobal(QCursor::pos());
          QRect widgetRect = sagittalview->rect();

          m_sagittalRenderer->SetDisplayPoint(widgetPos.x() - widgetRect.left(), widgetRect.bottom() - widgetPos.y(), 0);
          m_sagittalRenderer->DisplayToWorld();
          m_sagittalRenderer->GetWorldPoint(dPoint);

          iPoint[0] = sagittalslider->value() - 1;
          iPoint[1] = floor(dPoint[0] / spacing[1]) + ((fmod(dPoint[0], spacing[1]) > (0.5 * spacing[1])) ? 1 : 0) + 1;
          iPoint[2] = floor(dPoint[1] / spacing[2]) + ((fmod(dPoint[1], spacing[2]) > (0.5 * spacing[2])) ? 1 : 0) + 1;

          m_editorOperations->UpdatePaintEraseActors(Vector3i{iPoint[0], iPoint[1], iPoint[2]}, m_brushRadius, m_sagittalView);
        }
      }
    }

    updateViewports(ViewPorts::All);
  }
  else
  {
    labelselector->setSelectionMode(QAbstractItemView::ExtendedSelection);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::wandButtonToggle(bool value)
{
  if (value)
  {
    m_editorOperations->ClearSelection();
    // as this operation could select only connected parts of a segmentation, we deselect the currently selected set
    labelselector->blockSignals(true);
    labelselector->clearSelection();
    labelselector->blockSignals(false);
    labelselector->item(0)->setSelected(true);
    labelselector->scrollToItem(labelselector->item(0));
    updateViewports(ViewPorts::All);
  }
  else
  {
    m_editorOperations->ClearSelection();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::enableOperations(const bool value)
{
  erodeoperation->setEnabled(value);
  dilateoperation->setEnabled(value);
  openoperation->setEnabled(value);
  closeoperation->setEnabled(value);
  watershedoperation->setEnabled(value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::restartVoxelRender(void)
{
  m_volumeView = std::make_shared<VoxelVolumeRender>(m_dataManager, m_volumeRenderer, m_progress);

  if (!m_renderIsAVolume)
  {
    m_volumeView->viewAsMesh();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::selectLabels(std::set<unsigned short> labels)
{
  // can't select a group of labels if it contains the background label
  if ((labels.find(0) != labels.end()) || labels.empty())
  {
    labelselector->item(0)->setSelected(true);
    labelselector->scrollToItem(labelselector->item(0), QAbstractItemView::PositionAtCenter);
    return;
  }

  labelselector->blockSignals(true);
  labelselector->setSelectionMode(QAbstractItemView::ExtendedSelection);
  labelselector->clearSelection();

  for (auto label: labels)
  {
    labelselector->item(label)->setSelected(true);
  }

  labelselector->blockSignals(false);

  // scroll to the last one created label
  std::set<unsigned short>::reverse_iterator rit = labels.rbegin();
  labelselector->scrollToItem(labelselector->item(*rit), QAbstractItemView::PositionAtCenter);

  // need to do this because we were blocking labelselector signals and want to update
  // the selected labels on one call
  onSelectionChanged();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::applyUserAction(std::shared_ptr<SliceVisualization> view)
{
  if (paintbutton->isChecked())
  {
    QMutexLocker locker(&m_mutex);

    if (m_dataManager->GetSelectedLabelsSet().empty())
    {
      m_editorOperations->Paint(0);
    }
    else
    {
      // there should be just one label in the set
      m_editorOperations->Paint(m_dataManager->GetSelectedLabelsSet().begin().operator *());
    }

    updatePointLabel();
    return;
  }

  if (selectbutton->isChecked())
  {
    // need to block signals from the QApplication so it won't create new events to get queued while
    // processing this event. This somehow fixed a nasty visual tearing effect when creating a box
    // while the render draws meshes. (??)
    blockSignals(true);
    QApplication::removePostedEvents(this);

    m_editorOperations->AddSelectionPoint(Vector3ui(m_POI[0], m_POI[1], m_POI[2]));
    relabelbutton->setEnabled(true);

    blockSignals(false);
    return;
  }

  if (lassoButton->isChecked())
  {
    m_editorOperations->AddContourPoint(Vector3ui(m_POI[0], m_POI[1], m_POI[2]), view);
    relabelbutton->setEnabled(true);
    return;
  }

  if (erasebutton->isChecked())
  {
    QMutexLocker locker(&m_mutex);

    if (m_dataManager->GetSelectedLabelsSet().empty())
    {
      m_editorOperations->Paint(0);
    }
    else
    {
      m_editorOperations->Erase(m_dataManager->GetSelectedLabelsSet());
    }

    updatePointLabel();
    return;
  }

  if (pickerbutton->isChecked() && (0 != m_pointScalar))
  {
    if (m_dataManager->IsColorSelected(m_pointScalar))
    {
      labelselector->item(m_pointScalar)->setSelected(false);
      if (m_dataManager->GetSelectedLabelSetSize() != 0)
      {
        labelselector->scrollToItem(labelselector->item(m_dataManager->GetSelectedLabelsSet().rbegin().operator *()), QAbstractItemView::PositionAtCenter);
      }
    }
    else
    {
      labelselector->item(m_pointScalar)->setSelected(true);
      labelselector->scrollToItem(labelselector->item(m_pointScalar), QAbstractItemView::PositionAtCenter);
    }

    return;
  }

  if (wandButton->isChecked() && (0 != m_pointScalar))
  {
    cutbutton->setEnabled(true);
    relabelbutton->setEnabled(true);

    m_editorOperations->ContiguousAreaSelection(m_POI);

    labelselector->item(m_pointScalar)->setSelected(true);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::sessionInfo()
{
  QFileInfo fileInfo{m_segmentationFileName};

  QtSessionInfo infodialog(this);

  infodialog.SetDimensions(m_orientationData->GetImageSize());
  infodialog.SetSpacing(m_orientationData->GetImageSpacing());
  infodialog.SetFileInfo(fileInfo);

  int segmentationsNum = 0;
  for (unsigned int i = 1; i < m_dataManager->GetNumberOfLabels(); ++i)
  {
    if (!labelselector->item(i)->isHidden())
    {
      ++segmentationsNum;
    }
  }
  infodialog.SetNumberOfSegmentations(segmentationsNum);

  infodialog.SetDirectionCosineMatrix(m_orientationData->GetImageDirectionCosineMatrix());

  if (!m_referenceFileName.isEmpty())
  {
    QFileInfo referenceFileInfo{m_referenceFileName};
    infodialog.SetReferenceFileInfo(referenceFileInfo);
  }

  infodialog.exec();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool EspinaVolumeEditor::eventFilter(QObject *object, QEvent *event)
{
  // NOTE: we need to give keyboard focus to the slices in case a contourwidget is present in one of them
  switch (event->type())
  {
    case QEvent::Enter:
      if (object == axialview) axialview->setFocus();

      if (object == coronalview) coronalview->setFocus();

      if (object == sagittalview) sagittalview->setFocus();
      break;
    case QEvent::Leave:
      window()->setFocus();
      break;
    default:
      break;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::connectSignals()
{
  connect(a_fileOpen, SIGNAL(triggered()), this, SLOT(open()));
  connect(a_fileReferenceOpen, SIGNAL(triggered()), this, SLOT(referenceOpen()));
  connect(a_fileSave, SIGNAL(triggered()), this, SLOT(save()));
  connect(a_fileExit, SIGNAL(triggered()), this, SLOT(exit()));
  connect(a_fileInfo, SIGNAL(triggered()), this, SLOT(sessionInfo()));

  connect(a_undo, SIGNAL(triggered()), this, SLOT(undo()));
  connect(a_redo, SIGNAL(triggered()), this, SLOT(redo()));
  connect(a_hide_segmentations, SIGNAL(triggered()), this, SLOT(segmentationViewToggle()));

  connect(a_fulltoggle, SIGNAL(triggered()), this, SLOT(fullscreenToggle()));
  connect(a_preferences, SIGNAL(triggered()), this, SLOT(preferences()));

  connect(a_about, SIGNAL(triggered()), this, SLOT(about()));
  connect(a_keyhelp, SIGNAL(triggered()), this, SLOT(keyboardHelp()));

  connect(axialslider, SIGNAL(valueChanged(int)), this, SLOT(onAxialSliderModified(int)));
  connect(axialslider, SIGNAL(sliderReleased()), this, SLOT(onSliderReleased()));
  connect(axialslider, SIGNAL(sliderPressed()), this, SLOT(onSliderPressed()));

  connect(coronalslider, SIGNAL(valueChanged(int)), this, SLOT(onCoronalSliderModified(int)));
  connect(coronalslider, SIGNAL(sliderReleased()), this, SLOT(onSliderReleased()));
  connect(coronalslider, SIGNAL(sliderPressed()), this, SLOT(onSliderPressed()));

  connect(sagittalslider, SIGNAL(valueChanged(int)), this, SLOT(onSagittalSliderModified(int)));
  connect(sagittalslider, SIGNAL(sliderReleased()), this, SLOT(onSliderReleased()));
  connect(sagittalslider, SIGNAL(sliderPressed()), this, SLOT(onSliderPressed()));

  connect(labelselector, SIGNAL(itemSelectionChanged()), this, SLOT(onSelectionChanged()));
  connect(labelselector, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem*)), this, SLOT(onLabelSelectionInteraction(QListWidgetItem*, QListWidgetItem *)));

  connect(XspinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxXModified(int)));
  connect(YspinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxYModified(int)));
  connect(ZspinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxZModified(int)));

  connect(erodeoperation, SIGNAL(clicked(bool)), this, SLOT(erodeVolumes()));
  connect(dilateoperation, SIGNAL(clicked(bool)), this, SLOT(dilateVolumes()));
  connect(openoperation, SIGNAL(clicked(bool)), this, SLOT(openVolumes()));
  connect(closeoperation, SIGNAL(clicked(bool)), this, SLOT(closeVolumes()));
  connect(watershedoperation, SIGNAL(clicked(bool)), this, SLOT(watershedVolumes()));

  connect(rendertypebutton, SIGNAL(clicked(bool)), this, SLOT(renderTypeSwitch()));
  connect(axestypebutton, SIGNAL(clicked(bool)), this, SLOT(axesViewToggle()));

  connect(viewbutton, SIGNAL(toggled(bool)), this, SLOT(ToggleButtonDefault(bool)));
  connect(paintbutton, SIGNAL(toggled(bool)), this, SLOT(eraseOrPaintButtonToggle(bool)));
  connect(erasebutton, SIGNAL(toggled(bool)), this, SLOT(eraseOrPaintButtonToggle(bool)));
  connect(cutbutton, SIGNAL(clicked(bool)), this, SLOT(cut()));
  connect(relabelbutton, SIGNAL(clicked(bool)), this, SLOT(relabel()));
  connect(pickerbutton, SIGNAL(clicked(bool)), this, SLOT(ToggleButtonDefault(bool)));
  connect(selectbutton, SIGNAL(clicked(bool)), this, SLOT(ToggleButtonDefault(bool)));
  connect(wandButton, SIGNAL(toggled(bool)), this, SLOT(wandButtonToggle(bool)));

  connect(axialresetbutton, SIGNAL(clicked(bool)), this, SLOT(resetViews()));
  connect(coronalresetbutton, SIGNAL(clicked(bool)), this, SLOT(resetViews()));
  connect(sagittalresetbutton, SIGNAL(clicked(bool)), this, SLOT(resetViews()));
  connect(voxelresetbutton, SIGNAL(clicked(bool)), this, SLOT(resetViews()));

  connect(axialsizebutton, SIGNAL(clicked(bool)), this, SLOT(onViewZoom()));
  connect(sagittalsizebutton, SIGNAL(clicked(bool)), this, SLOT(onViewZoom()));
  connect(coronalsizebutton, SIGNAL(clicked(bool)), this, SLOT(onViewZoom()));
  connect(rendersizebutton, SIGNAL(clicked(bool)), this, SLOT(onViewZoom()));

  connect(renderdisablebutton, SIGNAL(clicked(bool)), this, SLOT(renderViewToggle()));
  connect(eyebutton, SIGNAL(clicked(bool)), this, SLOT(segmentationViewToggle()));

  connect(lassoButton, SIGNAL(clicked(bool)), this, SLOT(ToggleButtonDefault(bool)));

  connect(&m_sessionTimer, SIGNAL(timeout()), this, SLOT(saveSession()));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EspinaVolumeEditor::loadSettings()
{
  QSettings editorSettings("UPM", "Espina Volume Editor");
  editorSettings.beginGroup("Editor");

  // get timer settings, create session timer and connect signals
  if (!editorSettings.contains("Autosave Session Data"))
  {
    m_saveSessionEnabled = true;
    m_saveSessionTime = 20 * 60 * 1000;
    editorSettings.setValue("Autosave Session Data", true);
    editorSettings.setValue("Autosave Session Time", 20);
  }
  else
  {
    bool returnValue = false;
    m_saveSessionEnabled = editorSettings.value("Autosave Session Data").toBool();
    m_saveSessionTime = editorSettings.value("Autosave Session Time").toUInt(&returnValue) * 60 * 1000;

    if (!returnValue)
    {
      m_saveSessionTime = 20 * 60 * 1000;
    }

  }

  if (!editorSettings.contains("UndoRedo System Buffer Size"))
  {
    editorSettings.setValue("UndoRedo System Buffer Size", 150 * 1024 * 1024);
    editorSettings.setValue("Filters Radius", 1);
    editorSettings.setValue("Watershed Flood Level", 0.50);
    editorSettings.setValue("Segmentation Opacity", 75);
    editorSettings.setValue("Paint-Erase Radius", 1);
    // no need to set values, classes have their own default values at init
  }
  else
  {
    bool returnValue = false;

    auto size = editorSettings.value("UndoRedo System Buffer Size").toULongLong(&returnValue);
    if (!returnValue)
    {
      m_dataManager->SetUndoRedoBufferSize(150 * 1024 * 1024);
      editorSettings.setValue("UndoRedo System Buffer Size", 150 * 1024 * 1024);
    }
    else
    {
      m_dataManager->SetUndoRedoBufferSize(size);
    }

    m_editorOperations->SetFiltersRadius(editorSettings.value("Filters Radius").toInt(&returnValue));
    if (!returnValue)
    {
      m_editorOperations->SetFiltersRadius(1);
      editorSettings.setValue("Filters Radius", 1);
    }

    m_editorOperations->SetWatershedLevel(editorSettings.value("Watershed Flood Level").toDouble(&returnValue));
    if (!returnValue)
    {
      m_editorOperations->SetWatershedLevel(0.50);
      editorSettings.setValue("Watershed Flood Level", 0.50);
    }

    auto opacity = editorSettings.value("Segmentation Opacity").toDouble(&returnValue);
    if (!returnValue)
    {
      opacity = 0.75;
      editorSettings.setValue("Segmentation Opacity", 75);
    }
    m_sagittalView->setSegmentationOpacity(opacity/100.0);
    m_axialView   ->setSegmentationOpacity(opacity/100.0);
    m_coronalView ->setSegmentationOpacity(opacity/100.0);

    m_brushRadius = editorSettings.value("Paint-Erase Radius").toUInt(&returnValue);
    if (!returnValue)
    {
      m_brushRadius = 1;
      editorSettings.setValue("Paint-Erase Radius", 1);
    }
  }

  editorSettings.sync();
}
