///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: EditorOperations.cxx
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

// defines & typedefs
typedef itk::ShapeLabelObject< unsigned short, 3 > LabelObjectType;
typedef itk::LabelMap< LabelObjectType > LabelMapType;

///////////////////////////////////////////////////////////////////////////////////////////////////
// EditorOperations class
//
EditorOperations::EditorOperations(DataManager *data)
{
    this->_dataManager = data;
    this->_orientation = NULL;
    this->_progress = NULL;
    this->_selection = NULL;
    
    // default options for filters
    this->_filtersRadius = 1;
    this->_watershedLevel = 0.5;
}

EditorOperations::~EditorOperations()
{
	delete this->_selection;
}

void EditorOperations::Initialize(vtkSmartPointer<vtkRenderer> renderer, Coordinates *orientation, ProgressAccumulator *progress)
{
	this->_orientation = orientation;
	this->_progress = progress;
    
    // initial selection actor is invisible
	this->_selection = new Selection();
	this->_selection->Initialize(orientation, renderer, this->_dataManager);
}

void EditorOperations::AddSelectionPoint(const Vector3ui point)
{
	this->_selection->AddSelectionPoint(point);
}

void EditorOperations::ItkImageToPoints(itk::SmartPointer<ImageType> image)
{
	// iterate over the itk image as fast as possible and just hope that the
	// vtk image has the scallars allocated in the same axis order than the
	// itk one
	typedef itk::ImageRegionConstIteratorWithIndex<ImageType> IteratorType;
	IteratorType it(image, image->GetRequestedRegion());
    
    // copy itk image to structuredpoints
	ImageType::IndexType index;
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    	index = it.GetIndex();
    	_dataManager->SetVoxelScalar(index[0], index[1], index[2], it.Get());
    }
    
    return;
}

void EditorOperations::Cut(std::set<unsigned short> labels)
{
    _progress->ManualSet("Cut");
    _dataManager->OperationStart("Cut");
    
    Vector3ui min;
    Vector3ui max;
    std::set<unsigned short>::iterator it;

    switch(this->_selection->GetSelectionType())
    {
    	case Selection::EMPTY:
    		if (labels.empty())
    			return;

    		for (it = labels.begin(); it != labels.end(); it++)
    		{
				min = this->_dataManager->GetBoundingBoxMin(*it);
				max = this->_dataManager->GetBoundingBoxMax(*it);
				for (unsigned int x = min[0]; x <= max[0]; x++)
					for (unsigned int y = min[1]; y <= max[1]; y++)
						for (unsigned int z = min[2]; z <= max[2]; z++)
							if (labels.find(_dataManager->GetVoxelScalar(x, y, z)) != labels.end())
								_dataManager->SetVoxelScalar(x, y, z, 0);
    		}
    		break;
    	case Selection::VOLUME:
    		min = this->_selection->GetSelectedMinimumBouds();
    		max = this->_selection->GetSelectedMaximumBouds();
    	    for (unsigned int x = min[0]; x <= max[0]; x++)
    			for (unsigned int y = min[1]; y <= max[1]; y++)
    				for (unsigned int z = min[2]; z <= max[2]; z++)
    					if (this->_selection->VoxelIsInsideSelection(x, y, z))
    						_dataManager->SetVoxelScalar(x, y, z, 0);
    		break;
    	case Selection::CUBE:
    		if (labels.empty())
    			return;

    		for (it = labels.begin(); it != labels.end(); it++)
    		{
				min = this->_selection->GetSelectedMinimumBouds();
				max = this->_selection->GetSelectedMaximumBouds();
				for (unsigned int x = min[0]; x <= max[0]; x++)
					for (unsigned int y = min[1]; y <= max[1]; y++)
						for (unsigned int z = min[2]; z <= max[2]; z++)
							if (labels.find(_dataManager->GetVoxelScalar(x, y, z)) != labels.end())
								_dataManager->SetVoxelScalar(x, y, z, 0);
    		}
    		break;
    	default: // can't happen
    		break;
    }
    _progress->ManualReset();
    _dataManager->OperationEnd();
}

// BEWARE: new label scalar and boolean indicating if it's a new color returned by reference
// (i like this better than returning a struct, but who cares...)
bool EditorOperations::Relabel(QWidget *parent, Metadata *data, std::set<unsigned short> *labels, bool *isANewColor)
{
    Vector3d color;
    
    QtRelabel configdialog(parent);
    configdialog.SetInitialOptions(*labels, data, _dataManager);
    configdialog.exec();
    
    if (!configdialog.ModifiedData())
        return false;

    _dataManager->OperationStart("Relabel");

    unsigned short newlabel;
    
    if (!configdialog.IsNewLabel())
        newlabel = configdialog.GetSelectedLabel();
    else
    {
        QtColorPicker colorpicker(parent);
        colorpicker.SetInitialOptions(_dataManager);
        colorpicker.exec();

        if (!colorpicker.ModifiedData())
            return false;

        color = colorpicker.GetColor();

        newlabel = _dataManager->SetLabel(color);
        *isANewColor = true;
    }

    _progress->ManualSet("Relabel");
    
    Vector3ui min;
    Vector3ui max;
    std::set<unsigned short>::iterator it;

    switch(this->_selection->GetSelectionType())
    {
    	case Selection::EMPTY:
    		for (it = labels->begin(); it != labels->end(); it++)
    		{
				min = this->_dataManager->GetBoundingBoxMin(*it);
				max = this->_dataManager->GetBoundingBoxMax(*it);
				for (unsigned int x = min[0]; x <= max[0]; x++)
					for (unsigned int y = min[1]; y <= max[1]; y++)
						for (unsigned int z = min[2]; z <= max[2]; z++)
							if (*it == _dataManager->GetVoxelScalar(x, y, z))
								_dataManager->SetVoxelScalar(x, y, z, newlabel);
    		}
    		break;
    	case Selection::VOLUME:
    		min = this->_selection->GetSelectedMinimumBouds();
    		max = this->_selection->GetSelectedMaximumBouds();
    	    for (unsigned int x = min[0]; x <= max[0]; x++)
    			for (unsigned int y = min[1]; y <= max[1]; y++)
    				for (unsigned int z = min[2]; z <= max[2]; z++)
    					if (this->_selection->VoxelIsInsideSelection(x, y, z))
    						_dataManager->SetVoxelScalar(x, y, z, newlabel);
    		break;
    	case Selection::CUBE:
    		min = this->_selection->GetSelectedMinimumBouds();
    		max = this->_selection->GetSelectedMaximumBouds();
    	    for (unsigned int x = min[0]; x <= max[0]; x++)
    			for (unsigned int y = min[1]; y <= max[1]; y++)
    				for (unsigned int z = min[2]; z <= max[2]; z++)
						if (labels->find(_dataManager->GetVoxelScalar(x, y, z)) != labels->end())
							_dataManager->SetVoxelScalar(x, y, z, newlabel);
    		break;
    	default: // can't happen
    		break;
    }

    labels->clear();
    labels->insert(newlabel);
    _progress->ManualReset();
    _dataManager->OperationEnd();
    return true;
}

void EditorOperations::Erode(const unsigned short label)
{
	if (0 == label)
		return;

    _dataManager->OperationStart("Erode");

    typedef itk::BinaryBallStructuringElement<ImageType::PixelType, 3> StructuringElementType;
    typedef itk::ErodeObjectMorphologyImageFilter<ImageType, ImageType, StructuringElementType> BinaryErodeImageFilterType;
    itk::SmartPointer<BinaryErodeImageFilterType> erodeFilter = BinaryErodeImageFilterType::New();

    _progress->Observe(erodeFilter, "Erode", 1.0);

    itk::SmartPointer<ImageType> image = ImageType::New();
    image = this->_selection->GetSelectionItkImage(label, _filtersRadius);
    
    // BEWARE: radius on erode is _filtersRadius-1 because erode seems to be too strong. it seems to work fine with that value.
    //		   the less it can be is 0 as _filterRadius >= 1 always. It erodes the volume with a value of 0, although using 0
    //         with dilate produces no dilate effect.
    StructuringElementType structuringElement;
    structuringElement.SetRadius(_filtersRadius-1);
    structuringElement.CreateStructuringElement();

    erodeFilter->SetInput(image);
    erodeFilter->SetKernel(structuringElement);
    erodeFilter->SetObjectValue(label);
    erodeFilter->ReleaseDataFlagOn();
    
    try
    {
        erodeFilter->Update();
    } catch (itk::ExceptionObject & excp)
    {
        _progress->Ignore(erodeFilter);
        EditorError(excp);
        return;
    }
    
    ItkImageToPoints(erodeFilter->GetOutput());
    
    _progress->Ignore(erodeFilter);
    _progress->Reset();
    _dataManager->OperationEnd();
    return;
}

void EditorOperations::Dilate(const unsigned short label)
{
	if (0 == label)
		return;

    _dataManager->OperationStart("Dilate");

    typedef itk::BinaryBallStructuringElement<ImageType::PixelType, 3> StructuringElementType;
    typedef itk::DilateObjectMorphologyImageFilter<ImageType, ImageType, StructuringElementType> BinaryDilateImageFilterType;
    itk::SmartPointer<BinaryDilateImageFilterType> dilateFilter = BinaryDilateImageFilterType::New();

    _progress->Observe(dilateFilter, "Dilate", 1.0);
    
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = this->_selection->GetSelectionItkImage(label, _filtersRadius);

    StructuringElementType structuringElement;
    structuringElement.SetRadius(_filtersRadius);
    structuringElement.CreateStructuringElement();

    dilateFilter->SetInput(image);
    dilateFilter->SetKernel(structuringElement);
    dilateFilter->SetObjectValue(label);
    dilateFilter->ReleaseDataFlagOn();
    
    try
    {
        dilateFilter->Update();
    } catch (itk::ExceptionObject & excp)
    {
        _progress->Ignore(dilateFilter);
        EditorError(excp);
        return;
    }

    ItkImageToPoints(dilateFilter->GetOutput());
    
    _progress->Ignore(dilateFilter);
    _progress->Reset();
    _dataManager->OperationEnd();
    return;
}

void EditorOperations::Open(const unsigned short label)
{
	if (0 == label)
		return;

    _dataManager->OperationStart("Open");

    typedef itk::BinaryBallStructuringElement<ImageType::PixelType, 3> StructuringElementType;
    typedef itk::BinaryMorphologicalOpeningImageFilter<ImageType, ImageType, StructuringElementType> BinaryOpenImageFilterType;
    itk::SmartPointer<BinaryOpenImageFilterType> openFilter = BinaryOpenImageFilterType::New();

    _progress->Observe(openFilter, "Open", 1.0);
    
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = this->_selection->GetSelectionItkImage(label, _filtersRadius);

    StructuringElementType structuringElement;
    structuringElement.SetRadius(_filtersRadius); 
    structuringElement.CreateStructuringElement();

    openFilter->SetInput(image);
    openFilter->SetKernel(structuringElement);
    openFilter->SetForegroundValue(label);
    openFilter->ReleaseDataFlagOn();
    
    try
    {
        openFilter->Update();
    } catch (itk::ExceptionObject & excp)
    {
        _progress->Ignore(openFilter);
        EditorError(excp);
        return;
    }
    
    ItkImageToPoints(openFilter->GetOutput());
    
    _progress->Ignore(openFilter);
    _progress->Reset();
    _dataManager->OperationEnd();
    return;
}

void EditorOperations::Close(const unsigned short label)
{
	if (0 == label)
		return;

    _dataManager->OperationStart("Close");

    typedef itk::BinaryBallStructuringElement<ImageType::PixelType, 3> StructuringElementType;
    typedef itk::BinaryMorphologicalClosingImageFilter<ImageType, ImageType, StructuringElementType> BinaryCloseImageFilterType;
    itk::SmartPointer<BinaryCloseImageFilterType> closeFilter = BinaryCloseImageFilterType::New();

    _progress->Observe(closeFilter, "Close", 1.0);
    
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = this->_selection->GetSelectionItkImage(label, _filtersRadius);

    StructuringElementType structuringElement;
    structuringElement.SetRadius(_filtersRadius);
    structuringElement.CreateStructuringElement();

    closeFilter->SetInput(image);
    closeFilter->SetKernel(structuringElement);
    closeFilter->SetForegroundValue(label);
    closeFilter->ReleaseDataFlagOn();
    
    try
    {
        closeFilter->Update();
    } catch (itk::ExceptionObject & excp)
    {
        _progress->Ignore(closeFilter);
        EditorError(excp);
        return;
    }
    
    ItkImageToPoints(closeFilter->GetOutput());
    
    _progress->Ignore(closeFilter);
    _progress->Reset();
    _dataManager->OperationEnd();
    return;
}

void EditorOperations::Watershed(const unsigned short label)
{
	if (0 == label)
		return;

    _dataManager->OperationStart("Watershed");
    
    typedef itk::Image<float,3> FloatImageType;
    typedef itk::MorphologicalWatershedImageFilter<FloatImageType, ImageType> WatershedFilterType;
    typedef itk::SignedDanielssonDistanceMapImageFilter<ImageType, FloatImageType> DanielssonFilterType;
    
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = this->_selection->GetSelectionItkImage(label, 0);
    
    itk::SmartPointer<DanielssonFilterType> danielssonFilter = DanielssonFilterType::New();
    _progress->Observe(danielssonFilter, "Danielsson", (1.0/3.0));
    
    danielssonFilter->SetInput(image);
    danielssonFilter->SetInsideIsPositive(false);
    danielssonFilter->SetSquaredDistance(false);
    danielssonFilter->SetUseImageSpacing(true);
    danielssonFilter->ReleaseDataFlagOn();
    
    try
    {
        danielssonFilter->Update();
    } catch (itk::ExceptionObject & excp)
    {
        _progress->Ignore(danielssonFilter);
        EditorError(excp);
        return;
    }
    
    _progress->Ignore(danielssonFilter);
    
    itk::SmartPointer<WatershedFilterType> watershedFilter = WatershedFilterType::New();
    _progress->Observe(watershedFilter, "Watershed", (1.0/3.0));
    
    watershedFilter->SetInput(danielssonFilter->GetOutput());
    watershedFilter->SetLevel(_watershedLevel);
    watershedFilter->SetMarkWatershedLine(false);
    watershedFilter->SetFullyConnected(false);
    watershedFilter->ReleaseDataFlagOn();
    
    try
    {
        watershedFilter->Update();
    } catch (itk::ExceptionObject & excp)
    {
        _progress->Ignore(watershedFilter);
        EditorError(excp);
        return;
    }
    
    _progress->Ignore(watershedFilter);
    
    // we need only the points of our volume, not the background
    CleanImage(watershedFilter->GetOutput(), label);

    typedef itk::LabelImageToLabelMapFilter<ImageType, LabelMapType> ConverterType;
    itk::SmartPointer<ConverterType> converter = ConverterType::New();
    _progress->Observe(converter, "Convert", (1.0/3.0));
    converter->SetInput(watershedFilter->GetOutput());
    converter->ReleaseDataFlagOn();
    
    try
    {
        converter->Update();
    } catch (itk::ExceptionObject & excp)
    {
        _progress->Ignore(converter);
        EditorError(excp);
        return;
    }
    
    _progress->Ignore(converter);
    
    itk::SmartPointer<LabelMapType> outputLabelMap = converter->GetOutput();
    outputLabelMap->Optimize();
    
    // enter the label points into vtk structured points
    LabelMapType::LabelObjectContainerType::const_iterator iter;
    const LabelMapType::LabelObjectContainerType & labelObjectContainer = outputLabelMap->GetLabelObjectContainer();

    // for the randomized label colors
    srand(time(NULL));
    for(iter = labelObjectContainer.begin(); iter != labelObjectContainer.end(); iter++)
    {
        unsigned short newlabel = 0;
        bool found = false;

        // create a random color and make sure it's a new one (very small probability but we have to check anyways...)
        while (found == false)
        {
            double color[3];
            color[0] = ((double)rand()/(double)RAND_MAX);
            color[1] = ((double)rand()/(double)RAND_MAX);
            color[2] = ((double)rand()/(double)RAND_MAX);

            if (false == _dataManager->ColorIsInUse(color))
            {
                newlabel = _dataManager->SetLabel(Vector3d(color[0], color[1], color[2]));
                found = true;
            }
        }
        
        LabelObjectType * labelObject = iter->second;
        LabelObjectType::LineContainerType::const_iterator liter;
        LabelObjectType::LineContainerType lineContainer = labelObject->GetLineContainer();
        for(liter = lineContainer.begin(); liter != lineContainer.end(); liter++ )
        {
            const LabelMapType::IndexType & firstIdx = liter->GetIndex();
            const unsigned long & length = liter->GetLength();
            long endIdx0 = firstIdx[0] + length;
            for(LabelMapType::IndexType idx = firstIdx; idx[0] < endIdx0; idx[0]++)
            {
                assert((idx[0] >= 0) && (idx[1] >= 0) && (idx[2] >= 0));
                _dataManager->SetVoxelScalar(idx[0], idx[1], idx[2], newlabel);
            }
        }
    }

    _progress->Reset();
    _dataManager->OperationEnd();
}

void EditorOperations::CleanImage(itk::SmartPointer<ImageType> image, const unsigned short label)
{
	typedef itk::ImageRegionConstIteratorWithIndex<ImageType> IteratorType;
	IteratorType it(image, image->GetRequestedRegion());

    // copy itk image to structuredpoints
	ImageType::IndexType index;
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
    	index = it.GetIndex();
    	unsigned short pointScalar = _dataManager->GetVoxelScalar(index[0], index[1], index[2]);

        switch(label)
        {
            case 0:
                if (0 == pointScalar)
                    image->SetPixel(index, 0);
                break;
            default:
                if (pointScalar != label)
                    image->SetPixel(index, 0);
                break;
        }
    }
}

void EditorOperations::EditorError(itk::ExceptionObject &excp)
{
    _progress->Reset();
    
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    
    std::string text = std::string("An error occurred.\nThe ");
    text += _dataManager->GetActualActionString();
    text += std::string(" operation has been aborted.");
    msgBox.setText(text.c_str());
    msgBox.setDetailedText(excp.what());
    msgBox.exec();
    
    _dataManager->OperationCancel();
    
    return;   
}

// originally the image was saved directly from the vtkStructuredPoints in memory but, although the
// segmha file saved was correct, the orientation of the image was saved as ???, this doen't happen
// with itk::ImageFileWriter as the orientation is saved correctly as RAI.
void EditorOperations::SaveImage(const std::string filename)
{
    _progress->ManualSet("Save Image");
    
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = this->_selection->GetItkImage();
    
    // must restore the image origin before writing
    Vector3d point = _orientation->GetImageOrigin();
    typedef itk::ChangeInformationImageFilter<ImageType> ChangeInfoType;
    itk::SmartPointer<ChangeInfoType> infoChanger = ChangeInfoType::New();
    infoChanger->SetInput(image);
    infoChanger->ChangeOriginOn();
    infoChanger->ReleaseDataFlagOn();
    ImageType::PointType newOrigin;
    newOrigin[0] = point[0];
    newOrigin[1] = point[1];
    newOrigin[2] = point[2];
    infoChanger->SetOutputOrigin(newOrigin);
    _progress->Observe(infoChanger,"Fix Image", 0.2);
    infoChanger->Update();
    _progress->Ignore(infoChanger);
    
    // convert to labelmap and restore original scalars for labels
	typedef itk::LabelImageToLabelMapFilter<ImageType, LabelMapType> ConverterType;
	itk::SmartPointer<ConverterType> converter = ConverterType::New();

	converter->SetInput(infoChanger->GetOutput());
	converter->ReleaseDataFlagOn();
	_progress->Observe(converter, "Label Map", 0.2);
	converter->Update();
	_progress->Ignore(converter);
	converter->GetOutput()->Optimize();

	if(0 == converter->GetOutput()->GetNumberOfLabelObjects())
	{
		QMessageBox msgBox;
		msgBox.setIcon(QMessageBox::Warning);
		msgBox.setText("There are no segmentations in the image. Not saving an empty image.");
		msgBox.exec();
		_progress->ManualReset();
		return;
	}

    typedef itk::ChangeLabelLabelMapFilter<LabelMapType> ChangeType;
    itk::SmartPointer<ChangeType> labelChanger = ChangeType::New();
    labelChanger->SetInput(converter->GetOutput());
    labelChanger->ReleaseDataFlagOn();
    if (labelChanger->CanRunInPlace() == true)
    	labelChanger->SetInPlace(true);

    for (unsigned short i = 1; i < _dataManager->GetNumberOfLabels(); i++)
    	labelChanger->SetChange(i,_dataManager->GetScalarForLabel(i));

    _progress->Observe(labelChanger, "Fix Labels", 0.2);
    labelChanger->Update();
    _progress->Ignore(labelChanger);

	// itklabelmap->itkimage
	typedef itk::LabelMapToLabelImageFilter<LabelMapType, ImageType> LabelMapToImageFilterType;
	itk::SmartPointer<LabelMapToImageFilterType> labelConverter = LabelMapToImageFilterType::New();

	labelConverter->SetInput(labelChanger->GetOutput());
	labelConverter->SetNumberOfThreads(1);
	labelConverter->ReleaseDataFlagOn();

	_progress->Observe(labelConverter, "Convert Image", 0.2);
	labelConverter->Update();
	_progress->Ignore(labelConverter);

    // save as an mha and rename
    std::string tempfilename = filename + std::string(".mha");
    typedef itk::ImageFileWriter<ImageType> WriterType;
    itk::SmartPointer<itk::MetaImageIO> io = itk::MetaImageIO::New();
    io->SetFileName(tempfilename.c_str());
    itk::SmartPointer<WriterType> writer = WriterType::New();
    writer->SetImageIO(io);
    writer->SetFileName(tempfilename.c_str());
    writer->SetInput(labelConverter->GetOutput());
    writer->UseCompressionOn();
    _progress->Observe(writer, "Write", 0.2);
    
	try
	{
		writer->Write();
	} 
	catch (itk::ExceptionObject &excp)
	{
	    QMessageBox msgBox;
	    msgBox.setIcon(QMessageBox::Critical);
		std::string text = std::string("An error occurred saving the segmentation file.\nThe operation has been aborted.");
		msgBox.setText(text.c_str());
		msgBox.setDetailedText(excp.what());
		msgBox.exec();
		_progress->Ignore(writer);
	    _progress->ManualReset();
		return;
	}

	if (0 != (rename(tempfilename.c_str(), filename.c_str())))
	{
	    QMessageBox msgBox;
	    msgBox.setIcon(QMessageBox::Critical);
		msgBox.setText("An error occurred saving the segmentation file.\nThe operation has been aborted.");
		msgBox.setDetailedText(QString("The temporal file couldn't be renamed."));
		msgBox.exec();

		if (0 != (remove(tempfilename.c_str())))
		{
		    QMessageBox msgBox;
		    msgBox.setIcon(QMessageBox::Critical);
			std::string text = std::string("The temporal file \"");
			text += tempfilename;
			text += std::string("\" couldn't be deleted.");
			msgBox.setText(text.c_str());
			msgBox.exec();
		}
	}
    
    _progress->Ignore(writer);
    _progress->ManualReset();
    return;
}

itk::SmartPointer<LabelMapType> EditorOperations::GetImageLabelMap()
{
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = this->_selection->GetItkImage();

    typedef itk::LabelImageToLabelMapFilter<ImageType, LabelMapType> ConverterType;
    itk::SmartPointer<ConverterType> converter = ConverterType::New();
    this->_progress->Observe(converter, "Convert", (1.0/1.0));
    converter->SetInput(image);
    
    try
    {
        converter->Update();
    } 
    catch (itk::ExceptionObject & excp)
    {
    	this->_progress->Ignore(converter);
        EditorError(excp);
        return NULL;
    }
    
    this->_progress->Ignore(converter);
    itk::SmartPointer<LabelMapType> outputLabelMap = converter->GetOutput();
    outputLabelMap->Register();
    
    return outputLabelMap;
}

void EditorOperations::SetFirstFreeValue(const unsigned short value)
{
    this->_dataManager->SetFirstFreeValue(value);
}

const unsigned int EditorOperations::GetFiltersRadius(void)
{
	return this->_filtersRadius;
}

void EditorOperations::SetFiltersRadius(const unsigned int value)
{
	this->_filtersRadius = value;
}

const double EditorOperations::GetWatershedLevel(void)
{
	return _watershedLevel;
}

void EditorOperations::SetWatershedLevel(const double value)
{
	this->_watershedLevel = value;
}

void EditorOperations::ContiguousAreaSelection(Vector3ui point, const unsigned short label)
{
	this->_progress->ManualSet("Threshold");
	this->_selection->AddArea(point, label);
   	this->_progress->ManualReset();
}

const Vector3ui EditorOperations::GetSelectedMinimumBouds()
{
	return this->_selection->GetSelectedMinimumBouds();
}

const Vector3ui EditorOperations::GetSelectedMaximumBouds()
{
	return this->_selection->GetSelectedMaximumBouds();
}

const bool EditorOperations::IsFirstColorSelected()
{
	return (Selection::EMPTY == this->_selection->GetSelectionType());
}

void EditorOperations::SetSliceViews(SliceVisualization* axialView, SliceVisualization* coronalView, SliceVisualization* sagittalView)
{
	this->_selection->SetSliceViews(axialView, coronalView, sagittalView);
}

void EditorOperations::ClearSelection(void)
{
	this->_selection->ClearSelection();
}
