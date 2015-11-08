#ifndef SKYTYPE_REGISTER_H
#define SKYTYPE_REGISTER_H

#include <errno.h>
#include "libzlog.h"
#include "zstate/skytype.h"

class Register : private skytype::SkyObject {
 public:
  Register(zlog::Log::Stream *log) :
    SkyObject(log), state_(0)
  {}

  int Read(int *value) {
    int ret = QueryHelper();
    if (ret)
      return ret;
    *value = state_;
    return 0;
  }

  int Write(int value) {
    ceph::bufferlist bl;
    bl.append((char*)&value, sizeof(value));
    return UpdateHelper(bl);
  }

 private:
  int state_;

  void Apply(ceph::bufferlist& bl, uint64_t position) {
    int *data = (int*)bl.c_str();
    state_ = *data;
  }
};

#endif
