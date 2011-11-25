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

///////////////////////////////////////////////////////////////////////////////////////////////////
// Metadata class
//
class Metadata
{
    public:
        // constructor & destructor
        Metadata(void);
        ~Metadata(void);

        // read metadata from a segmha file
        bool Read(QString);

        // write metadata to a segmha file
        bool Write(QString, std::map<unsigned short, unsigned short>*);

        // returns the segment name of the object
        std::string GetObjectSegmentName(unsigned short);

        // compact object vector deleting unused objects and storing them into UnusedObjects vector
        void CompactObjects(void);

        // mark object as used in the segmentation, that is, not empty
        bool MarkObjectAsUsed(unsigned short);

        // Get vector containing unused objects, CompactObjects() must be called first to populate the vector
        std::vector<unsigned int> GetUnusedObjectsLabels(void);
    private:
        // private structures definitions
        //
        // BEWARE: bool used;
        // it's a workaround for previous espina versions, some objects are defined but have no voxels and because of
        // that the vector ObjectMetadata must be compacted after reading it
        struct ObjectMetadata
        {
			unsigned int label;
			unsigned int segment;
			unsigned int selected;
			bool used;
			ObjectMetadata(): label(0), segment(0), selected(0), used(false) {};
        };

        struct CountingBrickMetadata
        {
        	Vector3ui inclusive;
        	Vector3ui exclusive;
        	CountingBrickMetadata(): inclusive(Vector3ui(0,0,0)), exclusive(Vector3ui(0,0,0)) {};
        };

        struct SegmentMetadata
        {
        	std::string name;
        	unsigned int value;
        	Vector3ui color;
        	SegmentMetadata(): name("Unassigned"), value(0), color(Vector3ui(0,0,255)) {};
        };

        // private members
        //
        // adds an object definition
        void AddObject(unsigned int, unsigned int, unsigned int);
        // adds a counting brick definition
        void AddBrick(Vector3ui, Vector3ui);
        // adds a segment definition
        void AddSegment(std::string, unsigned int, Vector3ui);
        // stores the position of the "Unassigned Tag", if any (depends on hasUnassignedTag boolean value)
        void SetUnassignedTagPosition(unsigned int);

        // class attributes
        //
        // true if the segmha file readed had a segment with "Unassigned" name, false otherwise
        bool hasUnassignedTag;
        //
        // position in the SegmentMetadata vector of the "Unassigned" tag, only valid if hasUnassignedTag==true
        int unassignedTagPosition;
        //
        // vector containing Object metadata
        std::vector<struct ObjectMetadata> ObjectVector;
        //
        // vector containing Counting Brick metadata
        std::vector<struct CountingBrickMetadata> CountingBrickVector;
        //
        // vector containing Segment metadata
        std::vector<struct SegmentMetadata> SegmentVector;
        //
        // vector containing unused objects
        std::vector<unsigned int> UnusedObjects;
};

#endif // _METADATA_H_
