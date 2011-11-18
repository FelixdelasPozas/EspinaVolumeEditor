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
    private:
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

        // private structures definitions
        //
        struct ObjectMetadata
        {
			unsigned int label;
			unsigned int segment;
			unsigned int selected;
        };

        struct CountingBrickMetadata
        {
        	Vector3ui inclusive;
        	Vector3ui exclusive;
        };

        struct SegmentMetadata
        {
        	std::string name;
        	unsigned int value;
        	Vector3ui color;
        };

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
};

#endif // _METADATA_H_
