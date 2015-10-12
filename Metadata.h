///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Metadata.h
// Purpose: read, store and write Espina file metadata (segmha)
// Notes:
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _METADATA_H_
#define _METADATA_H_

// c++ includes
#include <vector>
#include <map>

// qt includes
#include <QFile>
#include <QString>

// project includes
#include "VectorSpaceAlgebra.h"

class DataManager;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Metadata class
//
class Metadata
{
  public:
    /** \brief Metadata class constructor.
     *
     */
    Metadata();

    /** \brief Read metadata from a segmha file, returns true on success and false otherwise.
     * \param[in] fileName name of the segmha file.
     *
     */
    bool Read(const QString &fileName);

    /** \brief Write metadata to a segmha file, returns true on success and false otherwise.
     * \param[in] fileName name of the segmha file.
     *
     */
    bool Write(const QString &fileName, std::shared_ptr<DataManager> data) const;

    /** \brief Returns the segment name of the object
     * \param[in] objectNum object number.
     *
     */
    std::string GetObjectSegmentName(unsigned short objectNum) const;

    /** \brief Returns the segment scalar.
     * \param[in] objectNum object number.
     *
     */
    unsigned short GetObjectScalar(unsigned short objectNum) const;

    /** \brief Compact object vector deleting unused objects and storing them into UnusedObjects vector.
     *
     */
    void CompactObjects(void);

    /** \brief Marks object as used in the segmentation, that is, not empty.
     * \param[in] objectNum object number.
     *
     */
    bool MarkObjectAsUsed(unsigned short objectNum);

    /** \brief Returns a vector containing unused objects, CompactObjects() must be called first to populate the vector.
     *
     */
    std::vector<unsigned int> GetUnusedObjectsLabels() const;

    friend class SaveSessionThread;
    friend class EspinaVolumeEditor;
  private:

    // BEWARE: bool used;
    // it's a workaround for previous espina versions, some objects are defined but have no voxels and because of
    // that the vector ObjectMetadata must be compacted after reading it
    struct ObjectMetadata
    {
        unsigned int scalar;
        unsigned int segment;
        unsigned int selected;
        bool used;
        ObjectMetadata()
        : scalar{0}, segment{0}, selected{0}, used{false}
        {};

    };

    struct CountingBrickMetadata
    {
        Vector3ui inclusive;
        Vector3ui exclusive;
        CountingBrickMetadata()
        : inclusive{Vector3ui{0, 0, 0}}, exclusive{Vector3ui{0, 0, 0}}
        {};
    };

    struct SegmentMetadata
    {
        std::string name;
        unsigned int value;
        QColor color;
        SegmentMetadata()
        : name{"Unassigned"}, value{0}, color{QColor()}
        {};
    };

    /** \brief Adds an object definition.
     * \param[in] label label number.
     * \param[in] segment segment number.
     * \param[in] selected 0 for false, other value otherwise.
     *
     */
    void AddObject(const unsigned int label, const unsigned int segment, const unsigned int selected);

    /** \brief Adds a counting brick definition.
     * \param[in] inclusive minimum bounding box point.
     * \param[in] exclusive maximum bounding box point.
     *
     */
    void AddBrick(const Vector3ui &inclusive, const Vector3ui &exclusive);

    /** \brief Adds a segment definition.
     * \param[in] name name of the segment.
     * \param[in] value label value.
     * \param[in] color color of the segment.
     *
     */
    void AddSegment(const std::string &name, const unsigned int value, const QColor &color);

    /** \brief Stores the position of the "Unassigned Tag", if any (depends on hasUnassignedTag boolean value).
     * \param[in] position position of the tag.
     *
     */
    void SetUnassignedTagPosition(const unsigned int position);

    bool hasUnassignedTag;     /** true if the segmha file readed had a segment with "Unassigned" name, false otherwise. */
    int unassignedTagPosition; /** position in the SegmentMetadata vector of the "Unassigned" tag, only valid if hasUnassignedTag==true */
    std::vector<struct ObjectMetadata>        ObjectVector;        /** vector containing Object metadata. */
    std::vector<struct CountingBrickMetadata> CountingBrickVector; /** vector containing Counting Brick metadata. */
    std::vector<struct SegmentMetadata>       SegmentVector;       /** vector containing Segment metadata. */
    std::vector<unsigned int>                 UnusedObjects;       /** vector containing unused objects. */
};

#endif // _METADATA_H_
