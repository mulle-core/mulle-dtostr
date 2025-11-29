// Implementation of the Schubfach algorithm:
// https://drive.google.com/file/d/1IEeATSVnEE6TkrHlCYNY2GjaraBjOT4f.
// Copyright (c) 2025 - present, Victor Zverovich
// Distributed under the MIT license (see LICENSE).

namespace schubfach {

/// Writes the shortest correctly rounded decimal representation of `x` to
/// `buffer`. `buffer` size should be at least 25.
void dtoa_schubfach(double x, char* buffer) noexcept;

}  // namespace schubfach
