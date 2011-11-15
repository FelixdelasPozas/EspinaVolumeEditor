///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: SliceVisualization.cxx
// Purpose: generate slices & crosshairs for axial, coronal and sagittal views. Also handles
//          pick function and selection of slice pixels.
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// vtk includes
#include <vtkImageReslice.h>
#include <vtkDataSetMapper.h>
#include <vtkProperty.h>
#include <vtkCellArray.h>
#include <vtkFastNumericConversion.h>
#include <vtkTextProperty.h>
#include <vtkCoordinate.h>
#include <vtkImageCanvasSource2D.h>
#include <vtkTexture.h>
#include <vtkPolyDataMapper.h>
#include <vtkImageItem.h>
#include <vtkRenderWindow.h>
#include <vtkLine.h>

// project includes
#include "SliceVisualization.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// SliceVisualization class
//
SliceVisualization::SliceVisualization(OrientationType orientation)
{
	// initilize options, minimum required
	_segmentationOpacity = 50;
	_blendimages = NULL;
	_thumbRenderer = NULL;
	_renderer = NULL;
	
	_orientation = orientation;
}

void SliceVisualization::Initialize(
        vtkSmartPointer<vtkStructuredPoints> points, 
        vtkSmartPointer<vtkLookupTable> colorTable, 
        vtkSmartPointer<vtkRenderer> renderer,
        Coordinates* OrientationData)
{
    // get data properties
    _size = OrientationData->GetTransformedSize();
    Vector3ui point = (_size / 2) - Vector3ui(1); // unsigned int, not double because we want the slice index
    double matrix[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
                              
    _spacing = OrientationData->GetImageSpacing();
    _max = Vector3d((_size[0]-1)*_spacing[0], (_size[1]-1)*_spacing[1], (_size[2]-1)*_spacing[2]);
    _axesMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    _renderer = renderer;
    
    // define reslice matrix
    switch(_orientation)
    {
        case Sagittal:
            matrix[3] = point[0] * _spacing[0];
            matrix[2] = matrix[4] = matrix[9] = 1;
            break;
        case Coronal:
            matrix[7] = point[1] * _spacing[1],
            matrix[0] = matrix[6] = matrix [9] = 1;
            break;
        case Axial:
            matrix[11] = point[2] * _spacing[2];
            matrix[0] = matrix[5] = matrix[10] = 1;
            break;
        default:
            break;
    }
    _axesMatrix->DeepCopy(matrix);
    
    // generate all actors
    GenerateSlice(points, colorTable);
    GenerateCrosshair();
    
    // create text legend
    _text = vtkSmartPointer<vtkTextActor>::New();
    
    sprintf(_textbuffer, "None");
    _text->SetInput(_textbuffer);
    _text->SetTextScaleMode(vtkTextActor::TEXT_SCALE_MODE_NONE);

    vtkSmartPointer<vtkCoordinate> textcoord = _text->GetPositionCoordinate();
    textcoord->SetCoordinateSystemToNormalizedViewport();
    textcoord->SetValue(0.02, 0.02);
    
    vtkSmartPointer<vtkTextProperty> textproperty = _text->GetTextProperty();
    textproperty->SetColor(1, 1, 1);
    textproperty->SetFontFamilyToArial();
    textproperty->SetFontSize(11);
    textproperty->BoldOff();
    textproperty->ItalicOff();
    textproperty->ShadowOff();
    textproperty->SetJustificationToLeft();
    textproperty->SetVerticalJustificationToBottom();    
    _text->Modified();
    _text->PickableOff();
    _renderer->AddViewProp(_text);

    // we set point out of range to force an update of the first call as all components will be
    // different from the update point
    _point = _size + Vector3ui(1);
    
    GenerateThumbnail();
}

SliceVisualization::~SliceVisualization()
{
	// remove additional renderer and actors
	if (this->_thumbRenderer)
		_thumbRenderer->RemoveAllViewProps();
	
	if (this->_renderer)
		_renderer->GetRenderWindow()->RemoveRenderer(_thumbRenderer);
}

void SliceVisualization::GenerateSlice(
        vtkSmartPointer<vtkStructuredPoints> points, 
        vtkSmartPointer<vtkLookupTable> colorTable)
{
    vtkSmartPointer<vtkImageReslice> reslice = vtkSmartPointer<vtkImageReslice>::New();
    reslice->SetOptimization(true);
    reslice->BorderOn();
    reslice->SetInput(points);
    reslice->SetOutputDimensionality(2);
    reslice->SetResliceAxes(_axesMatrix);
    
    _segmentationcolors = vtkSmartPointer<vtkImageMapToColors>::New();
    _segmentationcolors->SetLookupTable(colorTable);
    _segmentationcolors->SetOutputFormatToRGBA();
    _segmentationcolors->SetInputConnection(reslice->GetOutputPort());
    
    _imageactor = vtkSmartPointer<vtkImageActor>::New();
    _imageactor->SetInput(_segmentationcolors->GetOutput());
    _imageactor->SetInterpolate(false);
    
    _picker = vtkSmartPointer<vtkPropPicker>::New();
    _picker->PickFromListOn();
    _picker->InitializePickList();
    _picker->AddPickList(_imageactor);
    _imageactor->PickableOn();
    
    _renderer->AddActor(_imageactor);
}

void SliceVisualization::GenerateCrosshair()
{
    vtkSmartPointer<vtkPolyData> verticaldata = vtkSmartPointer<vtkPolyData>::New();
    vtkSmartPointer<vtkPolyData> horizontaldata = vtkSmartPointer<vtkPolyData>::New();
    
    vtkSmartPointer<vtkDataSetMapper> vertmapper = vtkSmartPointer<vtkDataSetMapper>::New();
    vertmapper->SetInput(verticaldata);
    
    vtkSmartPointer<vtkDataSetMapper> horimapper = vtkSmartPointer<vtkDataSetMapper>::New();
    horimapper->SetInput(horizontaldata);
    
    _vertactor = vtkSmartPointer<vtkActor>::New();
    _vertactor->SetMapper(vertmapper);
    _vertactor->GetProperty()->SetColor(1,1,1);
    _vertactor->GetProperty()->SetLineStipplePattern(0xF0F0);
    _vertactor->GetProperty()->SetLineStippleRepeatFactor(1);
    _vertactor->GetProperty()->SetPointSize(1);
    _vertactor->GetProperty()->SetLineWidth(2);
    _vertactor->SetPickable(false);

    _horiactor = vtkSmartPointer<vtkActor>::New();
    _horiactor->SetMapper(horimapper);
    _horiactor->GetProperty()->SetColor(1,1,1);
    _horiactor->GetProperty()->SetLineStipplePattern(0xF0F0);
    _horiactor->GetProperty()->SetLineStippleRepeatFactor(1);
    _horiactor->GetProperty()->SetPointSize(1);
    _horiactor->GetProperty()->SetLineWidth(2);
    _horiactor->SetPickable(false);
    
    _renderer->AddActor(_vertactor);
    _renderer->AddActor(_horiactor);
    
    _POIHorizontalLine = horizontaldata;
    _POIVerticalLine = verticaldata;
}

void SliceVisualization::Update(Vector3ui point)
{
    if (_point == point)
        return;
    
    UpdateSlice(point);
    _thumbRenderer->Render();
    UpdateCrosshair(point);
}

void SliceVisualization::UpdateSlice(Vector3ui point)
{
    double slice_point;
    
    // change slice by changing reslice axes
    switch(_orientation)
    {
        case Sagittal:
            if (_point[0] == point[0])
                return;
            slice_point = point[0] * _spacing[0];
            _axesMatrix->SetElement(0, 3, slice_point);
            _point[0] = point[0];
            sprintf(_textbuffer, "Slice %d of %d", point[0]+1, _size[0]);
            if (_actor != NULL)
            {
                // we also have a selection
                if ((_point[0] >= _minSelection[0]) && (_point[0] <= _maxSelection[0]))
                    _actor->SetVisibility(true);
                else
                    _actor->SetVisibility(false);
            }
            break;
        case Coronal:
            if (_point[1] == point[1])
                return;
            slice_point = point[1] * _spacing[1];
            _axesMatrix->SetElement(1, 3, slice_point);
            _point[1] = point[1];
            sprintf(_textbuffer, "Slice %d of %d", point[1]+1, _size[1]);
            if (_actor != NULL)
            {
                // we also have a selection
                if ((_point[1] >= _minSelection[1]) && (_point[1] <= _maxSelection[1]))
                    _actor->SetVisibility(true);
                else
                    _actor->SetVisibility(false);
            }
            break;
        case Axial:
            if (_point[2] == point[2])
                return;
            slice_point = point[2] * _spacing[2];
            _axesMatrix->SetElement(2, 3, slice_point);
            _point[2] = point[2];
            sprintf(_textbuffer, "Slice %d of %d", point[2]+1, _size[2]);
            if (_actor != NULL)
            {
                // we also have a selection
                if ((_point[2] >= _minSelection[2]) && (_point[2] <= _maxSelection[2]))
                    _actor->SetVisibility(true);
                else
                    _actor->SetVisibility(false);
            }
            break;
        default:
            break;
    }
    
    _text->SetInput(_textbuffer);
    _text->Modified();

}

void SliceVisualization::UpdateCrosshair(Vector3ui point)
{
    vtkSmartPointer<vtkPoints> verticalpoints = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkPoints> horizontalpoints = vtkSmartPointer<vtkPoints>::New();
    
    Vector3d point3d(Vector3d(point[0]*_spacing[0], point[1]*_spacing[1], point[2]*_spacing[2]));
    
    switch(_orientation)
    {
        case Sagittal:
            if ((_point[1] == point[1]) && (_point[2] == point[2]))
                return;
            horizontalpoints->InsertNextPoint(      0, point3d[2], 0);
            horizontalpoints->InsertNextPoint(_max[1], point3d[2], 0);
            verticalpoints->InsertNextPoint(point3d[1],       0, 0);
            verticalpoints->InsertNextPoint(point3d[1], _max[2], 0);
            _point[1] = point[1];
            _point[2] = point[2];
            break;
        case Coronal:
            if ((_point[0] == point[0]) && (_point[2] == point[2]))
                return;
            horizontalpoints->InsertNextPoint(      0, point3d[2], 0);
            horizontalpoints->InsertNextPoint(_max[0], point3d[2], 0);
            verticalpoints->InsertNextPoint(point3d[0],       0, 0);
            verticalpoints->InsertNextPoint(point3d[0], _max[2], 0);
            _point[0] = point[0];
            _point[2] = point[2];
            break;
        case Axial:
            if ((_point[0] == point[0]) && (_point[1] == point[1]))
                return;
            horizontalpoints->InsertNextPoint(      0, point3d[1], 0);
            horizontalpoints->InsertNextPoint(_max[0], point3d[1], 0);
            verticalpoints->InsertNextPoint(point3d[0],       0, 0);
            verticalpoints->InsertNextPoint(point3d[0], _max[1], 0);
            _point[0] = point[0];
            _point[1] = point[1];
            break;
        default:
            break;
    }
    
    vtkSmartPointer<vtkCellArray> verticalline = vtkSmartPointer<vtkCellArray>::New();
    verticalline->InsertNextCell(2);
    verticalline->InsertCellPoint(0);
    verticalline->InsertCellPoint(1);
    
    vtkSmartPointer<vtkCellArray> horizontalline = vtkSmartPointer<vtkCellArray>::New();
    horizontalline->InsertNextCell(2);
    horizontalline->InsertCellPoint(0);
    horizontalline->InsertCellPoint(1);
    
    _POIHorizontalLine->Reset();
    _POIHorizontalLine->SetPoints(horizontalpoints);
    _POIHorizontalLine->SetLines(horizontalline);
    _POIVerticalLine->Reset();
    _POIVerticalLine->SetPoints(verticalpoints);
    _POIVerticalLine->SetLines(verticalline);

    _POIHorizontalLine->Update();
    _POIVerticalLine->Update();
}

SliceVisualization::PickingType SliceVisualization::GetPickPosition(int* X, int* Y)
{
    PickingType pickedProp = None;
    
    // thumbnail must be onscreen to be really picked
    _picker->Pick(*X, *Y, 0.0, _thumbRenderer);
    if ((_picker->GetViewProp() != NULL) && (true == _thumbRenderer->GetDraw()))
        pickedProp = Thumbnail;
    else
    {
        // nope, did the user pick the slice?
        _picker->Pick(*X, *Y, 0.0, _renderer);
        if (_picker->GetViewProp() != NULL)
            pickedProp = Slice;
    }
    
    if (None == pickedProp)
        return pickedProp;

    // or the thumbnail or the slice have been picked
    double point[3];
    _picker->GetPickPosition(point);

    // picked values passed by reference, not elegant, we first use round to get the nearest point
    switch(_orientation)
    {
        case Axial:
            *X = vtkFastNumericConversion::Round(point[0]/_spacing[0]);
            *Y = vtkFastNumericConversion::Round(point[1]/_spacing[1]);
            break;
        case Coronal:
            *X = vtkFastNumericConversion::Round(point[0]/_spacing[0]);
            *Y = vtkFastNumericConversion::Round(point[1]/_spacing[2]);
            break;
        case Sagittal:
            *X = vtkFastNumericConversion::Round(point[0]/_spacing[1]);
            *Y = vtkFastNumericConversion::Round(point[1]/_spacing[2]);
            break;
        default:
            break;
    }
    
    return pickedProp;
}

void SliceVisualization::SetSelection(std::vector< Vector3ui > points)
{
    std::vector < Vector3ui >::iterator it;
    
    // initialize with first point
    _maxSelection = _minSelection = points[0];
    
    // get selection bounds    
    for (it = points.begin(); it != points.end(); it++)
    {
        if ((*it)[0] < _minSelection[0])
            _minSelection[0] = (*it)[0];

        if ((*it)[1] < _minSelection[1])
            _minSelection[1] = (*it)[1];

        if ((*it)[2] < _minSelection[2])
            _minSelection[2] = (*it)[2];

        if ((*it)[0] > _maxSelection[0])
            _maxSelection[0] = (*it)[0];

        if ((*it)[1] > _maxSelection[1])
            _maxSelection[1] = (*it)[1];

        if ((*it)[2] > _maxSelection[2])
            _maxSelection[2] = (*it)[2];
    }

    if (_actor == NULL)
    {
        // dummpy plane, just for initialization. we will modify the size later
        _selectionPlane = vtkSmartPointer<vtkPlaneSource>::New();
        _selectionPlane->SetOrigin(0,0,0);
        _selectionPlane->SetPoint1(0,1,0);
        _selectionPlane->SetPoint2(1,0,0);
          
        vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(_selectionPlane->GetOutputPort());
        
        _actor = vtkSmartPointer<vtkActor>::New();
        _actor->SetMapper(mapper);
        _actor->GetProperty()->SetOpacity(0.2);
        _actor->GetProperty()->SetColor(200,200,255);
        
        _renderer->AddActor(_actor);
    }

    switch (_orientation)
    {
        case Sagittal:
            _selectionPlane->SetOrigin((static_cast<double>(_minSelection[1])-0.5)*_spacing[1],
                                       (static_cast<double>(_minSelection[2])-0.5)*_spacing[2],0);
            _selectionPlane->SetPoint1((static_cast<double>(_minSelection[1])-0.5)*_spacing[1],
                                       (static_cast<double>(_maxSelection[2])+0.5)*_spacing[2],0);
            _selectionPlane->SetPoint2((static_cast<double>(_maxSelection[1])+0.5)*_spacing[1],
                                       (static_cast<double>(_minSelection[2])-0.5)*_spacing[2],0);
            break;
        case Coronal:
            _selectionPlane->SetOrigin((static_cast<double>(_minSelection[0])-0.5)*_spacing[0],
                                       (static_cast<double>(_minSelection[2])-0.5)*_spacing[2],0);
            _selectionPlane->SetPoint1((static_cast<double>(_minSelection[0])-0.5)*_spacing[0],
                                       (static_cast<double>(_maxSelection[2])+0.5)*_spacing[2],0);
            _selectionPlane->SetPoint2((static_cast<double>(_maxSelection[0])+0.5)*_spacing[0],
                                       (static_cast<double>(_minSelection[2])-0.5)*_spacing[2],0);
            break;
        case Axial:
            _selectionPlane->SetOrigin((static_cast<double>(_minSelection[0])-0.5)*_spacing[0],
                                       (static_cast<double>(_minSelection[1])-0.5)*_spacing[1],0);
            _selectionPlane->SetPoint1((static_cast<double>(_minSelection[0])-0.5)*_spacing[0],
                                       (static_cast<double>(_maxSelection[1])+0.5)*_spacing[1],0);
            _selectionPlane->SetPoint2((static_cast<double>(_maxSelection[0])+0.5)*_spacing[0],
                                       (static_cast<double>(_minSelection[1])-0.5)*_spacing[1],0);
            break;
        default:
            break;
    }

    // need to set visibility again, as the user could have moved the slider
    // and we are now in a new slice and with a new selection point.
    _actor->SetVisibility(true);
    _selectionPlane->Update();
}

void SliceVisualization::ClearSelection()
{
    if (_actor != NULL)
    {
        _renderer->RemoveActor(_actor);
        _actor = NULL;
    }
}

void SliceVisualization::GenerateThumbnail()
{
    // get image bounds for thumbnail activation, we're only interested in bounds[1] & bounds[3]
    // as we know already the values of the others
    double bounds[6];
    _imageactor->GetBounds(bounds);
    _boundsX = bounds[1];
    _boundsY = bounds[3];

    // create thumbnail renderer
    _thumbRenderer = vtkSmartPointer<vtkRenderer>::New();
    _thumbRenderer->AddActor(_imageactor);
    _thumbRenderer->ResetCamera();
    _thumbRenderer->SetInteractive(false);
    
    // coordinates are normalized display coords (range 0-1 in double)
    _thumbRenderer->SetViewport(0.65, 0.0, 1.0, 0.35);
    _renderer->GetRenderWindow()->AddRenderer(_thumbRenderer);

    _renderer->GetRenderWindow()->AlphaBitPlanesOn();
    _renderer->GetRenderWindow()->SetDoubleBuffer(true);
    _renderer->GetRenderWindow()->SetNumberOfLayers(2);
    _renderer->SetLayer(0);
    _thumbRenderer->SetLayer(1);
    _thumbRenderer->DrawOff();

    // create the slice border points
    double p0[3] = { 0.0, 0.0, 0.0 };
    double p1[3] = { _boundsX, 0.0, 0.0 };
    double p2[3] = { _boundsX, _boundsY, 0.0 };
    double p3[3] = { 0.0, _boundsY, 0.0 };

    vtkSmartPointer < vtkPoints > points = vtkSmartPointer<vtkPoints>::New();
    points->InsertNextPoint(p0);
    points->InsertNextPoint(p1);
    points->InsertNextPoint(p2);
    points->InsertNextPoint(p3);
    points->InsertNextPoint(p0);

    vtkSmartPointer < vtkCellArray > lines = vtkSmartPointer<vtkCellArray>::New();
    for (unsigned int i = 0; i < 4; i++)
    {
        vtkSmartPointer < vtkLine > line = vtkSmartPointer<vtkLine>::New();
        line->GetPointIds()->SetId(0, i);
        line->GetPointIds()->SetId(1, i + 1);
        lines->InsertNextCell(line);
    }
    
    vtkSmartPointer<vtkPolyData> sliceborder = vtkSmartPointer<vtkPolyData>::New();
    sliceborder->SetPoints(points);
    sliceborder->SetLines(lines);
    sliceborder->Update();

    vtkSmartPointer <vtkPolyDataMapper> slicemapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    slicemapper->SetInput(sliceborder);

    vtkSmartPointer < vtkActor > sliceactor = vtkSmartPointer<vtkActor>::New();
    sliceactor->SetMapper(slicemapper);
    sliceactor->GetProperty()->SetColor(1,1,1);
    sliceactor->GetProperty()->SetPointSize(0);
    sliceactor->GetProperty()->SetLineWidth(2);
    sliceactor->SetPickable(false);

    _thumbRenderer->AddActor(sliceactor);
    
    // Create a polydata to store the selection box 
    _square = vtkSmartPointer<vtkPolyData>::New();

    vtkSmartPointer <vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInput(_square);

    vtkSmartPointer < vtkActor > actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(1,1,1);
    actor->GetProperty()->SetPointSize(1);
    actor->GetProperty()->SetLineWidth(2);
    actor->SetPickable(false);
    
    _thumbRenderer->AddActor(actor);
}

void SliceVisualization::ZoomEvent()
{
    double xy[3];
    double *value;
    double lowerleft[2];
    double upperright[2];
    
    vtkSmartPointer<vtkCoordinate> coords = vtkSmartPointer<vtkCoordinate>::New();
    
    coords->SetViewport(_renderer);
    coords->SetCoordinateSystemToNormalizedViewport();
    
    xy[0] = 0.0; xy[1] = 0.0; xy[2] = 0.0;
    coords->SetValue(xy);
    value = coords->GetComputedWorldValue(_renderer);
    lowerleft[0] = value[0]; lowerleft[1] = value[1]; 
    
    xy[0] = 1.0; xy[1] = 1.0; xy[2] = 0.0;
    coords->SetValue(xy);
    value = coords->GetComputedWorldValue(_renderer);
    upperright[0] = value[0]; upperright[1] = value[1];
    
    // is the slice completely inside the viewport?
    if ((0.0 >= lowerleft[0]) && (0.0 >= lowerleft[1]) && (_boundsX <= upperright[0]) && (_boundsY <= upperright[1]))
    {
        _thumbRenderer->DrawOff();
        _thumbRenderer->GetRenderWindow()->Render();
    }
    else
    {
        double p0[3] = { lowerleft[0], lowerleft[1], 0.0 };
        double p1[3] = { lowerleft[0], upperright[1], 0.0 };
        double p2[3] = { upperright[0], upperright[1], 0.0 };
        double p3[3] = { upperright[0], lowerleft[1], 0.0 };

        // Create a vtkPoints object and store the points in it
        vtkSmartPointer < vtkPoints > points = vtkSmartPointer<vtkPoints>::New();
        points->InsertNextPoint(p0);
        points->InsertNextPoint(p1);
        points->InsertNextPoint(p2);
        points->InsertNextPoint(p3);
        points->InsertNextPoint(p0);

        // Create a cell array to store the lines in and add the lines to it
        vtkSmartPointer < vtkCellArray > lines = vtkSmartPointer<vtkCellArray>::New();

        for (unsigned int i = 0; i < 4; i++)
        {
            vtkSmartPointer < vtkLine > line = vtkSmartPointer<vtkLine>::New();
            line->GetPointIds()->SetId(0, i);
            line->GetPointIds()->SetId(1, i + 1);
            lines->InsertNextCell(line);
        }
        
        // Add the points & lines to the dataset
        _square->Reset();
        _square->SetPoints(points);
        _square->SetLines(lines);
        _square->Update();

        _thumbRenderer->DrawOn();
        _thumbRenderer->GetRenderWindow()->Render();
    }
}

void SliceVisualization::SetReferenceImage(vtkSmartPointer<vtkStructuredPoints> stackImage)
{
	vtkSmartPointer < vtkImageReslice > reslice = vtkSmartPointer< vtkImageReslice > ::New();
	reslice->SetOptimization(true);
	reslice->BorderOn();
	reslice->SetInput(stackImage);
	reslice->SetOutputDimensionality(2);
	reslice->SetResliceAxes(_axesMatrix);

	// the image is grayscale, so only uses 256 colors
	vtkSmartPointer<vtkLookupTable> colorTable = vtkSmartPointer<vtkLookupTable>::New();
	colorTable->SetTableRange(0,255);
	colorTable->SetValueRange(0.0,1.0);
	colorTable->SetSaturationRange(0.0,0.0);
	colorTable->SetHueRange(0.0,0.0);
	colorTable->SetAlphaRange(1.0,1.0);
	colorTable->SetNumberOfColors(256);
	colorTable->Build();
	
	vtkSmartPointer<vtkImageMapToColors> stackcolors = vtkSmartPointer<vtkImageMapToColors>::New();
	stackcolors->SetLookupTable(colorTable);
	stackcolors->SetOutputFormatToRGBA();
	stackcolors->SetInputConnection(reslice->GetOutputPort());
	
	// remove actual actor and add the blended actor with both the segmentation and the stack
	_blendimages = vtkSmartPointer<vtkImageBlend>::New();
	_blendimages->SetInput(0, stackcolors->GetOutput());
	_blendimages->SetInput(1, _segmentationcolors->GetOutput());
	_blendimages->SetOpacity(1, static_cast<double>(_segmentationOpacity/100.0));
	_blendimages->SetBlendModeToNormal();
	_blendimages->Update();
	
	_blendactor = vtkSmartPointer<vtkImageActor>::New();
	_blendactor->SetInput(_blendimages->GetOutput());
	_blendactor->PickableOn();
	_blendactor->SetInterpolate(false);
	
	_picker->DeletePickList(_imageactor);
	_picker->AddPickList(_blendactor);

	_renderer->RemoveActor(_imageactor);
	_renderer->AddActor(_blendactor);
	
	_thumbRenderer->RemoveActor(_imageactor);
	_thumbRenderer->AddActor(_blendactor);

	// change color for the crosshair because reference images tend to be too white and it messes with it
    _horiactor->GetProperty()->SetColor(0,0,0);
    _vertactor->GetProperty()->SetColor(0,0,0);
}

unsigned int SliceVisualization::GetSegmentationOpacity()
{
	return _segmentationOpacity;
}

void SliceVisualization::SetSegmentationOpacity(unsigned int value)
{
	_segmentationOpacity = value;
	
	if (this->_blendimages)
		_blendimages->SetOpacity(1,static_cast<float>(value/100.0));
}
