{
	description = "Audio Driver";

	inputs = {
		nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
		flake-utils.url = "github:numtide/flake-utils";
	};

	outputs = { self, nixpkgs, flake-utils, ... }:
		let
		  pname = "echoes";
		  version = "0.0.1";
		in 
		flake-utils.lib.eachDefaultSystem (system:
			let pkgs = nixpkgs.legacyPackages.${system};
	      	in {
				packages.lib = pkgs.stdenv.mkDerivation {
					inherit pname version;
					src = self;

					nativeBuildInputs = [ pkgs.zig ];
					buildInputs = pkgs.lib.optionals pkgs.stdenv.hostPlatform.isLinux [ pkgs.alsa-lib ];

					buildPhase = ''
						zig build -Dname=${pname} -Dversion=${version}
					'';

					installPhase = ''
						mkdir -p $out/bin
						cp zig-out/lib/libead.a $out/bin
					'';

				};
	      	}
		);
}
