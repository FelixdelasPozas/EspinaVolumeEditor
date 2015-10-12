///////////////////////////////////////////////////////////////////////////////////////////////////
// Project: Espina Volume Editor
// Author: Félix de las Pozas Alvarez
//
// File: ProgressAccumulator.h
// Purpose: total progress reporter for a filter or multi-filter process 
// Notes: some parts "borrowed" (that is, blatantly stolen) from qtITK example by Luis Ibañez and 
//        AllPurposeProgressAccumulator from itksnap. thanks.
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _PROGRESS_ACCUMULATOR_H_
#define _PROGRESS_ACCUMULATOR_H_

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
    enum class callerType: char { VTK, ITK, None };

    struct observerTags
    {
        callerType type;
        unsigned long tagStart;
        unsigned long tagEnd;
        unsigned long tagProgress;
        std::string text;
        double weight;
        observerTags()
        : type{callerType::None}, tagStart{0}, tagEnd{0}, tagProgress{0}, text{}, weight{0} {}
    };

    // command class invoked
    using ITKCommandType = itk::MemberCommand<ProgressAccumulator>;
    using VTKCommandType = vtkCallbackCommand;

    /** \brief ProgressAccumulator class constructor.
     *
     */
    ProgressAccumulator();

    /** \brief Sets the progress bar that this class will use.
     * \param[in] bar progress bar used by accumulator.
     * \param[in] label label of the bar.
     *
     */
    void SetProgressBar(QProgressBar *bar, QLabel *label);

    /** \brief Observe a ITK process (add as a observer of that process).
     * \param[in] filter filter to observe.
     * \param[in] text text of the filter.
     * \param[in] weight weight of the filter in the process.
     *
     */
    void Observe(itk::Object *filter, const std::string &text, const double weight);

    /** \brief Observe a VTK process (add as a observer of that process).
     * \param[in] filter filter to observe.
     * \param[in] text text of the filter.
     * \param[in] weight weight of the filter in the process.
     *
     */
    void Observe(vtkObject *filter, const std::string &text, const double weight);

    /** \brief Ignore a ITK process (remove from the list of observers of that object.
     * \param[in] filter filter to ignore progress.
     *
     */
    void Ignore(itk::Object *filter);

    /** \brief Ignore a VTK process (remove from the list of observers of that object.
     * \param[in] filter filter to ignore progress.
     *
     */
    void Ignore(vtkObject *filter);

    // ignore all registered processes
    void IgnoreAll();

    /** \brief Set accumulated progress to 0.
     *
     *
     */
    void Reset();

    /** \brief Set the name and progress value manually.
     * \param[in] text text of the filter.
     * \param[in] value progress value.
     * \param[in] calledFromThread true if this is being called from a thread and false otherwise.
     *
     */
    void ManualSet(const std::string &text, const int value = 0, bool calledFromThread = false);

    /** \brief Manually update the progress value.
     * \param[in] value progress value.
     * \param[in] calledFromThread true if this is being called from a thread and false otherwise.
     *
     */
    void ManualUpdate(const int value, bool calledFromThread = false);

    /** \brief Manually reset progress value.
     * \param[in] calledFromThread true if this is being called from a thread and false otherwise.
     *
     */
    void ManualReset(bool calledFromThread = false);
  private:
    /** \brief Helper method to manage a ITK process event.
     * \param[in] caller pointer to caller object.
     * \param[in] event event object.
     *
     */
    void ITKProcessEvent(itk::Object *caller, const itk::EventObject &event);

    /** \brief Helper method to manage a VTK process event.
     * \param[in] caller pointer to caller object.
     * \param[in] eid event id.
     * \param[in] clientdata caller additional data.
     * \param[in] calldata event additional data.
     *
     */
    static void VTKProcessEvent(vtkObject *caller, unsigned long eid, void *clientdata, void *calldata);

    /** \brief Progress event callback.
     * \param[in] caller pointer to caller object.
     * \param[in] value progress value.
     *
     */
    void CallbackProgress(void *caller, const double value);

    /** \brief Progress start callback.
     * \param[in] caller pointer to caller object.
     *
     */
    void CallbackStart(void *caller);

    /** \brief Progress end callback.
     * \param[in] caller pointer to caller object.
     *
     */
    void CallbackEnd(void *caller);

    itk::SmartPointer<ITKCommandType> m_ITKCommand; /** itk command. */
    vtkSmartPointer<VTKCommandType>   m_VTKCommand; /** vtk command. */

    std::map<void *, observerTags*> m_observed; /** observed processes.   */
    double                          m_progress; /** accumulated progress. */

    QProgressBar *m_progressBar;   /** QProgress bar showing the progress. */
    QLabel       *m_progressLabel; /** QLabel associated to the bar.       */
};

#endif // _PROGRESS_ACCUMULATOR_H_
