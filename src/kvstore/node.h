#ifndef ZLOG_KVSTORE_NODE_H
#define ZLOG_KVSTORE_NODE_H
#include <cassert>
#include <memory>
#include <string>
#include <iostream>

class Node;
using NodeRef = std::shared_ptr<Node>;

/*
 * The read-only flag is a temporary hack for enforcing read-only property on
 * the connected Node. What is really needed is a more sophisticated approach
 * that avoids duplicating the read-only flag as well as what is probably some
 * call overhead associated with this design. Overall, this isn't pretty but
 * lets us have confidence in the correctness which is the priority right now.
 * There is probably a lot of overhead always returning copies of the
 * shared_ptr NodeRef.
 *
 * TODO: if a NodeRef was stored as nil to indicate that a nil pointer or
 * perhaps and empty tree then we need an extra way to do this because the ref
 * might be nil only because it isn't cached, in the general case.
 */
class NodePtr {
 public:

  NodePtr(const NodePtr& other) {
    ref_ = other.ref_;
    offset_ = other.offset_;
    csn_ = other.csn_;
    read_only_ = true;
  }

  NodePtr& operator=(const NodePtr& other) {
    assert(!read_only());
    ref_ = other.ref_;
    offset_ = other.offset_;
    csn_ = other.csn_;
    return *this;
  }

  NodePtr(NodePtr&& other) = delete;
  NodePtr& operator=(NodePtr&& other) & = delete;

  NodePtr(NodeRef ref, bool read_only) :
    ref_(ref), csn_(-1), offset_(-1), read_only_(read_only)
  {}

  inline bool read_only() const {
    return read_only_;
  }

  inline void set_read_only() {
    assert(!read_only());
    read_only_ = true;
  }

  inline NodeRef ref() const {
    return ref_;
  }

  inline void set_ref(NodeRef ref) {
    assert(!read_only());
    ref_ = ref;
  }

  inline int offset() const {
    return offset_;
  }

  inline void set_offset(int offset) {
    assert(!read_only());
    offset_ = offset;
  }

  inline int64_t csn() const {
    return csn_;
  }

  inline void set_csn(int64_t csn) {
    assert(!read_only());
    csn_ = csn;
  }

 private:
  NodeRef ref_;
  int64_t csn_;
  int offset_;
  bool read_only_;
};

/*
 * use signed types here and in protobuf so we can see the initial neg values
 */
class Node {
 public:
  NodePtr left;
  NodePtr right;

  // TODO: allow rid to have negative initialization value
  Node(std::string key, std::string val, bool red, NodeRef lr, NodeRef rr,
      uint64_t rid, int field_index, bool read_only, bool altered, bool depends) :
    left(lr, read_only), right(rr, read_only), key_(key), val_(val),
    red_(red), rid_(rid), field_index_(field_index), read_only_(read_only),
    altered_(altered), depends_(depends)
  {}

  static NodeRef& Nil() {
    static NodeRef node = std::make_shared<Node>("", "",
        false, nullptr, nullptr, (uint64_t)-1, -1, true,
        false, false);
    return node;
  }

  static NodeRef Copy(NodeRef src, uint64_t rid) {
    if (src == Nil())
      return Nil();

    auto node = std::make_shared<Node>(src->key(), src->val(), src->red(),
        src->left.ref(), src->right.ref(), rid, -1, false, false, false);

    node->left.set_csn(src->left.csn());
    node->left.set_offset(src->left.offset());

    node->right.set_csn(src->right.csn());
    node->right.set_offset(src->right.offset());

    return node;
  }

  inline bool read_only() const {
    return read_only_;
  }

  inline void set_read_only() {
    assert(!read_only());
    left.set_read_only();
    right.set_read_only();
    read_only_ = true;
  }

  inline bool red() const {
    return red_;
  }

  inline void set_red(bool red) {
    assert(!read_only());
    red_ = red;
  }

  inline void swap_color(NodeRef other) {
    assert(!read_only());
    assert(!other->read_only());
    std::swap(red_, other->red_);
  }

  inline int field_index() const {
    return field_index_;
  }

  inline void set_field_index(int field_index) {
    assert(!read_only());
    field_index_ = field_index;
  }

  inline uint64_t rid() const {
    return rid_;
  }

  // TODO: return const reference?
  inline std::string key() const {
    return key_;
  }

  // TODO: return const reference?
  inline std::string val() const {
    return val_;
  }

  /*
   * Note that the only caller of this method is a call performing an update
   * to an existing key-value pair. in this case we set depends_ on to be
   * true, but in general this may need to change if other set_value callers
   * are introduced.
   */
  inline void set_value(const std::string& value) {
    assert(!read_only());
    val_ = value;
    altered_ = true;
    depends_ = true;
  }

  inline bool altered() const {
    return altered_;
  }

  inline bool depends() const {
    return depends_;
  }

  inline void steal_payload(NodeRef& other) {
    assert(!read_only());
    assert(!other->read_only());
    key_ = std::move(other->key_);
    val_ = std::move(other->val_);
  }

  inline bool subtree_ro_dependent() const {
    assert(subtree_ro_dependent_set_);
    return subtree_ro_dependent_;
  }

  inline void set_subtree_ro_dependent(bool val) {
    assert(!subtree_ro_dependent_set_);
    subtree_ro_dependent_ = val;
    subtree_ro_dependent_set_ = true;
  }

  inline uint64_t vn() const {
    assert(vn_ >= 0);
    return vn_;
  }

  inline void set_vn(uint64_t vn) {
    assert(vn_ == -1);
    vn_ = vn;
  }

  inline uint64_t ssv() const {
    assert(ssv_ >= 0);
    return ssv_;
  }

  inline bool has_ssv() const {
    return ssv_ != -1;
  }

  inline void set_ssv(uint64_t ssv) {
    assert(!has_ssv());
    ssv_ = ssv;
  }

  inline uint64_t nsv() const {
    if (subtree_ro_dependent())
      return ssv();
    else
      return vn();
  }

 private:
  std::string key_;
  std::string val_;
  bool red_;
  uint64_t rid_;
  int field_index_;
  bool read_only_;
  bool altered_;
  bool depends_;

  // version number
  int64_t vn_ = -1;

  // source structure version
  int64_t ssv_ = -1;

  /*
   * TODO: instead of asserting a _set property like a tri-state variable, we
   * can lump a bunch together into states that reflect the life cycle of the
   * node. for instance things that are valid after serialization, or
   * something along these lines.
   */
  bool subtree_ro_dependent_;
  bool subtree_ro_dependent_set_ = false;
};

#endif
