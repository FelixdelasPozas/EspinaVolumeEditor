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
#include <QMutex>
#include <QTimer>
#include "ui_QtGui.h"

// itk includes
#include <itkImage.h>
#include <itkLabelMap.h>
#include <itkShapeLabelObject.h>
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
#include "SaveSession.h"

// defines and typedefs
typedef itk::ShapeLabelObject < unsigned short,3 >  LabelObjectType;
typedef itk::LabelMap    < LabelObjectType >  LabelMapType;

using namespace std;

class EspinaVolumeEditor : public QMainWindow, private Ui_MainWindow
{
        Q_OBJECT
    public:
        EspinaVolumeEditor(QApplication *app, QWidget *parent = 0);
        ~EspinaVolumeEditor();
        
        // mutex to assure that no editing action is interrupted by the save session operation that can kick in at any time
        QMutex *actionLock;

        // friends can touch your private parts
        friend class SaveSessionThread;
    public slots:
    protected:
    protected slots:
    	virtual void EditorOpen();
    	virtual void EditorReferenceOpen();
        virtual void EditorSave();
        virtual void EditorExit();
        virtual void FullscreenToggle();
        virtual void AxialInteraction(vtkObject*, unsigned long event, void*, void*, vtkCommand*);
        virtual void CoronalInteraction(vtkObject*, unsigned long event, void*, void*, vtkCommand*);
        virtual void SagittalInteraction(vtkObject*, unsigned long event, void*, void*, vtkCommand*);
        virtual void SwitchSegmentationView();
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
        virtual void DisableRenderView();
        virtual void SaveSession();
        virtual void SaveSessionStart();
        virtual void SaveSessionEnd();
        virtual void SaveSessionProgress(int);
    private:
        typedef enum { All, Slices, Voxel, Axial, Coronal, Sagittal } VIEWPORTSENUM;
        
        bool updatevoxelrenderer;
        bool updateslicerenderers;
        bool updatepointlabel;
        bool renderisavolume;

        void AxialXYPick(unsigned long);
        void CoronalXYPick(unsigned long);
        void SagittalXYPick(unsigned long);
        void GetPointLabel();
        void FillColorLabels();
        void UpdateViewports(VIEWPORTSENUM);
        void UpdateUndoRedoMenu();
        void RestoreSavedSession();
        void RemoveSessionFiles();
        
        // four renderers for four QVTKWidget viewports
        vtkSmartPointer<vtkRenderer>           _voxelViewRenderer;
        vtkSmartPointer<vtkRenderer>           _axialViewRenderer;
        vtkSmartPointer<vtkRenderer>           _sagittalViewRenderer;
        vtkSmartPointer<vtkRenderer>           _coronalViewRenderer;

        // misc attributes
        DataManager                           *_dataManager;
        
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
        unsigned short int                     _selectedLabel;
        
        // to handle itk vtk filter progress information
        ProgressAccumulator                   *_progress;
        
        // area selection
        EditorOperations                      *_editorOperations;
        
        // misc boolean values that affect editor 
        bool 								   _hasReferenceImage;
        bool								   _segmentationsAreVisible;

        // espina metadata
        Metadata							  *_fileMetadata;

        // session timer
        SaveSessionThread					  *_saveSessionThread;
        QTimer 								  *_sessionTimer;

        // names of files for saving session or reopening a failed session
        std::string 						   _segmentationFileName;
        std::string							   _referenceFileName;
};

#endif
