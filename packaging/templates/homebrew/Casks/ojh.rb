cask "ojh" do
  version "{{version}}"
  sha256 "{{sha256_macos}}"

  url "{{url_macos}}"
  name "Objective Judge Horizon"
  desc "Benchmark for turn-based strategy games"
  homepage "{{homepage}}"

  binary "ojh-#{version}-macos-universal/bin/ojh"

  caveats <<~EOS
    ojh is not notarized by Apple, so macOS may refuse to open it ("Apple could not verify ojh").
    Allow it once with:
      xattr -dr com.apple.quarantine "#{staged_path}"
  EOS
end
