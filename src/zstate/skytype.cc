#include <rados/buffer.h>
#include "../libzlog.hpp"
#include "skytype.h"

namespace skytype {

SkyObject::SkyObject(zlog::Log::Stream *stream) :
  stream_(stream)
{
  int ret = stream_->Reset();
  assert(!ret);
  log_ = stream_->GetLog();
}

int SkyObject::UpdateHelper(ceph::bufferlist& bl)
{
  int ret = stream_->Append(bl);
  return ret;
}

int SkyObject::QueryHelper()
{
  int ret = stream_->Sync();
  if (ret)
    return ret;

  for (;;) {
    uint64_t pos;
    ceph::bufferlist bl;
    ret = stream_->ReadNext(bl, &pos);
    switch (ret) {
      case 0:
        Apply(bl, pos);
        break;

      case -EBADF:
        return 0; // end of stream

      case -ENODEV:
        ret = log_->Fill(pos);
        if (!ret || ret == -EROFS)
          break; // try again
        return ret;

      case -EFAULT:
        break; // skip entry

      default:
        return ret;
    }
  }
}

}
