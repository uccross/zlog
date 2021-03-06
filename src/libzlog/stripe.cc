#include "stripe.h"
#include <sstream>

namespace zlog {

std::string Stripe::make_oid(uint64_t stripe_id, uint32_t index)
{
  std::stringstream oid;
  oid << stripe_id << "." << index;
  return oid.str();
}

std::string Stripe::make_oid(uint64_t stripe_id, uint32_t width, uint64_t position)
{
  auto index = position % width;
  return make_oid(stripe_id, index);
}

std::vector<std::string> Stripe::make_oids(const uint64_t stripe_id,
    const uint32_t width)
{
  std::vector<std::string> oids;
  oids.reserve(width);

  for (uint32_t i = 0; i < width; i++) {
    oids.emplace_back(make_oid(stripe_id, i));
  }

  return oids;
}

MultiStripe MultiStripe::decode(
    const flatbuffers::VectorIterator<
      flatbuffers::Offset<zlog::fbs::MultiStripe>,
      const zlog::fbs::MultiStripe*>& it)
{
  return MultiStripe(
      it->base_id(),
      it->width(),
      it->slots(),
      it->min_position(),
      it->instances(),
      it->max_position());
}

flatbuffers::Offset<zlog::fbs::MultiStripe> MultiStripe::encode(
    flatbuffers::FlatBufferBuilder& fbb) const
{
  return zlog::fbs::CreateMultiStripe(fbb,
      base_id_,
      width_,
      slots_,
      instances_,
      min_position_,
      max_position_);
}

nlohmann::json MultiStripe::dump() const
{
  nlohmann::json j;
  j["base_id"] = base_id_;
  j["width"] = width_;
  j["slots"] = slots_;
  j["min_position"] = min_position_;
  j["instances"] = instances_;
  j["max_position"] = max_position_;
  return j;
}

}
