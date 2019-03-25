#include "include/core/base.h"
#include "include/core/private/base_p.h"

namespace zyppng {

  BasePrivate::~BasePrivate()
  { }

  Base::Base() : d_ptr( new BasePrivate )
  {
    d_ptr->z_ptr = this;
  }

  Base::~Base()
  { }

  Base::Base ( BasePrivate &dd )
    : d_ptr ( &dd )
  {
    d_ptr->z_ptr = this;
  }

  Base::TrackPtr Base::parent() const
  {
    return d_func()->parent;
  }

  void Base::addChild( Base::Ptr child )
  {
    Z_D();
    if ( !child )
      return;

    //we are already the parent
    if ( child->d_func()->parent.operator->() == this )
      return;

    if ( child->d_func()->parent ) {
      child->d_func()->parent->removeChild( child );
    }

    d_func()->children.insert( child );

    auto tracker = Base::TrackPtr(this);
    child->d_func()->parent = tracker;
  }

  void Base::removeChild( const Base::Ptr child )
  {
    if ( !child )
      return;

    //we are not the child of this object
    if ( child->d_func()->parent.operator->() != this )
      return;

    Z_D();
    d->children.erase( child );

    auto tracker = Base::TrackPtr();
    child->d_func()->parent = tracker;
  }

  const std::unordered_set<Base::Ptr> &Base::children() const
  {
    return d_func()->children;
  }

} // namespace zyppng
