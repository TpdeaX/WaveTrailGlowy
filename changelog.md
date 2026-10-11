# Changelog

## v1.2.1
* Fixed trail density and accumulation inconsistency across speed portals (0.5x, 1x, 2x, 3x, 4x) using equidistant spatial interpolation.
* Ensured consistent visual glow brightness and thickness regardless of game speed or display refresh rate (60Hz–360Hz).
* Prevented trail jump artifacts when switching game modes or respawning.

## v1.2.0
* Added Tail Fade-out (gradual decay towards the tail).
* Added Trail Tapering (gradually narrows trail width towards the tail).
* Added Neon Additive Blending toggle for hyper-bright bloom.
* Added Glow Shape presets (Circle, Diamond, Flare).
* Added Click Pulse Flash effect (CPS reactivity on wave clicks) with customizable flash color.
* Added toggle to disable glow during Practice Mode.
* Fixed trail sometimes not disappearing on death when "Glowy On Death" was disabled.
* Fixed trail continuing to produce glow during the level end animation transition.

## v1.0.0
* Ported to geode 2.2.
* Added Rainbow Const.
* Logo created by RadiacGD :D