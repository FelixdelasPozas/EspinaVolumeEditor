///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ProgressAccumulator.h
// Purpose: total progress reporter for a filter or multi-filter process 
// Notes: some parts "borrowed" (that is, blatantly stolen) from qtITK example by Luis Ibañez and 
//        AllPurposeProgressAccumulator from itksnap. thanks.
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _TOTAL_PROGRESS_H_
#define _TOTAL_PROGRESS_H_

// itk includes
#include <itkObject.h>
#include <itkObjectFactory.h>
#include <itkCommand.h>
#include <itkProcessObject.h>

// vtk includes
#include <vtkSmartPointer.h>
#include <vtkCallbackCommand.h>
#include <vtkObject.h>

// qt includes
#include <QApplication>
#include <QProgressBar>
#include <QLabel>
#include <QObject>

// c++ includes
#include <map>
#include <string>

///////////////////////////////////////////////////////////////////////////////////////////////////
// ProgressAccumulator class
//
class ProgressAccumulator
{
    public:
        enum callerType { VTK, ITK, None };
        
        struct observerTags
        {
            callerType type;    
            unsigned long tagStart;
            unsigned long tagEnd;
            unsigned long tagProgress;
            std::string text;
            double weight;
            observerTags() : type(None), tagStart(0), tagEnd(0), tagProgress(0), text(), weight(0) {}
        };
        
        // command class invoked
        typedef itk::MemberCommand<ProgressAccumulator> ITKCommandType;
        typedef vtkCallbackCommand VTKCommandType;

        // constructor
        ProgressAccumulator(QApplication *app);
        ~ProgressAccumulator();

        // sets the progress bar that this class will use
        void SetProgressBar(QProgressBar*, QLabel*);
        
        // observe a ITK process (add as a observer of that process)
        void Observe(itk::Object*, std::string, double);
        
        // observe a VTK process (add as a observer of that process)
        void Observe(vtkObject*, std::string, double);
        
        // ignore a ITK process (remove from the list of observers of that object
        void Ignore(itk::Object*);
        
        // ignore a VTK process (remove from the list of observers of that object
        void Ignore(vtkObject*);
        
        // ignore all registered processes
        void IgnoreAll();
        
        // set accumulated progress to 0
        void Reset();
        
        // manual operations
        void ManualSet(std::string, int = 0);
        void ManualUpdate(int);
        void ManualReset();
    private:
        // manage a ITK process event
        void ITKProcessEvent(itk::Object*, const itk::EventObject &);
        
        // manage a VTK process event
        static void VTKProcessEvent(vtkObject*, unsigned long, void *, void *);
        
        // callbacks
        void CallbackProgress(void*, double);
        void CallbackStart(void*);
        void CallbackEnd(void*);

        // itk command
        itk::SmartPointer<ITKCommandType>   _itkCommand;
        
        // vtk command
        vtkSmartPointer<VTKCommandType>     _vtkCommand;
        
        // manage observed processes
        std::map<void *, observerTags* >    _observed;
        double                              _accumulatedProgress; 
        
        // Qt elements to show progress
        QProgressBar                       *_progressBar;
        QLabel                             *_progressLabel;
        
        // to force the processing of events
        QApplication                       *_qApp;
};

#endif // _TOTAL_PROGRESS_H_
