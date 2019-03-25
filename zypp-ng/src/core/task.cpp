#include "core/private/task_p.h"
#include "core/context.h"
#include <iostream>

namespace zyppng {

  Task::Task( TaskPrivate &d ) : Base( d )
  {
  }

  void Task::updateProgress(const std::string &label, int progress)
  {
    Z_D();

    bool emit = false;
    if ( !label.empty() ) {
      emit = true;
      d->_progressText = label;
    }

    if ( progress != d->_currentProgress ) {
      emit = true;
      d->_currentProgress = progress;
    }

    if ( emit ) {
      d->_progressChanged.emit( *this, d->_progressText, d->_currentProgress );
    }
  }

  Task::Task() : Base ( *new TaskPrivate )
  {
  }

  Task::~Task()
  {
    std::cout<< "Destroyed Task " << d_func()->_progressText << std::endl;
  }

  std::vector<Task::WeakPtr> Task::subtasks() const
  {
    return findChildren<Task>();
  }

  std::weak_ptr<Context> Task::executionCtx() const
  {
    return d_func()->_myContext;
  }

  /**
	 * Starts the task in the \a executionCtx, making the task
	 * a child of it.
	 */
  void Task::start( std::weak_ptr<Context> executionCtx )
  {
    Z_D();

    if ( !executionCtx.lock() )
      return;

    d_func()->_currentProgress = 0;
    d_func()->_progressText.clear();

    d->_myContext = executionCtx;
    d->_started.emit( shared_this<Task>() );

    run( executionCtx );
  }

  int Task::progress() const
  {
    return d_func()->_currentProgress;
  }

  std::string Task::progressText() const
  {
    return d_func()->_progressText;
  }

  /**
   * Finalizing the task, resetting the context and emitting the finished signal
   */
  void Task::finalize()
  {
    Z_D();

    auto lock = shared_this<Task>();
    d->_myContext.reset();
    d->_finished.emit( lock );
  }

  SignalProxy<void (const zyppng::Task &, zyppng::Task::State)> Task::sigStateChanged()
  {
    return SignalProxy<void (const zyppng::Task &, zyppng::Task::State)>( d_func()->_stateChanged );
  }

  SignalProxy<void (const Task &, std::string, int)> Task::sigProgressChanged()
  {
    return SignalProxy<void (const zyppng::Task &, std::string, int)>( d_func()->_progressChanged );
  }

  SignalProxy<void (const Task &, const Error &)> Task::sigError()
  {
    return SignalProxy<void (const Task &, const Error &)> ( d_func()->_error );
  }

  SignalProxy<void (zyppng::Task::WeakPtr, zyppng::Task::WeakPtr)> Task::sigSubTaskStarted()
  {
    return SignalProxy<void ( Task::WeakPtr, Task::WeakPtr )>( d_func()->_subTaskStarted );
  }

  SignalProxy<void ( Task::WeakPtr )> Task::sigStarted()
  {
    return SignalProxy<void ( Task::WeakPtr )>( d_func()->_started );
  }

  SignalProxy<void ( Task::WeakPtr )> Task::sigFinished()
  {
    return SignalProxy<void ( Task::WeakPtr )>( d_func()->_finished );
  }
}
