///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ProgressAccumulator.cpp
// Purpose: total progress reporter for a filter or multi-filter process 
// Notes: some parts "borrowed" (that is, blatantly stolen) from qtITK example by Luis Ibañez and 
//        AllPurposeProgressAccumulator from itksnap. thanks.
///////////////////////////////////////////////////////////////////////////////////////////////////

// itk includes
#include <itkProcessObject.h>

//vtk includes
#include <vtkAlgorithm.h>

// qt includes
#include <QApplication>

// project includes
#include "ProgressAccumulator.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// ProgressAccumulator class
//
ProgressAccumulator::ProgressAccumulator()
: m_ITKCommand   {nullptr}
, m_VTKCommand   {nullptr}
, m_progress     {0}
, m_progressBar  {nullptr}
, m_progressLabel{nullptr}
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::Reset()
{
  m_progress = 0;

  if (m_progressBar)
  {
    m_progressBar->setValue(100);
  }

  if (m_progressLabel)
  {
    m_progressLabel->setText("Ready");
  }

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::SetProgressBar(QProgressBar *bar, QLabel *label)
{
  m_progressBar = bar;
  m_progressLabel = label;

  m_ITKCommand = ITKCommandType::New();
  m_ITKCommand->SetCallbackFunction(this, &ProgressAccumulator::ITKProcessEvent);

  m_VTKCommand = VTKCommandType::New();
  m_VTKCommand->SetCallback(&ProgressAccumulator::VTKProcessEvent);
  m_VTKCommand->SetClientData(this);

  m_progressBar->setMinimum(0);
  m_progressBar->setMaximum(100);

  if (!m_progressBar->isEnabled())
  {
    m_progressBar->setEnabled(true);
    m_progressBar->setUpdatesEnabled(true);
  }

  if (!m_progressLabel->isEnabled())
  {
    m_progressLabel->setEnabled(true);
    m_progressLabel->setUpdatesEnabled(true);
  }

  m_progressLabel->show();
  m_progressBar->show();
  m_progressBar->reset();

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::VTKProcessEvent(vtkObject *caller, unsigned long eid, void *clientdata, void *calldata)
{
  // Figure out the pointers
  auto alg = dynamic_cast<vtkAlgorithm *>(caller);
  auto self = dynamic_cast<ProgressAccumulator *>(clientdata);
  Q_ASSERT(alg & self);

  switch(eid)
  {
    case vtkCommand::ProgressEvent:
      self->CallbackProgress(caller, alg->GetProgress());
      break;
    case vtkCommand::StartEvent:
      self->CallbackStart(caller);
      break;
    case vtkCommand::EndEvent:
      self->CallbackEnd(caller)
      break;
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::ITKProcessEvent(itk::Object * caller, const itk::EventObject & event)
{
  // Figure out the pointers
  auto obj = dynamic_cast<itk::ProcessObject *>(caller);
  Q_ASSERT(obj);

  if (typeid(itk::ProgressEvent) == typeid(event))
  {
    this->CallbackProgress(caller, obj->GetProgress());
  }

  if (typeid(itk::StartEvent) == typeid(event))
  {
    this->CallbackStart(caller);
  }

  if (typeid(itk::EndEvent) == typeid(event))
  {
    this->CallbackEnd(caller);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::CallbackProgress(void* caller, double progress)
{
  auto tags = m_observed[caller];

  auto value = tags->weight * progress * 100.0;
  m_progressBar->setValue(static_cast<int>(value + m_progress));
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::CallbackStart(void* caller)
{
  m_progressBar->setValue(static_cast<int>(m_progress));
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::CallbackEnd(void* caller)
{
  auto tags = m_observed[caller];

  m_progress += tags->weight * 100.0;
  m_progressBar->setValue(static_cast<int>(m_progress));
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::Observe(itk::Object *caller, const std::string &text, const double weight)
{
  // are we already observing this one?
  if (m_observed.find(caller) != m_observed.end()) return;

  auto tags = new struct observerTags;
  tags->type = ITK;
  tags->tagProgress = caller->AddObserver(itk::ProgressEvent(), m_ITKCommand.GetPointer());
  tags->tagStart = caller->AddObserver(itk::StartEvent(), m_ITKCommand.GetPointer());
  tags->tagEnd = caller->AddObserver(itk::EndEvent(), m_ITKCommand.GetPointer());
  tags->text = text;
  tags->weight = weight;

  m_observed.insert(std::make_pair(caller, tags));

  m_progressLabel->setText(QString(tags->text.c_str()));
  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::Observe(vtkObject *caller, const std::string &text, const double weight)
{
  // are we already observing this one?
  if (m_observed.find(caller) != m_observed.end()) return;

  auto tags = new struct observerTags;
  tags->type = callerType::VTK;
  tags->tagProgress = caller->AddObserver(vtkCommand::ProgressEvent, m_VTKCommand);
  tags->tagStart = caller->AddObserver(vtkCommand::StartEvent, m_VTKCommand);
  tags->tagEnd = caller->AddObserver(vtkCommand::EndEvent, m_VTKCommand);
  tags->text = text;
  tags->weight = weight;

  m_observed.insert(std::make_pair(caller, tags));

  m_progressLabel->setText(QString(tags->text.c_str()));
  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::Ignore(itk::Object *caller)
{
  auto tags = m_observed[caller];

  caller->RemoveObserver(tags->tagProgress);
  caller->RemoveObserver(tags->tagStart);
  caller->RemoveObserver(tags->tagEnd);

  m_observed.erase(caller);
  QApplication::restoreOverrideCursor();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::Ignore(vtkObject *caller)
{
  auto tags = m_observed[caller];

  caller->RemoveObserver(tags->tagProgress);
  caller->RemoveObserver(tags->tagStart);
  caller->RemoveObserver(tags->tagEnd);

  m_observed.erase(caller);
  QApplication::restoreOverrideCursor();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::IgnoreAll()
{
  std::map<void *, struct observerTags*>::iterator it;

  for (it = m_observed.begin(); it != m_observed.end(); it++)
  {
    if((*it).second->type == callerType::ITK)
    {
      auto obj = reinterpret_cast<itk::Object *>((*it).first);

      obj->RemoveObserver((*it).second->tagProgress);
      obj->RemoveObserver((*it).second->tagStart);
      obj->RemoveObserver((*it).second->tagEnd);
    }
    else
    {
      auto obj = reinterpret_cast<vtkObject *>((*it).first);

      obj->RemoveObserver((*it).second->tagProgress);
      obj->RemoveObserver((*it).second->tagStart);
      obj->RemoveObserver((*it).second->tagEnd);
    }

    m_observed.erase(it);
  }
  QApplication::restoreOverrideCursor();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::ManualSet(const std::string &text, const int value, bool calledFromThread)
{
  m_progressLabel->setText(QString(text.c_str()));
  m_progressBar->setValue(value);

  if (!calledFromThread)
  {
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::ManualUpdate(const int value, bool calledFromThread)
{
  m_progressBar->setValue(value);
  m_progressBar->update();

  if (!calledFromThread) QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProgressAccumulator::ManualReset(bool calledFromThread)
{
  m_progress = 0;

  if (m_progressLabel)
  {
    m_progressLabel->setText("Ready");
  }

  if (m_progressBar)
  {
    m_progressBar->setValue(100);
  }

  if (!calledFromThread)
  {
    QApplication::restoreOverrideCursor();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  }
}
