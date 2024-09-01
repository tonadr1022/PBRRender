#pragma once

namespace gl {

template <typename T>
class Buffer {
 public:
  Buffer() = default;

  ~Buffer() {
    if (mapped_) Unmap();
    if (id_) glDeleteBuffers(1, &id_);
  }

  void Init(uint32_t size_bytes, GLbitfield flags, const void* data) {
    if (id_) glDeleteBuffers(1, &id_);
    glCreateBuffers(1, &id_);
    glNamedBufferStorage(id_, size_bytes, data, flags);
  }

  Buffer(Buffer&& other) noexcept { *this = std::move(other); }

  Buffer& operator=(Buffer&& other) noexcept {
    if (&other == this) return *this;
    this->~Buffer();
    id_ = std::exchange(other.id_, 0);
    offset_ = std::exchange(other.offset_, 0);
    return *this;
  }

  void Bind(GLuint target) const { glBindBuffer(target, id_); }
  void BindBase(GLuint target, GLuint slot) const { glBindBufferBase(target, slot, id_); }

  void SubDataStart(size_t count, void* data) {
    glNamedBufferSubData(id_, 0, count * sizeof(T), data);
    num_allocs_ = count;
    offset_ += count * sizeof(T);
  }

  void SubData(size_t count, void* data) {
    glNamedBufferSubData(id_, offset_, count * sizeof(T), data);
    num_allocs_ += count;
    offset_ += count * sizeof(T);
  }

  void* Map(uint32_t access) {
    mapped_ = true;
    return glMapNamedBuffer(id_, access);
  }

  void ResetOffset() { offset_ = 0; }
  void SetOffset(uint32_t offset) { offset_ = offset; }

  [[nodiscard]] void* MapRange(uint32_t offset, uint32_t length_bytes, GLbitfield access) const {
    return glMapNamedBufferRange(id_, offset, length_bytes, access);
  }

  void Unmap() {
    mapped_ = false;
    glUnmapNamedBuffer(id_);
  }
  [[nodiscard]] inline uint32_t Id() const { return id_; }

  Buffer(Buffer& other) = delete;
  Buffer& operator=(Buffer& other) = delete;

  [[nodiscard]] uint32_t Offset() const { return offset_; }
  [[nodiscard]] uint32_t NumAllocs() const { return num_allocs_; }

 private:
  uint32_t offset_{0};
  uint32_t id_{0};
  bool mapped_{false};
  uint32_t num_allocs_{0};
};
}  // namespace gl
