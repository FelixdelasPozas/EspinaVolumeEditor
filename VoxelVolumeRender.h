///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: VoxelVolumeRender.h
// Purpose: renders a vtkStructuredPoints volume (raycasting volume or colored meshes)
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _VOXELVOLUMERENDER_H_
#define _VOXELVOLUMERENDER_H_

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkLookupTable.h>
#include <vtkStructuredPoints.h>
#include <vtkRenderer.h>
#include <vtkPiecewiseFunction.h>
#include <vtkColorTransferFunction.h>
#include <vtkVolumeRayCastMapper.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkMultiBlockDataSet.h>

// c++ includes
#include <set>
#include <map>

// project includes
#include "ProgressAccumulator.h"
#include "DataManager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// VoxelVolumeRender class
//
class VoxelVolumeRender
{
    public:
        // constructor & destructor
		VoxelVolumeRender(DataManager*, vtkSmartPointer<vtkRenderer>, ProgressAccumulator*);
		~VoxelVolumeRender();

        // update extent of focused object(s)
        void UpdateFocusExtent(void);

        // volume rendering switchers and status
        void ViewAsMesh();
        void ViewAsVolume();

        // color management
        void ColorHighlight(const unsigned short);
        void ColorDim(const unsigned short, double = 0.0);
        void ColorHighlightExclusive(unsigned short);
        void ColorDimAll(void);
        void UpdateColorTable(void);
        void ComputeMesh(const unsigned short);
    private:
        // private methods
        //
        // compute volume using raycast and reconstruct highlighted labels set while doing it
        void ComputeRayCastVolume();
        //
        // compute volume using GPU assisted raycast
        void ComputeGPURender();
        //
        // compute mesh representation for the volume
        void ComputeMeshes(void);

        // attributes
        vtkSmartPointer<vtkRenderer>         				_renderer;
        ProgressAccumulator          						*_progress;
        DataManager                         	  			*_dataManager;
        //
        // to update color table
        vtkSmartPointer<vtkPiecewiseFunction>      			_opacityfunction;
        vtkSmartPointer<vtkColorTransferFunction>  			_colorfunction;
        //
        // actors for the volume
        vtkSmartPointer<vtkVolume> 							_volume;
        vtkSmartPointer<vtkActor>							_mesh;
        //
        // software raycast volume mapper
        vtkSmartPointer<vtkVolumeRayCastMapper> 			_CPUmapper;
        //
        // GPU fast volume mapper
        vtkSmartPointer<vtkGPUVolumeRayCastMapper> 			_GPUmapper;
        //
        // set of highlighted labels (selected labels)
        std::set<unsigned short>							_highlightedLabels;
        //
        // view bounding box (focusing on a selected area)
        Vector3ui											_min, _max;
        //
        // mesh actors list
        struct ActorInformation
        {
        		Vector3ui 					actorMin;
        		Vector3ui 					actorMax;
        		ActorInformation(): actorMin(Vector3ui(0,0,0)), actorMax(Vector3ui(0,0,0)) { };
        };
        std::map<const unsigned short, struct VoxelVolumeRender::ActorInformation* >	_segmentationInfoList;
        //
        // mesh/volume rendering status
        bool												_renderingIsVolume;
        //
        // MultiBlockDataSet will avoid having one actor/mapper for each segmentation, with
        // this we will have just one actor and one mapper for all segmentations
        vtkSmartPointer<vtkMultiBlockDataSet> 				_blockData;
        //
        // lookuptable needed for mesh color modification
        vtkSmartPointer<vtkLookupTable>						_meshLUT;

};

#endif
