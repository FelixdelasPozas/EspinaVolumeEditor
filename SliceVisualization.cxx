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
#include <vtkTextProperty.h>
#include <vtkCoordinate.h>
#include <vtkImageCanvasSource2D.h>
#include <vtkTexture.h>
#include <vtkPolyDataMapper.h>
#include <vtkImageItem.h>
#include <vtkRenderWindow.h>
#include <vtkLine.h>
#include <vtkIconGlyphFilter.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkImageCast.h>
#include <vtkImageDataGeometryFilter.h>
#include <vtkMath.h>
#include <vtkImageMapper3D.h>

// project includes
#include "SliceVisualization.h"
#include "Selection.h"
#include "BoxSelectionWidget.h"
#include "BoxSelectionRepresentation2D.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// SliceVisualization class
//
SliceVisualization::SliceVisualization(OrientationType orientation)
{
	// initilize options, minimum required
	this->_segmentationOpacity = 75;
	this->_segmentationHidden = false;
	this->_blendimages = NULL;
	this->_thumbRenderer = NULL;
	this->_renderer = NULL;
	this->_orientation = orientation;
	this->_sliceWidget = NULL;
	this->_boundsX = 0;
	this->_boundsY = 0;
	this->_referenceColors = NULL;
	this->_reslice = NULL;

	// create 2D actors texture
	vtkSmartPointer<vtkImageCanvasSource2D> volumeTextureIcon = vtkSmartPointer<vtkImageCanvasSource2D>::New();
	volumeTextureIcon->SetScalarTypeToUnsignedChar();
	volumeTextureIcon->SetExtent(0, 23, 0, 7, 0, 0);
	volumeTextureIcon->SetNumberOfScalarComponents(4);
	volumeTextureIcon->SetDrawColor(0, 0, 0, 0);
	volumeTextureIcon->FillBox(0, 23, 0, 7);
	volumeTextureIcon->SetDrawColor(0, 0, 0, 100);
	volumeTextureIcon->FillBox(16, 19, 0, 3);
	volumeTextureIcon->FillBox(20, 24, 4, 7);
	volumeTextureIcon->SetDrawColor(255, 255, 255, 100);
	volumeTextureIcon->FillBox(16, 19, 4, 7);
	volumeTextureIcon->FillBox(20, 24, 0, 3);

    // vtk texture representation
	this->_texture = vtkSmartPointer<vtkTexture>::New();
	this->_texture->SetInputConnection(volumeTextureIcon->GetOutputPort());
	this->_texture->SetInterpolate(false);
	this->_texture->SetRepeat(false);
	this->_texture->SetEdgeClamp(false);
	this->_texture->SetBlendingMode(vtkTexture::VTK_TEXTURE_BLENDING_MODE_NONE);
}

void SliceVisualization::Initialize(
        vtkSmartPointer<vtkStructuredPoints> points, 
        vtkSmartPointer<vtkLookupTable> colorTable, 
        vtkSmartPointer<vtkRenderer> renderer,
        Coordinates* OrientationData)
{
    // get data properties
	this->_size = OrientationData->GetTransformedSize();
    Vector3ui point = (_size / 2) - Vector3ui(1); // unsigned int, not double because we want the slice index
    double matrix[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
                              
    this->_spacing = OrientationData->GetImageSpacing();
    this->_max = Vector3d((this->_size[0]-1)*this->_spacing[0], (this->_size[1]-1)*this->_spacing[1], (this->_size[2]-1)*this->_spacing[2]);
    this->_axesMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    this->_renderer = renderer;
    
    // define reslice matrix
    switch(this->_orientation)
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
    this->_axesMatrix->DeepCopy(matrix);
    
    // generate all actors
    GenerateSlice(points, colorTable);
    GenerateCrosshair();
    
    // create text legend
    this->_text = vtkSmartPointer<vtkTextActor>::New();
    
    this->_textbuffer = std::string("None");
    this->_text->SetInput(this->_textbuffer.c_str());
    this->_text->SetTextScaleMode(vtkTextActor::TEXT_SCALE_MODE_NONE);

    vtkSmartPointer<vtkCoordinate> textcoord = this->_text->GetPositionCoordinate();
    textcoord->SetCoordinateSystemToNormalizedViewport();
    textcoord->SetValue(0.02, 0.02);
    
    vtkSmartPointer<vtkTextProperty> textproperty = this->_text->GetTextProperty();
    textproperty->SetColor(1, 1, 1);
    textproperty->SetFontFamilyToArial();
    textproperty->SetFontSize(11);
    textproperty->BoldOff();
    textproperty->ItalicOff();
    textproperty->ShadowOff();
    textproperty->SetJustificationToLeft();
    textproperty->SetVerticalJustificationToBottom();    
    this->_text->Modified();
    this->_text->PickableOff();
    this->_renderer->AddViewProp(_text);

    // we set point out of range to force an update of the first call as all components will be
    // different from the update point
    this->_point = this->_size + Vector3ui(1);

    GenerateThumbnail();
}

SliceVisualization::~SliceVisualization()
{
	// remove segmentation actors
	ClearSelections();

	if (this->_sliceWidget)
		this->_sliceWidget = NULL;

	// remove thumb renderer
	if (this->_thumbRenderer)
		this->_thumbRenderer->RemoveAllViewProps();
	
	if (this->_renderer)
		this->_renderer->GetRenderWindow()->RemoveRenderer(this->_thumbRenderer);
}

void SliceVisualization::GenerateSlice(
        vtkSmartPointer<vtkStructuredPoints> points, 
        vtkSmartPointer<vtkLookupTable> colorTable)
{
    vtkSmartPointer<vtkImageReslice> reslice = vtkSmartPointer<vtkImageReslice>::New();
    reslice->SetOptimization(true);
    reslice->BorderOn();
    reslice->SetInputData(points);
    reslice->SetOutputDimensionality(2);
    reslice->SetResliceAxes(_axesMatrix);
    reslice->Update();
    
    this->_segmentationcolors = vtkSmartPointer<vtkImageMapToColors>::New();
    this->_segmentationcolors->SetLookupTable(colorTable);
    this->_segmentationcolors->SetOutputFormatToRGBA();
    this->_segmentationcolors->SetInputConnection(reslice->GetOutputPort());
    this->_segmentationcolors->Update();
    
    this->_segmentationActor = vtkSmartPointer<vtkImageActor>::New();
    this->_segmentationActor->SetInputData(this->_segmentationcolors->GetOutput());
    this->_segmentationActor->SetInterpolate(false);
    
    this->_picker = vtkSmartPointer<vtkPropPicker>::New();
    this->_picker->PickFromListOn();
    this->_picker->InitializePickList();
    this->_picker->AddPickList(this->_segmentationActor);
    this->_segmentationActor->PickableOn();
    this->_segmentationActor->Update();
    
    this->_renderer->AddActor(this->_segmentationActor);
}

void SliceVisualization::GenerateCrosshair()
{
    vtkSmartPointer<vtkPolyData> verticaldata = vtkSmartPointer<vtkPolyData>::New();
    vtkSmartPointer<vtkPolyData> horizontaldata = vtkSmartPointer<vtkPolyData>::New();
    
    vtkSmartPointer<vtkDataSetMapper> vertmapper = vtkSmartPointer<vtkDataSetMapper>::New();
    vertmapper->SetInputData(verticaldata);
    
    vtkSmartPointer<vtkDataSetMapper> horimapper = vtkSmartPointer<vtkDataSetMapper>::New();
    horimapper->SetInputData(horizontaldata);
    
    this->_vertactor = vtkSmartPointer<vtkActor>::New();
    this->_vertactor->SetMapper(vertmapper);
    this->_vertactor->GetProperty()->SetColor(1,1,1);
    this->_vertactor->GetProperty()->SetLineStipplePattern(0xF0F0);
    this->_vertactor->GetProperty()->SetLineStippleRepeatFactor(1);
    this->_vertactor->GetProperty()->SetPointSize(1);
    this->_vertactor->GetProperty()->SetLineWidth(2);
    this->_vertactor->SetPickable(false);

    this->_horiactor = vtkSmartPointer<vtkActor>::New();
    this->_horiactor->SetMapper(horimapper);
    this->_horiactor->GetProperty()->SetColor(1,1,1);
    this->_horiactor->GetProperty()->SetLineStipplePattern(0xF0F0);
    this->_horiactor->GetProperty()->SetLineStippleRepeatFactor(1);
    this->_horiactor->GetProperty()->SetPointSize(1);
    this->_horiactor->GetProperty()->SetLineWidth(2);
    this->_horiactor->SetPickable(false);
    
    this->_renderer->AddActor(this->_vertactor);
    this->_renderer->AddActor(this->_horiactor);
    
    this->_POIHorizontalLine = horizontaldata;
    this->_POIVerticalLine = verticaldata;
}

void SliceVisualization::Update(Vector3ui point)
{
    if (this->_point == point)
        return;
    
    UpdateSlice(point);
    this->_thumbRenderer->Render();
    this->_renderer->Render();
    UpdateCrosshair(point);
}

void SliceVisualization::UpdateSlice(Vector3ui point)
{
    double slice_point;
    std::stringstream out;
    this->_textbuffer = std::string("Slice ");
    
    // change slice by changing reslice axes
    switch(_orientation)
    {
        case Sagittal:
            if (this->_point[0] == point[0])
                return;
            slice_point = point[0] * this->_spacing[0];
            this->_axesMatrix->SetElement(0, 3, slice_point);
            this->_point[0] = point[0];
            out << point[0]+1 << " of " << this->_size[0];
            break;
        case Coronal:
            if (this->_point[1] == point[1])
                return;
            slice_point = point[1] * this->_spacing[1];
            this->_axesMatrix->SetElement(1, 3, slice_point);
            this->_point[1] = point[1];
            out << point[1]+1 << " of " << this->_size[1];
            break;
        case Axial:
            if (this->_point[2] == point[2])
                return;
            slice_point = point[2] * this->_spacing[2];
            this->_axesMatrix->SetElement(2, 3, slice_point);
            this->_point[2] = point[2];
            out << point[2]+1 << " of " << this->_size[2];
            break;
        default:
            break;
    }
    
    // recalculate visibility for all volume selection actors
	for_each(this->_actorList.begin(), this->_actorList.end(), std::bind1st(std::mem_fun(&SliceVisualization::ModifyActorVisibility), this));

	this->_axesMatrix->Modified();
	this->_textbuffer += out.str();
	this->_text->SetInput(this->_textbuffer.c_str());
	this->_text->Modified();
	this->_segmentationcolors->Update();
	this->_segmentationActor->GetMapper()->Update();
	this->_segmentationActor->Update();
  if(this->_blendActor)
  {
    this->_reslice->Update();
    this->_referenceColors->Update();
    this->_blendActor->GetMapper()->Update();
    this->_blendActor->Update();
  }
}

void SliceVisualization::UpdateCrosshair(Vector3ui point)
{
    vtkSmartPointer<vtkPoints> verticalpoints = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkPoints> horizontalpoints = vtkSmartPointer<vtkPoints>::New();
    
    Vector3d point3d(Vector3d(point[0]*this->_spacing[0], point[1]*this->_spacing[1], point[2]*this->_spacing[2]));
    
    switch(this->_orientation)
    {
        case Sagittal:
            if ((this->_point[1] == point[1]) && (this->_point[2] == point[2]))
                return;
            horizontalpoints->InsertNextPoint(      		0, point3d[2], 0);
            horizontalpoints->InsertNextPoint(this->_max[1], point3d[2], 0);
            verticalpoints->InsertNextPoint(point3d[1],    		    0, 0);
            verticalpoints->InsertNextPoint(point3d[1],   this->_max[2], 0);
            this->_point[1] = point[1];
            this->_point[2] = point[2];
            break;
        case Coronal:
            if ((this->_point[0] == point[0]) && (this->_point[2] == point[2]))
                return;
            horizontalpoints->InsertNextPoint(            0, point3d[2], 0);
            horizontalpoints->InsertNextPoint(this->_max[0], point3d[2], 0);
            verticalpoints->InsertNextPoint(point3d[0],               0, 0);
            verticalpoints->InsertNextPoint(point3d[0],   this->_max[2], 0);
            this->_point[0] = point[0];
            this->_point[2] = point[2];
            break;
        case Axial:
            if ((this->_point[0] == point[0]) && (this->_point[1] == point[1]))
                return;
            horizontalpoints->InsertNextPoint(            0, point3d[1], 0);
            horizontalpoints->InsertNextPoint(this->_max[0], point3d[1], 0);
            verticalpoints->InsertNextPoint(point3d[0],               0, 0);
            verticalpoints->InsertNextPoint(point3d[0],   this->_max[1], 0);
            this->_point[0] = point[0];
            this->_point[1] = point[1];
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
    
    this->_POIHorizontalLine->Reset();
    this->_POIHorizontalLine->SetPoints(horizontalpoints);
    this->_POIHorizontalLine->SetLines(horizontalline);
    this->_POIVerticalLine->Reset();
    this->_POIVerticalLine->SetPoints(verticalpoints);
    this->_POIVerticalLine->SetLines(verticalline);

    this->_POIHorizontalLine->Modified();
    this->_POIVerticalLine->Modified();
}

SliceVisualization::PickingType SliceVisualization::GetPickData(int* X, int* Y)
{
    PickingType pickedProp = None;
    
    // thumbnail must be onscreen to be really picked
    this->_picker->Pick(*X, *Y, 0.0, this->_thumbRenderer);
    if ((this->_picker->GetViewProp() != NULL) && (true == this->_thumbRenderer->GetDraw()))
        pickedProp = Thumbnail;
    else
    {
        // nope, did the user pick the slice?
    	this->_picker->Pick(*X, *Y, 0.0, this->_renderer);
        if (this->_picker->GetViewProp() != NULL)
            pickedProp = Slice;
    }
    
    if (None == pickedProp)
        return pickedProp;

    // or the thumbnail or the slice have been picked
    double point[3];
    this->_picker->GetPickPosition(point);

    // picked values passed by reference, not elegant, we first use round to get the nearest point
    switch(this->_orientation)
    {
        case Axial:
            *X = vtkMath::Round(point[0]/this->_spacing[0]);
            *Y = vtkMath::Round(point[1]/this->_spacing[1]);
            break;
        case Coronal:
            *X = vtkMath::Round(point[0]/this->_spacing[0]);
            *Y = vtkMath::Round(point[1]/this->_spacing[2]);
            break;
        case Sagittal:
            *X = vtkMath::Round(point[0]/this->_spacing[1]);
            *Y = vtkMath::Round(point[1]/this->_spacing[2]);
            break;
        default:
            break;
    }
    
    return pickedProp;
}

void SliceVisualization::ClearSelections()
{
	while(!this->_actorList.empty())
	{
		struct ActorData* actorInformation = this->_actorList.back();
		this->_renderer->RemoveActor(actorInformation->actor);
		actorInformation->actor = NULL;
		this->_actorList.pop_back();
		delete actorInformation;
	}

	// supposedly, because of the smartpointers, the subvolumes used for reslicing must
	// be destroyed now
}

void SliceVisualization::GenerateThumbnail()
{
    // get image bounds for thumbnail activation, we're only interested in bounds[1] & bounds[3]
    // as we know already the values of the others
    double bounds[6];
    this->_segmentationActor->GetBounds(bounds);
    this->_boundsX = bounds[1];
    this->_boundsY = bounds[3];

    // create thumbnail renderer
    this->_thumbRenderer = vtkSmartPointer<vtkRenderer>::New();
    this->_thumbRenderer->AddActor(this->_segmentationActor);
    this->_thumbRenderer->ResetCamera();
    this->_thumbRenderer->SetInteractive(false);
    
    // coordinates are normalized display coords (range 0-1 in double)
    this->_thumbRenderer->SetViewport(0.65, 0.0, 1.0, 0.35);
    this->_renderer->GetRenderWindow()->AddRenderer(this->_thumbRenderer);

    this->_renderer->GetRenderWindow()->AlphaBitPlanesOn();
    this->_renderer->GetRenderWindow()->SetDoubleBuffer(true);
    this->_renderer->GetRenderWindow()->SetNumberOfLayers(2);
    this->_renderer->SetLayer(0);
    this->_thumbRenderer->SetLayer(1);
    this->_thumbRenderer->DrawOff();

    // create the slice border points
    double p0[3] = { 0.0, 0.0, 0.0 };
    double p1[3] = { this->_boundsX, 0.0, 0.0 };
    double p2[3] = { this->_boundsX, this->_boundsY, 0.0 };
    double p3[3] = { 0.0, this->_boundsY, 0.0 };

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
    sliceborder->Modified();

    vtkSmartPointer <vtkPolyDataMapper> slicemapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    slicemapper->SetInputData(sliceborder);
    slicemapper->Update();

    this->_sliceActor = vtkSmartPointer<vtkActor>::New();
    this->_sliceActor->SetMapper(slicemapper);
    this->_sliceActor->GetProperty()->SetColor(1,1,1);
    this->_sliceActor->GetProperty()->SetPointSize(0);
    this->_sliceActor->GetProperty()->SetLineWidth(2);
    this->_sliceActor->SetPickable(false);

    this->_thumbRenderer->AddActor(this->_sliceActor);
    
    // Create a polydata to store the selection box 
    this->_square = vtkSmartPointer<vtkPolyData>::New();

    vtkSmartPointer <vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(this->_square);

    this->_squareActor = vtkSmartPointer<vtkActor>::New();
    this->_squareActor->SetMapper(mapper);
    this->_squareActor->GetProperty()->SetColor(1,1,1);
    this->_squareActor->GetProperty()->SetPointSize(1);
    this->_squareActor->GetProperty()->SetLineWidth(2);
    this->_squareActor->SetPickable(false);
    
    this->_thumbRenderer->AddActor(this->_squareActor);
}

void SliceVisualization::ZoomEvent()
{
    double xy[3];
    double *value;
    double lowerleft[2];
    double upperright[2];
    
    vtkSmartPointer<vtkCoordinate> coords = vtkSmartPointer<vtkCoordinate>::New();
    
    coords->SetViewport(this->_renderer);
    coords->SetCoordinateSystemToNormalizedViewport();
    
    xy[0] = 0.0; xy[1] = 0.0; xy[2] = 0.0;
    coords->SetValue(xy);
    value = coords->GetComputedWorldValue(this->_renderer);
    lowerleft[0] = value[0]; lowerleft[1] = value[1]; 
    
    xy[0] = 1.0; xy[1] = 1.0; xy[2] = 0.0;
    coords->SetValue(xy);
    value = coords->GetComputedWorldValue(this->_renderer);
    upperright[0] = value[0]; upperright[1] = value[1];
    
    // is the slice completely inside the viewport?
    if ((0.0 >= lowerleft[0]) && (0.0 >= lowerleft[1]) && (this->_boundsX <= upperright[0]) && (this->_boundsY <= upperright[1]))
    {
    	this->_thumbRenderer->DrawOff();
    	this->_thumbRenderer->GetRenderWindow()->Render();
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
        this->_square->Reset();
        this->_square->SetPoints(points);
        this->_square->SetLines(lines);
        this->_square->Modified();

        this->_thumbRenderer->DrawOn();
        this->_thumbRenderer->GetRenderWindow()->Render();
    }
}

void SliceVisualization::SetReferenceImage(vtkSmartPointer<vtkStructuredPoints> stackImage)
{
	_reslice = vtkSmartPointer< vtkImageReslice > ::New();
	_reslice->SetOptimization(true);
	_reslice->BorderOn();
	_reslice->SetInputData(stackImage);
	_reslice->SetOutputDimensionality(2);
	_reslice->SetResliceAxes(this->_axesMatrix);
	_reslice->Update();

	// the image is grayscale, so only uses 256 colors
	vtkSmartPointer<vtkLookupTable> colorTable = vtkSmartPointer<vtkLookupTable>::New();
	colorTable->SetTableRange(0,255);
	colorTable->SetValueRange(0.0,1.0);
	colorTable->SetSaturationRange(0.0,0.0);
	colorTable->SetHueRange(0.0,0.0);
	colorTable->SetAlphaRange(1.0,1.0);
	colorTable->SetNumberOfColors(256);
	colorTable->Build();
	
	this->_referenceColors = vtkSmartPointer<vtkImageMapToColors>::New();
	this->_referenceColors->SetInputData(_reslice->GetOutput());
	this->_referenceColors->SetLookupTable(colorTable);
	this->_referenceColors->SetUpdateExtentToWholeExtent();
	this->_referenceColors->Update();
	
//	vtkSmartPointer<vtkImageMapToColors> stackcolors = vtkSmartPointer<vtkImageMapToColors>::New();
//	stackcolors->SetLookupTable(colorTable);
//	stackcolors->SetOutputFormatToRGBA();
//	stackcolors->SetInputConnection(reslice->GetOutputPort());
//	stackcolors->Update();
//
//	// remove actual actor and add the blended actor with both the segmentation and the stack
//	this->_blendimages = vtkSmartPointer<vtkImageBlend>::New();
//	this->_blendimages->SetInputData(0, stackcolors->GetOutput());
//	this->_blendimages->SetInputData(1, this->_segmentationcolors->GetOutput());
//	this->_blendimages->SetOpacity(1, static_cast<double>(this->_segmentationOpacity/100.0));
//	this->_blendimages->SetBlendModeToNormal();
//	this->_blendimages->Update();

	this->_blendActor = vtkSmartPointer<vtkImageActor>::New();
//	this->_blendActor->SetInputData(_blendimages->GetOutput());
	this->_blendActor->SetInputData(_referenceColors->GetOutput());
	this->_blendActor->PickableOn();
	this->_blendActor->SetInterpolate(false);
	this->_blendActor->Update();
	
	double pos[3];
	this->_blendActor->GetPosition(pos);
	pos[2] -= 0.5;
	this->_blendActor->SetPosition(pos);

//	this->_picker->DeletePickList(this->_segmentationActor);
	this->_picker->AddPickList(this->_blendActor);

//	this->_renderer->RemoveActor(this->_segmentationActor);
	this->_renderer->AddActor(this->_blendActor);

//	this->_thumbRenderer->RemoveActor(this->_segmentationActor);
	this->_thumbRenderer->AddActor(this->_blendActor);

	// change color for the crosshair because reference images tend to be too white and it messes with it
	this->_horiactor->GetProperty()->SetColor(0,0,0);
	this->_vertactor->GetProperty()->SetColor(0,0,0);
	this->_squareActor->GetProperty()->SetColor(0,0,0);
	this->_sliceActor->GetProperty()->SetColor(0,0,0);

  // this stupid action allows the selection actors to be seen, if we don't do this actors are occluded by
  // the blended one
	for_each(this->_actorList.begin(), this->_actorList.end(), std::bind1st(std::mem_fun(&SliceVisualization::ModifyActorVisibility), this));
}

unsigned int SliceVisualization::GetSegmentationOpacity()
{
	return this->_segmentationOpacity;
}

void SliceVisualization::SetSegmentationOpacity(unsigned int value)
{
	this->_segmentationOpacity = value;

	if (this->_segmentationHidden)
		return;

	if (this->_blendimages)
		this->_blendimages->SetOpacity(1,static_cast<float>(value/100.0));
}

void SliceVisualization::ToggleSegmentationView(void)
{
	float opacity = 0.0;

	if(this->_segmentationHidden)
	{
			this->_segmentationHidden = false;
			opacity = _segmentationOpacity/100.0;
	}
	else
	{
			this->_segmentationHidden = true;
	}

	if (this->_blendimages)
		this->_blendimages->SetOpacity(1,opacity);

	for_each(this->_actorList.begin(), this->_actorList.end(), std::bind1st(std::mem_fun(&SliceVisualization::ModifyActorVisibility), this));
}

void SliceVisualization::ModifyActorVisibility(struct ActorData* actorInformation)
{
	int minSlice, maxSlice;
	int slice = 0;

	if (this->_segmentationHidden == true)
	{
		actorInformation->actor->SetVisibility(false);

		if (this->_sliceWidget != NULL)
		{
			this->_sliceWidget->GetRepresentation()->SetVisibility(false);
			this->_sliceWidget->SetEnabled(false);
		}
	}
	else
	{
	    switch (this->_orientation)
	    {
	        case Sagittal:
	        	slice = this->_point[0];
	            break;
	        case Coronal:
	        	slice = this->_point[1];
	            break;
	        case Axial:
	        	slice = this->_point[2];
	            break;
	        default:
	            break;
	    }

	    minSlice = actorInformation->minSlice;
	    maxSlice = actorInformation->maxSlice;

		if ((minSlice <= slice) && (maxSlice >= slice))
			actorInformation->actor->SetVisibility(true);
		else
			actorInformation->actor->SetVisibility(false);

		if (this->_sliceWidget != NULL)
		{
			// correct the fact that selection volumes has a minSlice-1 and maxSlice+1 for correct marching cubes
			minSlice++;
			maxSlice--;

			if ((minSlice <= slice) && (maxSlice >= slice))
			{
				this->_sliceWidget->GetRepresentation()->SetVisibility(true);
				this->_sliceWidget->SetEnabled(true);
			}
			else
			{
				this->_sliceWidget->GetRepresentation()->SetVisibility(false);
				this->_sliceWidget->SetEnabled(false);
			}
		}
	}
	actorInformation->actor->GetMapper()->Update();
}

void SliceVisualization::SetSelectionVolume(const vtkSmartPointer<vtkImageData> selectionBuffer, bool useActorBounds)
{
	vtkSmartPointer<vtkImageReslice> reslice = vtkSmartPointer<vtkImageReslice> ::New();
	reslice->SetOptimization(true);
	reslice->BorderOn();
	reslice->SetInputData(selectionBuffer);
	reslice->SetOutputDimensionality(2);
	reslice->SetResliceAxes(this->_axesMatrix);
	reslice->Update();

	// we need integer indexes, must cast first
	vtkSmartPointer<vtkImageCast> caster = vtkSmartPointer<vtkImageCast>::New();
	caster->SetInputData(reslice->GetOutput());
	caster->SetOutputScalarTypeToInt();
	caster->Update();

	// transform the vtkImageData to vtkPolyData
	vtkSmartPointer<vtkImageDataGeometryFilter> imageDataGeometryFilter = vtkSmartPointer<vtkImageDataGeometryFilter>::New();
	imageDataGeometryFilter->SetInputData(caster->GetOutput());
	imageDataGeometryFilter->SetGlobalWarningDisplay(false);
	imageDataGeometryFilter->SetThresholdCells(true);
	imageDataGeometryFilter->SetThresholdValue(Selection::SELECTION_UNUSED_VALUE);
	imageDataGeometryFilter->Update();

    // create filter to apply the same texture to every point in the set
    vtkSmartPointer<vtkIconGlyphFilter> iconFilter = vtkSmartPointer<vtkIconGlyphFilter>::New();
    iconFilter->SetInputData(imageDataGeometryFilter->GetOutput());
    iconFilter->SetIconSize(8,8);
    iconFilter->SetUseIconSize(false);
    // if spacing < 1, we're fucked, literally
    switch (this->_orientation)
    {
        case SliceVisualization::Axial:
        	iconFilter->SetDisplaySize(static_cast<int>(this->_spacing[0]),static_cast<int>(this->_spacing[1]));
            break;
        case SliceVisualization::Coronal:
        	iconFilter->SetDisplaySize(static_cast<int>(this->_spacing[0]),static_cast<int>(this->_spacing[2]));
            break;
        case SliceVisualization::Sagittal:
        	iconFilter->SetDisplaySize(static_cast<int>(this->_spacing[1]),static_cast<int>(this->_spacing[2]));
            break;
        default:
            break;
    }
    iconFilter->SetIconSheetSize(24,8);
    iconFilter->SetGravityToCenterCenter();

    // actor mapper
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(iconFilter->GetOutputPort());

    // create a new actor
    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->SetTexture(this->_texture);
    actor->SetDragable(false);
    actor->UseBoundsOff();

	this->_renderer->AddActor(actor);

    double bounds[6];
    selectionBuffer->GetBounds(bounds);

    struct ActorData *actorInformation = new struct ActorData();
    actorInformation->actor = actor;

    switch (this->_orientation)
    {
        case SliceVisualization::Sagittal:
        	if (useActorBounds)
        	{
				actorInformation->minSlice = static_cast<int>(bounds[0]/_spacing[0]);
				actorInformation->maxSlice = static_cast<int>(bounds[1]/_spacing[0]);
        	}
        	else
        	{
				actorInformation->minSlice = 0;
				actorInformation->maxSlice = this->_size[0];
        	}
            break;
        case SliceVisualization::Coronal:
        	if (useActorBounds)
        	{
				actorInformation->minSlice = static_cast<int>(bounds[2]/_spacing[1]);
				actorInformation->maxSlice = static_cast<int>(bounds[3]/_spacing[1]);
        	}
        	else
        	{
        		actorInformation->minSlice = 0;
        		actorInformation->maxSlice = this->_size[1];
        	}
            break;
        case SliceVisualization::Axial:
        	if (useActorBounds)
        	{
				actorInformation->minSlice = static_cast<int>(bounds[4]/_spacing[2]);
				actorInformation->maxSlice = static_cast<int>(bounds[5]/_spacing[2]);
        	}
        	else
        	{
        		actorInformation->minSlice = 0;
        		actorInformation->maxSlice = this->_size[2];
        	}
            break;
        default:
            break;
    }

    this->_actorList.push_back(actorInformation);
    ModifyActorVisibility(actorInformation);
}

const SliceVisualization::OrientationType SliceVisualization::GetOrientationType(void)
{
	return this->_orientation;
}

vtkSmartPointer<vtkRenderer> SliceVisualization::GetViewRenderer(void)
{
	return this->_renderer;
}

void SliceVisualization::SetSliceWidget(vtkSmartPointer<vtkAbstractWidget> widget)
{
	this->_sliceWidget = widget;
}

vtkImageActor* SliceVisualization::GetSliceActor(void)
{
	if (this->_blendActor)
		return this->_blendActor.GetPointer();

	// else return the segmentation actor pointer, as there is not a reference image loaded
	return this->_segmentationActor.GetPointer();
}
