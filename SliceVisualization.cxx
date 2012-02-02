///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: SliceVisualization.cxx
// Purpose: generate slices & crosshairs for axial, coronal and sagittal views. Also handles
//          pick function and selection of slice pixels.
// Notes: 
///////////////////////////////////////////////////////////////////////////////////////////////////

// c++ includes
#include <sstream>

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

#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkIconGlyphFilter.h>
#include <vtkTexturedActor2D.h>
#include <vtkPointData.h>
#include <vtkPolyDataMapper2D.h>

#include <vtkTextureMapToPlane.h>
#include <vtkTransformTextureCoords.h>

// project includes
#include "SliceVisualization.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// SliceVisualization class
//
SliceVisualization::SliceVisualization(OrientationType orientation)
{
	// initilize options, minimum required
	_segmentationOpacity = 75;
	_segmentationHidden = false;
	_blendimages = NULL;
	_thumbRenderer = NULL;
	_renderer = NULL;
	_iconActor = NULL;
	
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
    
    _textbuffer = std::string("None");
    _text->SetInput(_textbuffer.c_str());
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
    std::stringstream out;
    _textbuffer = std::string("Slice ");
    
    // change slice by changing reslice axes
    switch(_orientation)
    {
        case Sagittal:
            if (_point[0] == point[0])
                return;
            slice_point = point[0] * _spacing[0];
            _axesMatrix->SetElement(0, 3, slice_point);
            _point[0] = point[0];
            out << point[0]+1 << " of " << _size[0];
            if (_iconActor != NULL)
            {
                // we also have a selection
                if ((_point[0] >= _minSelection[0]) && (_point[0] <= _maxSelection[0]))
                	_iconActor->SetVisibility(true);
                else
                	_iconActor->SetVisibility(false);
            }
            break;
        case Coronal:
            if (_point[1] == point[1])
                return;
            slice_point = point[1] * _spacing[1];
            _axesMatrix->SetElement(1, 3, slice_point);
            _point[1] = point[1];
            out << point[1]+1 << " of " << _size[1];
            if (_iconActor != NULL)
            {
                // we also have a selection
                if ((_point[1] >= _minSelection[1]) && (_point[1] <= _maxSelection[1]))
                	_iconActor->SetVisibility(true);
                else
                	_iconActor->SetVisibility(false);
            }
            break;
        case Axial:
            if (_point[2] == point[2])
                return;
            slice_point = point[2] * _spacing[2];
            _axesMatrix->SetElement(2, 3, slice_point);
            _point[2] = point[2];
            out << point[2]+1 << " of " << _size[2];
            if (_iconActor != NULL)
            {
                // we also have a selection
                if ((_point[2] >= _minSelection[2]) && (_point[2] <= _maxSelection[2]))
                	_iconActor->SetVisibility(true);
                else
                	_iconActor->SetVisibility(false);
            }
            break;
        default:
            break;
    }
    
    _textbuffer += out.str();
    _text->SetInput(_textbuffer.c_str());
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

SliceVisualization::PickingType SliceVisualization::GetPickData(int* X, int* Y)
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
	_minSelection = points[0];
    _maxSelection = points[1];

    // only executed the first time a selection is created
    if (NULL == _iconActor)
    {
        // create striped texture
    	vtkSmartPointer<vtkImageCanvasSource2D> textureIcon = vtkSmartPointer<vtkImageCanvasSource2D>::New();
    	textureIcon->SetScalarTypeToUnsignedChar();
    	textureIcon->SetExtent(0, 7, 0, 7, 0, 0);
    	textureIcon->SetNumberOfScalarComponents(4);
    	textureIcon->SetDrawColor(0, 0, 0, 100);
    	textureIcon->FillBox(0, 1, 0, 1);
    	textureIcon->FillBox(2, 3, 2, 3);
    	textureIcon->FillBox(4, 5, 0, 1);
    	textureIcon->FillBox(6, 7, 2, 3);
    	textureIcon->FillBox(0, 1, 4, 5);
    	textureIcon->FillBox(2, 3, 6, 7);
    	textureIcon->FillBox(4, 5, 4, 5);
    	textureIcon->FillBox(6, 7, 6, 7);

    	textureIcon->SetDrawColor(255, 255, 255, 100);
    	textureIcon->FillBox(0, 1, 2, 3);
    	textureIcon->FillBox(0, 1, 6, 7);
    	textureIcon->FillBox(2, 3, 0, 1);
    	textureIcon->FillBox(2, 3, 4, 5);
    	textureIcon->FillBox(4, 5, 2, 3);
    	textureIcon->FillBox(4, 5, 6, 7);
    	textureIcon->FillBox(6, 7, 0, 1);
    	textureIcon->FillBox(6, 7, 4, 5);

    	// array of points coordinates
        vtkSmartPointer<vtkDoubleArray> dataArray = vtkSmartPointer<vtkDoubleArray>::New();
        dataArray->SetNumberOfComponents(3);

        // vtkpoints representation
        _slicePoints = vtkSmartPointer<vtkPoints>::New();
        _slicePoints->SetDataTypeToDouble();
        _slicePoints->SetData(dataArray);

        // index set of every point in the set, right now all 0 as we have only one texture
        _indexArray = vtkSmartPointer<vtkIntArray>::New();
        _indexArray->SetNumberOfComponents(1);

        // representation of points-indexes
        vtkSmartPointer<vtkPolyData> pointData = vtkSmartPointer<vtkPolyData>::New();
        pointData->SetPoints(_slicePoints);
        pointData->GetPointData()->SetScalars(_indexArray);

        // create filter to apply the same texture to every point in the set
        vtkSmartPointer<vtkIconGlyphFilter> iconFilter = vtkSmartPointer<vtkIconGlyphFilter>::New();
        iconFilter->SetInput(pointData);
        iconFilter->SetIconSize(8,8);
        iconFilter->SetUseIconSize(false);

        // if spacing < 1, we're fucked, literally
        switch (_orientation)
        {
            case Sagittal:
            	iconFilter->SetDisplaySize(static_cast<int>(_spacing[1]),static_cast<int>(_spacing[2]));
                break;
            case Coronal:
            	iconFilter->SetDisplaySize(static_cast<int>(_spacing[0]),static_cast<int>(_spacing[2]));
                break;
            case Axial:
            	iconFilter->SetDisplaySize(static_cast<int>(_spacing[0]),static_cast<int>(_spacing[1]));
                break;
            default:
                break;
        }
        iconFilter->SetIconSheetSize(8,8);
        iconFilter->SetGravityToCenterCenter();

        // actor mapper
        vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(iconFilter->GetOutputPort());

        // vtk texture representation
        vtkSmartPointer<vtkTexture> texture =  vtkSmartPointer<vtkTexture>::New();
        texture->SetInputConnection(textureIcon->GetOutputPort());
        texture->SetInterpolate(false);
        texture->SetRepeat(false);
        texture->SetEdgeClamp(true);

        // create a new actor
        _iconActor = vtkSmartPointer<vtkActor>::New();
        _iconActor->SetMapper(mapper);
        _iconActor->SetTexture(texture);

        _renderer->AddActor(_iconActor);
    }

    // clear point set and index before a new set is created
    _slicePoints->Initialize();
    _indexArray->Initialize();

    // fill points
    switch (_orientation)
    {
        case Sagittal:
        	for (unsigned int i = _minSelection[1]; i < _maxSelection[1]+1; i++)
        		for (unsigned int j = _minSelection[2]; j < _maxSelection[2]+1; j++)
        			_slicePoints->InsertNextPoint(i*_spacing[1], j*_spacing[2], 0.0);
            break;
        case Coronal:
        	for (unsigned int i = _minSelection[0]; i < _maxSelection[0]+1; i++)
        		for (unsigned int j = _minSelection[2]; j < _maxSelection[2]+1; j++)
        			_slicePoints->InsertNextPoint(i*_spacing[0], j*_spacing[2], 0.0);
            break;
        case Axial:
        	for (unsigned int i = _minSelection[0]; i < _maxSelection[0]+1; i++)
        		for (unsigned int j = _minSelection[1]; j < _maxSelection[1]+1; j++)
        			_slicePoints->InsertNextPoint(i*_spacing[0], j*_spacing[1], 0.0);
            break;
        default:
            break;
    }

    // fill indexes
	for (int i = 0; i < _slicePoints->GetNumberOfPoints(); i++)
		_indexArray->InsertNextTuple1(0);

	// make pipeline move
	_slicePoints->Modified();
    _indexArray->Modified();

    // need to set visibility again, as the user could have moved the slider
    // and we are now in a new slice and with a new selection point.
    _iconActor->SetVisibility(true);
}

void SliceVisualization::ClearSelection()
{
    if (_iconActor != NULL)
    {
        _renderer->RemoveActor(_iconActor);
        _iconActor = NULL;
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

    _sliceActor = vtkSmartPointer<vtkActor>::New();
    _sliceActor->SetMapper(slicemapper);
    _sliceActor->GetProperty()->SetColor(1,1,1);
    _sliceActor->GetProperty()->SetPointSize(0);
    _sliceActor->GetProperty()->SetLineWidth(2);
    _sliceActor->SetPickable(false);

    _thumbRenderer->AddActor(_sliceActor);
    
    // Create a polydata to store the selection box 
    _square = vtkSmartPointer<vtkPolyData>::New();

    vtkSmartPointer <vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInput(_square);

    _squareActor = vtkSmartPointer<vtkActor>::New();
    _squareActor->SetMapper(mapper);
    _squareActor->GetProperty()->SetColor(1,1,1);
    _squareActor->GetProperty()->SetPointSize(1);
    _squareActor->GetProperty()->SetLineWidth(2);
    _squareActor->SetPickable(false);
    
    _thumbRenderer->AddActor(_squareActor);
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
    _squareActor->GetProperty()->SetColor(0,0,0);
    _sliceActor->GetProperty()->SetColor(0,0,0);
}

unsigned int SliceVisualization::GetSegmentationOpacity()
{
	return _segmentationOpacity;
}

void SliceVisualization::SetSegmentationOpacity(unsigned int value)
{
	_segmentationOpacity = value;

	if (_segmentationHidden)
		return;

	if (this->_blendimages)
		_blendimages->SetOpacity(1,static_cast<float>(value/100.0));
}

void SliceVisualization::ToggleSegmentationView(void)
{
	float opacity = 0.0;

	switch(_segmentationHidden)
	{
		case true:
			_segmentationHidden = false;
			opacity = _segmentationOpacity/100.0;
			break;
		case false:
			_segmentationHidden = true;
			break;
		default:
			break;
	}

	if (this->_blendimages)
		_blendimages->SetOpacity(1,opacity);
}
