///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: VoxelVolumeRender.h
// Purpose: renders a vtkStructuredPoints volume (raycasting volume or colored meshes)
// Notes: An implementation with vtkMultiBlockDataSet that allow mesh representation with just one
//        actor and mapper is disabled but the code is still present because it's an objective
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
#include <vtkSmartVolumeMapper.h>

// c++ includes
#include <set>
#include <map>

// Qt
#include <QObject>

// project includes
#include "ProgressAccumulator.h"
#include "DataManager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// VoxelVolumeRender class
//
class VoxelVolumeRender
: public QObject
{
    Q_OBJECT
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

  public slots:
    /** \brief Updates the actor when the data changes.
     *
     */
    void onDataModified();

  private:
    /** \brief Computes volumes using plain CPU raycast.
     *
     */
    void computeVolumes();

    /** \brief Computes the mesh representation for a given segmentation label.
     * \param[in] label object label.
     *
     */
    void computeMesh(const unsigned short label);


    vtkSmartPointer<vtkRenderer>              m_renderer;          /** view's renderer.                             */
    std::shared_ptr<ProgressAccumulator>      m_progress;          /** progress accumlator to report progress.      */
    std::shared_ptr<DataManager>              m_dataManager;       /** application data manager.                    */

    vtkSmartPointer<vtkPiecewiseFunction>     m_opacityfunction;   /** voxel opacity function.                      */
    vtkSmartPointer<vtkColorTransferFunction> m_colorfunction;     /** voxel coloring function.                     */
    vtkSmartPointer<vtkSmartVolumeMapper>     m_volumeMapper;      /** voxel volume mapper.                         */

    vtkSmartPointer<vtkVolume>                m_volume;            /** volumetric actor.                            */
    vtkSmartPointer<vtkActor>                 m_mesh;              /** mesh actor.                                  */

    std::set<unsigned short>                  m_highlightedLabels; /** set of highlighted labels (selected labels). */

    Vector3ui                                 m_min;               /** bounding box minimum point.                  */
    Vector3ui                                 m_max;               /** bounding box maximum point.                  */

    struct Pipeline
    {
        vtkSmartPointer<vtkActor> mesh; /** actor                       */
        Vector3ui                 min;  /** bounding box minimum point. */
        Vector3ui                 max;  /** bounding box maximum point. */

        Pipeline(): min{Vector3ui{0,0,0}}, max{Vector3ui{0,0,0}}, mesh{nullptr} {};
    };

    std::map<const unsigned short, std::shared_ptr<Pipeline>> m_actors; /** list of actors in the view. */

    bool m_renderingIsVolume; /** true if rendering the volume as voxel and false if rendering the mesh. */
};

#endif
