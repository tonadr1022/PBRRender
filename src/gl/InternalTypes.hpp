#pragma once

template <typename T>
concept AllocFunc = std::is_invocable_v<T> && std::same_as<std::invoke_result_t<T>, void>;
