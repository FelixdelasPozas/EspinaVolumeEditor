///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: DataManager.h
// Purpose: converts a itk::LabelMap to vtkStructuredPoints data and back 
// Notes: vtkStructuredPoints scalar values aren't the labelmap values so mapping between 
//        labelmap and structuredpoints scalars is neccesary
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _DATAMANAGER_H_
#define _DATAMANAGER_H_

// vtk includes
#include <vtkStructuredPoints.h>
#include <vtkLookupTable.h>
#include <vtkSmartPointer.h>

// itk includes
#include <itkLabelMap.h>
#include <itkSmartPointer.h>
#include <itkShapeLabelObject.h>

// c++ includes
#include <map>
#include <set>

// project includes
#include "Coordinates.h"
#include "Metadata.h"
#include "VectorSpaceAlgebra.h"

// Qt
#include <QObject>

// defines & typedefs
using LabelObjectType = itk::ShapeLabelObject<unsigned short, 3>;
using LabelMapType = itk::LabelMap<LabelObjectType>;

// UndoRedoSystem forward declaration
class UndoRedoSystem;

class QColor;

///////////////////////////////////////////////////////////////////////////////////////////////////
// DataManager class
//
class DataManager
: public QObject
{
    Q_OBJECT
  public:
    /** \brief DataManager class constructor.
     *
     */
    DataManager();

    /** \brief DataManager class destructor.
     *
     */
    ~DataManager(void);

    /** \brief Initializes the data manager.
     * \param[in] labelMap label map image.
     * \param[in] coordinates image coordinates.
     * \param[in] metadata image metadata.
     *
     */
    void Initialize(itk::SmartPointer<LabelMapType> labelMap,
                    std::shared_ptr<Coordinates>    coordinates,
                    std::shared_ptr<Metadata>       metadata);

    /** \brief Undo/Redo system start operation signaling.
     * \param[in] operationName name of the starting operation.
     *
     */
    void OperationStart(const std::string &operationName);

    /** \brief Undo/Redo system end operation signaling.
     *
     */
    void OperationEnd();

    /** \brief Undo/Redo system cancel operation signaling.
     *
     */
    void OperationCancel();

    /** \brief Returns current undo action name.
     *
     */
    const std::string GetUndoActionString() const;

    /** \brief Returns the current redo action name.
     *
     */
    const std::string GetRedoActionString() const;

    /** \brief Returns the current operation name.
     *
     */
    const std::string GetActualActionString() const;

    /** \brief Returns true if the undo buffer is empty.
     *
     */
    bool IsUndoBufferEmpty() const;

    /** \brief Returns true if the redo buffer is empty.
     *
     */
    bool IsRedoBufferEmpty() const;

    /** \brief Undo the last undo operation.
     *
     */
    void DoUndoOperation();

    /** \brief Redo the last redo operation.
     *
     */
    void DoRedoOperation();

    /** \brief Sets the undo/redo buffer size.
     * \param[in] size size of the buffer in bytes.
     *
     */
    void SetUndoRedoBufferSize(const unsigned long int size);

    /** \brief Returns the undo/redo system buffer size.
     *
     */
    const unsigned long int GetUndoRedoBufferSize() const;

    /** \brief Returns the undo/redo buffer current capacity.
     *
     */
    const unsigned long int GetUndoRedoBufferCapacity() const;

    /** \brief Highlights the color of the given scalar value.
     * \param[in] value value of the color to highlight.
     *
     */
    void ColorHighlight(const unsigned short value);

    /** \brief Dims the color of the given scalar.
     * \param[in] value value of the color to dim.
     *
     */
    void ColorDim(const unsigned short value);

    /** \brief Highlights the color of the given value exclusively.
     *
     */
    void ColorHighlightExclusive(const unsigned short value);

    /** \brief Dims all the colors.
     *
     */
    void ColorDimAll(void);

    /** \brief Returns true if the color is in use by another scalar value.
     *
     */
    const bool ColorIsInUse(const QColor &color) const;

    /** \brief Returns the number of used colors.
     *
     *
     */
    const unsigned int GetNumberOfColors() const;

    /** \brief Returns the QColor struct of the color assigned to the given value.
     * \param[in] value color scalar value.
     *
     */
    const QColor GetColorComponents(const unsigned short value) const;

    /** \brief Changes the color assigned to the given scalar value.
     *
     */
    void SetColorComponents(const unsigned short value, const QColor &color);

    /** \brief Changes the scalar value of the given point.
     * \param[in] point point coordinates.
     * \param[in] value new scalar value.
     *
     */
    void SetVoxelScalar(const Vector3ui &point, const unsigned short value);

    /** \brief Changes the scalar value of the given point bypassing the undo/redo system.
     * Used inside exception treatment code.
     * \param[in] point point coordinates.
     * \param[in] value new scalar value.
     *
     */
    void SetVoxelScalarRaw(const Vector3ui &point, const unsigned short value);

    /** \brief Creates a new label and assigns a new scalar to that label, starting from an initial
     * optional value. Modifies color table and returns new label position (not scalar used for that label).
     * \param[in] color color of the new label value.
     *
     */
    const unsigned short SetLabel(const QColor &color);

    /** \brief Sets the image to be managed.
     * \param[in] image image data.
     *
     */
    void SetStructuredPoints(vtkSmartPointer<vtkStructuredPoints> image);

    /** \brief Set the first scalar value that is free to assign a label (it's NOT the label number).
     * \param[in] value scalar value.
     *
     */
    void SetFirstFreeValue(const unsigned short value);

    /** \brief Sets the group of selected labels.
     * \param[in] labels group of selected labels.
     *
     */
    void SetSelectedLabelsSet(const std::set<unsigned short> &labels);

    /** \brief Returns the lookuptable used for coloring.
     *
     */
    vtkSmartPointer<vtkLookupTable> GetLookupTable() const;

    /** \brief Returns a pointer to the image data object.
     *
     */
    vtkSmartPointer<vtkStructuredPoints> GetStructuredPoints() const;

    /** \brief Returns the original labelmap used to generate the image data.
     *
     */
    itk::SmartPointer<LabelMapType> GetLabelMap() const;

    /** \brief Returns the orientation data.
     *
     */
    std::shared_ptr<Coordinates> GetOrientationData() const;

    /** \brief Returns the scalar used for the given label.
     * \param[in] label label value.
     *
     */
    unsigned short GetScalarForLabel(const unsigned short label);

    /** \brief Returns the label used for the given scalar.
     * \param[in] value scalar value.
     *
     */
    unsigned short GetLabelForScalar(const unsigned short value) const;

    /** \brief Returns the centroid of the object with the given label.
     * \param[in] label label value.
     */
    Vector3d GetCentroidForObject(const unsigned short int label);

    /** \brief Returns the scalar value of the given position.
     * \param[in] point point coordinates.
     *
     */
    const unsigned short GetVoxelScalar(const Vector3ui &point) const;

    /** \brief Returns the first scalar value that is free to assign to a label (NOT the label number).
     *
     */
    const unsigned short GetFirstFreeValue() const;

    /** \brief Returns the last used scalar in _labelValues.
     *
     */
    const unsigned short GetLastUsedValue() const;

    /** \brief Returns the color of the given scalar value.
     * \param[in] value scalar value.
     *
     */
    const QColor GetRGBAColorForScalar(const unsigned short value) const;

    /** \brief Returns the number of voxels assigned to a given label.
     * \param[in] label label value.
     *
     */
    unsigned long long int GetNumberOfVoxelsForLabel(unsigned short label);

    /** \brief Returns the bounding box minimum values for the givel label.
     * \param[in] label label value.
     *
     */
    Vector3ui GetBoundingBoxMin(unsigned short label);

    /** \brief Returns the bounding box maximum values for the givel label.
     * \param[in] label label value.
     *
     */
    Vector3ui GetBoundingBoxMax(unsigned short label);

    /** \brief Returns the number of labels used including the background label.
     *
     */
    const unsigned int GetNumberOfLabels(void) const;

    /** \brief Returns the set of selected labels.
     *
     */
    const std::set<unsigned short> GetSelectedLabelsSet(void) const;

    /** \brief Returns the selected label set size.
     *
     */
    const int GetSelectedLabelSetSize(void) const;

    /** \brief Returns true if the givel label is selected.
     *
     */
    const bool IsColorSelected(unsigned short label) const;

    struct ObjectInformation
    {
        unsigned short         scalar;   /** original scalar value in the image loaded */
        Vector3d               centroid; /** centroid of the object */
        unsigned long long int size;     /** size of the object in voxels */
        Vector3ui              min;      /** Bounding Box: min values */
        Vector3ui              max;      /** Bounding Box: max values */

        ObjectInformation()
        : scalar{0}, centroid{Vector3d{0, 0, 0}}, size{0}, min{Vector3ui{0, 0, 0}}, max{Vector3ui{0, 0, 0}} {};
    };

    /** \brief Returns the table of objects.
     *
     */
    std::map<unsigned short, std::shared_ptr<DataManager::ObjectInformation>>* GetObjectTablePointer();

    /** \brief Replaces the lookuptable with the given one.
     * \param[inout] lookuptable color table to switch.
     *
     */
    void SwitchLookupTables(vtkSmartPointer<vtkLookupTable> lookuptable);

    /** \brief Signals the data as modified.
     *
     */
    void SignalDataAsModified();

    const double HIGHLIGHT_ALPHA = 1.0;
    const double DIM_ALPHA = 0.4;

    friend class SaveSessionThread;
    friend class EspinaVolumeEditor;

  signals:
    void modified();

  private:
    /** \brief Helper method to reset the lookuptable to initial state based on original labelmap, used during init too
     *
     */
    void GenerateLookupTable();

    /** \brief Helper method to copy the values of one lookuptable to another, does not change pointers (important).
     *
     */
    void CopyLookupTable(vtkSmartPointer<vtkLookupTable> from, vtkSmartPointer<vtkLookupTable> to) const;

    /** \brief Updates the values of the action information vector.
     *
     */
    void StatisticsActionUpdate(void);

    /** \brief Clears the action information vector.
     *
     */
    void StatisticsActionClear(void);

    itk::SmartPointer<LabelMapType>      m_labelMap;         /** original labelmap object.        */
    vtkSmartPointer<vtkStructuredPoints> m_structuredPoints; /** image data object.               */
    vtkSmartPointer<vtkLookupTable>      m_lookupTable;      /** color table.                     */
    std::shared_ptr<Coordinates>         m_orientationData;  /** image orientation data.          */
    std::shared_ptr<UndoRedoSystem>      m_actionsBuffer;    /** undo/redo system.                */
    unsigned short                       m_firstFreeValue;   /** first free value for new labels. */
    std::set<unsigned short>             m_selectedLabels;   /** set of selected labels.          */

    std::map<unsigned short, std::shared_ptr<ObjectInformation>> ObjectVector; /** object information vector. */

    struct ActionInformation
    {
        long long int size;     /** size of the action in voxels.                                         */
        Vector3ll     centroid; /** sum of the x,y,z coords of the points added/substraced in the action. */
        Vector3ui     min;      /** Bounding Box: min values.                                             */
        Vector3ui     max;      /** Bounding Box: max values.                                             */

        ActionInformation()
        : size{0}, centroid{Vector3ll{0, 0, 0}}, min{Vector3ui{0, 0, 0}}, max{Vector3ui{0, 0, 0}} {};
    };

    std::map<unsigned short, std::shared_ptr<ActionInformation>> ActionInformationVector; /** action information vector. */
};

#endif // _DATAMANAGER_H_
