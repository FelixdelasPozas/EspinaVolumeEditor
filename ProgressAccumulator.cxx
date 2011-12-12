///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ProgressAccumulator.h
// Purpose: total progress reporter for a filter or multi-filter process 
// Notes: some parts "borrowed" (that is, blatantly stolen) from qtITK example by Luis Ibañez and 
//        AllPurposeProgressAccumulator from itksnap. thanks.
///////////////////////////////////////////////////////////////////////////////////////////////////

// itk includes
#include <itkProcessObject.h>

//vtk includes
#include <vtkAlgorithm.h>

// project includes
#include "ProgressAccumulator.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// ProgressAccumulator class
//
ProgressAccumulator::ProgressAccumulator(QApplication *app)
{
    _itkCommand = NULL;
    _vtkCommand = NULL;
    _progressBar = NULL;
    _progressLabel = NULL;
    _accumulatedProgress = 0;
    _qApp = app;
}

ProgressAccumulator::~ProgressAccumulator()
{
}

void ProgressAccumulator::Reset()
{
    _accumulatedProgress = 0;
    
    if (_progressBar != NULL)
        _progressBar->setValue(100);
    
    if (_progressLabel != NULL)
        _progressLabel->setText("Ready");
    
    _qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ProgressAccumulator::SetProgressBar(QProgressBar *pb, QLabel *pl)
{
    _progressBar = pb;
    _progressLabel = pl;
    
    _itkCommand = ITKCommandType::New();
    _itkCommand->SetCallbackFunction(this, &ProgressAccumulator::ITKProcessEvent);
    
    _vtkCommand = VTKCommandType::New();
    _vtkCommand->SetCallback(&ProgressAccumulator::VTKProcessEvent);
    _vtkCommand->SetClientData(this);

    _progressBar->setMinimum(0);
    _progressBar->setMaximum(100);
    
    if (!_progressBar->isEnabled())
    {
        _progressBar->setEnabled(true);
        _progressBar->setUpdatesEnabled(true);
    }
    
    if (!_progressLabel->isEnabled())
    {
        _progressLabel->setEnabled(true);
        _progressLabel->setUpdatesEnabled(true);
    }

    _progressLabel->show();
    _progressBar->show();
    _progressBar->reset();
    
    _qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ProgressAccumulator::VTKProcessEvent(vtkObject *caller, unsigned long eid, void *clientdata, void *calldata)
{
    // Figure out the pointers
    vtkAlgorithm *alg = dynamic_cast<vtkAlgorithm *> (caller);
    ProgressAccumulator *self = static_cast<ProgressAccumulator *> (clientdata);

    if (eid == vtkCommand::ProgressEvent)
        self->CallbackProgress(caller, alg->GetProgress());
    
    if (eid == vtkCommand::StartEvent)
        self->CallbackStart(caller);

    if (eid == vtkCommand::EndEvent)
        self->CallbackEnd(caller);
}

void ProgressAccumulator::ITKProcessEvent(itk::Object * caller, const itk::EventObject & event)
{
    // Figure out the pointers
    itk::ProcessObject::Pointer obj = dynamic_cast< itk::ProcessObject *>( caller );
    
    if (typeid(itk::ProgressEvent) == typeid(event))
        this->CallbackProgress(caller, obj->GetProgress());
    
    if (typeid(itk::StartEvent) == typeid(event))
        this->CallbackStart(caller);

    if (typeid(itk::EndEvent) == typeid(event))
        this->CallbackEnd(caller);
}

void ProgressAccumulator::CallbackProgress(void* caller, double progress)
{
    struct observerTags *tags = _observed[caller];
    
    const double value = tags->weight * progress * 100.0;
    _progressBar->setValue(static_cast<int>(value + _accumulatedProgress));
    _qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ProgressAccumulator::CallbackStart(void* caller)
{
    _progressBar->setValue(static_cast<int>(_accumulatedProgress));
    _qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ProgressAccumulator::CallbackEnd(void* caller)
{
    struct observerTags *tags = _observed[caller];
    
    _accumulatedProgress += tags->weight * 100.0;
    _progressBar->setValue(static_cast<int>(_accumulatedProgress));
    _qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ProgressAccumulator::Observe(itk::Object *caller, std::string text, double weight)
{
    // are we already observing this one?
    if (_observed.find(caller) != _observed.end())
        return;

    struct observerTags *tags = new struct observerTags;

    tags->type = ITK;
    tags->tagProgress = caller->AddObserver(itk::ProgressEvent(), _itkCommand.GetPointer());
    tags->tagStart = caller->AddObserver(itk::StartEvent(), _itkCommand.GetPointer());
    tags->tagEnd = caller->AddObserver(itk::EndEvent(), _itkCommand.GetPointer());
    tags->text = text;
    tags->weight = weight;
    
    _observed.insert(std::make_pair(caller, tags));
    
    _progressLabel->setText(QString(tags->text.c_str()));
    _qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
    _qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ProgressAccumulator::Observe(vtkObject *caller, std::string text, double weight)
{
    // are we already observing this one?
    if (_observed.find(caller) != _observed.end())
        return;
    
    struct observerTags *tags = new struct observerTags;
    
    tags->type = VTK;
    tags->tagProgress = caller->AddObserver(vtkCommand::ProgressEvent, _vtkCommand);
    tags->tagStart = caller->AddObserver(vtkCommand::StartEvent, _vtkCommand);
    tags->tagEnd = caller->AddObserver(vtkCommand::EndEvent, _vtkCommand);
    tags->text = text;
    tags->weight = weight;
    
    _observed.insert(std::make_pair(caller, tags));
    
    _progressLabel->setText(QString(tags->text.c_str()));
    _qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
    _qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ProgressAccumulator::Ignore(itk::Object *caller)
{
    struct observerTags *tags = _observed[caller];
    
    caller->RemoveObserver(tags->tagProgress);
    caller->RemoveObserver(tags->tagStart);
    caller->RemoveObserver(tags->tagEnd);
    
    _observed.erase(caller);
    _qApp->restoreOverrideCursor();
}

void ProgressAccumulator::Ignore(vtkObject *caller)
{
    struct observerTags *tags = _observed[caller];
    
    caller->RemoveObserver(tags->tagProgress);
    caller->RemoveObserver(tags->tagStart);
    caller->RemoveObserver(tags->tagEnd);
    
    _observed.erase(caller);
    _qApp->restoreOverrideCursor();
}

void ProgressAccumulator::IgnoreAll()
{
    std::map<void *, struct observerTags* >::iterator it;
    
    for (it = _observed.begin(); it != _observed.end(); it++)
    {
        itk::Object* obj = reinterpret_cast<itk::Object *>((*it).first);
        
        obj->RemoveObserver((*it).second->tagProgress);
        obj->RemoveObserver((*it).second->tagStart);
        obj->RemoveObserver((*it).second->tagEnd);
        
        _observed.erase(obj);
    }
    _qApp->restoreOverrideCursor();
}

// value defaults to 0 so it's optional, see .h
void ProgressAccumulator::ManualSet(std::string text, int value, bool fromThread)
{
    _progressLabel->setText(QString(text.c_str()));
    _progressBar->setValue(value);

    if (!fromThread)
    {
		_qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
		_qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

void ProgressAccumulator::ManualUpdate(int value, bool fromThread)
{
	_progressBar->setValue(value);
	_progressBar->update();

	if (!fromThread)
		_qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ProgressAccumulator::ManualReset(bool fromThread)
{
	_accumulatedProgress = 0;
	
	if (_progressLabel != NULL)
    _progressLabel->setText("Ready");
	
    if (_progressBar != NULL)
    	_progressBar->setValue(100);
    
    if (!fromThread)
    {
		_qApp->restoreOverrideCursor();
		_qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}
