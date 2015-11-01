///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: VoxelVolumeRender.h
// Purpose: renders a vtkStructuredPoints volume (raycasting volume or colored meshes)
// Notes: An implementation with vtkMultiBlockDataSet that allow mesh representation with just one
//        actor and mapper is disabled but the code is still present because it's and objective
//        worth pursuing in the future
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
//#include <vtkMultiBlockDataSet.h>

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
    /** \brief VoxelVolumeRender class constructor.
     * \param[in] dataManager application data manager.
     * \param[in] renderer view's renderer.
     * \param[in] pa application progress accmulator.
     *
     */
    VoxelVolumeRender(std::shared_ptr<DataManager> dataManager, vtkSmartPointer<vtkRenderer>, std::shared_ptr<ProgressAccumulator> pa);

    /** \brief VoxelVolumeRenderer class destructor.
     *
     */
    ~VoxelVolumeRender();

    /** \brief Update extent of focused object(s).
     *
     */
    void updateFocusExtent();

    /** \brief Switches the rendering method to mesh.
     *
     */
    void viewAsMesh();

    /** \brief Switches the rendering method to volumetric.
     *
     */
    void viewAsVolume();

    /** \brief Highlights the givel label color.
     * \param[in] label object label.
     *
     */
    void colorHighlight(const unsigned short label);

    /** \brief Dims the given label color.
     * \param[in] label object label.
     * \param[in] opacity opacity value [0,1]
     *
     */
    void colorDim(const unsigned short label, double opacity = 0.0);

    /** \brief Hightlights the given label color exclusively.
     *
     */
    void colorHighlightExclusive(const unsigned short label);

    /** \brief Dims all colors.
     *
     */
    void colorDimAll();

    /** \brief Updates the color table.
     *
     */
    void updateColorTable();
  private:
    /** \brief Computes volumes using GPU assisted raycast.
     *
     */
    void computeGPURender();

    /** \brief Computes the mesh representation for a given segmentation label.
     * \param[in] label object label.
     *
     */
    void computeMesh(const unsigned short label);


    vtkSmartPointer<vtkRenderer> m_renderer;
    std::shared_ptr<ProgressAccumulator> m_progress;
    std::shared_ptr<DataManager> m_dataManager;

    vtkSmartPointer<vtkPiecewiseFunction> m_opacityfunction;
    vtkSmartPointer<vtkColorTransferFunction> m_colorfunction;

    vtkSmartPointer<vtkVolume> m_volume; /** volumetric actor. */
    vtkSmartPointer<vtkActor> m_mesh; /** mesh actor. */

    vtkSmartPointer<vtkGPUVolumeRayCastMapper> m_GPUmapper; /** GPU raycast volume mapper. */

    std::set<unsigned short> m_highlightedLabels; /** set of highlighted labels (selected labels). */

    Vector3ui m_min; /** bounding box minimum point. */
    Vector3ui m_max; /** bounding box maximum point. */

    struct Pipeline
    {
        vtkSmartPointer<vtkActor> mesh; /** actor */
        Vector3ui min; /** bounding box minimum point. */
        Vector3ui max; /** bounding box maximum point. */

        Pipeline(): min{Vector3ui{0,0,0}}, max{Vector3ui{0,0,0}}, mesh{nullptr} {};
    };

    std::map<const unsigned short, std::shared_ptr<Pipeline>> m_actors;

    bool m_renderingIsVolume;
};

#endif
