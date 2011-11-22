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

// vtk includes
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkImageCanvasSource2D.h>
#include <vtkTexture.h>
#include <vtkImageClip.h>
#include <vtkImageExport.h>
#include <vtkImageCast.h>
#include <vtkMetaImageWriter.h>

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
    _dataManager = data;
    _renderer = NULL;
    _orientation = NULL;
    _progress = NULL;
    _min = _max = _size = Vector3ui(0,0,0);
    
    // default options for filters
    _filtersRadius = 1;
    _watershedLevel = 0.5;
}

EditorOperations::~EditorOperations()
{
    if (_actor != NULL)
        _renderer->RemoveActor(_actor);
}

void EditorOperations::Initialize(vtkSmartPointer<vtkRenderer> renderer, Coordinates *orientation, ProgressAccumulator *progress)
{
    _renderer = renderer;
    _orientation = orientation;
    _progress = progress;
    _size = orientation->GetTransformedSize() - Vector3ui(1,1,1);
    _min = Vector3ui(0,0,0);
    _max = _size;
    
    // initial selection cube is invisible
    _selectionCube = vtkSmartPointer<vtkCubeSource>::New();
    _selectionCube->SetBounds(0,1,0,1,0,1);
    
    // create texture
    vtkSmartPointer<vtkImageCanvasSource2D> image = vtkSmartPointer<vtkImageCanvasSource2D>::New();
    image->SetScalarTypeToUnsignedChar();
    image->SetExtent(0, 15, 0, 15, 0, 0);
    image->SetNumberOfScalarComponents(4);
    image->SetDrawColor(0,0,0,0);             // transparent color 
    image->FillBox(0,15,0,15);
    image->SetDrawColor(255,255,255,150);     // "somewhat transparent" white
    image->DrawSegment(0, 0, 15, 15);
    image->DrawSegment(1, 0, 15, 14);
    image->DrawSegment(0, 1, 14, 15);
    image->DrawSegment(15, 0, 15, 0);
    image->DrawSegment(0, 15, 0, 15);
    image->Update();
    
    // apply texture
    vtkSmartPointer<vtkTexture> texture = vtkSmartPointer<vtkTexture>::New();
    texture->SetInputConnection(image->GetOutputPort());
    texture->RepeatOn();
    texture->InterpolateOn();
    texture->ReleaseDataFlagOn();
      
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(_selectionCube->GetOutputPort());
    
    _actor = vtkSmartPointer<vtkActor>::New();
    _actor->SetMapper(mapper);
    _actor->SetTexture(texture);
    _actor->GetProperty()->SetOpacity(1);
    _actor->SetVisibility(false);
    
    _renderer->AddActor(_actor);
}

void EditorOperations::ComputeSelectionCube()
{
    std::vector < Vector3ui >::iterator it;
    
    // initialize with first point
    _max = _min = _selectedPoints[0];
    
    // get selection bounds    
    for (it = _selectedPoints.begin(); it != _selectedPoints.end(); it++)
    {
        if ((*it)[0] < _min[0])
            _min[0] = (*it)[0];

        if ((*it)[1] < _min[1])
            _min[1] = (*it)[1];

        if ((*it)[2] < _min[2])
            _min[2] = (*it)[2];

        if ((*it)[0] > _max[0])
            _max[0] = (*it)[0];

        if ((*it)[1] > _max[1])
            _max[1] = (*it)[1];

        if ((*it)[2] > _max[2])
            _max[2] = (*it)[2];
    }
    
    Vector3d spacing = _orientation->GetImageSpacing();
    
    // we must select voxel boundaries
    _selectionCube->SetBounds(
            static_cast<double>((_min[0])-0.5)*spacing[0], 
            static_cast<double>((_max[0])+0.5)*spacing[0], 
            static_cast<double>((_min[1])-0.5)*spacing[1],
            static_cast<double>((_max[1])+0.5)*spacing[1],
            static_cast<double>((_min[2])-0.5)*spacing[2],
            static_cast<double>((_max[2])+0.5)*spacing[2]);
    _selectionCube->Modified();
}

void EditorOperations::AddSelectionPoint(Vector3ui point)
{
    // how many points do we have?
    switch (_selectedPoints.size())
    {
        case 2:
            _selectedPoints.pop_back();
            _selectedPoints.push_back(point);
            break;
        case 0: /* no break */
            _actor->SetVisibility(true);
        default:
            _selectedPoints.push_back(point);
            break;
    }
    ComputeSelectionCube();
}

void EditorOperations::ClearSelection()
{
    _selectedPoints.clear();
    _min = Vector3ui(0,0,0);
    _max = _size;
    
    // make the actor invisible again
    _actor->SetVisibility(false);
}

std::vector<Vector3ui> EditorOperations::GetSelection()
{
    return _selectedPoints;
}

itk::SmartPointer<ImageType> EditorOperations::GetItkImageFromSelection()
{
	// first crop the region and then use the vtk-itk pipeline to get a
	// itk::Image of the region so we can apply a itk filter to it. 
	vtkSmartPointer<vtkImageClip> imageClip = vtkSmartPointer<vtkImageClip>::New();
	imageClip->SetInput(_dataManager->GetStructuredPoints());
	imageClip->SetOutputWholeExtent(_min[0], _max[0], _min[1], _max[1], _min[2], _max[2]);
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

void EditorOperations::CutSelection(unsigned short label)
{
    if (label == 0)
    	return;

    _progress->ManualSet("Cut");
    _dataManager->OperationStart("Cut");
    
    for (unsigned int z = _min[2]; z <= _max[2]; z++)
        for (unsigned int x = _min[0]; x <= _max[0]; x++)
            for (unsigned int y = _min[1]; y <= _max[1]; y++)
				if (label == _dataManager->GetVoxelScalar(x, y, z))
					_dataManager->SetVoxelScalar(x, y, z, 0);
    
    _progress->ManualReset();
    _dataManager->OperationEnd();
}

bool EditorOperations::RelabelSelection(QWidget *parent, unsigned short label, vtkSmartPointer<vtkLookupTable> colors, Metadata *data)
{
    Vector3d color;
    bool newcolor = false;
    
    QtRelabel configdialog(parent);
    configdialog.SetInitialOptions(label, colors, data);
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
        colorpicker.SetInitialOptions(colors);
        colorpicker.exec();
        
        if (!colorpicker.ModifiedData())
            return false;
        
        color = colorpicker.GetColor();

        newlabel = _dataManager->SetLabel(color);
        newcolor = true;
    }

    _progress->ManualSet("Relabel");
    
    for (unsigned int z = _min[2]; z <= _max[2]; z++)
        for (unsigned int x = _min[0]; x <= _max[0]; x++)
            for (unsigned int y = _min[1]; y <= _max[1]; y++)
                if (label == _dataManager->GetVoxelScalar(x,y,z))
                    _dataManager->SetVoxelScalar(x,y,z,newlabel);
    
    _progress->ManualReset();
    _dataManager->OperationEnd();
    return newcolor;
}

void EditorOperations::ErodeSelection(unsigned short label)
{
    _dataManager->OperationStart("Erode");

    typedef itk::BinaryBallStructuringElement<ImageType::PixelType, 3> StructuringElementType;
    typedef itk::ErodeObjectMorphologyImageFilter<ImageType, ImageType, StructuringElementType> BinaryErodeImageFilterType;
    BinaryErodeImageFilterType::Pointer erodeFilter = BinaryErodeImageFilterType::New();

    _progress->Observe(erodeFilter, "Erode", 1.0);

    itk::SmartPointer<ImageType> image = ImageType::New();
    image = GetItkImageFromSelection();
    
    StructuringElementType structuringElement;
    structuringElement.SetRadius(_filtersRadius);
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

void EditorOperations::DilateSelection(unsigned short label)
{
    _dataManager->OperationStart("Dilate");

    typedef itk::BinaryBallStructuringElement<ImageType::PixelType, 3> StructuringElementType;
    typedef itk::DilateObjectMorphologyImageFilter<ImageType, ImageType, StructuringElementType> BinaryDilateImageFilterType;
    BinaryDilateImageFilterType::Pointer dilateFilter = BinaryDilateImageFilterType::New();

    _progress->Observe(dilateFilter, "Dilate", 1.0);
    
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = GetItkImageFromSelection();

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

void EditorOperations::OpenSelection(unsigned short label)
{
    _dataManager->OperationStart("Open");

    typedef itk::BinaryBallStructuringElement<ImageType::PixelType, 3> StructuringElementType;
    typedef itk::BinaryMorphologicalOpeningImageFilter<ImageType, ImageType, StructuringElementType> BinaryOpenImageFilterType;
    BinaryOpenImageFilterType::Pointer openFilter = BinaryOpenImageFilterType::New();

    _progress->Observe(openFilter, "Open", 1.0);
    
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = GetItkImageFromSelection();

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

void EditorOperations::CloseSelection(unsigned short label)
{
    _dataManager->OperationStart("Close");

    typedef itk::BinaryBallStructuringElement<ImageType::PixelType, 3> StructuringElementType;
    typedef itk::BinaryMorphologicalClosingImageFilter<ImageType, ImageType, StructuringElementType> BinaryCloseImageFilterType;
    BinaryCloseImageFilterType::Pointer closeFilter = BinaryCloseImageFilterType::New();

    _progress->Observe(closeFilter, "Close", 1.0);
    
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = GetItkImageFromSelection();

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

void EditorOperations::WatershedSelection(unsigned short label)
{
    _dataManager->OperationStart("Watershed");
    
    typedef itk::Image<float,3> FloatImageType;
    typedef itk::MorphologicalWatershedImageFilter<FloatImageType, ImageType> WatershedFilterType;
    typedef itk::SignedDanielssonDistanceMapImageFilter<ImageType, FloatImageType> DanielssonFilterType;
    
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = GetItkImageFromSelection();
    
    DanielssonFilterType::Pointer danielssonFilter = DanielssonFilterType::New();
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
    
    WatershedFilterType::Pointer watershedFilter = WatershedFilterType::New();
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
            Vector3d color = Vector3d(((double)rand()/(double)RAND_MAX),((double)rand()/(double)RAND_MAX),((double)rand()/(double)RAND_MAX));

            double rgba[4];
            bool match = false;
            
            for (int i = 0; i < _dataManager->GetLookupTable()->GetNumberOfTableValues(); i++)
            {
                _dataManager->GetLookupTable()->GetTableValue(i, rgba);
                if ((rgba[0] == color[0]) && (rgba[1] == color[1]) && (rgba[2] == color[2])) 
                    match = true;
            }

            if (match == false)
            {
                newlabel = _dataManager->SetLabel(color);
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
    return;
}

void EditorOperations::CleanImage(itk::SmartPointer<ImageType> image, unsigned short scalar)
{
    ImageType::IndexType index = {{ 0, 0, 0 }};

    for (unsigned int z = _min[2]; z <= _max[2]; z++)
        for (unsigned int x = _min[0]; x <= _max[0]; x++)
            for (unsigned int y = _min[1]; y <= _max[1]; y++)
            {
                index[0] = x;
                index[1] = y;
                index[2] = z;
                unsigned short pointScalar = _dataManager->GetVoxelScalar(x,y,z);
                
                switch(scalar)
                {
                    case 0:
                        if (0 == pointScalar)
                            image->SetPixel(index, 0);
                        break;
                    default:
                        if (pointScalar != scalar)
                            image->SetPixel(index, 0);
                        break;
                }
            }
}

void EditorOperations::EditorError(itk::ExceptionObject &excp)
{
    char text[100];
    
    _progress->Reset();
    
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    
    sprintf(text, "An error occurred.\nThe %s operation has been aborted.", _dataManager->GetActualActionString().c_str());
    msgBox.setText(text);
    msgBox.setDetailedText(excp.what());
    msgBox.exec();
    
    _dataManager->OperationCancel();
    
    return;   
}

// originally the image was saved directly from the vtkStructuredPoints in memory but, although the
// segmha file saved was correct, the orientation of the image was saved as ???, this doen't happen
// with itk::ImageFileWriter as the orientation is saved as RAI.
void EditorOperations::SaveImage(std::string filename)
{
    _progress->ManualSet("Save Image");
    
    Vector3ui temp_min = _min;
    Vector3ui temp_max = _max;
    _max = _size;
    _min = Vector3ui(0,0,0);
    
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = GetItkImageFromSelection();

    _max = temp_max;
    _min = temp_min;
    
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
	assert(0 != converter->GetOutput()->GetNumberOfLabelObjects());

    typedef itk::ChangeLabelLabelMapFilter<LabelMapType> ChangeType;
    itk::SmartPointer<ChangeType> labelChanger = ChangeType::New();
    labelChanger->SetInput(converter->GetOutput());
    if (labelChanger->CanRunInPlace() == true)
    	labelChanger->SetInPlace(true);

    std::map<unsigned short int, unsigned short int>::iterator it;
    for (it = _dataManager->GetLabelValueTable()->begin(); it != _dataManager->GetLabelValueTable()->end(); it++)
    	labelChanger->SetChange(it->first,it->second);

    _progress->Observe(labelChanger, "Fix Labels", 0.2);
    labelChanger->Update();
    _progress->Ignore(labelChanger);

	// itklabelmap->itkimage
	typedef itk::LabelMapToLabelImageFilter<LabelMapType, ImageType> LabelMapToImageFilterType;
	itk::SmartPointer<LabelMapToImageFilterType> labelconverter = LabelMapToImageFilterType::New();

	labelconverter->SetInput(labelChanger->GetOutput());
	labelconverter->SetNumberOfThreads(1);
	labelconverter->ReleaseDataFlagOn();

	_progress->Observe(labelconverter, "Convert Image", 0.2);
	labelconverter->Update();
	_progress->Ignore(labelconverter);

    // save as an mha and rename
    std::string tempfilename = filename + std::string(".mha");
    typedef itk::ImageFileWriter<ImageType> WriterType;
    itk::MetaImageIO::Pointer io = itk::MetaImageIO::New();
    io->SetFileName(tempfilename.c_str());
    itk::SmartPointer<WriterType> writer = WriterType::New();
    writer->SetImageIO(io);
    writer->SetFileName(tempfilename.c_str());
    writer->SetInput(labelconverter->GetOutput());
    writer->UseCompressionOn();
    _progress->Observe(writer, "Write", 0.2);
    
	try
	{
		writer->Write();
	} 
	catch (itk::ExceptionObject &excp)
	{
	    char text[100];
	    QMessageBox msgBox;
	    msgBox.setIcon(QMessageBox::Critical);
		sprintf(text, "An error occurred saving the segmentation file.\nThe operation has been aborted.");
		msgBox.setText(text);
		msgBox.setDetailedText(excp.what());
		msgBox.exec();
		_progress->Ignore(writer);
	    _progress->ManualReset();
		return;
	}

	if (0 != (rename(tempfilename.c_str(), filename.c_str())))
	{
	    char text[100];
	    QMessageBox msgBox;
	    msgBox.setIcon(QMessageBox::Critical);
		sprintf(text, "An error occurred saving the segmentation file.\nThe operation has been aborted.");
		msgBox.setText(text);
		msgBox.setDetailedText(QString("The temporal file couldn't be renamed."));
		msgBox.exec();

		if (0 != (remove(tempfilename.c_str())))
		{
		    char text[100];
		    QMessageBox msgBox;
		    msgBox.setIcon(QMessageBox::Critical);
			sprintf(text, "The temporal file \"%s\" couldn't be deleted.", tempfilename.c_str());
			msgBox.setText(text);
			msgBox.exec();
		}
	}
    
    _progress->Ignore(writer);
    _progress->ManualReset();
    return;
}

itk::SmartPointer<LabelMapType> EditorOperations::GetImageLabelMap()
{
    Vector3ui temp_min = _min;
    Vector3ui temp_max = _max;
    _max = _size;
    _min = Vector3ui(0,0,0);
    
    itk::SmartPointer<ImageType> image = ImageType::New();
    image = GetItkImageFromSelection();

    _max = temp_max;
    _min = temp_min;

    typedef itk::LabelImageToLabelMapFilter<ImageType, LabelMapType> ConverterType;
    itk::SmartPointer<ConverterType> converter = ConverterType::New();
    _progress->Observe(converter, "Convert", (1.0/1.0));
    converter->SetInput(image);
    
    try
    {
        converter->Update();
    } 
    catch (itk::ExceptionObject & excp)
    {
        _progress->Ignore(converter);
        EditorError(excp);
        return NULL;
    }
    
    itk::SmartPointer<LabelMapType> outputLabelMap = converter->GetOutput();
    outputLabelMap->Register();
    
    return outputLabelMap;
}

void EditorOperations::SetFirstFreeValue(unsigned short value)
{
    _dataManager->SetFirstFreeValue(value);
}

unsigned int EditorOperations::GetFiltersRadius(void)
{
	return _filtersRadius;
}

void EditorOperations::SetFiltersRadius(unsigned int value)
{
	_filtersRadius = value;
}

double EditorOperations::GetWatershedLevel(void)
{
	return _watershedLevel;
}

void EditorOperations::SetWatershedLevel(double value)
{
	_watershedLevel = value;
}
