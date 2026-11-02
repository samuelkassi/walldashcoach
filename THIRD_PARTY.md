# Third-party code

This project is MIT licensed. Nothing under a licence that conflicts with that may be copied in, and that includes SpeedFlipTrainer (GPL-3.0).

## Used

- **BakkesMod SDK**: headers and `pluginsdk.lib`, from your BakkesMod install. Not included in this repository.
- **Dear ImGui** v1.75 (MIT, Omar Cornut, https://github.com/ocornut/imgui): the core files in `third_party/imgui/`, with its licence alongside. Includes the stb headers it depends on (Sean Barrett, MIT or public domain). This copy came by way of BakkesmodPluginTemplate, without the template's add-ons. It needs to stay compatible with the version BakkesMod itself uses.
- **BakkesmodPluginTemplate** (Martinii89, https://github.com/Martinii89/BakkesmodPluginTemplate, no licence listed): `BakkesMod.props` and the general project layout come from it. To be reviewed before release. Early commits in this repository also contain its `logging.h`, `update_version.ps1` and `resource.h`, which have since been replaced or removed.

## Read for reference, nothing copied

- **FastAerialTrainer** (no licence found, https://github.com/Vyncc/FastAerialTrainer): event names and the idea of hooking them. The overlay's range bars take after its look, with their own code.
- **SpeedFlipTrainer** (tynidev, GPL-3.0, https://github.com/tynidev/SpeedFlipTrainer): that `GetDodgeDirection()`, `IsDodging()` and the engine physics frame counter exist and how they behave. The overlay takes after the look of its meters, with its own code.
