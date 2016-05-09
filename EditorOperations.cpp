///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: EditorOperations.cpp
// Purpose: Manages selection area and editor operations & filters
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// c++ includes
#include <ctime>
#include <cstdlib>
#include <cassert>
#include <cstddef>
#include <cstdio>

// itk includes
#include <itkSize.h>
#include <itkLabelMap.h>
#include <itkIndex.h>
#include <itkRGBPixel.h>
#include <itkBinaryBallStructuringElement.h>
#include <itkErodeObjectMorphologyImageFilter.h>
#include <itkDilateObjectMorphologyImageFilter.h>
#include <itkBinaryMorphologicalOpeningImageFilter.h>
#include <itkBinaryMorphologicalClosingImageFilter.h>
#include <itkMorphologicalWatershedImageFilter.h>
#include <itkSignedDanielssonDistanceMapImageFilter.h>
#include <itkExceptionObject.h>
#include <itkLabelImageToLabelMapFilter.h>
#include <itkVTKImageImport.h>
#include <itkImageDuplicator.h>
#include <itkImageRegionConstIteratorWithIndex.h>
#include <itkCastImageFilter.h>
#include <itkImageFileWriter.h>
#include <itkMetaImageIO.h>
#include <itkChangeInformationImageFilter.h>
#include <itkChangeLabelLabelMapFilter.h>
#include <itkLabelMapToLabelImageFilter.h>
#include <itkConnectedThresholdImageFilter.h>

// vtk includes
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkImageCanvasSource2D.h>
#include <vtkTexture.h>
#include <vtkImageClip.h>
#include <vtkImageExport.h>
#include <vtkImageCast.h>
#include <vtkMetaImageWriter.h>
#include <vtkImageToImageStencil.h>
#include <vtkDiscreteMarchingCubes.h>
#include <vtkDecimatePro.h>
#include <vtkVolumeTextureMapper3D.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkVolumeProperty.h>
#include <vtkTextureMapToPlane.h>
#include <vtkTransformTextureCoords.h>

// project includes
#include "Coordinates.h"
#include "EditorOperations.h"
#include "QtRelabel.h"
#include "QtColorPicker.h"
#include "itkvtkpipeline.h"

// qt includes
#include <QMessageBox>
#include <QFileDialog>
#include <QObject>

using LabelObjectType = itk::ShapeLabelObject<unsigned short, 3>;
using LabelMapType = itk::LabelMap<LabelObjectType>;
using ConstIteratorType = itk::ImageRegionConstIteratorWithIndex<ImageType>;

using StructuringElementType = itk::BinaryBallStructuringElement<ImageType::PixelType, 3>;
using BinaryErodeImageFilterType = itk::ErodeObjectMorphologyImageFilter<ImageType, ImageType, StructuringElementType>;
using BinaryDilateImageFilterType = itk::DilateObjectMorphologyImageFilter<ImageType, ImageType, StructuringElementType>;
using BinaryOpenImageFilterType = itk::BinaryMorphologicalOpeningImageFilter<ImageType, ImageType, StructuringElementType>;
using BinaryCloseImageFilterType = itk::BinaryMorphologicalClosingImageFilter<ImageType, ImageType, StructuringElementType>;

using FloatImageType = itk::Image<float, 3>;
using WatershedFilterType = itk::MorphologicalWatershedImageFilter<FloatImageType, ImageType>;
using DanielssonFilterType = itk::SignedDanielssonDistanceMapImageFilter<ImageType, FloatImageType>;
using ConverterType = itk::LabelImageToLabelMapFilter<ImageType, LabelMapType>;

using ChangeInfoType = itk::ChangeInformationImageFilter<ImageType>;
using ChangeType = itk::ChangeLabelLabelMapFilter<LabelMapType>;
using LabelMapToImageFilterType = itk::LabelMapToLabelImageFilter<LabelMapType, ImageType>;
using WriterType = itk::ImageFileWriter<ImageType>;

///////////////////////////////////////////////////////////////////////////////////////////////////
// EditorOperations class
//
EditorOperations::EditorOperations(std::shared_ptr<DataManager> dataManager)
: m_orientation   {nullptr}
, m_dataManager   {dataManager}
, m_selection     {nullptr}
, m_progress      {nullptr}
, m_radius        {1}
, m_watershedLevel{0.5}
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::Initialize(vtkSmartPointer<vtkRenderer> renderer, std::shared_ptr<Coordinates> orientation,
    std::shared_ptr<ProgressAccumulator> progress)
{
  m_orientation = orientation;
  m_progress = progress;

  m_selection = std::make_shared<Selection>();
  m_selection->initialize(orientation, renderer, m_dataManager);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::AddSelectionPoint(const Vector3ui &point)
{
  m_selection->addSelectionPoint(point);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::AddContourPoint(const Vector3ui &point, std::shared_ptr<SliceVisualization> sliceView)
{
  m_selection->addContourInitialPoint(point, sliceView);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::ItkImageToPoints(itk::SmartPointer<ImageType> image)
{
  // iterate over the itk image as fast as possible and just hope that the
  // vtk image has the scallars allocated in the same axis order than the
  // itk one
  ConstIteratorType it(image, image->GetLargestPossibleRegion());

  // copy itk image to structuredpoints
  ImageType::IndexType index;
  for (it.GoToBegin(); !it.IsAtEnd(); ++it)
  {
    index = it.GetIndex();
    m_dataManager->SetVoxelScalar(Vector3ui{index[0], index[1], index[2]}, it.Get());
  }
  m_dataManager->SignalDataAsModified();

  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::Cut(std::set<unsigned short> labels)
{
  if (labels.empty()) return;

  m_progress->ManualSet("Cut");
  m_dataManager->OperationStart("Cut");

  Vector3ui min;
  Vector3ui max;

  switch (m_selection->type())
  {
    case Selection::Type::DISC:
    case Selection::Type::EMPTY:
      for (auto it: labels)
      {
        min = m_dataManager->GetBoundingBoxMin(it);
        max = m_dataManager->GetBoundingBoxMax(it);
        for (unsigned int x = min[0]; x <= max[0]; x++)
          for (unsigned int y = min[1]; y <= max[1]; y++)
            for (unsigned int z = min[2]; z <= max[2]; z++)
            {
              Vector3ui point{x,y,z};
              if (it == m_dataManager->GetVoxelScalar(point)) m_dataManager->SetVoxelScalar(point, 0);
            }
      }
      break;
    case Selection::Type::VOLUME:
      min = m_selection->minimumBouds();
      max = m_selection->maximumBouds();
      for (unsigned int x = min[0]; x <= max[0]; x++)
        for (unsigned int y = min[1]; y <= max[1]; y++)
          for (unsigned int z = min[2]; z <= max[2]; z++)
          {
            Vector3ui point{x,y,z};
            if (m_selection->isInsideSelection(point)) m_dataManager->SetVoxelScalar(point, 0);
          }
      break;
    case Selection::Type::CONTOUR:
      for (auto it: labels)
      {
        min = m_selection->minimumBouds();
        max = m_selection->maximumBouds();
        for (unsigned int x = min[0]; x <= max[0]; x++)
        {
          for (unsigned int y = min[1]; y <= max[1]; y++)
          {
            for (unsigned int z = min[2]; z <= max[2]; z++)
            {
              Vector3ui point{x,y,z};
              if (m_selection->isInsideSelection(point))
              {
                Vector3ui point{x,y,z};
                if (labels.find(m_dataManager->GetVoxelScalar(point)) != labels.end()) m_dataManager->SetVoxelScalar(point, 0);
              }
            }
          }
        }
      }
      break;
    case Selection::Type::CUBE:
      for (auto it: labels)
      {
        min = m_selection->minimumBouds();
        max = m_selection->maximumBouds();
        for (unsigned int x = min[0]; x <= max[0]; x++)
        {
          for (unsigned int y = min[1]; y <= max[1]; y++)
          {
            for (unsigned int z = min[2]; z <= max[2]; z++)
            {
              Vector3ui point{x,y,z};
              if (labels.find(m_dataManager->GetVoxelScalar(point)) != labels.end()) m_dataManager->SetVoxelScalar(point, 0);
            }
          }
        }
      }
      break;
    default:
      break;
  }

  m_dataManager->SignalDataAsModified();

  m_progress->ManualReset();
  m_dataManager->OperationEnd();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool EditorOperations::Relabel(QWidget *parent, std::shared_ptr<Metadata> data, std::set<unsigned short> *labels, bool *isANewColor)
{
  QtRelabel configdialog(parent);
  configdialog.setInitialOptions(*labels, data, m_dataManager);
  configdialog.exec();

  if (!configdialog.isModified()) return false;

  m_dataManager->OperationStart("Relabel");

  unsigned short newlabel;

  if (!configdialog.isNewLabel())
  {
    newlabel = configdialog.selectedLabel();
  }
  else
  {
    QtColorPicker colorpicker(parent);
    colorpicker.SetInitialOptions(m_dataManager);
    colorpicker.exec();

    if (!colorpicker.ModifiedData()) return false;

    auto color = colorpicker.GetColor();

    newlabel = m_dataManager->SetLabel(color);
    *isANewColor = true;
  }

  m_progress->ManualSet("Relabel");

  Vector3ui min;
  Vector3ui max;

  switch (m_selection->type())
  {
    case Selection::Type::DISC:
    case Selection::Type::EMPTY:
    {
      for (auto it: *labels)
      {
        min = m_dataManager->GetBoundingBoxMin(it);
        max = m_dataManager->GetBoundingBoxMax(it);
        for (unsigned int x = min[0]; x <= max[0]; x++)
        {
          for (unsigned int y = min[1]; y <= max[1]; y++)
          {
            for (unsigned int z = min[2]; z <= max[2]; z++)
            {
              Vector3ui point{x,y,z};
              if (it == m_dataManager->GetVoxelScalar(point)) m_dataManager->SetVoxelScalar(point, newlabel);
            }
          }
        }
      }
    }
      break;
    case Selection::Type::VOLUME:
      min = m_selection->minimumBouds();
      max = m_selection->maximumBouds();
      for (unsigned int x = min[0]; x <= max[0]; x++)
      {
        for (unsigned int y = min[1]; y <= max[1]; y++)
        {
          for (unsigned int z = min[2]; z <= max[2]; z++)
          {
            Vector3ui point{x,y,z};
            if (m_selection->isInsideSelection(point)) m_dataManager->SetVoxelScalar(point, newlabel);
          }
        }
      }
      break;
    case Selection::Type::CONTOUR:
      if (labels->empty()) labels->insert(0);

      min = m_selection->minimumBouds();
      max = m_selection->maximumBouds();
      for (unsigned int x = min[0]; x <= max[0]; x++)
      {
        for (unsigned int y = min[1]; y <= max[1]; y++)
        {
          for (unsigned int z = min[2]; z <= max[2]; z++)
          {
            Vector3ui point{x,y,z};
            if (m_selection->isInsideSelection(point))
            {
              if (labels->find(m_dataManager->GetVoxelScalar(point)) != labels->end()) m_dataManager->SetVoxelScalar(point, newlabel);
            }
          }
        }
      }
      break;
    case Selection::Type::CUBE:
      if (labels->empty()) labels->insert(0);

      min = m_selection->minimumBouds();
      max = m_selection->maximumBouds();
      for (unsigned int x = min[0]; x <= max[0]; x++)
      {
        for (unsigned int y = min[1]; y <= max[1]; y++)
        {
          for (unsigned int z = min[2]; z <= max[2]; z++)
          {
            Vector3ui point{x,y,z};
            if (labels->find(m_dataManager->GetVoxelScalar(point)) != labels->end()) m_dataManager->SetVoxelScalar(point, newlabel);
          }
        }
      }
      break;
    default:
      break;
  }
  m_dataManager->SignalDataAsModified();

  labels->clear();
  labels->insert(newlabel);
  m_progress->ManualReset();
  m_dataManager->OperationEnd();
  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::Erode(const unsigned short label)
{
  if (0 == label) return;

  m_dataManager->OperationStart("Erode");

  auto erodeFilter = BinaryErodeImageFilterType::New();

  m_progress->Observe(erodeFilter, "Erode", 1.0);

  auto image = ImageType::New();
  image = m_selection->itkImage(label, m_radius);

  // BEWARE: radius on erode is _filtersRadius-1 because erode seems to be too strong. it seems to work fine with that value.
  //		   the less it can be is 0 as _filterRadius >= 1 always. It erodes the volume with a value of 0, although using 0
  //         with dilate produces no dilate effect.
  StructuringElementType structuringElement;
  structuringElement.SetRadius(m_radius - 1);
  structuringElement.CreateStructuringElement();

  erodeFilter->SetInput(image);
  erodeFilter->SetKernel(structuringElement);
  erodeFilter->SetObjectValue(label);
  erodeFilter->ReleaseDataFlagOn();

  try
  {
    erodeFilter->Update();
  }
  catch (itk::ExceptionObject & excp)
  {
    m_progress->Ignore(erodeFilter);
    EditorError(excp);
    return;
  }

  ItkImageToPoints(erodeFilter->GetOutput());

  m_progress->Ignore(erodeFilter);
  m_progress->Reset();
  m_dataManager->OperationEnd();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::Dilate(const unsigned short label)
{
  if (0 == label) return;

  m_dataManager->OperationStart("Dilate");

  auto dilateFilter = BinaryDilateImageFilterType::New();

  m_progress->Observe(dilateFilter, "Dilate", 1.0);

  itk::SmartPointer<ImageType> image = ImageType::New();
  image = m_selection->itkImage(label, m_radius);

  StructuringElementType structuringElement;
  structuringElement.SetRadius(m_radius);
  structuringElement.CreateStructuringElement();

  dilateFilter->SetInput(image);
  dilateFilter->SetKernel(structuringElement);
  dilateFilter->SetObjectValue(label);
  dilateFilter->ReleaseDataFlagOn();

  try
  {
    dilateFilter->Update();
  }
  catch (itk::ExceptionObject & excp)
  {
    m_progress->Ignore(dilateFilter);
    EditorError(excp);
    return;
  }

  ItkImageToPoints(dilateFilter->GetOutput());

  m_progress->Ignore(dilateFilter);
  m_progress->Reset();
  m_dataManager->OperationEnd();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::Open(const unsigned short label)
{
  if (0 == label) return;

  m_dataManager->OperationStart("Open");

  auto openFilter = BinaryOpenImageFilterType::New();

  m_progress->Observe(openFilter, "Open", 1.0);

  itk::SmartPointer<ImageType> image = ImageType::New();
  image = m_selection->itkImage(label, m_radius);

  StructuringElementType structuringElement;
  structuringElement.SetRadius(m_radius);
  structuringElement.CreateStructuringElement();

  openFilter->SetInput(image);
  openFilter->SetKernel(structuringElement);
  openFilter->SetForegroundValue(label);
  openFilter->ReleaseDataFlagOn();

  try
  {
    openFilter->Update();
  }
  catch (itk::ExceptionObject & excp)
  {
    m_progress->Ignore(openFilter);
    EditorError(excp);
    return;
  }

  ItkImageToPoints(openFilter->GetOutput());

  m_progress->Ignore(openFilter);
  m_progress->Reset();
  m_dataManager->OperationEnd();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::Close(const unsigned short label)
{
  if (0 == label) return;

  m_dataManager->OperationStart("Close");

  auto closeFilter = BinaryCloseImageFilterType::New();

  m_progress->Observe(closeFilter, "Close", 1.0);

  itk::SmartPointer<ImageType> image = ImageType::New();
  image = m_selection->itkImage(label, m_radius);

  StructuringElementType structuringElement;
  structuringElement.SetRadius(m_radius);
  structuringElement.CreateStructuringElement();

  closeFilter->SetInput(image);
  closeFilter->SetKernel(structuringElement);
  closeFilter->SetForegroundValue(label);
  closeFilter->ReleaseDataFlagOn();

  try
  {
    closeFilter->Update();
  }
  catch (itk::ExceptionObject & excp)
  {
    m_progress->Ignore(closeFilter);
    EditorError(excp);
    return;
  }

  ItkImageToPoints(closeFilter->GetOutput());

  m_progress->Ignore(closeFilter);
  m_progress->Reset();
  m_dataManager->OperationEnd();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
std::set<unsigned short> EditorOperations::Watershed(const unsigned short label)
{
  std::set<unsigned short> createdLabels;
  if (0 == label) return createdLabels;

  m_dataManager->OperationStart("Watershed");

  auto image = ImageType::New();
  image = m_selection->itkImage(label, 0);

  auto danielssonFilter = DanielssonFilterType::New();
  m_progress->Observe(danielssonFilter, "Danielsson", (1.0 / 3.0));

  danielssonFilter->SetInput(image);
  danielssonFilter->SetInsideIsPositive(false);
  danielssonFilter->SetSquaredDistance(false);
  danielssonFilter->SetUseImageSpacing(true);
  danielssonFilter->ReleaseDataFlagOn();

  try
  {
    danielssonFilter->Update();
  }
  catch (itk::ExceptionObject & excp)
  {
    m_progress->Ignore(danielssonFilter);
    EditorError(excp);
    return createdLabels;
  }

  m_progress->Ignore(danielssonFilter);

  auto watershedFilter = WatershedFilterType::New();
  m_progress->Observe(watershedFilter, "Watershed", (1.0 / 3.0));

  watershedFilter->SetInput(danielssonFilter->GetOutput());
  watershedFilter->SetLevel(m_watershedLevel);
  watershedFilter->SetMarkWatershedLine(false);
  watershedFilter->SetFullyConnected(false);
  watershedFilter->ReleaseDataFlagOn();

  try
  {
    watershedFilter->Update();
  }
  catch (itk::ExceptionObject & excp)
  {
    m_progress->Ignore(watershedFilter);
    EditorError(excp);
    return createdLabels;
  }

  m_progress->Ignore(watershedFilter);

  // we need only the points of our volume, not the background
  CleanImage(watershedFilter->GetOutput(), label);

  auto converter = ConverterType::New();
  m_progress->Observe(converter, "Convert", (1.0 / 3.0));
  converter->SetInput(watershedFilter->GetOutput());
  converter->ReleaseDataFlagOn();

  try
  {
    converter->Update();
  }
  catch (itk::ExceptionObject & excp)
  {
    m_progress->Ignore(converter);
    EditorError(excp);
    return createdLabels;
  }

  m_progress->Ignore(converter);

  auto outputLabelMap = converter->GetOutput();
  outputLabelMap->Optimize();

  std::srand(static_cast<unsigned int>(std::time(nullptr)));
  for (int i = 0; i < outputLabelMap->GetNumberOfLabelObjects(); ++i)
  {
    auto labelObject = outputLabelMap->GetNthLabelObject(i);

    unsigned short newlabel = 0;
    bool found = false;

    // create a random color and make sure it's a new one (very small probability but we have to check anyways...)
    while (found == false)
    {
      auto color = QColor::fromRgbF(((double) rand() / (double) RAND_MAX), ((double) rand() / (double) RAND_MAX), ((double) rand() / (double) RAND_MAX));

      if (false == m_dataManager->ColorIsInUse(color))
      {
        newlabel = m_dataManager->SetLabel(color);
        createdLabels.insert(newlabel);
        found = true;
      }
    }

    for (int j = 0; j < labelObject->GetNumberOfLines(); ++j)
    {
      auto line = labelObject->GetLine(j);
      auto firstIdx = line.GetIndex();
      auto length = line.GetLength();
      auto endIdx0 = firstIdx[0] + length;
      for (LabelMapType::IndexType idx = firstIdx; idx[0] < endIdx0; idx[0]++)
      {
        assert((idx[0] >= 0) && (idx[1] >= 0) && (idx[2] >= 0));
        m_dataManager->SetVoxelScalar(Vector3ui{idx[0], idx[1], idx[2]}, newlabel);
      }
    }
  }
  m_dataManager->SignalDataAsModified();

  m_progress->Reset();
  m_dataManager->OperationEnd();
  return createdLabels;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::CleanImage(itk::SmartPointer<ImageType> image, const unsigned short label) const
{
  ConstIteratorType it(image, image->GetLargestPossibleRegion());

  ImageType::IndexType index;
  for (it.GoToBegin(); !it.IsAtEnd(); ++it)
  {
    index = it.GetIndex();
    unsigned short pointScalar = m_dataManager->GetVoxelScalar(Vector3ui{index[0], index[1], index[2]});

    switch (label)
    {
      case 0:
        if (0 == pointScalar) image->SetPixel(index, 0);
        break;
      default:
        if (pointScalar != label) image->SetPixel(index, 0);
        break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::EditorError(itk::ExceptionObject &excp) const
{
  m_progress->Reset();

  QMessageBox msgBox;
  msgBox.setIcon(QMessageBox::Critical);

  auto text = std::string("An error occurred.\nThe ");
  text += m_dataManager->GetActualActionString();
  text += std::string(" operation has been aborted.");
  msgBox.setCaption("Error");
  msgBox.setText(text.c_str());
  msgBox.setDetailedText(excp.what());
  msgBox.exec();

  m_dataManager->OperationCancel();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::SaveImage(const std::string &filename) const
{
  m_progress->ManualSet("Save Image");

  auto image = ImageType::New();
  image = m_selection->itkImage();

  // must restore the image origin before writing
  auto point = m_orientation->GetImageOrigin();

  auto infoChanger = ChangeInfoType::New();
  infoChanger->SetInput(image);
  infoChanger->ChangeOriginOn();
  infoChanger->ReleaseDataFlagOn();

  ImageType::PointType newOrigin;
  newOrigin[0] = point[0];
  newOrigin[1] = point[1];
  newOrigin[2] = point[2];
  infoChanger->SetOutputOrigin(newOrigin);
  m_progress->Observe(infoChanger, "Fix Image", 0.2);
  infoChanger->Update();
  m_progress->Ignore(infoChanger);

  // convert to labelmap and restore original scalars for labels
  auto converter = ConverterType::New();
  converter->SetInput(infoChanger->GetOutput());
  converter->ReleaseDataFlagOn();
  m_progress->Observe(converter, "Label Map", 0.2);
  converter->Update();
  m_progress->Ignore(converter);
  converter->GetOutput()->Optimize();

  if (0 == converter->GetOutput()->GetNumberOfLabelObjects())
  {
    QMessageBox msgBox;
    msgBox.setCaption("Error trying to save image");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("There are no segmentations in the image. Not saving an empty image.");
    msgBox.exec();
    m_progress->ManualReset();
    return;
  }

  auto labelChanger = ChangeType::New();
  labelChanger->SetInput(converter->GetOutput());
  labelChanger->ReleaseDataFlagOn();

  if (labelChanger->CanRunInPlace() == true)
  {
    labelChanger->SetInPlace(true);
  }

  for (unsigned short i = 1; i < m_dataManager->GetNumberOfLabels(); i++)
  {
    labelChanger->SetChange(i, m_dataManager->GetScalarForLabel(i));
  }

  m_progress->Observe(labelChanger, "Fix Labels", 0.2);
  labelChanger->Update();
  m_progress->Ignore(labelChanger);

  auto labelConverter = LabelMapToImageFilterType::New();
  labelConverter->SetInput(labelChanger->GetOutput());
  labelConverter->SetNumberOfThreads(1);
  labelConverter->ReleaseDataFlagOn();

  m_progress->Observe(labelConverter, "Convert Image", 0.2);
  labelConverter->Update();
  m_progress->Ignore(labelConverter);

  // save as an mha and rename
  auto tempfilename = filename + std::string(".mha");
  auto io = itk::MetaImageIO::New();
  io->SetFileName(tempfilename.c_str());
  auto writer = WriterType::New();
  writer->SetImageIO(io);
  writer->SetFileName(tempfilename.c_str());
  writer->SetInput(labelConverter->GetOutput());
  writer->UseCompressionOn();
  m_progress->Observe(writer, "Write", 0.2);

  try
  {
    writer->Write();
  }
  catch (itk::ExceptionObject &excp)
  {
    QMessageBox msgBox;
    msgBox.setCaption("Error trying to save image");
    msgBox.setIcon(QMessageBox::Critical);
    auto text = std::string("An error occurred saving the segmentation file.\nThe operation has been aborted.");
    msgBox.setText(text.c_str());
    msgBox.setDetailedText(excp.what());
    msgBox.exec();
    m_progress->Ignore(writer);
    m_progress->ManualReset();
    return;
  }

  if (0 != (rename(tempfilename.c_str(), filename.c_str())))
  {
    QMessageBox msgBox;
    msgBox.setCaption("Error trying to rename a file");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("An error occurred saving the segmentation file.\nThe operation has been aborted.");
    msgBox.setDetailedText(QString("The temporal file couldn't be renamed."));
    msgBox.exec();

    if (0 != (remove(tempfilename.c_str())))
    {
      QMessageBox msgBox;
      msgBox.setCaption("Error trying to delete a file");
      msgBox.setIcon(QMessageBox::Critical);
      auto text = std::string("The temporal file \"");
      text += tempfilename;
      text += std::string("\" couldn't be deleted.");
      msgBox.setText(text.c_str());
      msgBox.exec();
    }
  }

  m_progress->Ignore(writer);
  m_progress->ManualReset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
itk::SmartPointer<LabelMapType> EditorOperations::GetImageLabelMap() const
{
  auto image = ImageType::New();
  image = m_selection->itkImage();

  auto converter = ConverterType::New();
  m_progress->Observe(converter, "Convert", (1.0 / 1.0));
  converter->SetInput(image);

  try
  {
    converter->Update();
  }
  catch (itk::ExceptionObject & excp)
  {
    m_progress->Ignore(converter);
    EditorError(excp);
    return NULL;
  }

  m_progress->Ignore(converter);
  auto outputLabelMap = converter->GetOutput();
  outputLabelMap->Register();

  return outputLabelMap;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::SetFirstFreeValue(const unsigned short value)
{
  m_dataManager->SetFirstFreeValue(value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned int EditorOperations::GetFiltersRadius(void) const
{
  return m_radius;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::SetFiltersRadius(const unsigned int value)
{
  m_radius = value;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const double EditorOperations::GetWatershedLevel(void) const
{
  return m_watershedLevel;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::SetWatershedLevel(const double value)
{
  m_watershedLevel = value;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::ContiguousAreaSelection(const Vector3ui &point)
{
  m_progress->ManualSet("Threshold");
  m_selection->addArea(point);
  m_progress->ManualReset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3ui EditorOperations::GetSelectedMinimumBouds() const
{
  return m_selection->minimumBouds();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const Vector3ui EditorOperations::GetSelectedMaximumBouds() const
{
  return m_selection->maximumBouds();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
const bool EditorOperations::IsSelectionEmpty() const
{
  return (Selection::Type::EMPTY == m_selection->type());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::SetSliceViews(std::shared_ptr<SliceVisualization> axial,
                                     std::shared_ptr<SliceVisualization> coronal,
                                     std::shared_ptr<SliceVisualization> sagittal)
{
  m_selection->setSliceViews(axial, coronal, sagittal);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::ClearSelection(void)
{
  m_selection->clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
Selection::Type EditorOperations::GetSelectionType(void) const
{
  return m_selection->type();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::UpdatePaintEraseActors(const Vector3i &point, int radius, std::shared_ptr<SliceVisualization> sliceView)
{
  m_selection->setSelectionDisc(point, radius, sliceView);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::UpdateContourSlice(const Vector3ui &point)
{
  if (m_selection && (m_selection->type() == Selection::Type::CONTOUR))
  {
    m_selection->updateContourSlice(point);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::Paint(const unsigned short label)
{
  if (Selection::Type::DISC == m_selection->type())
  {
    Vector3ui min = m_selection->minimumBouds();
    Vector3ui max = m_selection->maximumBouds();
    for (unsigned int x = min[0]; x <= max[0]; x++)
    {
      for (unsigned int y = min[1]; y <= max[1]; y++)
      {
        for (unsigned int z = min[2]; z <= max[2]; z++)
        {
          Vector3ui point{x,y,z};
          if (m_selection->isInsideSelection(point)) m_dataManager->SetVoxelScalar(point, label);
        }
      }
    }

    m_dataManager->SignalDataAsModified();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void EditorOperations::Erase(const std::set<unsigned short> labels)
{
  if (Selection::Type::DISC == m_selection->type())
  {
    Vector3ui min = m_selection->minimumBouds();
    Vector3ui max = m_selection->maximumBouds();
    for (unsigned int x = min[0]; x <= max[0]; x++)
    {
      for (unsigned int y = min[1]; y <= max[1]; y++)
      {
        for (unsigned int z = min[2]; z <= max[2]; z++)
        {
          Vector3ui point{x,y,z};
          if (m_selection->isInsideSelection(point))
          {
            if (labels.find(m_dataManager->GetVoxelScalar(point)) != labels.end()) m_dataManager->SetVoxelScalar(point, 0);
          }
        }
      }
    }

    m_dataManager->SignalDataAsModified();
  }
}
