# Third-party code

This project is MIT licensed. Nothing under a licence that conflicts with that may be copied in, and that includes SpeedFlipTrainer (GPL-3.0).

## Used

- **BakkesMod SDK**: headers and `pluginsdk.lib`, from your BakkesMod install. Not included in this repository.
- **BakkesmodPluginTemplate** (Martinii89, https://github.com/Martinii89/BakkesmodPluginTemplate, no licence listed): `BakkesMod.props` and the general project layout come from it. To be reviewed before release. Early commits in this repository also contain its `logging.h`, `update_version.ps1` and `resource.h`, which have since been replaced or removed.

## Read for reference, nothing copied

- **FastAerialTrainer** (no licence found, https://github.com/Vyncc/FastAerialTrainer): event names and the idea of hooking them.
- **SpeedFlipTrainer** (tynidev, GPL-3.0, https://github.com/tynidev/SpeedFlipTrainer): that `GetDodgeDirection()`, `IsDodging()` and the engine physics frame counter exist and how they behave.
