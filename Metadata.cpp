///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: Metadata.cpp
// Purpose: read, store and write Espina file metadata (segmha)
// Notes: It assumes that segment values are consecutive (in Metadata::Write).
////////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes
#include <QTextStream>
#include <QRegExp>

// project includes
#include "Metadata.h"
#include "DataManager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Metadata class
//
Metadata::Metadata(void)
: hasUnassignedTag     {false}
, unassignedTagPosition{0}
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
Metadata::~Metadata()
{
  ObjectVector.clear();
  CountingBrickVector.clear();
  SegmentVector.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool Metadata::read(const QString &filename)
{
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text) || !file.seek(0))
  {
    return false;
  }

  // parse espina additional data stored at the end of a regular mha file
  QRegExp objects("^Object\\s*:\\s*label\\s*=\\s*(\\d+)\\s*segment\\s*=\\s*(\\d+)\\s*selected\\s*=\\s*(\\d+)");
  QRegExp brick("^Counting Brick\\s*:\\s*inclusive\\s*=\\s*\\[(\\d+), (\\d+), (\\d+)\\]\\s*exclusive\\s*=\\s*\\[(\\d+), (\\d+), (\\d+)\\]");
  QRegExp labels("^Segment\\s*:\\s*name\\s*=\\s*\"(\\w+[\\w|\\s]*)\"\\s*value\\s*=\\s*(\\d+)\\s*color\\s*=\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)");

  // unassigned tag position
  unsigned int position = 1;

  while (!file.atEnd())
  {
    auto line = file.readLine();

    // assuming c++ evaluates expressions right to left, the regular expressions "brick" and "labels" are only evaluated at the right moment, not always
    // then the line will only be parsed once at best, in the "objects" parser
    if (objects.indexIn(line) >= 0)
    {
      auto result = true;

      auto label = objects.cap(1).toUInt(&result, 10);
      if (result == false) return false;
      auto segment = objects.cap(2).toUInt(&result, 10);
      if (result == false) return false;
      auto selected = objects.cap(3).toUInt(&result, 10);
      if (result == false) return false;

      addObject(label, segment, selected);
    }

    if ((brick.indexIn(line) >= 0) && (objects.indexIn(line) == -1))
    {
      auto result = true;

      auto x = brick.cap(1).toUInt(&result, 10);
      if (result == false) return false;
      auto y = brick.cap(2).toUInt(&result, 10);
      if (result == false) return false;
      auto z = brick.cap(3).toUInt(&result, 10);
      if (result == false) return false;

      auto inclusive = Vector3ui{x, y, z};

      x = brick.cap(4).toUInt(&result, 10);
      if (result == false) return false;
      y = brick.cap(5).toUInt(&result, 10);
      if (result == false) return false;
      z = brick.cap(6).toUInt(&result, 10);
      if (result == false) return false;

      auto exclusive = Vector3ui{x, y, z};

      addBrick(inclusive, exclusive);
    }

    if ((labels.indexIn(line) >= 0) && (brick.indexIn(line) == -1) && (objects.indexIn(line) == -1))
    {
      auto result = true;

      auto name = std::string(labels.cap(1).toStdString());

      if (0 == name.compare("Unassigned"))
      {
        setUnassignedTagPosition(position);
      }

      auto value = labels.cap(2).toUInt(&result, 10);
      if (result == false) return false;
      auto r = labels.cap(3).toUInt(&result, 10);
      if (result == false) return false;
      auto g = labels.cap(4).toUInt(&result, 10);
      if (result == false) return false;
      auto b = labels.cap(5).toUInt(&result, 10);
      if (result == false) return false;

      auto color = QColor(r, g, b);

      addSegment(name, value, color);
      position++;
    }
  }

  file.close();
  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool Metadata::write(const QString &filename, std::shared_ptr<DataManager> data) const
{
  QFile file(filename);
  if (!file.open(QIODevice::Append | QIODevice::Text) || !file.seek(file.size()))
  {
    return false;
  }

  QTextStream out(&file);

  out << "\n";

  for (auto it: ObjectVector)
  {
    auto label = data->GetLabelForScalar(it->scalar);

    if (0LL == data->GetNumberOfVoxelsForLabel(label)) continue;

    // write object metadata
    out << "Object: label=" << it->scalar;
    out << " segment="      << it->segment;
    out << " selected="     << it->selected;
    out << "\n";
  }

  if (ObjectVector.size() < (data->GetNumberOfLabels() - 1))
  {
    auto label = ObjectVector.size() + 1;
    auto position = ((true == hasUnassignedTag) ? unassignedTagPosition : SegmentVector.size() + 1);

    while (label != data->GetNumberOfLabels())
    {
      out << "Object: label=" << static_cast<int>(data->GetScalarForLabel(label));
      out << " segment=" << position;
      out << " selected=1\n";
      label++;
    }
  }

  out << "\n";

  for (auto it: CountingBrickVector)
  {
    out << "Counting Brick: inclusive=[" << it->inclusive[0] << ", " << it->inclusive[1] << ", " << it->inclusive[2];
    out << "] exclusive=[" << it->exclusive[0] << ", " << it->exclusive[1] << ", " << it->exclusive[2];
    out << "]\n";
  }

  out << "\n";

  for (auto it: SegmentVector)
  {
    out << "Segment: name=\"";
    out << it->name.c_str();
    out << "\" value=" << it->value;
    out << " color= " << it->color.red() << ", " << it->color.green() << ", " << it->color.blue() << "\n";
  }

  // BEWARE: assumes that segment values are consecutive, and only adds this segment definition if there are new labels.
  if ((!hasUnassignedTag) && (ObjectVector.size() < (data->GetNumberOfLabels() - 1)))
  {
    out << "Segment: name=\"Unassigned\" value=" << SegmentVector.size() + 1 << " color= 0, 0, 255" << "\n";
  }

  file.close();
  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Metadata::addObject(const unsigned int label, const unsigned int segment, const unsigned int selected)
{
  auto object = std::make_shared<ObjectMetadata>();
  object->scalar = label;
  object->segment = segment;
  object->selected = selected;

  ObjectVector << object;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Metadata::addBrick(const Vector3ui &inclusive, const Vector3ui &exclusive)
{
  auto brick = std::make_shared<CountingBrickMetadata>();
  brick->inclusive = inclusive;
  brick->exclusive = exclusive;

  CountingBrickVector << brick;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Metadata::addSegment(const std::string &name, const unsigned int value, const QColor &color)
{
  auto segment = std::make_shared<SegmentMetadata>();
  segment->name = name;
  segment->value = value;
  segment->color = color;

  SegmentVector << segment;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Metadata::setUnassignedTagPosition(const unsigned int position)
{
  hasUnassignedTag = true;
  unassignedTagPosition = position;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
std::string Metadata::objectSegmentName(const unsigned short objectNum) const
{
  if (objectNum > ObjectVector.size()) return std::string("Unassigned");

  // vector index goes 0->(n-1)
  return std::string(SegmentVector[ObjectVector[objectNum - 1]->segment - 1]->name);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool Metadata::markAsUsed(const unsigned short label)
{
  for (auto it: ObjectVector)
  {
    if (it->scalar == label)
    {
      it->used = true;
      return true;
    }
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void Metadata::compact()
{
  QList<std::shared_ptr<ObjectMetadata>> toDelete;

  for (auto object: ObjectVector)
  {
    if (object->used == false)
    {
      UnusedObjects << object->scalar;
      toDelete << object;
    }
  }

  if(!toDelete.isEmpty())
  {
    for(auto object: toDelete)
    {
      ObjectVector.remove(object);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
QList<unsigned int> Metadata::unusedLabels()
{
  if(UnusedObjects.empty())
  {
    compact();
  }

  return UnusedObjects;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
unsigned short Metadata::objectScalar(const unsigned short label) const
{
  // vector index goes 0->(n-1)
  return ObjectVector[label - 1]->scalar;
}
