///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Metadata.cxx
// Purpose: read, store and write Espina file metadata (segmha)
// Notes: It assumes that segment values are consecutive (in Metadata::Write).
////////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes
#include <QTextStream>
#include <QRegExp>

// project includes
#include "Metadata.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Metadata class
//
Metadata::Metadata(void)
{
	hasUnassignedTag = false;
	unassignedTagPosition = 0;
}

Metadata::~Metadata(void)
{
	// all data is released using their own destructors
}

bool Metadata::Read(QString filename)
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;

	if (false == file.seek(0))
		return false;

	// parse espina additional data stored at the end of a regular mha file
	QRegExp objects("^Object\\s*:\\s*label\\s*=\\s*(\\d+)\\s*segment\\s*=\\s*(\\d+)\\s*selected\\s*=\\s*(\\d+)");
	QRegExp brick("^Counting Brick\\s*:\\s*inclusive\\s*=\\s*\\[(\\d+), (\\d+), (\\d+)\\]\\s*exclusive\\s*=\\s*\\[(\\d+), (\\d+), (\\d+)\\]");
	QRegExp labels("^Segment\\s*:\\s*name\\s*=\\s*\"(\\w+[\\w|\\s]*)\"\\s*value\\s*=\\s*(\\d+)\\s*color\\s*=\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)");

	// unassigned tag position
	unsigned int position = 0;

	while (!file.atEnd())
	{
		QString line = file.readLine();

		// assuming c++ evaluates expressions right to left, the regular expressions "brick" and "labels" are only evaluated at the right moment, not always
		// then the line will only be parsed once at best, in the "objects" parser
		if (objects.indexIn(line) >= 0)
		{
			bool result = true;

			unsigned int label = objects.cap(1).toUInt(&result, 10);
			if (result == false)
				return false;
			unsigned int segment = objects.cap(2).toUInt(&result, 10);
			if (result == false)
				return false;
			unsigned int selected = objects.cap(3).toUInt(&result, 10);
			if (result == false)
				return false;

			AddObject(label,segment,selected);
		}

		if ((brick.indexIn(line) >= 0) && (objects.indexIn(line) == -1))
		{
			bool result = true;

			unsigned int x = brick.cap(1).toUInt(&result,10);
			if (result == false)
				return false;
			unsigned int y = brick.cap(2).toUInt(&result,10);
			if (result == false)
				return false;
			unsigned int z = brick.cap(3).toUInt(&result,10);
			if (result == false)
				return false;

			Vector3ui inclusive = Vector3ui(x,y,z);

			x = brick.cap(4).toUInt(&result,10);
			if (result == false)
				return false;
			y = brick.cap(5).toUInt(&result,10);
			if (result == false)
				return false;
			z = brick.cap(6).toUInt(&result,10);
			if (result == false)
				return false;

			Vector3ui exclusive = Vector3ui(x,y,z);

			AddBrick(inclusive, exclusive);
		}

		if ((labels.indexIn(line) >= 0) && (brick.indexIn(line) == -1) && (objects.indexIn(line) == -1))
		{
			bool result = true;

			std::string name = std::string(labels.cap(1).toStdString());

			if (0 == name.compare("Unassigned"))
				this->SetUnassignedTagPosition(position);

			unsigned int value = labels.cap(2).toUInt(&result, 10);
			if (result == false)
				return false;
			unsigned int r = labels.cap(3).toUInt(&result,10);
			if (result == false)
				return false;
			unsigned int g = labels.cap(4).toUInt(&result,10);
			if (result == false)
				return false;
			unsigned int b = labels.cap(5).toUInt(&result,10);
			if (result == false)
				return false;

			Vector3ui color = Vector3ui(r,g,b);

			AddSegment(name, value, color);
			position++;
		}
	}

	file.close();
	return true;
}

bool Metadata::Write(QString filename, std::map<unsigned short, unsigned short> *labelValues)
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadWrite | QIODevice::Text))
		return false;

	if (false == file.seek(file.size()))
		return false;

	QTextStream out(&file);

	std::vector<struct ObjectMetadata>::iterator objectit;
	for (objectit = this->ObjectVector.begin(); objectit != this->ObjectVector.end(); objectit++)
	{
		out << "Object: label=" << (*objectit).label;
		out << " segment=" << (*objectit).segment;
		out << " selected=" << (*objectit).selected;
		out << "\n";
	}

	if (ObjectVector.size() < (*labelValues).size())
	{
		unsigned int label = ObjectVector.size()+1;
		int position = (true == hasUnassignedTag) ? unassignedTagPosition : SegmentVector.size()+1;

		while (label != (*labelValues).size())
		{
			out << "Object: label=" << static_cast<int>((*labelValues)[label]);
			out << " segment=" << position;
			out << " selected=1\n";
			label++;
		}
	}

	std::vector<struct CountingBrickMetadata>::iterator brickit;
	for (brickit = this->CountingBrickVector.begin(); brickit != this->CountingBrickVector.end(); brickit++)
	{
		out << "Counting Brick: inclusive=[" << (*brickit).inclusive[0] << ", " << (*brickit).inclusive[1] << ", " << (*brickit).inclusive[2];
		out << "] exlusive=[" << (*brickit).exclusive[0] << ", " << (*brickit).exclusive[1] << ", " << (*brickit).exclusive[2];
		out << "]\n";
	}

	std::vector<struct SegmentMetadata>::iterator segmentit;
	for (segmentit = this->SegmentVector.begin(); segmentit != this->SegmentVector.end(); segmentit++)
	{
		out << "Segment: name=\"";
		out << (*segmentit).name.c_str();
		out << "\" value=" << (*segmentit).value;
		out << " color= " << (*segmentit).color[0] << ", " << (*segmentit).color[1] << ", " << (*segmentit).color[2] << "\n";
	}

	// BEWARE: assumes that segment values are consecutive
	if (false == hasUnassignedTag)
		out << "Segment: name=\"Unassigned\" value=" << this->SegmentVector.size()+1 << " color= 0, 0, 255" << "\n";

	file.close();
	return true;
}

void Metadata::AddObject(unsigned int label, unsigned int segment, unsigned int selected)
{
	struct ObjectMetadata* object = new struct ObjectMetadata;
	object->label = label;
	object->segment = segment;
	object->selected = selected;

	this->ObjectVector.push_back(*object);
}

void Metadata::AddBrick(Vector3ui inclusive, Vector3ui exclusive)
{
	struct CountingBrickMetadata* brick = new struct CountingBrickMetadata;
	brick->inclusive = inclusive;
	brick->exclusive = exclusive;

	this->CountingBrickVector.push_back(*brick);
}

void Metadata::AddSegment(std::string name, unsigned int value, Vector3ui color)
{
	struct SegmentMetadata* segment = new struct SegmentMetadata;
	segment->name = name;
	segment->value = value;
	segment->color = color;

	this->SegmentVector.push_back(*segment);
}

void Metadata::SetUnassignedTagPosition(unsigned int position)
{
	hasUnassignedTag = true;
	unassignedTagPosition = position;
}
