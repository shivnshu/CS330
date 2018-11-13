let
    pkgs = import <nixpkgs> {};
in
    { stdenv ? pkgs.stdenv }:

    stdenv.mkDerivation {
        name = "test";
        buildInputs = [
            pkgs.fuse
        ];
}
