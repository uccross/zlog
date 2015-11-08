#ifndef SKYTYPE_H
#define SKYTYPE_H
#include <rados/buffer.h>
#include "../libzlog.hpp"

namespace skytype {

class SkyObject {
 protected:
  /*
   *
   */
  explicit SkyObject(zlog::Log::Stream *stream);

  /*
   *
   */
  virtual void Apply(ceph::bufferlist& bl, uint64_t position) = 0;

  /*
   *
   */
  int UpdateHelper(ceph::bufferlist& bl);

  /*
   *
   */
  int QueryHelper();

 private:
  zlog::Log::Stream *stream_;
  zlog::Log *log_;
};

}

#endif
