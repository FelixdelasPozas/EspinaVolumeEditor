///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: SaveSession.cxx
// Purpose: Thread for saving a editor session
// Notes:
///////////////////////////////////////////////////////////////////////////////////////////////////

// qt includes
#include <EspinaVolumeEditor.h>
#include <QString>
#include <QDir>
#include <QSettings>
#include <QMessageBox>

// itk includes
#include <itkSmartPointer.h>
#include <itkImageFileWriter.h>
#include <itkMetaImageIO.h>

// project includes
#include "SaveSession.h"
#include "DataManager.h"
#include "Metadata.h"
#include <fstream>

using WriterType = itk::ImageFileWriter<ImageType>;

///////////////////////////////////////////////////////////////////////////////////////////////////
// SaveSessionThread class
//
SaveSessionThread::SaveSessionThread(EspinaVolumeEditor* editor)
: QThread{}
{
	moveToThread(this);
	m_editor = editor;

	connect(this, SIGNAL(finished()), editor, SLOT(SaveSessionEnd()), Qt::QueuedConnection);
	connect(this, SIGNAL(startedSaving()), editor, SLOT(SaveSessionStart()), Qt::QueuedConnection);
	connect(this, SIGNAL(progress(int)), editor, SLOT(SaveSessionProgress(int)), Qt::QueuedConnection);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void SaveSessionThread::run()
{
  std::string homedir = std::string(getenv("HOME"));
  std::string username = std::string(getenv("USER"));
  std::string baseFilename = homedir + std::string("/.espinaeditor-") + username;
  std::string temporalFilename = baseFilename + std::string(".session");
  std::string temporalFilenameMHA = baseFilename + std::string(".mha");

  // needed to save the program state in the right moment so we wait for the lock to be open. Grab the lock and notify of action.
  QMutexLocker locker(&(m_editor->m_mutex));
  emit startedSaving();

  QFile file(QString(temporalFilename.c_str()));
  if(file.exists() && !file.remove())
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Error saving session");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
    msgBox.setDetailedText("Previous session file exists but couldn't be removed.");
    msgBox.exec();
    return;
  }

  QFile fileMHA(QString(temporalFilenameMHA.c_str()));
  if(fileMHA.exists() && !fileMHA.remove())
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Error saving session");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
    msgBox.setDetailedText("Previous MHA session file exists but couldn't be removed.");
    msgBox.exec();
    return;
  }

  auto image = ImageType::New();
  image = m_editor->m_editorOperations->m_selection->itkImage();

  // save as an mha
  auto io = itk::MetaImageIO::New();
  io->SetFileName(temporalFilenameMHA.c_str());

  auto writer = WriterType::New();
  writer->SetImageIO(io);
  writer->SetFileName(temporalFilenameMHA.c_str());
  writer->SetInput(image);
  writer->UseCompressionOn();

  try
  {
    writer->Write();
  }
  catch (itk::ExceptionObject &excp)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Error saving session");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("An error occurred saving the editor MHA session file.\nThe operation has been aborted.");
    msgBox.setDetailedText(excp.what());
    msgBox.exec();
    return;
  }

  emit progress(50);

  std::ofstream outfile;
  outfile.open(temporalFilename.c_str(), std::ofstream::out|std::ofstream::trunc|std::ofstream::binary);

  if (!outfile.is_open())
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Error saving session");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText("An error occurred saving the editor session file.\nThe operation has been aborted.");
    msgBox.setDetailedText("Couldn't open session file for writing.");
    msgBox.exec();
    return;
  }

  // dump all relevant data and objects to file, first the size of the std::map or std::vector, the objects
  // themselves and also all the relevant data.
  // the order:
  //    - EspinaEditor relevant data: POI and file names...
  //    - metadata ObjectMetadata
  //    - metadata CountingBrickMetadata
  //    - metadata SegmentMetadata
  //    - metadata relevant data: hasUnassignedTag, unassignedTadPosition
  //    - datamanager ObjectInformation
  // in the editor we must follow the same order while loading this data, obviously...

  // EspinaEditor relevant data
  auto size = m_editor->m_segmentationFileName.size();
  write(outfile, size);
  outfile << m_editor->m_segmentationFileName.toStdString();

  write(outfile, m_editor->m_hasReferenceImage);
  if (m_editor->m_hasReferenceImage)
  {
    size = m_editor->m_referenceFileName.size();
    write(outfile, size);
    outfile << m_editor->m_referenceFileName.toStdString();
  }

  write(outfile, m_editor->m_POI[0]);
  write(outfile, m_editor->m_POI[1]);
  write(outfile, m_editor->m_POI[2]);

  // Metadata::ObjectMetadata std::vector dump
  size = m_editor->m_fileMetadata->ObjectVector.size();
  write(outfile, size);

  for (auto it: m_editor->m_fileMetadata->ObjectVector)
  {
    write(outfile, it->scalar);
    write(outfile, it->segment);
    write(outfile, it->selected);
    // we could write the whole structure just by doing write(outfile, object) but i don't want to write down the
    // "used" bool field, i already know it's true
  }

  // Metadata::CountingBrickMetadata std::vector dump
  size = m_editor->m_fileMetadata->CountingBrickVector.size();
  write(outfile, size);

  for (auto it: m_editor->m_fileMetadata->CountingBrickVector)
  {
    write(outfile, it->inclusive[0]);
    write(outfile, it->inclusive[1]);
    write(outfile, it->inclusive[2]);
    write(outfile, it->exclusive[0]);
    write(outfile, it->exclusive[1]);
    write(outfile, it->exclusive[2]);
  }

  // Metadata::SegmentMetadata std::vector dump
  size = m_editor->m_fileMetadata->SegmentVector.size();
  write(outfile, size);

  for (auto it: m_editor->m_fileMetadata->SegmentVector)
  {
    write(outfile, it->color.red());
    write(outfile, it->color.green());
    write(outfile, it->color.blue());
    write(outfile, it->value);
    size = it->name.size();
    write(outfile, size);
    outfile << it->name;
  }

  // Metadata relevant data
  write(outfile, m_editor->m_fileMetadata->hasUnassignedTag);
  write(outfile, m_editor->m_fileMetadata->unassignedTagPosition);

  // DataManager::ObjectInformation std::map dump
  size = m_editor->m_dataManager->ObjectVector.size();
  write(outfile, size);

  for (auto it: m_editor->m_dataManager->ObjectVector)
  {
    unsigned short int position = it.first;
    write(outfile, position);
    write(outfile, it.second->scalar);
    write(outfile, it.second->size);
    write(outfile, it.second->centroid[0]);
    write(outfile, it.second->centroid[1]);
    write(outfile, it.second->centroid[2]);
    write(outfile, it.second->min[0]);
    write(outfile, it.second->min[1]);
    write(outfile, it.second->min[2]);
    write(outfile, it.second->max[0]);
    write(outfile, it.second->max[1]);
    write(outfile, it.second->max[2]);
  }

  outfile.close();
  emit progress(100);

  // store labels
  QSettings editorSettings("UPM", "Espina Volume Editor");
  QString filename(temporalFilename.c_str());
  filename.replace(QChar('/'), QChar('\\'));
  editorSettings.beginGroup("Editor");

  std::set<unsigned short> labelIndexes = m_editor->m_dataManager->GetSelectedLabelsSet();
  std::set<unsigned short> labelScalars;

  for (auto it: labelIndexes)
  {
    labelScalars.insert(m_editor->m_dataManager->GetScalarForLabel(it));
  }

  QList<QVariant> labelList;
  for (auto it: labelScalars)
  {
    labelList.append(static_cast<int>(it));
  }

  QVariant variant;
  variant.setValue(labelList);
  editorSettings.setValue(filename, variant);
}
