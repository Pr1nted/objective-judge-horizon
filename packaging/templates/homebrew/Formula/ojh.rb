class Ojh < Formula
  desc "Benchmark for turn-based strategy games"
  homepage "{{homepage}}"
  url "{{url_source}}"
  sha256 "{{sha256_source}}"
  license {{license_ruby}}

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/ojh 2>&1", 2)
    system bin/"ojh", "selftest", "sha256"
  end
end
