# Upstream test data policy and iOS validation

Source: https://github.com/Dhaiwat10/cricket07-browser (MIT-licensed port infrastructure).

The upstream README explicitly states that the repository includes **no EA game files or game code**. The website requires the player to select a locally owned Cricket 07 installation. Therefore, do not scrape or attempt to retrieve game assets from the hosted website.

## Reuse directly
- tests/difftest.py, tests/difftest_api.c, tests/run_difftest.sh
- runtime/ and translator/ sources
- host/ and web/ compatibility implementations as references
- upstream build scripts for compiler settings and behavior

## Test progression
1. Validate upstream source and dependency versions; run static checks without game assets.
2. Compile portable runtime units for arm64 iOS simulator and device, resolving platform-specific assumptions.
3. Use synthetic test input to check guest memory bounds, virtual keys, timers and file-path handling.
4. Use upstream differential testing with a user-supplied licensed executable and function export, in a private environment.
5. Compare native iOS rendering and audio with the upstream browser port using the same user-owned installation.
6. Test controllers and application lifecycle on real iPhone hardware.

## Asset policy
- No proprietary executable, textures, soundbanks or stadium data in GitHub Actions artifacts.
- No credentials or signing certificates in the public repository.
- Imported files stay in app-private storage and are never uploaded by default.

## Known blocker
The upstream game translation is generated from the user's own executable. No playable iOS IPA can be built solely from the public repository.
