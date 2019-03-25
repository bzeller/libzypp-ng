#include <glibmm/init.h>
#include <glibmm/main.h>
#include <core/context.h>
#include <repo/refreshrepo.h>
#include <iostream>

#include <progressbar/progressbar.h>
#include <progressbar/statusbar.h>

struct ProgressBar : public zyppng::Base
{
  ProgressBar( const char *label_r, int line_r = 0, int max_r = 100 )
    : widget( progressbar_new( "", max_r ), &progressbar_free ),
    max ( max_r ),
    line( line_r )
  {
    updateProcess( label_r, 0 );
    std::cout << std::endl << std::flush;
  }

  virtual ~ProgressBar();

  void updateProcess ( const std::string &text, int prog )
  {
    std::cout << "\033[s" << std::flush;
    std::cout << "\033[H" << std::flush;
    if ( line > 0 )
      std::cout << "\033["<<line<<"B" << std::flush;

    if ( text != label ) {
      label = text;
      progressbar_update_label( widget.get(), label.c_str() );
    }

    progressbar_update( widget.get(), prog );

    std::cout << "\033[u" << std::flush;
  }

private:
  std::string label;
  std::unique_ptr<progressbar, void(*)(progressbar*)> widget;
  int max = 0;
  int line = 0;
};

ProgressBar::~ProgressBar()
{
  std::cout<<"Progressbar deleted: "<<label<<std::endl;
}

int main ( int , char *[] )
{
  Glib::init();

  Glib::RefPtr<Glib::MainLoop> mainLoop = Glib::MainLoop::create();

  auto stopMain = [ &mainLoop ]( std::weak_ptr<zyppng::Task> ) {
    mainLoop->quit();
  };

  int maxLine = 0;

  auto reportProgress = [] ( const zyppng::Task &task, const std::string &text, int prog ) {
    std::vector< std::weak_ptr<ProgressBar> > bars = task.findChildren<ProgressBar>();
    if ( bars.empty() ) {
      std::cerr << "No progressbar assigned";
      return;
    }
    std::shared_ptr<ProgressBar> bar = (*bars.begin()).lock();
    bar->updateProcess( text, prog );
  };

  auto assignProgressbar = [ &reportProgress, &maxLine ] ( zyppng::Task::WeakPtr task ) {
    auto newTask = task.lock();
    if ( !newTask )
      return;

    std::shared_ptr<ProgressBar> bar( new ProgressBar( "Progress starting", maxLine ) );
    maxLine++;

    //bind the progressbars lifetime to the task object
    newTask->addChild( bar );
    newTask->sigProgressChanged().connect( reportProgress );
  };

  auto handleSubtaskCreated = [ &assignProgressbar ] ( zyppng::Task::WeakPtr, zyppng::Task::WeakPtr sub ) {
    assignProgressbar( sub );
  };


  zyppng::Context::Ptr context ( new zyppng::Context );
  zyppng::RefreshRepositories::Ptr refreshTask ( new zyppng::RefreshRepositories( { "repoName1", "repoName2", "repoName3", "repoName4", "repoName5" } ) );

  refreshTask->sigSubTaskStarted().connect( handleSubtaskCreated );
  assignProgressbar( refreshTask );

  context->runTask( refreshTask );

  refreshTask->sigFinished().connect( stopMain );

  mainLoop->run();
  return 0;
}
