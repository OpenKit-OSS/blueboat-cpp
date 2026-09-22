#include "blueboat/common/msgpack_codec.hpp"

#include <msgpack.hpp>

namespace blueboat {

namespace {

void pack_json(msgpack::packer<msgpack::sbuffer> &pk, const Value &value) {
  if (value.is_null()) {
    pk.pack_nil();
  } else if (value.is_boolean()) {
    if (value.get<bool>()) {
      pk.pack_true();
    } else {
      pk.pack_false();
    }
  } else if (value.is_number_unsigned()) {
    pk.pack_uint64(value.get<std::uint64_t>());
  } else if (value.is_number_integer()) {
    pk.pack_int64(value.get<std::int64_t>());
  } else if (value.is_number_float()) {
    pk.pack_double(value.get<double>());
  } else if (value.is_string()) {
    const auto &s = value.get_ref<const std::string &>();
    pk.pack_str(static_cast<std::uint32_t>(s.size()));
    pk.pack_str_body(s.data(), s.size());
  } else if (value.is_array()) {
    pk.pack_array(static_cast<std::uint32_t>(value.size()));
    for (const auto &element : value) {
      pack_json(pk, element);
    }
  } else if (value.is_object()) {
    pk.pack_map(static_cast<std::uint32_t>(value.size()));
    for (auto it = value.begin(); it != value.end(); ++it) {
      pk.pack_str(static_cast<std::uint32_t>(it.key().size()));
      pk.pack_str_body(it.key().data(), it.key().size());
      pack_json(pk, it.value());
    }
  } else {
    pk.pack_nil();
  }
}

Value unpack_object(const msgpack::object &obj) {
  switch (obj.type) {
  case msgpack::type::NIL:
    return nullptr;
  case msgpack::type::BOOLEAN:
    return obj.via.boolean;
  case msgpack::type::POSITIVE_INTEGER:
    return obj.via.u64;
  case msgpack::type::NEGATIVE_INTEGER:
    return obj.via.i64;
  case msgpack::type::FLOAT32:
  case msgpack::type::FLOAT64:
    return obj.via.f64;
  case msgpack::type::STR:
    return std::string(obj.via.str.ptr, obj.via.str.size);
  case msgpack::type::BIN:
    return std::string(obj.via.bin.ptr, obj.via.bin.size);
  case msgpack::type::ARRAY: {
    Value arr = Value::array();
    for (std::uint32_t i = 0; i < obj.via.array.size; i++) {
      arr.push_back(unpack_object(obj.via.array.ptr[i]));
    }
    return arr;
  }
  case msgpack::type::MAP: {
    Value map = Value::object();
    for (std::uint32_t i = 0; i < obj.via.map.size; i++) {
      auto &kv = obj.via.map.ptr[i];
      std::string key = kv.key.type == msgpack::type::STR ? std::string(kv.key.via.str.ptr, kv.key.via.str.size) : "";
      map[key] = unpack_object(kv.val);
    }
    return map;
  }
  case msgpack::type::EXT:
    return nullptr;
  default:
    return nullptr;
  }
}

} // namespace

std::string encode_msgpack(const Value &value) {
  msgpack::sbuffer buffer;
  msgpack::packer<msgpack::sbuffer> packer(buffer);
  pack_json(packer, value);
  return std::string(buffer.data(), buffer.size());
}

Value decode_msgpack(const std::string &bytes) {
  msgpack::object_handle handle = msgpack::unpack(bytes.data(), bytes.size());
  return unpack_object(handle.get());
}

} // namespace blueboat
