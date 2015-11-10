#ifndef SKYTYPE_H
#define SKYTYPE_H
#include <rados/buffer.h>
#include "../libzlog.hpp"

namespace skytype {

class SkyObject {
 public:
  class Transaction {
    /*
     * Speculative commit records are produced and appended to the log. A
     * commit record contains a read set for isolation. The read set is a list
     * of (object, version) read by the transaction where the version is the
     * last offset in the shared log that modified the object. A transaction
     * succeeds if none of its reads are stale (objects unchanged since being
     * read).
     *
     *
     */
   public:
    // appends a commit record, and plays the log forward until the commit
    // point to determine abort/commit decision.
    //
    // read only transactions don't add a commit record but rather just play
    // the log forward to the current tail. after checking the tail from teh
    // sequencer, if there is nothing to read, the trasnactions can
    // commit/abort locally.
    //
    // write-only transactions append commit record, but can commit
    // immediately without playing log forward.
    //
    // map.insert(0) always succeeds woudl be an example of write-only xtn. if
    // we wanted semantics like an error if 0 already existed, then this is
    // actually a read and a write.
    //
    // uses multi-append for write-set
    //
    int Submit();
  };

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
  int UpdateHelper(ceph::bufferlist& bl, Transaction& txn);

  /*
   *
   */
  int QueryHelper();

  /*
   *
   */
  int QueryHelper(Transaction& txn);

 private:
  zlog::Log::Stream *stream_;
  zlog::Log *log_;
};

}

#endif
