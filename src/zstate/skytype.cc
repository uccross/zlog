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

/*
 * Modifications made within a transactional context are saved and later
 * written as part of the commit record for this transaction.
 */
int SkyObject::UpdateHelper(ceph::bufferlist& bl, SkyObject::Transaction& txn)
{
  return 0;
}

/*
 * Doesn't roll the log foward, but instead saves the object id and version
 * being read in the transaction object.
 */
int SkyObject::QueryHelper(SkyObject::Transaction& txn)
{
  return 0;
}

/*
 * When playing back the stream we have to be careful with commit records. A
 * commit record describes a transaction that may involve multiple objects.
 * Before making a commit/abort decision based on the transaction's read set
 * the log is played forward for each object involved before checking for read
 * conflicts.
 *
 * Not all clients may host all objects, (from tango paper) "so writes or
 * reads can involve objects that are not locally hosted at the client that
 * generates the commit record or the client that encounters it."
 *
 * - Remote writes at the generating client
 *     - Generating client is the client that executed the transaction,
 *     created the commit record, and appended it to the log.
 *
 *     - Remote write is a write to an object that is not hosted by the client
 *     generating and executing the transaction.
 *
 *     - Client does not need to play a stream to append to it (TODO... how?),
 *     so the generating client can append commit record to stream of remote
 *     object.
 *
 *
 * - Remote writes at the consuming client
 *     - This is when a client is playing back a log and encounters a commit
 *     record that contains a write to an object that it doesn't locally host.
 *     - Update local objects, ignore updates to objects that it doesn't host.
 *
 * - Remote reads at the consuming client
 *     - Sees a commit record with a read set that contains an object that the
 *     consuming client doesn't locally host. So it can't compute commit/abort
 *     decision.
 *
 *     - Solution is to have the generating client immediately play the log
 *     forward until the commit point when it commits a transaction. Using the
 *     local commit/abort decision a decision record is appended to the same
 *     set of stream.
 *
 *     - Consuming clients that see the commit record wait for the decision
 *     record.
 *
 *     - Current implementation requires develoeprs to mark objects as
 *     requiring decision recods. Some sort of dynamic scheme might be
 *     possible later.
 *
 * - Remote reads at the generating client
 *     - Not supported.
 *     - Would require an RPC
 *     - Collaborative conflict resolution.
 *
 * - Decision record is optimization. Another client hosting read set can
 *   produce it. Or if needed, any client can reconstruct the state to make
 *   the decision.
 */
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
        // ReadNext returns the correct position for this error case
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
