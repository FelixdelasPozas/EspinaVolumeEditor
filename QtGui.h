///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: QtGui.h
// Purpose: Volume Editor Qt GUI class
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _QTEDITORESPINA_H_
#define _QTEDITORESPINA_H_

// qt includes
#include "ui_QtGui.h"

// itk includes
#include <itkImage.h>
#include <itkLabelMap.h>
#include <itkLabelObject.h>
#include <itkSmartPointer.h>

// vtk includes 
#include <vtkRenderer.h>
#include <vtkLookupTable.h>
#include <vtkStructuredPoints.h>
#include <vtkSmartPointer.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkCommand.h>

// c++ includes
#include <map>
#include <vector>

// project includes
#include "SliceVisualization.h"
#include "AxesRender.h"
#include "VectorSpaceAlgebra.h"
#include "Coordinates.h"
#include "VoxelVolumeRender.h"
#include "ProgressAccumulator.h"
#include "EditorOperations.h"
#include "DataManager.h"
#include "Metadata.h"

// defines and typedefs
typedef itk::LabelObject < unsigned short,3 >  LabelObjectType;
typedef itk::LabelMap    < LabelObjectType >  LabelMapType;

using namespace std;

class EspinaVolumeEditor : public QMainWindow, private Ui_MainWindow
{
        Q_OBJECT
    public:
        EspinaVolumeEditor(QApplication *app, QWidget *parent = 0);
        ~EspinaVolumeEditor();
        
        // INTERFACE FUNCTIONS //////////////////////////////////////////////////
        
        // set the initial scalar to be used by the editor to assign to a label.
        // OPTIONAL: if not set the initial value is assumed to be 1
        void SetInitialFreeValue(unsigned short);
        
        // get the last scalar used in the editor to assign a value to a label
        unsigned short GetLastUsedScalarValue();
        
        // must be used to get sure that the user has created new labels, if false
        // the value returned for GetLastUsedScalarValue is 0
        bool GetUserCreatedNewLabels();
        
        // get the label map resulting from the volume edition
        itk::SmartPointer<LabelMapType> GetOutput();
        
        // get a pointer to the rgba values of the scalar used by the user in
        // the editor. 
        // BEWARE: deallocation must be done by the caller of the function as
        // those values won't be deleted when the editor class is destroyed.
        double* GetRGBAColorFromValue(unsigned short);
        
        // must be called before using any of the gets to be sure that the used
        // has accepted the modifications to the volume
        bool VolumeModified();
    public slots:
        
    protected:
    protected slots://
    	virtual void EditorOpen();
    	virtual void EditorReferenceOpen();
        virtual void EditorSave();
        virtual void EditorExit();
        virtual void FullscreenToggle();
        virtual void AxialInteraction(vtkObject*, unsigned long event, void*, void*, vtkCommand*);
        virtual void CoronalInteraction(vtkObject*, unsigned long event, void*, void*, vtkCommand*);
        virtual void SagittalInteraction(vtkObject*, unsigned long event, void*, void*, vtkCommand*);
        virtual void Preferences();
        virtual void About();
        virtual void MoveAxialSlider(int);
        virtual void MoveCoronalSlider(int);
        virtual void MoveSagittalSlider(int);
        virtual void SliceSliderReleased();
        virtual void SliceSliderPressed();
        virtual void ChangeXspinBox(int);
        virtual void ChangeYspinBox(int);
        virtual void ChangeZspinBox(int);
        virtual void LabelSelectionChanged(int);
        virtual void ViewReset();
        virtual void SwitchAxesView();
        virtual void SwitchVoxelRender();
        virtual void EditorSelectionEnd(bool);
        virtual void EditorCut();
        virtual void EditorRelabel();
        virtual void ErodeVolume();
        virtual void DilateVolume();
        virtual void OpenVolume();
        virtual void CloseVolume();
        virtual void WatershedVolume();
        virtual void OperationUndo();
        virtual void OperationRedo();
        virtual void ViewZoom();
    private:
        typedef enum { All, Slices, Voxel, Axial, Coronal, Sagittal } VIEWPORTSENUM;
        
        bool updatevoxelrenderer;
        bool updateslicerenderers;
        bool updatepointlabel;

        void AxialXYPick(unsigned long);
        void CoronalXYPick(unsigned long);
        void SagittalXYPick(unsigned long);
        void SetPointLabel();
        void FillColorLabels();
        void UpdateViewports(VIEWPORTSENUM);
        void UpdateUndoRedoMenu();
        
        // four renderers for four QVTKWidget viewports
        vtkSmartPointer<vtkRenderer>           _voxelViewRenderer;
        vtkSmartPointer<vtkRenderer>           _axialViewRenderer;
        vtkSmartPointer<vtkRenderer>           _sagittalViewRenderer;
        vtkSmartPointer<vtkRenderer>           _coronalViewRenderer;

        // misc attributes
        DataManager                           *_dataManager;
        VoxelVolumeRender::RenderType          _volumeRenderType;
        
        // these are managed with new and delete and not with smartpointers so
        // BEWARE. created and destroyed by editor, so no need to keep references 
        SliceVisualization                    *_axialSliceVisualization;
        SliceVisualization                    *_coronalSliceVisualization;
        SliceVisualization                    *_sagittalSliceVisualization;
        AxesRender                            *_axesRender;
        Coordinates                           *_orientationData;
        VoxelVolumeRender                     *_volumeRender;
        
        // point of interest and its label 
        Vector3ui                              _POI;
        unsigned short                         _pointScalar;
        
        // used to get mouse left button press and release events for painting
        // and for axes rendering
        vtkSmartPointer<vtkEventQtSlotConnect> _connections;
        
        // label selectable by the user, used for painting  
        unsigned short                         _selectedLabel;
        
        // to handle itk vtk filter progress information
        ProgressAccumulator                   *_progress;
        
        // area selection
        EditorOperations                      *_editorOperations;
        
        // only used after program exit to inform that the user has accepted the
        // changes to the volume. if _acceptedChanges == false then all other
        // funtions that depend on user accepting changes like GetOutput alter
        // their values.
        bool 								   _acceptedChanges;
        
        // misc boolean values that affect editor 
        bool 								   _hasReferenceImage;

        // espina metadata
        Metadata							  *_fileMetadata;
};


#endif
