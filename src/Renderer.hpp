#pragma once

#include "gl/DynamicBuffer.hpp"
#include "gl/InternalTypes.hpp"
#include "types.hpp"

using MeshHandle = uint32_t;

class Renderer {
 public:
  void Init();
  template <typename VertexType, typename AllocFunc>
  MeshHandle AllocateVertices(uint32_t count, AllocFunc func) {
    uint32_t mesh_handle = next_mesh_handle_++;
    if constexpr (std::is_same_v<VertexType, Vertex>) {
      uint32_t offset;
      uint32_t handle = pos_tex_vbo_.Allocate(count, offset, func);
      mesh_allocs_.emplace(handle, VertexIndexAlloc{.vertex_handle = handle, .index_handle = 0});
    }
    return mesh_handle;
  }

  template <typename IndexType, typename AllocFunc>
  void AllocateIndices(MeshHandle mesh_handle, uint32_t count, AllocFunc func) {
    auto alloc_it = mesh_allocs_.find(mesh_handle);
    if (alloc_it == mesh_allocs_.end()) {
      spdlog::error(
          "mesh alloc not found. Must allocate vertices first and use the returned handle");
      EASSERT(0);
    }
    if (alloc_it->second.index_handle != 0) {
      // TODO: decide whether it's better to free?
      spdlog::error("mesh alloc already has an index allocation");
      return;
    }
    if constexpr (std::is_same_v<IndexType, uint32_t>) {
      uint32_t offset;
      alloc_it->second.index_handle = index_buffer_32_.Allocate(count, offset, func);
      alloc_it->second.index_buffer_idx = 1;
    } else if constexpr (std::is_same_v<IndexType, uint16_t>) {
      uint32_t offset;
      alloc_it->second.index_handle = index_buffer_16_.Allocate(count, offset, func);
      alloc_it->second.index_buffer_idx = 0;
    } else {
      spdlog::error("Index type must be uint32_t");
      EASSERT(0);
    }
  }

 private:
  DynamicBuffer<Vertex> pos_tex_vbo_;
  DynamicBuffer<uint32_t> index_buffer_32_;
  DynamicBuffer<uint16_t> index_buffer_16_;
  struct VertexIndexAlloc {
    uint32_t vertex_handle{};
    uint32_t index_handle{};
    // indices pointing the proper buffers to free from
    uint16_t vertex_buffer_idx{};
    // 0 for uint16_t, 1 for uint32_t
    uint16_t index_buffer_idx{};
  };

  std::unordered_map<uint32_t, VertexIndexAlloc> mesh_allocs_;
  uint32_t next_mesh_handle_{1};
};
