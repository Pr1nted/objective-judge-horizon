cask "ojh" do
  version "{{version}}"
  sha256 "{{sha256_macos}}"

  url "{{url_macos}}"
  name "Objective Judge Horizon"
  desc "Benchmark for turn-based strategy games"
  homepage "{{homepage}}"

  binary "ojh-#{version}-macos-universal/bin/ojh"
end
