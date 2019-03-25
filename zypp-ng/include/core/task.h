#ifndef ZYPP_NG_CORE_TASK_H_INCLUDED
#define ZYPP_NG_CORE_TASK_H_INCLUDED

#include <core/zyppglobal.h>
#include <core/base.h>
#include <core/signals.h>
#include <core/idstring.h>
#include <string>
#include <memory>

namespace zyppng {

  struct Error {
    std::string errorDesc;
  };


  class TaskPrivate;
  class Context;

  class LIBZYPP_NG_EXPORT Task : public Base
  {
    NON_COPYABLE(Task);
    ZYPP_DECLARE_PRIVATE(Task)

  public:

    using Ptr = std::shared_ptr<Task>;
    using WeakPtr = std::weak_ptr<Task>;
    using TrackPtr = weak_trackable_ptr<Task>;

    enum State {
      Pending,
      Running,
      Finished,
      Cancelled
    };

    Task ();
    virtual ~Task();

    //called by Context
    void start  (std::weak_ptr<Context> executionCtx );

    // implemented by each Task
    virtual IdString taskType () const = 0;
    virtual void cancel  () = 0;

    State state  () const;
    int   progress () const;
    std::string progressText () const;
    Error lastError  () const;

    std::vector<WeakPtr> subtasks() const;
    std::weak_ptr<Context> executionCtx () const;

    SignalProxy<void ( const Task &, State ) > sigStateChanged ();
    SignalProxy<void ( const Task &, std::string, int )> sigProgressChanged ();
    SignalProxy<void ( const Task &, const Error & )> sigError ();
    SignalProxy<void ( Task::WeakPtr, Task::WeakPtr )> sigSubTaskStarted ();
    SignalProxy<void ( Task::WeakPtr )> sigStarted  ();
    SignalProxy<void ( Task::WeakPtr )> sigFinished ();

  protected:
    Task ( TaskPrivate &d );

    //implements the actual logic
    virtual void run ( std::weak_ptr<Context> executionCtx ) = 0;

    void updateProgress ( const std::string &label, int progress );
    void finalize  ();

  };

};

#endif
