with import <nixpkgs>{};
stdenv.mkDerivation rec {
    name = "gem5-git";
    env = buildEnv { name = name; paths = buildInputs; };
    zlib-dev = lib.getDev zlib;
    zlib-lib = lib.getLib zlib;
    buildInputs = [
        mercurial
        git
        gcc
        scons
        gnum4
        swig
        pythonFull
        gperftools
        pcre
        zlib-dev
        zlib-lib
    ];
}
