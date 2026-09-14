{ lib, stdenv, fetchurl, cmake }:

stdenv.mkDerivation rec {
  pname = "ojh";
  version = "{{version}}";

  src = fetchurl {
    url = "{{url_source}}";
    sha256 = "{{sha256_source}}";
  };

  nativeBuildInputs = [ cmake ];

  doCheck = true;

  meta = with lib; {
    description = "{{summary}}";
    homepage = "{{homepage}}";
    platforms = platforms.unix;
    mainProgram = "ojh";
  };
}
