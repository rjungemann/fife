class Fife < Formula
  desc "Send and receive MIDI and OSC messages from the command-line (cross-platform)"
  homepage "https://github.com/rjungemann/fife"
  license ""
  head "https://github.com/rjungemann/fife.git", branch: 'main'

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    bin.install %w[build/fife]
  end
end
