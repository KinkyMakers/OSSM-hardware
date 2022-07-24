{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  nativeBuildInputs = with pkgs; [ picocom grabserial clang wxmaxima gcc libelf ];
}
